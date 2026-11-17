#!/usr/bin/env python3
"""Say whether the project's compile database (`compile_commands.json`) can be trusted.

    python scripts/compdb-check.py [--root <project>] [--pins <pin file>] [--path <database>]
                                   [--build-file <file>]... [--source <file>]... [--json]

clangd is only as good as the compile database it reads: a stale one gives wrong diagnostics. This reads the
database the pin file names (`compdb.path` in `.conclave/toolchain-cpp.json`, `compile_commands.json` under the pin
file's `root` when it names none; `--path`, relative to `--root`, overrides it) and reports one status:

    current   it parses, every source file it names exists, and no build file is newer than it
    stale     a build file is newer than it, a source file it names no longer exists, or a source file asked
              for with --source has no entry in it (each is named)
    unknown   a build file cannot be read (one named by the pin file or by --build-file that is absent, or any
              whose time the system will not give), a source file the database names cannot be inspected, or
              the database is larger than this check reads, so freshness cannot be told
    invalid   it cannot be read, is not a JSON list of entries, or holds an entry that is not one (the entry is
              named by its number)
    missing   there is no file at its path

The build files are those of `--build-file` (relative to `--root`), else the pin file's `compdb.build_files`, else
`build.ninja`, `Makefile` and `CMakeLists.txt` where they exist in the pin file's `root` and beside the database.

A build that finds its sources by itself (a glob in a Makefile or in CMake) changes no build file when a source is
added, so the database stays `current` without the new file. `--source <file>` (relative to `--root`,
repeatable) asks whether that file has an entry: after adding or moving a source and regenerating the database,
name the file to prove it is there. A file is the same file under every name: links are followed on both sides.
Every file asked for that has no entry is named, however many (the 20 that bound the other lists do not apply).

It exits 0 for `current`, 1 for any other status, 2 for bad usage or a pin file that does not load. It never
regenerates the database: when the pin file gives the project's own command (`compdb.regenerate`), that command is
printed, quoted for the shell of the system it runs on and saying which (PowerShell on Windows, sh elsewhere), and
nothing is run. It reads at most 256 MiB of a database, reports, and writes nothing. Standard library only.
"""
import argparse
import importlib.util
import json
import os
import re
import shlex
import stat
import sys
from pathlib import Path

PINS = ".conclave/toolchain-cpp.json"
BUILD_FILES = ("build.ninja", "Makefile", "CMakeLists.txt")
NAMED = 20   # the most files a report names; the rest are counted
READ_LIMIT = 256 * 1024 * 1024   # bytes of a database this check reads: one larger is not checked, never loaded


def _pins_module():
    """toolchain-check.py, which owns the pin file's format, loaded from beside this script."""
    path = Path(__file__).resolve().parent / "toolchain-check.py"
    spec = importlib.util.spec_from_file_location("_toolchain_check", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def entry_fault(entry):
    """What is wrong with one entry of a compile database, or None. The format requires each entry's `directory`,
    its `file`, and the compile command as `command` (one string) or `arguments` (a list)."""
    if not isinstance(entry, dict):
        return "not an object"
    for key in ("directory", "file"):
        if key not in entry:
            return f"no `{key}`"
        if not isinstance(entry[key], str) or not entry[key].strip():
            return f"`{key}` is not a non-empty string"
    if "command" not in entry and "arguments" not in entry:
        return "neither `command` nor `arguments`"
    if "command" in entry and (not isinstance(entry["command"], str) or not entry["command"].strip()):
        return "`command` is not a command line"
    if "arguments" in entry:
        arguments = entry["arguments"]
        if (not isinstance(arguments, list) or not arguments
                or any(not isinstance(argument, str) for argument in arguments) or not arguments[0].strip()):
            return "`arguments` is not a list of strings that starts with the compiler"
    return None


def _shown(path, project):
    try:
        return path.relative_to(project).as_posix()
    except ValueError:
        return path.as_posix()


def _reason(exc):
    return exc.strerror or str(exc)


def _lexical_key(path):
    """A path as written, without `.` and `..`, in the system's own case: where it is, whatever it links to."""
    return os.path.normcase(os.path.normpath(str(path)))


def _same_file_key(path):
    """A path as text every name of one file shares: links followed (a build may reach `src/a.c` through a linked
    folder, and its entry is that file's all the same), no `.` or `..`, and the system's own rule for case. A
    path the system will not resolve is compared as it is written."""
    try:
        return os.path.normcase(os.path.realpath(str(path)))
    except (OSError, ValueError):
        return _lexical_key(path)


def database_path(project, pin_file, path=None):
    """Where a loaded pin file's compile database is: `path`, relative to the project, overrides it."""
    project = Path(project).resolve()
    return (project / path) if path is not None else (project / pin_file["root"] / pin_file["compdb"]["path"])


def read_database(database, shown, read_limit=READ_LIMIT):
    """A compile database read and checked: (entries, written, None) where `written` is its time in nanoseconds,
    or (None, None, (status, detail)) when it is `missing`, `invalid` or larger than `read_limit` (`unknown`).
    `shown` is how the file is named in the detail. No fault of the file system raises: it is a status. The one
    reader of the format: hardening-check.py reads through it too."""
    try:
        with open(database, "rb") as handle:
            raw = handle.read(read_limit + 1)   # bounded: the file is whatever the project holds
        written = Path(database).stat().st_mtime_ns
    except FileNotFoundError:
        return None, None, ("missing", f"{shown} does not exist")
    except OSError as exc:
        return None, None, ("invalid", f"{shown} cannot be read ({_reason(exc)})")
    if len(raw) > read_limit:
        return None, None, ("unknown", f"{shown} is larger than the {read_limit} bytes this check reads, so it "
                                       "was not checked")
    try:
        entries = json.loads(raw.decode("utf-8"))
    except UnicodeDecodeError as exc:
        return None, None, ("invalid", f"{shown} is not UTF-8 ({exc.reason})")
    except (ValueError, RecursionError) as exc:
        return None, None, ("invalid", f"{shown} is not JSON ({exc})")
    if not isinstance(entries, list):
        return None, None, ("invalid", f"{shown} is not a JSON list of entries")
    if not entries:
        return None, None, ("invalid", f"{shown} holds no entry, so it describes no build")
    for number, entry in enumerate(entries, 1):
        fault = entry_fault(entry)
        if fault:
            return None, None, ("invalid", f"{shown}: entry {number}: {fault}")
    return entries, written, None


def entry_source(entry, database):
    """The source file an entry of a compile database compiles: its `file`, under its `directory`, which is under
    the database's folder when relative."""
    directory = Path(entry["directory"])
    if not directory.is_absolute():
        directory = Path(database).parent / directory
    source_file = Path(entry["file"])
    return source_file if source_file.is_absolute() else directory / source_file


def check_database(project, pin_file, path=None, build_files=None, read_limit=READ_LIMIT, sources=None,
                   entries_out=None):
    """The state of the compile database of a project whose pin file is loaded: {"path", "status", "detail",
    "entries", "newer", "gone", "unlisted", "regenerate"}. `path` and `build_files`, relative to the project,
    override the pin file's. `sources`, relative to the project, are files that must each have an entry: one
    without is listed under "unlisted" and makes the database stale. `entries_out`, a list, receives the entries
    read (hardening-check.py checks them in the same read). No fault of the file system raises: it is a status."""
    project = Path(project).resolve()
    base = project / pin_file["root"]
    compdb = pin_file["compdb"]
    database = database_path(project, pin_file, path)
    result = {"path": _shown(database, project), "status": None, "detail": "", "entries": None, "newer": [],
              "gone": [], "unlisted": [], "regenerate": list(compdb["regenerate"])}

    def ended(status, detail):
        result.update(status=status, detail=detail)
        return result

    entries, written, refused = read_database(database, result["path"], read_limit)
    if refused:
        return ended(*refused)
    if entries_out is not None:
        entries_out.extend(entries)
    result["entries"] = len(entries)

    # The build files: named ones must be readable, the usual ones are compared where they exist.
    if build_files is not None:
        files, source = [project / file for file in build_files], "--build-file"
    elif compdb["build_files"] is not None:
        files, source = [base / file for file in compdb["build_files"]], "the pin file"
    else:
        # Each once: the database's folder is the root itself when the database is kept there.
        usual = [folder / name for folder in (base, database.parent) for name in BUILD_FILES]
        files, source = list(dict.fromkeys(usual)), None
    times, unreadable = {}, []
    for file in files:
        try:
            times[file] = file.stat().st_mtime_ns
        except OSError as exc:
            absent = isinstance(exc, (FileNotFoundError, NotADirectoryError))
            if source is None and absent:
                continue   # a usual build file the project does not have is no fault; one it cannot read is
            unreadable.append(f"{_shown(file, project)} ({'it does not exist' if absent else _reason(exc)})")
    result["newer"] = [_shown(file, project) for file, changed in times.items() if changed > written]

    gone, hidden, listed = {}, {}, set()
    for entry in entries:
        source_file = entry_source(entry, database)
        listed.add(_same_file_key(source_file))
        if source_file in gone or source_file in hidden:
            continue
        # Asked of the system directly: `is_file` raises for what the system refuses to show, and a file that
        # cannot be inspected is neither there nor gone.
        try:
            there = stat.S_ISREG(source_file.stat().st_mode)
        except (FileNotFoundError, NotADirectoryError, ValueError):
            there = False
        except OSError as exc:
            hidden[source_file] = f"{_shown(source_file, project)} ({_reason(exc)})"
            continue
        if not there:
            gone[source_file] = _shown(source_file, project)
    result["gone"] = list(gone.values())[:NAMED]
    # Every one is named, however many: they are the caller's own list, each asked about by name.
    unlisted = [_shown(project / source, project) for source in dict.fromkeys(sources or ())
                if _same_file_key(project / source) not in listed]
    result["unlisted"] = unlisted

    # What is certain (stale) and what could not be told (unknown) are both reported, whichever sets the status:
    # a stale database with a file that could not be inspected names that file too.
    certain, untold = [], []
    if result["newer"]:
        certain.append(f"older than {', '.join(result['newer'])}")
    if gone:
        more = f" and {len(gone) - NAMED} more" if len(gone) > NAMED else ""
        certain.append(f"{len(gone)} source file(s) it names no longer exist: {', '.join(result['gone'])}{more}")
    if unlisted:
        certain.append(f"{len(unlisted)} source file(s) asked for have no entry in it: {', '.join(unlisted)}")
    so = "" if certain else ", so its freshness cannot be told"
    if unreadable:
        origin = f"named by {source}" if source else "of the project"
        untold.append(f"a build file {origin} cannot be read{so}: {', '.join(unreadable)}")
    if hidden:
        names = list(hidden.values())
        more = f" and {len(names) - NAMED} more" if len(names) > NAMED else ""
        untold.append(f"{len(names)} source file(s) it names cannot be inspected{so}: "
                      f"{', '.join(names[:NAMED])}{more}")
    if certain or untold:
        return ended("stale" if certain else "unknown", f"{result['path']}: " + "; ".join(certain + untold))
    count = "1 entry" if len(entries) == 1 else f"{len(entries)} entries"
    return ended("current", f"{result['path']}, {count}")


_POWERSHELL_BARE = re.compile(r"[A-Za-z0-9_/\\-]+")


def shell_line(arguments, windows):
    """A command as one line that can be pasted into a shell, and which shell: PowerShell on Windows, sh
    elsewhere. PowerShell: every argument that is not plain letters, digits, `_`, `-`, `/` and `\\` is put in
    single quotes (an unquoted `-DNAME=1.2` is split at its dot), and a quoted program needs the call operator."""
    if not windows:
        return "sh", shlex.join(arguments)
    quoted = [argument if _POWERSHELL_BARE.fullmatch(argument) else "'" + argument.replace("'", "''") + "'"
              for argument in arguments]
    return "PowerShell", ("& " if quoted[0] != arguments[0] else "") + " ".join(quoted)


def regenerate_line(result, windows=None):
    """The project's own command that writes the database again, for a database that is not current, or None.
    It is shown, never run: a check that builds is not read-only."""
    if result["status"] == "current" or not result["regenerate"]:
        return None
    shell, line = shell_line(result["regenerate"], os.name == "nt" if windows is None else windows)
    return f"regenerate with ({shell}): {line}"


def main(argv=None):
    parser = argparse.ArgumentParser(prog="compdb-check.py",
                                     description="Say whether the compile database can be trusted. Runs nothing.")
    parser.add_argument("--root", default=".", help="the project (default: the working directory)")
    parser.add_argument("--pins", help=f"the pin file (default: {PINS} under the root)")
    parser.add_argument("--path", help="the compile database, relative to the root (default: the pin file's)")
    parser.add_argument("--build-file", action="append", help="a file the database must not be older than, relative "
                                                              "to the root (repeatable; default: the pin file's)")
    parser.add_argument("--source", action="append", help="a source file that must have an entry in the database, "
                                                          "relative to the root (repeatable)")
    parser.add_argument("--json", action="store_true", help="print the report as one JSON document")
    args = parser.parse_args(argv)
    project = Path(args.root).resolve()
    # Where the file is named, not where a link leads: a source reached through a link inside the project is the
    # project's to ask about.
    inside = _lexical_key(project) + os.sep
    for source in args.source or ():
        if not (_lexical_key(project / source) + os.sep).startswith(inside) or \
                _lexical_key(project / source) == _lexical_key(project):
            parser.error(f"--source {source} is not a file inside the project ({project.as_posix()})")
    pins = Path(args.pins) if args.pins else project / PINS
    module = _pins_module()
    try:
        pin_file = module.load_pin_file(pins)
    except module.PinError as exc:
        print(f"compdb-check: {exc}", file=sys.stderr)
        if args.json:
            print(json.dumps({"path": None, "status": "refused", "detail": str(exc), "entries": None, "newer": [],
                              "gone": [], "unlisted": [], "regenerate": []}, indent=2))
        return 2
    result = check_database(project, pin_file, path=args.path, build_files=args.build_file, sources=args.source)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"compile database: {result['status']}: {result['detail']}")
        line = regenerate_line(result)
        if line:
            print(line)
    return 0 if result["status"] == "current" else 1


if __name__ == "__main__":
    sys.exit(main())

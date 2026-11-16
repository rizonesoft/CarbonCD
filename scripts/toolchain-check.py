#!/usr/bin/env python3
"""Check the build tools a project pins: which are installed, and whether each is new enough.

    python scripts/toolchain-check.py --pins .conclave/toolchain-cpp.json [--json] [--root <project>]

The pin file names each tool, the commands that satisfy it in order of preference and, for each command, how to
ask its version and the least version the project accepts:

    {"schema_version": 1,
     "tools": [{"name": "compiler-cache",
                "commands": [{"command": "ccache", "version_args": ["--version"],
                              "version_pattern": "ccache version ([0-9.]+)", "min_version": "4.0"},
                             {"command": "sccache", "version_args": ["--version"],
                              "version_pattern": "sccache ([0-9.]+)", "min_version": "0.5"}],
                "install": "apt install ccache", "optional": true}]}

A pin file may also name where the project keeps tools of its own, which no PATH reaches:

    {"schema_version": 1, "root": "platform", "search": ["vendor/bin", "node_modules/.bin"], "tools": [...]}

`root` is the app's folder inside the project (`.` when left out) and `search` lists folders inside it; a command
is looked for there first, then on PATH, and never in the working directory.

A pin file may say where the project's compile database is and what it must not be older than, for
compdb-check.py (each key optional; the files are read under `root`):

    {"schema_version": 1, "tools": [...],
     "compdb": {"path": "build/compile_commands.json", "build_files": ["CMakeLists.txt", "src/flags.cmake"],
                "regenerate": ["cmake", "-S", ".", "-B", "build"]}}

It prints one line per tool (its name, one word, the detail) and exits 3 when a tool that is not optional is not
ok, 2 when the pin file does not load. The word is `ok`, `setup_required` for a required tool that is not ok, and
`optional` for a tool the pin marks optional that is not ok, so a reader never takes a tool the project may do
without for setup it needs. `--json` keeps the two facts apart: `status` (`ok` or `setup_required`) and `optional`.
It reads and reports: it installs nothing and writes nothing. Standard library only.
"""
import argparse
import json
import os
import re
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path, PurePosixPath

TIME_LIMIT = 15.0        # seconds a tool gets to print its version
OUTPUT_CAP = 64 * 1024   # bytes read from a tool before it is stopped
PATTERN_LIMIT = 5.0      # seconds a version pattern gets to read that output
PATTERN_LENGTH = 500     # characters a version pattern may hold

NAME = re.compile(r"[a-z0-9][a-z0-9+.-]*")
COMMAND = re.compile(r"[A-Za-z0-9][A-Za-z0-9+._-]*")  # a bare name: found on PATH, never a path into the project
# What a version argument may hold. On Windows a .cmd launcher is started through cmd.exe, which reads other
# characters as its own syntax; none of them is needed to ask a tool its version.
ARGUMENT = re.compile(r"[A-Za-z0-9=_./:+-]+")
# At most eight parts of at most nine digits: no release is numbered past that, and a longer run of digits is
# output to refuse, not a number to convert.
VERSION = re.compile(r"[0-9]{1,9}(?:\.[0-9]{1,9}){0,7}")
TOP_KEYS = {"schema_version", "tools"}
TOP_OPTIONAL = {"root", "search", "compdb"}
COMPDB_KEYS = ("path", "build_files", "regenerate")
COMPDB_PATH = "compile_commands.json"
TOOL_KEYS = {"name", "commands", "install", "optional"}
COMMAND_KEYS = {"command", "version_args", "version_pattern", "min_version"}


class PinError(ValueError):
    """A pin file that cannot be used: the message names the file and, where there is one, the tool and the key."""


def _keys(where, found, required, optional=()):
    unknown = sorted(set(found) - set(required) - set(optional))
    if unknown:
        raise PinError(f"{where}: unknown key {unknown[0]!r}")
    missing = sorted(set(required) - set(found))
    if missing:
        raise PinError(f"{where}: missing key {missing[0]!r}")


def _load_command(where, entry):
    if not isinstance(entry, dict):
        raise PinError(f"{where}: a command is an object with the keys {', '.join(sorted(COMMAND_KEYS))}")
    name = entry.get("command")
    if not isinstance(name, str) or not COMMAND.fullmatch(name):
        raise PinError(f"{where}: command: a bare program name, not a path: {name!r}")
    where = f"{where}: command {name!r}"
    _keys(where, entry, COMMAND_KEYS)
    args = entry["version_args"]
    if not isinstance(args, list) or not all(isinstance(arg, str) for arg in args):
        raise PinError(f"{where}: version_args: a list of strings")
    for arg in args:
        if not ARGUMENT.fullmatch(arg):
            raise PinError(f"{where}: version_args: {arg!r} holds a character outside A-Z a-z 0-9 = _ . / : + -")
    pattern = entry["version_pattern"]
    if not isinstance(pattern, str):
        raise PinError(f"{where}: version_pattern: a regular expression as text")
    if len(pattern) > PATTERN_LENGTH:
        raise PinError(f"{where}: version_pattern: longer than {PATTERN_LENGTH} characters")
    try:
        compiled = re.compile(pattern)
    except (re.error, RecursionError, OverflowError) as exc:
        raise PinError(f"{where}: version_pattern: does not compile ({exc or type(exc).__name__})") from exc
    if compiled.groups != 1:
        raise PinError(f"{where}: version_pattern: needs exactly one group, has {compiled.groups}")
    minimum = entry["min_version"]
    if not isinstance(minimum, str) or not VERSION.fullmatch(minimum):
        raise PinError(f"{where}: min_version: dotted numbers such as 1.10, not {str(minimum)[:40]!r}")
    return {"command": name, "version_args": list(args), "version_pattern": pattern, "min_version": minimum}


def _inside(value):
    """Whether a folder named in a pin file stays inside the project: relative, with no `..` and no drive."""
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    parts = PurePosixPath(value).parts
    return not value.startswith("/") and ".." not in parts and not (parts and ":" in parts[0])


def load_pins(path):
    """The tools a pin file names, checked: a list of {"name", "commands", "install", "optional"}.

    Raises PinError, naming the file and what is wrong with it, for anything the format does not allow."""
    return load_pin_file(path)["tools"]


def load_pin_file(path):
    """A pin file, checked: {"tools": [...], "root": the app's folder inside the project, "search": the folders
    inside it where the project keeps tools of its own}."""
    where = Path(path).as_posix()
    try:
        data = json.loads(Path(path).read_bytes().decode("utf-8"))
    except OSError as exc:
        raise PinError(f"{where}: cannot be read ({exc.strerror or exc})") from exc
    except UnicodeDecodeError as exc:
        raise PinError(f"{where}: not UTF-8 ({exc})") from exc
    except (ValueError, RecursionError) as exc:
        raise PinError(f"{where}: not JSON ({exc})") from exc
    if not isinstance(data, dict):
        raise PinError(f"{where}: a pin file is an object with the keys schema_version and tools")
    _keys(where, data, TOP_KEYS, TOP_OPTIONAL)
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise PinError(f"{where}: schema_version: must be 1")
    root = data.get("root", ".")
    if not _inside(root):
        raise PinError(f"{where}: root: the app's folder inside the project, not {root!r}")
    search = data.get("search", [])
    if not isinstance(search, list):
        raise PinError(f"{where}: search: a list of folders inside the project")
    for folder in search:
        if not _inside(folder) or not PurePosixPath(folder).parts:
            raise PinError(f"{where}: search: a folder inside the project, not {folder!r}")
    compdb = _load_compdb(where, data)
    if not isinstance(data["tools"], list):
        raise PinError(f"{where}: tools: a list of tools")
    tools, seen = [], set()
    for number, tool in enumerate(data["tools"], 1):
        if not isinstance(tool, dict):
            raise PinError(f"{where}: tool {number}: a tool is an object with the keys {', '.join(sorted(TOOL_KEYS))}")
        name = tool.get("name")
        if not isinstance(name, str) or not NAME.fullmatch(name):
            raise PinError(f"{where}: tool {number}: name: lowercase letters, digits, +, . and -, not {name!r}")
        tool_where = f"{where}: tool {name!r}"
        if name in seen:
            raise PinError(f"{tool_where}: name: used by two tools")
        seen.add(name)
        _keys(tool_where, tool, TOOL_KEYS - {"optional"}, ("optional",))
        if not isinstance(tool["commands"], list) or not tool["commands"]:
            raise PinError(f"{tool_where}: commands: a list of at least one command")
        commands = [_load_command(tool_where, entry) for entry in tool["commands"]]
        install = tool["install"]
        if not isinstance(install, str) or not install.strip() or "\n" in install or "\r" in install:
            raise PinError(f"{tool_where}: install: a one-line hint")
        optional = tool.get("optional", False)
        if not isinstance(optional, bool):
            raise PinError(f"{tool_where}: optional: true or false")
        tools.append({"name": name, "commands": commands, "install": install, "optional": optional})
    return {"tools": tools, "root": PurePosixPath(root).as_posix(),
            "search": [PurePosixPath(folder).as_posix() for folder in search], "compdb": compdb}


def _load_compdb(where, data):
    """A pin file's `compdb`, checked: {"path": the compile database, "build_files": the files it must not be
    older than (None: the usual ones, where they exist), "regenerate": the project's own command that writes it
    again, as arguments (shown by compdb-check.py, never run)}. The files are named under the pin file's `root`."""
    loaded = {"path": COMPDB_PATH, "build_files": None, "regenerate": []}
    if "compdb" not in data:
        return loaded
    where = f"{where}: compdb"
    compdb = data["compdb"]
    if not isinstance(compdb, dict):
        raise PinError(f"{where}: an object with the optional keys {', '.join(COMPDB_KEYS)}")
    _keys(where, compdb, (), COMPDB_KEYS)

    def a_file(value):
        return _inside(value) and bool(PurePosixPath(value).parts)

    if "path" in compdb:
        if not a_file(compdb["path"]):
            raise PinError(f"{where}: path: a file inside the project, not {compdb['path']!r}")
        loaded["path"] = PurePosixPath(compdb["path"]).as_posix()
    if "build_files" in compdb:
        files = compdb["build_files"]
        if not isinstance(files, list) or not files:
            raise PinError(f"{where}: build_files: a list of at least one file inside the project")
        for file in files:
            if not a_file(file):
                raise PinError(f"{where}: build_files: a file inside the project, not {file!r}")
        loaded["build_files"] = [PurePosixPath(file).as_posix() for file in files]
    if "regenerate" in compdb:
        command = compdb["regenerate"]
        if not isinstance(command, list) or not command:
            raise PinError(f"{where}: regenerate: a command as a list of at least one argument")
        for argument in command:
            if not isinstance(argument, str) or not argument.strip() or "\n" in argument or "\r" in argument:
                raise PinError(f"{where}: regenerate: each argument is a one-line string, not {argument!r}")
        loaded["regenerate"] = list(command)
    return loaded


def search_folders(project, pin_file):
    """The folders a loaded pin file's commands are looked for in before PATH, under the project root."""
    base = Path(project) / pin_file["root"]
    return [base / folder for folder in pin_file["search"]]


def find_command(name, folders=()):
    """The file a command is, or None: the first of `folders` that holds it, then PATH. Never the working
    directory, which on Windows the standard lookup tries first: a file there named like a pinned tool is not that
    tool. It must be a file the system will execute; on Windows the PATHEXT suffixes are tried, which is how the
    `.bat` and `.cmd` launchers of Composer and npm are found."""
    windows = os.name == "nt"
    suffixes = [ext for ext in os.environ.get("PATHEXT", ".COM;.EXE;.BAT;.CMD").split(os.pathsep) if ext] \
        if windows else []
    names = [name]
    if windows and Path(name).suffix.upper() not in {ext.upper() for ext in suffixes}:
        names = [name + ext for ext in suffixes]
    places = [Path(folder) for folder in folders]
    places += [Path(folder) for folder in os.environ.get("PATH", "").split(os.pathsep) if folder]
    for folder in places:
        for candidate in names:
            path = folder / candidate
            if path.is_file() and os.access(path, os.X_OK):
                return str(path)
    return None


def parse_version(text):
    """A captured version as a tuple of numbers, or None when the capture is not dotted numbers."""
    if not isinstance(text, str) or not VERSION.fullmatch(text):
        return None
    return tuple(int(part) for part in text.split("."))


def compare_versions(found, wanted):
    """-1, 0 or 1 as `found` is older than, the same as or newer than `wanted`, number by number. A missing
    trailing part is zero, so 1.2 is 1.2.0, and 1.10 is newer than 1.9."""
    left, right = parse_version(found), parse_version(wanted)
    if left is None or right is None:
        raise ValueError(f"not a version: {found if left is None else wanted!r}")
    width = max(len(left), len(right))
    left += (0,) * (width - len(left))
    right += (0,) * (width - len(right))
    return (left > right) - (left < right)


def _windows_job(proc):
    """A Windows job object holding the tool, or None when one cannot be made. A job holds every process the tool
    starts, so they can be ended after the tool itself has exited, when nothing else names them."""
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.CreateJobObjectW.argtypes = [wintypes.LPVOID, wintypes.LPCWSTR]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    job = kernel32.CreateJobObjectW(None, None)
    if not job:
        return None
    handle = kernel32.OpenProcess(0x0101, False, proc.pid)  # PROCESS_SET_QUOTA | PROCESS_TERMINATE
    assigned = bool(handle) and bool(kernel32.AssignProcessToJobObject(job, handle))
    if handle:
        kernel32.CloseHandle(handle)
    if not assigned:
        kernel32.CloseHandle(job)
        return None
    return job


def _windows_resume(proc):
    """Let a tool that was started suspended run. It is started suspended so that it is in its job before it can
    start anything: a child made before the job existed would be outside it. True when the tool is running."""
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    ntdll = ctypes.WinDLL("ntdll")
    ntdll.NtResumeProcess.restype = wintypes.LONG
    ntdll.NtResumeProcess.argtypes = [wintypes.HANDLE]
    handle = kernel32.OpenProcess(0x0800, False, proc.pid)  # PROCESS_SUSPEND_RESUME
    if not handle:
        return False
    try:
        return ntdll.NtResumeProcess(handle) == 0
    finally:
        kernel32.CloseHandle(handle)


def _windows_job_end(job):
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.TerminateJobObject(job, 1)
    kernel32.CloseHandle(job)


def _end_tree(proc, job=None):
    """End a tool and everything it started: a version command must leave nothing behind, whether it hung, flooded
    or exited with a child still running.

    Each step may find the process already gone, which is the outcome wanted, so that is not an error here; the
    caller's result still says which limit was hit."""
    if os.name == "nt":
        # With no job the tool was never resumed (run_bounded refuses to run what it cannot contain), so the
        # suspended process ended below is the whole tree.
        if job is not None:
            _windows_job_end(job)
    else:
        try:
            os.killpg(proc.pid, signal.SIGKILL)  # the tool leads its own session, so the group is its tree
        except (ProcessLookupError, PermissionError):
            pass
    try:
        proc.kill()
    except OSError:
        pass
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass


def run_bounded(argv, time_limit=TIME_LIMIT, output_cap=OUTPUT_CAP):
    """Run `argv` with no shell and no console window, reading standard output and standard error together.

    Returns {"output", "exit", "stopped", "error"}: `stopped` is "time" or "output" when a limit ended the run, and
    `error` is the system's reason when the program could not be started. Whatever the program started is ended
    with it on every way out."""
    windows = os.name == "nt"
    try:
        # Contained before it runs, so its whole tree can be ended. POSIX: its own session, made before the
        # program is executed. Windows: started suspended (CREATE_SUSPENDED), put in a job object, then resumed.
        proc = subprocess.Popen(
            argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            creationflags=(subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP | 0x00000004)
            if windows else 0,
            start_new_session=not windows)
    except OSError as exc:
        return {"output": "", "exit": None, "stopped": None, "error": exc.strerror or str(exc)}
    job, reader, stopped, error = None, None, None, None
    chunks, over = [], threading.Event()

    def read():
        total = 0
        while True:
            try:
                data = proc.stdout.read1(4096)
            except (OSError, ValueError):
                return
            if not data:
                return
            room = output_cap - total
            chunks.append(data[:max(room, 0)])
            total += len(data)
            if total > output_cap:
                over.set()
                return

    try:
        if windows:
            # A tool that cannot be contained is not run at all: it stays suspended and is ended below.
            job = _windows_job(proc)
            if job is None:
                error = "Windows did not let the program be put in a job object, so it was not run"
            elif not _windows_resume(proc):
                error = "Windows did not let the program be resumed, so it was not run"
        if error is None:
            reader = threading.Thread(target=read, daemon=True)
            reader.start()
            deadline = time.monotonic() + time_limit
            while reader.is_alive() and time.monotonic() < deadline:
                reader.join(0.05)
            if over.is_set():
                stopped = "output"
            elif reader.is_alive():
                stopped = "time"
            else:
                try:
                    proc.wait(timeout=max(deadline - time.monotonic(), 0.05))
                except subprocess.TimeoutExpired:
                    stopped = "time"
    finally:
        # Every way out from the moment the process exists, an interrupt (Ctrl+C) and a clean exit included: the
        # tool leads its own session, so nothing else ends what it started, and a launcher can exit while a child
        # it started runs on.
        _end_tree(proc, job)
        if reader is not None:
            reader.join(5)
        proc.stdout.close()
    if error is not None:
        return {"output": "", "exit": None, "stopped": None, "error": error}
    return {"output": b"".join(chunks).decode("utf-8", errors="replace"), "exit": proc.returncode,
            "stopped": stopped, "error": None}


_SEARCH = """import json, re, sys
job = json.load(sys.stdin)
found = re.search(job['pattern'], job['text'])
json.dump({'matched': found is not None, 'group': found.group(1) if found else None}, sys.stdout)
"""


def search_bounded(pattern, text, limit=None):
    """`re.search(pattern, text)` under a time limit: {"matched", "group"}, or None when it did not finish.

    The standard library cannot stop a match that backtracks without end, and a thread running one cannot be
    stopped either, so the match runs in a second interpreter that can."""
    limit = PATTERN_LIMIT if limit is None else limit
    try:
        ran = subprocess.run([sys.executable, "-I", "-S", "-c", _SEARCH], capture_output=True, timeout=limit,
                             input=json.dumps({"pattern": pattern, "text": text}).encode("utf-8"),
                             creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        return json.loads(ran.stdout.decode("utf-8")) if ran.returncode == 0 else None
    except (subprocess.TimeoutExpired, OSError, ValueError):
        return None


def check_tool(pin, time_limit=TIME_LIMIT, output_cap=OUTPUT_CAP, folders=()):
    """One pinned tool: {"name", "status", "detail", "version", "command", "optional"}, `status` being `ok` or
    `setup_required`. The first of its commands found (in `folders`, then on PATH) is the one checked, against
    its own minimum."""
    result = {"name": pin["name"], "status": "setup_required", "detail": "", "version": None, "command": None,
              "optional": pin["optional"]}
    for entry in pin["commands"]:
        path = find_command(entry["command"], folders)
        if path:
            break
    else:
        names = ", ".join(entry["command"] for entry in pin["commands"])
        looked = "in the project's tool folders or on PATH" if folders else "on PATH"
        result["detail"] = f"not installed ({names} not {looked}). Install: {pin['install']}"
        return result
    result["command"] = path
    shown = " ".join([path, *entry["version_args"]])
    ran = run_bounded([path, *entry["version_args"]], time_limit, output_cap)
    if ran["error"] is not None:
        result["detail"] = f"{path} could not be started: {ran['error']}"
    elif ran["stopped"] == "time":
        result["detail"] = f"{shown} did not answer within {time_limit:g} seconds and was stopped"
    elif ran["stopped"] == "output":
        result["detail"] = f"{shown} printed more than {output_cap} bytes and was stopped"
    elif ran["exit"] != 0:
        result["detail"] = f"{shown} exited {ran['exit']}"
    else:
        found = search_bounded(entry["version_pattern"], ran["output"])
        if found is None:
            result["detail"] = (f"{shown}: the pattern {entry['version_pattern']!r} did not finish reading its output "
                                f"within {PATTERN_LIMIT:g} seconds")
        elif not found["matched"]:
            result["detail"] = f"{shown}: the pattern {entry['version_pattern']!r} does not match its output"
        elif parse_version(found["group"]) is None:
            result["detail"] = f"{shown}: the pattern captured {str(found['group'])[:40]!r}, which is not a version"
        else:
            result["version"] = found["group"]
            if compare_versions(found["group"], entry["min_version"]) < 0:
                result["detail"] = (f"{path} is {found['group']}, older than the pinned {entry['min_version']}. "
                                    f"Install: {pin['install']}")
            else:
                result["status"], result["detail"] = "ok", path
    return result


def check_tools(pins, time_limit=TIME_LIMIT, output_cap=OUTPUT_CAP, folders=()):
    """Every pinned tool, in the file's order: one that fails never stops the next from being checked."""
    return [check_tool(pin, time_limit, output_cap, folders) for pin in pins]


def main(argv=None):
    parser = argparse.ArgumentParser(description="Check the build tools a pin file names. Installs nothing.")
    parser.add_argument("--pins", required=True, help="the pin file, for example .conclave/toolchain-cpp.json")
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    parser.add_argument("--root", default=".", help="the project the pin file's root and search folders are read "
                                                    "under (default: the working directory)")
    args = parser.parse_args(argv)
    try:
        pin_file = load_pin_file(args.pins)
    except PinError as exc:
        print(f"toolchain-check: {exc}", file=sys.stderr)
        return 2
    tools = check_tools(pin_file["tools"], folders=search_folders(Path(args.root).resolve(), pin_file))
    required_not_ok = sum(1 for tool in tools if tool["status"] != "ok" and not tool["optional"])
    if args.json:
        print(json.dumps({"pins": Path(args.pins).as_posix(), "tools": tools, "required_not_ok": required_not_ok},
                         indent=2))
    else:
        for tool in tools:
            word = "optional" if tool["optional"] and tool["status"] != "ok" else tool["status"]
            print(f"{tool['name']}  {word}  {tool['detail']}")
    return 3 if required_not_ok else 0


if __name__ == "__main__":
    sys.exit(main())

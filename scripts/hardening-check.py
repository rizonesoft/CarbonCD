#!/usr/bin/env python3
"""Say whether the project's own compile commands and built binaries carry the memory-safety hardening its pin
file declares.

    python scripts/hardening-check.py [--root <project>] [--pins <pin file>] [--json]

The policy is the `hardening` key of `.conclave/toolchain-cpp.json`, which scripts/toolchain-check.py loads:

    "hardening": {"compile": ["_MSVC_STL_HARDENING=1", "/guard:cf"], "sources": ["src"], "exclude": ["src/vendor"]}

`compile` lists definitions (`NAME`, met by any value, or `NAME=VALUE`, met by that value only) and options (they
start with `-` or `/`). Every entry of the compile database whose source file is under one of `sources` (folders
under the pin file's `root`, `.` when left out) and under none of `exclude` must carry each of them; the other
entries are counted, never checked. A project with no `hardening` key gets no finding: the kit never chooses a
project's flags.

How an entry is read, as the compiler would read it:

- its `arguments`, or its `command` split by Windows rules when its `directory` is a Windows path (a drive letter
  or a backslash) and by POSIX rules otherwise;
- an `@file` argument is replaced by that response file (relative to `directory`, at most 1 MiB, UTF-8 or UTF-16
  with a byte order mark, split by the same rules); one that cannot be read faithfully, or that names a response
  file itself, makes the result `unknown`, never a guess;
- a definition is met by `-DNAME[=VALUE]` or `-D NAME[=VALUE]`, and on an MSVC-style entry (its compiler, after a
  `ccache` or `sccache` launcher, is `cl` or `clang-cl`, wherever it runs) by `/DNAME[=VALUE]` or `/D NAME[=VALUE]` too (`#` standing for `=` there,
  as cl reads it); `-DNAME` is `NAME=1`;
- an option is met by the same argument, and on an MSVC-style entry a `/` option by its `-` spelling too;
- only what the compiler itself reads counts: nothing after cl's `/link`, no `-Wl,`, `-Wa,` or `-Wp,` argument, and
  no value of an option that takes the next argument (`-Xlinker`, `-Xclang`, `-o`, `-I` and the like);
- the last occurrence wins: a later `-UNAME`, `-U NAME`, `/UNAME`, `/U NAME` or other value undoes a definition, and
  the `-fno-` form of an `-f` option, `/option-` or another value of a `-name=value` option undoes an option;
- option families are read as GCC and Clang read them: the last stack protector option sets the level, and a
  stronger level meets a weaker requirement (`-fstack-protector-all` meets `-fstack-protector-strong`, a later
  `-fno-stack-protector` undoes both); the values of `-fsanitize=` add up and `-fno-sanitize=` takes them away;
  `-fcf-protection` is `-fcf-protection=full`.

The built binaries are what the linker wrote, and the only proof of control-flow guard, shadow-stack compatibility
and address randomisation. `binaries` names them (files under `root`, `*` allowed in a file name, names matched by the system's rule for
case) and `image` the
properties each must have, read from the headers with no outside tool (at most 64 MiB of a file):

    PE    DYNAMIC_BASE, HIGH_ENTROPY_VA (not applicable to a 32-bit image), NX_COMPAT, GUARD_CF, CET_COMPAT
    ELF   PIE, RELRO, BIND_NOW (full RELRO is RELRO and BIND_NOW), NX_STACK, STACK_PROTECTOR (unknown with no
          dynamic string table to tell it)

A property of the other format is not applicable, and a binary none of whose required properties applies is
`unknown`. A binary that carries a sanitizer runtime is refused (a sanitizer build is a test build) unless a
`test_binaries` pattern names it, which lifts only that refusal: the evidence is an imported, delay-loaded or needed
library whose name holds `asan`, `ubsan`, `tsan` or `msan`, or, for a runtime linked in, its own option variable
(`ASAN_OPTIONS` and the like) as a string in the file. Limit: a runtime linked in without that string in the file
is not found.

It prints a `hardening:compile:` line with its status, then one line per requirement (`ok`, or `missing` with how
many entries lack it and the first of them), then a `hardening:image:` line, then one line per binary. The compile
status is `ok`, `missing`, `unknown` when the database is not `current` by scripts/compdb-check.py (stale, missing,
invalid or too large: its detail and its regenerate line are printed), a response file or command cannot be read
faithfully, or no entry falls under the policy's sources, or `none` when no `compile` is declared. The image status
is `ok`, `missing` (a property is false or a sanitizer runtime is refused), `unknown` (a pattern names no file, a
file cannot be read or told) or `none`; a binary that is no valid image is `invalid`, naming the offset. The two run
independently. It exits 3 when either has a missing requirement, else 1 when either is unknown, else 0; 2 for bad
usage or a pin file that does not load. `--json` prints the loaded policy, the overall `status` and the `compile`
and `image` results as one document. It reads and reports: it writes nothing and runs nothing. Standard library
only.
"""
import argparse
import fnmatch
import importlib.util
import json
import os
import re
import shlex
import struct
import sys
from pathlib import Path, PurePosixPath

PINS = ".conclave/toolchain-cpp.json"
RESPONSE_LIMIT = 1024 * 1024   # bytes of one response file this check reads
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
WINDOWS_DIRECTORY = re.compile(r"^[A-Za-z]:[\\/]|\\")
MSVC_DRIVERS = {"cl", "clang-cl"}   # compilers that read / options wherever they run
LAUNCHERS = {"ccache", "sccache"}   # a compiler cache named before the compiler
EXIT = {"ok": 0, "none": 0, "unknown": 1, "missing": 3}


def _sibling(name, module_name):
    """A script beside this one, loaded as a module: the kit's one implementation of what it owns."""
    path = Path(__file__).resolve().parent / name
    spec = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Unreadable(ValueError):
    """An entry whose flags cannot be read faithfully: the message says which file and why."""


def split_windows(line):
    """A command line split as the Microsoft C runtime splits it (the rules CommandLineToArgvW follows, with the
    `""` inside quotes of the 2008 runtime). Raises Unreadable for a quote left open."""
    arguments, current, quoted, started, i = [], [], False, False, 0
    while i < len(line):
        char = line[i]
        if char == "\\":
            slashes = 0
            while i < len(line) and line[i] == "\\":
                slashes, i = slashes + 1, i + 1
            if i < len(line) and line[i] == '"':
                current.append("\\" * (slashes // 2))
                if slashes % 2:
                    current.append('"')
                    i += 1
                started = True
                continue
            current.append("\\" * slashes)
            started = True
            continue
        if char == '"':
            if quoted and i + 1 < len(line) and line[i + 1] == '"':
                current.append('"')
                i += 2
                continue
            quoted, started, i = not quoted, True, i + 1
            continue
        if char in " \t\r\n" and not quoted:
            if started:
                arguments.append("".join(current))
                current, started = [], False
            i += 1
            continue
        current.append(char)
        started, i = True, i + 1
    if quoted:
        raise Unreadable("a quote is left open")
    if started:
        arguments.append("".join(current))
    return arguments


def split_line(line, windows):
    if windows:
        return split_windows(line)
    try:
        return shlex.split(line, posix=True)
    except ValueError as exc:
        raise Unreadable(f"it cannot be split as a POSIX command line ({exc})") from exc


def _decode(raw):
    if raw.startswith(b"\xef\xbb\xbf"):
        return raw[3:].decode("utf-8")
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
        return raw.decode("utf-16")
    return raw.decode("utf-8")


def entry_arguments(entry, directory, windows, shown):
    """The arguments an entry compiles with, response files read in place. Raises Unreadable."""
    if "arguments" in entry:
        arguments = list(entry["arguments"])
    else:
        try:
            arguments = split_line(entry["command"], windows)
        except Unreadable as exc:
            raise Unreadable(f"the command of {shown}: {exc}") from exc
    expanded = []
    for argument in arguments:
        if not (argument.startswith("@") and len(argument) > 1):
            expanded.append(argument)
            continue
        name = argument[1:]
        path = Path(name) if Path(name).is_absolute() else directory / name
        where = f"response file {name} of {shown}"
        try:
            with open(path, "rb") as handle:
                raw = handle.read(RESPONSE_LIMIT + 1)
        except OSError as exc:
            raise Unreadable(f"{where} cannot be read ({exc.strerror or exc})") from exc
        if len(raw) > RESPONSE_LIMIT:
            raise Unreadable(f"{where} is larger than the {RESPONSE_LIMIT} bytes this check reads")
        try:
            text = _decode(raw)
        except UnicodeDecodeError as exc:
            raise Unreadable(f"{where} is neither UTF-8 nor UTF-16 with a byte order mark ({exc.reason})") from exc
        try:
            inner = split_line(text, windows)
        except Unreadable as exc:
            raise Unreadable(f"{where}: {exc}") from exc
        nested = [item for item in inner if item.startswith("@") and len(item) > 1]
        if nested:
            raise Unreadable(f"{where} names a response file itself ({nested[0]}), which this check does not follow")
        expanded.extend(inner)
    return expanded


def _definition(text, windows_form):
    """(NAME, VALUE) from the text after -D, or None when it names no macro."""
    if windows_form and "#" in text and "=" not in text:
        text = text.replace("#", "=", 1)
    name, equals, value = text.partition("=")
    if not IDENTIFIER.fullmatch(name):
        return None
    return name, (value if equals else "1")


# Options whose next argument is their value, never a flag of the compiler's own: that value is skipped, so a
# `-Xlinker -D_GLIBCXX_ASSERTIONS` (passed to the linker) or an `-o /guard:cf` meets nothing.
TAKES_VALUE = {"-o", "-x", "-I", "-L", "-l", "-MF", "-MT", "-MQ", "-MJ", "-include", "-imacros", "-isystem",
               "-iquote", "-idirafter", "-iprefix", "-iwithprefix", "-isysroot", "--sysroot", "-target", "--target",
               "-arch", "-Xlinker", "-Xassembler", "-Xpreprocessor", "-Xclang", "-mllvm", "-Xopenmp-target", "-z",
               "-T", "-u", "-e", "-aux-info", "-dumpdir", "-dumpbase", "--param"}
MSVC_TAKES_VALUE = {"/I", "-I", "/FI", "-FI", "/AI", "-AI", "/FU", "-FU", "/external:I", "-external:I", "/Tc", "/Tp"}
# The stack protector's levels, as GCC and Clang read them: the last of the family sets the level.
STACK_PROTECTOR = {"-fno-stack-protector": 0, "-fstack-protector-explicit": 1, "-fstack-protector": 2,
                   "-fstack-protector-strong": 3, "-fstack-protector-all": 4}
# A bare option that means one value of its `=` family.
ALIASES = {"-fcf-protection": "-fcf-protection=full"}
# Options whose values add up rather than replace each other, with the option that takes values away.
LISTS = {"-fsanitize=": "-fno-sanitize=", "-fsanitize-trap=": "-fno-sanitize-trap=",
         "-fsanitize-recover=": "-fno-sanitize-recover="}


def compiler_arguments(arguments, msvc):
    """The arguments the compiler itself reads, in order, without the compiler: a separated `-D NAME` or `-U NAME`
    (and `/D`, `/U` on an MSVC-style entry) joined into one, each option's own value skipped, a `-Wl,` or `-Wa,`
    argument (the linker's or the assembler's) left out, and nothing after cl's `/link` (the linker's)."""
    owned, i = [], 1
    while i < len(arguments):
        argument = arguments[i]
        if msvc and argument.lower() in ("/link", "-link"):
            break
        joined = ("-D", "-U") + (("/D", "/U") if msvc else ())
        if argument in joined:
            if i + 1 < len(arguments):
                owned.append(argument + arguments[i + 1])
            i += 2
            continue
        if argument in TAKES_VALUE or (msvc and argument in MSVC_TAKES_VALUE) or argument.startswith("-Xarch_"):
            i += 2
            continue
        if not argument.startswith(("-Wl,", "-Wa,", "-Wp,")):
            owned.append(argument)
        i += 1
    return owned


def _definition_state(owned, name, msvc):
    defined = None
    for argument in owned:
        head = argument[:2]
        if head == "-U" or (msvc and head == "/U"):
            if argument[2:] == name:
                defined = None
        elif head == "-D" or (msvc and head == "/D"):
            parsed = _definition(argument[2:], head == "/D")
            if parsed and parsed[0] == name:
                defined = parsed[1]
    return defined


def _canonical(argument, msvc):
    """One spelling of an option: cl's `-` form as its `/` form, and a bare alias as its `=` value."""
    if msvc and argument.startswith("-") and not argument.startswith("--"):
        slash = "/" + argument[1:]
        if not argument.startswith(("-f", "-m", "-D", "-U", "-I", "-X")):
            argument = slash
    return ALIASES.get(argument, argument)


def _option_state(owned, requirement, msvc):
    """Whether the option `requirement` is in force after the arguments the compiler reads, under the option
    families GCC, Clang and cl apply: the stack protector's levels (a stronger level meets a weaker requirement),
    list options whose values add up (`-fsanitize=`), `-name=value` options whose last value wins, `-fname` against
    `-fno-name`, and cl's `/option` against `/option-`."""
    wanted = _canonical(requirement, msvc)
    if wanted in STACK_PROTECTOR:
        level = 0
        for argument in owned:
            level = STACK_PROTECTOR.get(argument, level)
        required = STACK_PROTECTOR[wanted]
        return level == 0 if required == 0 else level >= required
    for adds, takes in LISTS.items():
        if wanted.startswith(adds):
            values = set()
            for argument in owned:
                if argument.startswith(adds):
                    values |= set(argument[len(adds):].split(","))
                elif argument.startswith(takes):
                    values -= set(argument[len(takes):].split(","))
            return set(wanted[len(adds):].split(",")) <= values
    family = wanted.split("=", 1)[0] if wanted.startswith("-") and "=" in wanted else None
    state = False
    for argument in owned:
        spelled = _canonical(argument, msvc)
        if spelled == wanted:
            state = True
        elif family and spelled.startswith(family + "="):
            state = False   # another value of the same option
        elif wanted.startswith("-fno-") and spelled == "-f" + wanted[5:]:
            state = False
        elif wanted.startswith("-f") and spelled in ("-fno-" + wanted[2:], "-fno-" + (family or wanted)[2:]):
            state = False
        elif msvc and wanted[0] == "/" and spelled == wanted + "-":
            state = False
    return state


def carries(arguments, requirement, msvc):
    """Whether a compile command's arguments (the compiler first) leave `requirement` in force at their end, as
    the compiler reads them (compiler_arguments)."""
    owned = compiler_arguments(arguments, msvc)
    if requirement[:2] in ("-D", "/D") and _definition(requirement[2:], requirement[0] == "/"):
        requirement = requirement[2:]   # a definition written with its option: held to the same rules
    if requirement[0] in "-/":
        return _option_state(owned, requirement, msvc)
    name, value = _definition(requirement, False)
    defined = _definition_state(owned, name, msvc)
    return defined is not None and ("=" not in requirement or defined == value)


def _key(path):
    return os.path.normcase(os.path.normpath(str(path)))


def _under(path_key, folder_key):
    return path_key == folder_key or path_key.startswith(folder_key.rstrip(os.sep) + os.sep)


def _program(argument):
    name = re.split(r"[\\/]", argument)[-1].lower()
    return name[:-4] if name.endswith(".exe") else name


def msvc_style(arguments):
    """Whether an entry's compiler reads `/` options: cl or clang-cl, after any compiler cache launcher. A MinGW
    compiler on Windows takes `/DNAME` for a file, so the system the entry ran on does not decide it."""
    programs = [_program(argument) for argument in arguments]
    while programs and programs[0] in LAUNCHERS:
        programs.pop(0)
    return bool(programs) and programs[0] in MSVC_DRIVERS


def check_compile(project, pin_file, compdb, read_limit=None):
    """The result of the policy of a loaded pin file against its compile database: {"status", "detail",
    "database", "checked", "excluded", "outside", "unreadable", "requirements", "regenerate"}. `compdb` is the
    loaded compdb-check module. An entry whose command cannot be read (a missing response file, an unbalanced
    quote) is counted in `unreadable` and the others are still checked: a requirement a readable entry lacks is
    `missing` whatever the unreadable ones hold, and only with no such finding is the result `unknown`."""
    project = Path(project).resolve()
    policy = pin_file["hardening"]
    result = {"status": None, "detail": "", "database": None, "checked": 0, "excluded": 0, "outside": 0,
              "unreadable": 0, "requirements": [], "regenerate": None}
    if policy is None:
        result.update(status="none", detail="no policy declared: add a `hardening` key to the pin file "
                                             "(docs/toolchain-cpp.md, Memory safety)")
        return result
    if not policy["compile"]:
        result.update(status="none", detail="no compile requirement declared")
        return result
    entries = []
    state = compdb.check_database(project, pin_file, entries_out=entries,
                                  read_limit=compdb.READ_LIMIT if read_limit is None else read_limit)
    result["database"] = state["path"]
    if state["status"] != "current":
        result.update(status="unknown", detail=f"the compile database is {state['status']}, so the flags it "
                                               f"holds may not be the build's: {state['detail']}",
                      regenerate=compdb.regenerate_line(state))
        return result
    base = project / pin_file["root"]
    sources = [_key(base / folder) for folder in policy["sources"]]
    exclude = [_key(base / folder) for folder in policy["exclude"]]
    database = compdb.database_path(project, pin_file)
    lacking = {requirement: [] for requirement in policy["compile"]}
    unreadable = []
    for entry in entries:
        source = compdb.entry_source(entry, database)
        source_key = _key(source)
        if any(_under(source_key, folder) for folder in exclude):
            result["excluded"] += 1
            continue
        if not any(_under(source_key, folder) for folder in sources):
            result["outside"] += 1
            continue
        result["checked"] += 1
        shown = compdb._shown(Path(os.path.normpath(str(source))), project)
        windows = bool(WINDOWS_DIRECTORY.search(entry["directory"]))
        directory = Path(entry["directory"])
        if not directory.is_absolute():
            directory = database.parent / directory
        try:
            arguments = entry_arguments(entry, directory, windows, shown)
        except Unreadable as exc:
            # The other entries are still read: a requirement one of them lacks is a finding whatever this one is.
            unreadable.append(str(exc))
            continue
        forms = msvc_style(arguments)
        for requirement in policy["compile"]:
            if not carries(arguments, requirement, forms):
                lacking[requirement].append(shown)
    counted = f"{result['excluded']} excluded, {result['outside']} outside the sources"
    if not result["checked"]:
        result.update(status="unknown", detail=f"{state['path']}: no entry is under the policy's sources "
                                               f"({', '.join(policy['sources'])}), so nothing was checked; "
                                               f"{counted}")
        return result
    for requirement, missing in lacking.items():
        result["requirements"].append({"requirement": requirement, "status": "missing" if missing else "ok",
                                       "lacking": len(missing), "first": missing[0] if missing else None})
    short = [item for item in result["requirements"] if item["status"] == "missing"]
    checked = f"{result['checked']} first-party entr{'y' if result['checked'] == 1 else 'ies'}"
    result["unreadable"] = len(unreadable)
    untold = ""
    if unreadable:
        many = f"{len(unreadable)} entries cannot be read; first: " if len(unreadable) > 1 else ""
        untold = f"{many}{unreadable[0]}, so its flags cannot be told"
    if short:
        result.update(status="missing", detail=f"{state['path']}: {len(short)} of {len(lacking)} requirement(s) "
                                               f"missing on {checked}; {counted}" + (f"; {untold}" if untold else ""))
    elif unreadable:
        result.update(status="unknown", detail=untold)
    else:
        result.update(status="ok", detail=f"{state['path']}: {len(lacking)} requirement(s) on {checked}; "
                                          f"{counted}")
    return result


# The built binaries (D04 T03 section 2): what the linker wrote, read from the PE and ELF headers.

IMAGE_LIMIT = 64 * 1024 * 1024   # bytes of one binary this check reads
PE_PROPERTIES = ("DYNAMIC_BASE", "HIGH_ENTROPY_VA", "NX_COMPAT", "GUARD_CF", "CET_COMPAT")
ELF_PROPERTIES = ("PIE", "RELRO", "BIND_NOW", "NX_STACK", "STACK_PROTECTOR")
PE_FLAGS = {"HIGH_ENTROPY_VA": 0x0020, "DYNAMIC_BASE": 0x0040, "NX_COMPAT": 0x0100, "GUARD_CF": 0x4000}
DEBUG_EX_DLLCHARACTERISTICS, CET_COMPAT_BIT = 20, 0x0001
PT_LOAD, PT_DYNAMIC, PT_INTERP, PT_GNU_STACK, PT_GNU_RELRO = 1, 2, 3, 0x6474E551, 0x6474E552
DT_NULL, DT_NEEDED, DT_STRTAB, DT_STRSZ, DT_BIND_NOW, DT_FLAGS, DT_FLAGS_1 = 0, 1, 5, 10, 24, 30, 0x6FFFFFFB
DF_BIND_NOW, DF_1_NOW, DF_1_PIE = 0x8, 0x1, 0x08000000
SANITIZER_WORDS = ("asan", "ubsan", "tsan", "msan")
# The option variable each sanitizer runtime reads at start: in the file when the runtime is linked into it.
SANITIZER_OPTIONS = (b"ASAN_OPTIONS", b"UBSAN_OPTIONS", b"TSAN_OPTIONS", b"MSAN_OPTIONS", b"HWASAN_OPTIONS")
NOT_APPLICABLE = "not applicable"


class Malformed(ValueError):
    """A header or table that is not what its format says: the message names the offset."""


class Image:
    """The bytes read of one binary, and whether the file goes on past them."""

    def __init__(self, data, cut):
        self.data, self.cut = data, cut

    def read(self, offset, size, fmt, what):
        if offset < 0 or offset + size > len(self.data):
            raise Malformed(f"{what} at offset {offset} lies past the end of the file")
        return struct.unpack_from(fmt, self.data, offset)[0]

    def within(self, offset, size, what):
        """Refuse a range the file declares but does not hold: a truncated copy is no proof of what was built."""
        if offset < 0 or offset + size > len(self.data):
            raise Malformed(f"{what} at offset {offset} ({size} bytes) lies past the end of the file")

    def string(self, offset, what, limit=4096):
        if offset < 0 or offset >= len(self.data):
            raise Malformed(f"{what} at offset {offset} lies past the end of the file")
        end = self.data.find(b"\0", offset, offset + limit)
        if end < 0:
            raise Malformed(f"{what} at offset {offset} is not terminated")
        return self.data[offset:end].decode("latin-1")


def read_pe(image):
    """A PE image's properties: each of PE_PROPERTIES true, false or NOT_APPLICABLE, "guard_functions", and
    "libraries" (the imported and delay-loaded DLLs). Raises Malformed."""
    read = image.read
    e_lfanew = read(0x3C, 4, "<I", "the PE header offset")
    read(e_lfanew, 4, "<I", "the PE header")   # it must lie inside what was read
    if image.data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
        raise Malformed(f"no PE signature at offset {e_lfanew}")
    coff = e_lfanew + 4
    sections_count = read(coff + 2, 2, "<H", "the section count")
    optional_size = read(coff + 16, 2, "<H", "the optional header size")
    optional = coff + 20
    magic = read(optional, 2, "<H", "the optional header magic")
    if magic not in (0x10B, 0x20B):
        raise Malformed(f"the optional header magic at offset {optional} is {magic:#x}, neither PE32 nor PE32+")
    plus = magic == 0x20B
    dll = read(optional + 70, 2, "<H", "the DLL characteristics")
    image_base = read(optional + 24, 8, "<Q", "the image base") if plus else read(optional + 28, 4, "<I", "the image base")
    count_at = optional + (108 if plus else 92)
    directories = min(read(count_at, 4, "<I", "the data directory count"), 16)
    if count_at + 4 + 8 * directories > optional + optional_size:
        raise Malformed(f"the data directories at offset {count_at + 4} run past the optional header")
    sections = []
    for number in range(sections_count):
        at = optional + optional_size + 40 * number
        virtual_size = read(at + 8, 4, "<I", f"section {number + 1}")
        address = read(at + 12, 4, "<I", f"section {number + 1}")
        raw_size = read(at + 16, 4, "<I", f"section {number + 1}")
        raw_at = read(at + 20, 4, "<I", f"section {number + 1}")
        if raw_size:
            image.within(raw_at, raw_size, f"section {number + 1}'s contents")
        sections.append((address, max(virtual_size, raw_size), raw_at, raw_size))

    def offset_of(rva, what):
        for address, size, raw_at, raw_size in sections:
            if address <= rva < address + size and rva - address < raw_size:
                return raw_at + rva - address
        raise Malformed(f"{what} (address {rva:#x}) is in no section of the file")

    def directory(index):
        if index >= directories:
            return 0, 0
        at = count_at + 4 + 8 * index
        return read(at, 4, "<I", "a data directory"), read(at + 4, 4, "<I", "a data directory")

    found = {name: bool(dll & bit) for name, bit in PE_FLAGS.items()}
    if not plus:
        found["HIGH_ENTROPY_VA"] = NOT_APPLICABLE   # a 32-bit image has no 64-bit address space to spread over
    guard_functions = 0
    rva, size = directory(10)
    if rva and size:
        at = offset_of(rva, "the load configuration")
        length = read(at, 4, "<I", "the load configuration size")
        field, width = (0x88, 8) if plus else (0x54, 4)
        if length >= field + width:
            guard_functions = read(at + field, width, "<Q" if plus else "<I", "the guard function count")
    cet = False
    rva, size = directory(6)
    if rva and size:
        at = offset_of(rva, "the debug directory")
        for number in range(min(size // 28, 64)):
            entry = at + 28 * number
            if read(entry + 12, 4, "<I", "a debug entry") == DEBUG_EX_DLLCHARACTERISTICS:
                raw = read(entry + 24, 4, "<I", "a debug entry")
                if raw:   # 0: the entry's data is not in the file
                    cet = bool(read(raw, 4, "<I", "the extended DLL characteristics") & CET_COMPAT_BIT)
    found["CET_COMPAT"] = cet
    libraries = []
    for index, width, name_field, what in ((1, 20, 12, "an import"), (13, 32, 4, "a delay import")):
        rva, size = directory(index)
        if not (rva and size):
            continue
        at = offset_of(rva, f"the table of {what}s")
        for number in range(4096):
            entry = at + width * number
            name_rva = read(entry + name_field, 4, "<I", f"{what}'s name")
            if not name_rva:
                break
            if index == 13 and not plus and not read(entry, 4, "<I", "a delay import's attributes") & 1:
                name_rva -= image_base   # the first, 32-bit delay import format holds addresses, not offsets
            libraries.append(image.string(offset_of(name_rva, f"{what}'s name"), f"{what}'s name"))
    return dict(found, guard_functions=guard_functions, libraries=libraries)


def read_elf(image):
    """An ELF image's properties: each of ELF_PROPERTIES true, false or None (STACK_PROTECTOR, when there is no
    dynamic string table to tell it), and "libraries" (the needed ones). Raises Malformed."""
    read = image.read
    klass, order = read(4, 1, "B", "the ELF class"), read(5, 1, "B", "the ELF byte order")
    if klass not in (1, 2) or order not in (1, 2):
        raise Malformed("the ELF class or byte order at offset 4 is not one the format defines")
    e, wide = ("<" if order == 1 else ">"), klass == 2
    word, size = (e + "Q", 8) if wide else (e + "I", 4)
    e_type = read(16, 2, e + "H", "the ELF type")
    phoff = read(32 if wide else 28, size, word, "the program header offset")
    phentsize = read(54 if wide else 42, 2, e + "H", "the program header size")
    phnum = read(56 if wide else 44, 2, e + "H", "the program header count")
    if phnum and phentsize < (56 if wide else 32):
        raise Malformed(f"the program header size at offset {54 if wide else 42} is {phentsize}, too small")
    segments = []
    for number in range(phnum):
        at = phoff + phentsize * number
        p_type = read(at, 4, e + "I", f"program header {number + 1}")
        if wide:
            flags = read(at + 4, 4, e + "I", f"program header {number + 1}")
            offset, vaddr, filesz = (read(at + field, 8, word, f"program header {number + 1}") for field in (8, 16, 32))
        else:
            offset, vaddr, filesz = (read(at + field, 4, word, f"program header {number + 1}") for field in (4, 8, 16))
            flags = read(at + 24, 4, e + "I", f"program header {number + 1}")
        if filesz:
            image.within(offset, filesz, f"the contents of program header {number + 1}")
        segments.append((p_type, flags, offset, vaddr, filesz))

    def offset_of(vaddr, what):
        for p_type, _flags, offset, start, filesz in segments:
            if p_type == PT_LOAD and start <= vaddr < start + filesz:
                return offset + vaddr - start
        raise Malformed(f"{what} (address {vaddr:#x}) is in no loaded segment")

    types = {segment[0] for segment in segments}
    tags = []
    for p_type, _flags, offset, _vaddr, filesz in segments:
        if p_type != PT_DYNAMIC:
            continue
        step = 16 if wide else 8
        for number in range(min(filesz // step, 4096)):
            at = offset + step * number
            tag = read(at, size, (e + "q") if wide else (e + "i"), "a dynamic entry")
            if tag == DT_NULL:
                break
            tags.append((tag, read(at + size, size, word, "a dynamic entry")))
        break
    values = {}
    for tag, value in tags:
        values.setdefault(tag, value)
    flags, flags_1 = values.get(DT_FLAGS, 0), values.get(DT_FLAGS_1, 0)
    libraries, protector = [], None
    if DT_NEEDED in values and DT_STRTAB not in values:
        raise Malformed("the dynamic section names needed libraries but no dynamic string table to read their "
                        "names from, so what the binary loads cannot be told")
    if DT_STRTAB in values:
        start = offset_of(values[DT_STRTAB], "the dynamic string table")
        length = values.get(DT_STRSZ)
        if length is not None and start + length > len(image.data):
            raise Malformed(f"the dynamic string table at offset {start} runs past the end of the file")
        for tag, value in tags:
            if tag != DT_NEEDED:
                continue
            if length is not None and value >= length:
                raise Malformed(f"a needed library's name at offset {start + value} lies past the end of the dynamic "
                                "string table")
            libraries.append(image.string(start + value, "a needed library's name"))
        if length is not None:   # without its size the table cannot be searched, so the answer is not known
            protector = b"\0__stack_chk_fail\0" in b"\0" + image.data[start:start + length]
    stack = [segment for segment in segments if segment[0] == PT_GNU_STACK]
    return {"PIE": e_type == 3 and (PT_INTERP in types or bool(flags_1 & DF_1_PIE)),
            "RELRO": PT_GNU_RELRO in types,
            "BIND_NOW": DT_BIND_NOW in values or bool(flags & DF_BIND_NOW) or bool(flags_1 & DF_1_NOW),
            "NX_STACK": bool(stack) and not stack[0][1] & 1,
            "STACK_PROTECTOR": protector, "libraries": libraries}


def inspect_binary(path, shown, required, exempt, limit=None):
    """One binary held to the `image` properties: {"path", "format", "status", "detail", "properties",
    "missing", "sanitizers"}. Status: ok, missing (a required property is false, or a sanitizer runtime is in a
    binary `test_binaries` does not name), unknown, or invalid. Never raises."""
    limit = IMAGE_LIMIT if limit is None else limit
    result = {"path": shown, "format": None, "status": None, "detail": "", "properties": {}, "missing": [],
              "sanitizers": []}

    def ended(status, detail):
        result.update(status=status, detail=detail)
        return result

    try:
        with open(path, "rb") as handle:
            data = handle.read(limit + 1)
    except OSError as exc:
        return ended("unknown", f"{shown} cannot be read ({exc.strerror or exc})")
    image = Image(data[:limit], len(data) > limit)
    try:
        if image.data[:4] == b"\x7fELF":
            result["format"], found = "elf", read_elf(image)
        elif image.data[:2] == b"MZ":
            result["format"], found = "pe", read_pe(image)
        else:
            return ended("invalid", f"{shown} is neither a PE nor an ELF image (offset 0)")
    except Malformed as exc:
        if image.cut:
            return ended("unknown", f"{shown}: {exc}: the file is larger than the {limit} bytes this check reads")
        return ended("invalid", f"{shown}: {exc}")
    except (struct.error, ValueError, IndexError, OverflowError) as exc:
        return ended("invalid", f"{shown}: a header this check cannot read ({type(exc).__name__}: {exc})")
    own = PE_PROPERTIES if result["format"] == "pe" else ELF_PROPERTIES
    result["sanitizers"] = [name for name in found["libraries"] if any(w in name.lower() for w in SANITIZER_WORDS)]
    result["sanitizers"] += [name.decode() for name in SANITIZER_OPTIONS if b"\0" + name + b"\0" in image.data]
    untold = []
    for name in required:
        value = found.get(name, NOT_APPLICABLE) if name in own else NOT_APPLICABLE
        result["properties"][name] = "ok" if value is True else "missing" if value is False else \
            "unknown" if value is None else NOT_APPLICABLE
        if value is False:
            result["missing"].append(name)
        elif value is None:
            untold.append(name)
    applicable = [name for name in required if result["properties"][name] != NOT_APPLICABLE]
    refused = result["sanitizers"] and not exempt
    named = []
    if result["missing"]:
        named.append(f"lacks {', '.join(result['missing'])}")
    if refused:
        named.append(f"carries a sanitizer runtime ({', '.join(result['sanitizers'])}); a sanitizer build is a "
                     "test build: name it in test_binaries if it is one")
    if named:
        return ended("missing", f"{shown} ({result['format'].upper()}) " + "; ".join(named))
    if untold:
        return ended("unknown", f"{shown}: {', '.join(untold)} cannot be told from this file (no dynamic string "
                                "table)")
    if required and not applicable:
        return ended("unknown", f"{shown} is {result['format'].upper()}, and none of the required properties "
                                f"({', '.join(required)}) applies to it")
    told = f"{len(applicable)} propert{'y' if len(applicable) == 1 else 'ies'}"
    exempted = " (a test binary: its sanitizer runtime is allowed)" if result["sanitizers"] and exempt else ""
    return ended("ok", f"{shown} ({result['format'].upper()}): {told}{exempted}")


def _matches(base, patterns):
    """The files `binaries` patterns name under `base` ({path: pattern}, in order), and the patterns that name
    nothing. `*` is allowed in the file name only. A pattern without `*` names its path even when it is not a
    file, so a directory or an unreadable file is reported, never skipped."""
    found, unmatched = {}, []
    for pattern in patterns:
        rel = PurePosixPath(pattern)
        folder = base.joinpath(*rel.parts[:-1]) if len(rel.parts) > 1 else base
        if "*" not in rel.name:
            path = folder / rel.name
            if os.path.lexists(path):
                found.setdefault(path, pattern)
            else:
                unmatched.append(pattern)
            continue
        try:
            names = sorted(entry for entry in os.listdir(folder) if fnmatch.fnmatch(entry, rel.name)
                           and os.path.isfile(folder / entry))
        except OSError:
            names = []
        for name in names:
            found.setdefault(folder / name, pattern)
        if not names:
            unmatched.append(pattern)
    return found, unmatched


def _same_folder(one, other):
    """Two folders under the root compared by the system's rule for case, as fnmatch.fnmatch compares names."""
    return os.path.normcase(str(one)) == os.path.normcase(str(other))


def _exempt(path, base, patterns):
    rel = PurePosixPath(os.path.relpath(path, base).replace(os.sep, "/"))
    for pattern in patterns:
        target = PurePosixPath(pattern)
        if _same_folder(rel.parent, target.parent) and fnmatch.fnmatch(rel.name, target.name):
            return True
    return False


def check_images(project, pin_file, limit=None):
    """The built binaries a loaded pin file's policy names, each held to its `image` properties: {"status",
    "detail", "binaries", "unmatched"}."""
    project = Path(project).resolve()
    policy = pin_file["hardening"]
    result = {"status": None, "detail": "", "binaries": [], "unmatched": []}
    if policy is None:
        result.update(status="none", detail="no policy declared")
        return result
    if not policy["binaries"]:
        result.update(status="none", detail="no binaries declared")
        return result
    base = project / pin_file["root"]
    found, result["unmatched"] = _matches(base, policy["binaries"])
    for path in found:
        shown = Path(os.path.normpath(str(path))).relative_to(project).as_posix()
        if os.path.isdir(path):
            result["binaries"].append({"path": shown, "format": None, "status": "unknown",
                                       "detail": f"{shown} is a folder, not a binary", "properties": {},
                                       "missing": [], "sanitizers": []})
            continue
        result["binaries"].append(inspect_binary(path, shown, policy["image"],
                                                 _exempt(path, base, policy["test_binaries"]), limit))
    states = [entry["status"] for entry in result["binaries"]]
    counts = ", ".join(f"{states.count(state)} {state}" for state in ("ok", "missing", "unknown", "invalid")
                       if states.count(state))
    # A count only: each pattern is named once, on its own line by main() and in the doctor's bounded list.
    unmatched = f"; {len(result['unmatched'])} pattern(s) name no file" if result["unmatched"] else ""
    if "missing" in states:
        status = "missing"
    elif result["unmatched"] or "unknown" in states or "invalid" in states:
        status = "unknown"
    else:
        status = "ok"
    result.update(status=status, detail=f"{len(states)} binar{'y' if len(states) == 1 else 'ies'}"
                                        f"{': ' + counts if counts else ''}{unmatched}")
    return result


def overall(compile_result, image_result):
    """The one status of both checks: missing when either has a missing requirement, else unknown when either is
    unknown, else ok when either is ok, else none."""
    states = (compile_result["status"], image_result["status"])
    for status in ("missing", "unknown", "ok"):
        if status in states:
            return status
    return "none"


def main(argv=None):
    parser = argparse.ArgumentParser(prog="hardening-check.py",
                                     description="Say whether the project's compile commands and built binaries "
                                                 "carry its declared hardening. Runs nothing.",
                                     epilog="A sanitizer runtime is found by the library a binary imports or needs, "
                                            "or, linked in, by its option variable (ASAN_OPTIONS and the like) in "
                                            "the file. Limit: a runtime linked in without that string in the file "
                                            "is not found. Patterns in binaries and test_binaries match file names "
                                            "by the system's rule for case.")
    parser.add_argument("--root", default=".", help="the project (default: the working directory)")
    parser.add_argument("--pins", help=f"the pin file (default: {PINS} under the root)")
    parser.add_argument("--json", action="store_true", help="print the policy and the result as one JSON document")
    args = parser.parse_args(argv)
    project = Path(args.root).resolve()
    pins = Path(args.pins) if args.pins else project / PINS
    module = _sibling("toolchain-check.py", "_toolchain_check")
    try:
        pin_file = module.load_pin_file(pins)
    except module.PinError as exc:
        print(f"hardening-check: {exc}", file=sys.stderr)
        if args.json:
            print(json.dumps({"policy": None, "status": "refused", "detail": str(exc), "compile": None,
                              "image": None}, indent=2))
        return 2
    compiled = check_compile(project, pin_file, _sibling("compdb-check.py", "_compdb_check"))
    images = check_images(project, pin_file)
    status = overall(compiled, images)
    if args.json:
        print(json.dumps({"policy": pin_file["hardening"], "status": status, "compile": compiled, "image": images},
                         indent=2))
        return EXIT[status]
    print(f"hardening:compile: {compiled['status']}: {compiled['detail']}")
    for item in compiled["requirements"]:
        if item["status"] == "ok":
            print(f"  ok       {item['requirement']}")
        else:
            print(f"  missing  {item['requirement']}: on {item['lacking']} entr"
                  f"{'y' if item['lacking'] == 1 else 'ies'}, first {item['first']}")
    if compiled["regenerate"]:
        print(compiled["regenerate"])
    print(f"hardening:image: {images['status']}: {images['detail']}")
    for entry in images["binaries"]:
        print(f"  {entry['status']:<8} {entry['detail']}")
    for pattern in images["unmatched"]:
        print(f"  unknown  {pattern} names no file")
    return EXIT[status]


if __name__ == "__main__":
    sys.exit(main())

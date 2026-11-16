#!/usr/bin/env python3
"""Put a compiler cache (ccache or sccache) in front of the compilers a build names.

    python scripts/cc-cache.py [--cache <name>] [--shell sh|powershell|cmd] [--root <project>] <compiler>...

A program cannot change its caller's `PATH`, so this writes the wrappers and prints the one statement that puts
them first on `PATH`, in the calling shell's form. A build script checks the exit before it evaluates the output:

    sh           setup=$(python3 scripts/cc-cache.py cc c++) || exit 1; eval "$setup"
    PowerShell   $setup = python scripts/cc-cache.py clang; if ($LASTEXITCODE) { exit 1 }; $setup | Invoke-Expression

For each compiler found on `PATH` it writes a wrapper of that name, which runs the cache with the real compiler's
full path and the arguments it was given, into a folder of this project's own under the user's cache folder
(`$XDG_CACHE_HOME` or `~/.cache`; `%LOCALAPPDATA%` on Windows). The cache is `--cache <name>`, else the first of
ccache and sccache on `PATH`; the folder records which, for `cache-stats.py`.

The folder is exactly what the last run decided: a wrapper this run did not write is removed. With
`BUILD_NO_CCACHE` set, or no cache installed, every wrapper is removed and the output is a comment, so evaluating
it changes nothing and a shell that already has the folder on `PATH` reaches the real compilers. A wrapper also
stands aside by itself when it runs with `BUILD_NO_CCACHE` set or with the cache's program gone.

A compiler is run by the path it was found at, never by what that path links to: `clang++` that is a link to
`clang` is still run as `clang++`, which is how the compiler knows it is compiling C++. Two setups for one
project (two build configurations started together) take turns: the folder is written under a lock.

Exit 0 also when there is nothing to wrap (a missing cache never breaks a build); 1 when a wrapper cannot be
written or another setup holds the folder for too long; 2 for a compiler name that is not a bare name (on
Windows, one given with its extension: name `clang`, not `clang.exe`) or a path that cannot be written safely.
Standard library only.
"""
import argparse
import hashlib
import os
import re
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHES = ("ccache", "sccache")
OPT_OUT = "BUILD_NO_CCACHE"
CHOICE = "cache-choice"   # in the wrapper folder: the name and the path of the cache the wrappers run
NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9+._-]*")
SHELLS = ("sh", "powershell", "cmd")
LOCK_WAIT = 30.0     # seconds a setup waits for another one of the same project
LOCK_STALE = 120.0   # seconds after which a lock is taken to be left by a setup that died


class Refused(Exception):
    """Something that ends the run with one line and exit 2."""


def cache_home(environ=None):
    """The user's cache folder."""
    environ = os.environ if environ is None else environ
    if os.name == "nt":
        return Path(environ.get("LOCALAPPDATA") or Path.home() / "AppData" / "Local")
    return Path(environ.get("XDG_CACHE_HOME") or Path.home() / ".cache")


def wrapper_folder(project, environ=None):
    """The folder a project's wrappers are kept in: its own, under the user's cache folder."""
    project = Path(project).resolve()
    return cache_home(environ) / "conclave" / "cc-cache" / hashlib.sha256(str(project).encode("utf-8")).hexdigest()[:12]


def _same(one, other):
    try:
        return os.path.samefile(one, other)
    except OSError:
        return os.path.normcase(os.path.abspath(one)) == os.path.normcase(os.path.abspath(other))


def find(name, skip=(), environ=None):
    """The program a bare name is on `PATH`, as a full path, or None. The folders in `skip` are left out (the
    wrapper folder, so a wrapper is never taken for the compiler it wraps), and the working directory is never
    tried, which on Windows the standard lookup tries first."""
    environ = os.environ if environ is None else environ
    windows = os.name == "nt"
    suffixes = [ext for ext in environ.get("PATHEXT", ".COM;.EXE;.BAT;.CMD").split(os.pathsep) if ext] \
        if windows else []
    names = [name]
    if windows and Path(name).suffix.upper() not in {ext.upper() for ext in suffixes}:
        names = [name + ext for ext in suffixes]
    for folder in environ.get("PATH", "").split(os.pathsep):
        if not folder or any(_same(folder, other) for other in skip):
            continue
        for candidate in names:
            path = Path(folder) / candidate
            if path.is_file() and os.access(path, os.X_OK):
                # The path it was found at, not what it links to: a compiler reads its own name
                # (clang++ linked to clang compiles C++ only when run as clang++).
                return Path(os.path.abspath(path))
    return None


def _sh(text):
    return "'" + str(text).replace("'", "'\"'\"'") + "'"


def _powershell(text):
    return "'" + str(text).replace("'", "''") + "'"


def _cmd_safe(path):
    """A path that can stand inside double quotes in a `.cmd` or a `set`: one holding `"` or `%` cannot."""
    if '"' in str(path) or "%" in str(path):
        raise Refused(f"{path} holds a character a cmd file cannot quote (\" or %)")
    return str(path)


def wrapper_text(cache, real, windows=None):
    """The wrapper that runs `real` through `cache`: the cache with the real compiler's full path and the
    arguments given. It stands aside (runs the compiler directly) with BUILD_NO_CCACHE set or the cache gone."""
    windows = os.name == "nt" if windows is None else windows
    if windows:
        cache, real = _cmd_safe(cache), _cmd_safe(real)
        return ("@echo off\r\n"
                "rem Written by scripts/cc-cache.py: runs this compiler through the compiler cache. Not for editing.\r\n"
                f"if defined {OPT_OUT} goto real\r\n"
                f'if not exist "{cache}" goto real\r\n'
                f'"{cache}" "{real}" %*\r\n'
                "exit /b %errorlevel%\r\n"
                ":real\r\n"
                f'"{real}" %*\r\n'
                "exit /b %errorlevel%\r\n")
    return ("#!/bin/sh\n"
            "# Written by scripts/cc-cache.py: runs this compiler through the compiler cache. Not for editing.\n"
            f'if [ -z "${{{OPT_OUT}:-}}" ] && [ -x {_sh(cache)} ]; then\n'
            f'    exec {_sh(cache)} {_sh(real)} "$@"\n'
            "fi\n"
            f'exec {_sh(real)} "$@"\n')


def statement(folder, shell):
    """The one statement that puts the wrapper folder first on `PATH`, for a shell."""
    if shell == "sh":
        return f'export PATH={_sh(folder)}:"$PATH"'
    if shell == "powershell":
        return f"$env:PATH = {_powershell(folder)} + [IO.Path]::PathSeparator + $env:PATH"
    return f'set "PATH={_cmd_safe(folder)};%PATH%"'


def comment(text, shell):
    """A line that changes nothing when the shell evaluates it."""
    return f"rem {text}" if shell == "cmd" else f"# {text}"


def _write(path, data, executable):
    """Write a file through a temporary file and a replacement, only when its content would change."""
    try:
        if path.is_file() and path.read_bytes() == data:
            return False
    except OSError:
        pass   # unreadable now: the write below reports what is wrong with it
    temporary = None
    try:
        fd, temporary = tempfile.mkstemp(dir=path.parent, prefix=".cc-cache-", suffix=".tmp")
        with os.fdopen(fd, "wb") as out:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        if executable and os.name != "nt":
            os.chmod(temporary, 0o755)
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)
    return True


def _keep_only(folder, keep):
    """Remove every file of the wrapper folder this run did not write."""
    if not folder.is_dir():
        return
    for path in folder.iterdir():
        if path.name not in keep and (path.is_file() or path.is_symlink()):
            path.unlink()


class Locked:
    """The wrapper folder held for one setup, from its first write to its last removal, so two setups of one
    project never see each other's half-written state. The lock is a file beside the folder, made exclusively;
    one older than LOCK_STALE was left by a setup that died and is taken over."""

    def __init__(self, folder, wait=None):
        self.path = folder.parent / (folder.name + ".lock")
        self.wait = LOCK_WAIT if wait is None else wait

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        deadline = time.monotonic() + self.wait
        while True:
            try:
                os.close(os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
                return self
            except FileExistsError:
                try:
                    if time.time() - self.path.stat().st_mtime > LOCK_STALE:
                        self.path.unlink()
                        continue
                except OSError:
                    continue   # it went away between the two looks: try again
                if time.monotonic() >= deadline:
                    raise TimeoutError(f"another setup has held {self.path} for {self.wait:g} seconds") from None
                time.sleep(0.05)

    def __exit__(self, *exc):
        try:
            self.path.unlink()
        except OSError:
            pass   # already gone: a later setup took a lock it judged stale


def recorded_choice(project, environ=None):
    """The cache a project's wrappers run, as (name, path), or None when it has none."""
    try:
        name, path = (wrapper_folder(project, environ) / CHOICE).read_text(encoding="utf-8").splitlines()[:2]
    except (OSError, ValueError):
        return None
    return name, Path(path)


def main(argv=None, root=None):
    parser = argparse.ArgumentParser(prog="cc-cache.py",
                                     description="Put a compiler cache in front of the compilers a build names.")
    parser.add_argument("compilers", nargs="+", help="the compilers the build runs, by name")
    parser.add_argument("--cache", choices=CACHES, help="the cache to use (default: the first of ccache and sccache "
                                                        "on PATH)")
    parser.add_argument("--shell", choices=SHELLS, default="powershell" if os.name == "nt" else "sh",
                        help="the shell that will evaluate the output (default: the system's)")
    parser.add_argument("--root", help="the project (default: the folder above scripts/)")
    args = parser.parse_args(argv)
    project = Path(root or args.root or ROOT).resolve()
    folder = wrapper_folder(project)
    windows = os.name == "nt"
    try:
        for name in args.compilers:
            if not NAME.fullmatch(name):
                raise Refused(f"{name!r} is not a compiler's name (a bare name, as the build runs it)")
            suffix = Path(name).suffix.upper()
            if windows and suffix and suffix in os.environ.get("PATHEXT", ".COM;.EXE;.BAT;.CMD").upper().split(";"):
                # The shell would find the real clang.exe before a wrapper named clang.exe.cmd.
                raise Refused(f"name the compiler without its extension: {Path(name).stem}, not {name}")
        off, cache = None, None
        if os.environ.get(OPT_OUT):
            off = f"{OPT_OUT} is set"
        else:
            wanted = (args.cache,) if args.cache else CACHES
            cache = next((found for found in (find(name, skip=[folder]) for name in wanted) if found), None)
            if cache is None:
                off = f"{args.cache} is not installed" if args.cache else \
                    f"no compiler cache is installed ({' or '.join(CACHES)})"
        wrappers, wrapped = {}, []
        if off is None:
            for name in args.compilers:
                real = find(name, skip=[folder])
                if real is None:
                    print(f"cc-cache: {name} is not installed: no wrapper for it", file=sys.stderr)
                    continue
                wrappers[name + ".cmd" if windows else name] = wrapper_text(cache, real, windows).encode("utf-8")
                wrapped.append(name)
            if not wrappers:
                off = "none of the compilers named is installed"
        if off is not None:
            if folder.is_dir():
                with Locked(folder):
                    _keep_only(folder, ())   # a shell that still has the folder on PATH reaches the real compilers
            print(comment(f"cc-cache: off: {off}", args.shell))
            print(f"cc-cache: off: {off}", file=sys.stderr)
            return 0
        line = statement(folder, args.shell)
        name = "sccache" if cache.stem.lower().startswith("sccache") else "ccache"
        with Locked(folder):
            folder.mkdir(parents=True, exist_ok=True)
            for wrapper, data in wrappers.items():
                _write(folder / wrapper, data, executable=True)
            _write(folder / CHOICE, f"{name}\n{cache}\n".encode("utf-8"), executable=False)
            _keep_only(folder, {*wrappers, CHOICE})
    except Refused as exc:
        print(f"cc-cache: {exc}", file=sys.stderr)
        return 2
    except TimeoutError as exc:
        print(f"cc-cache: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"cc-cache: {folder}: cannot be written ({exc.strerror or exc})", file=sys.stderr)
        return 1
    print(line)
    print(f"cc-cache: {name} in front of {', '.join(wrapped)} ({folder})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())

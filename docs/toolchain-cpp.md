# The C and C++ toolchain

How this project pins its build tools, keeps a compiler cache in front of its compilers, and keeps the compile database that clangd reads trustworthy. The files came from the Conclave-SDK `toolchain-cpp` kit once and are the project's own now: change them as the project needs.

The build itself is the project's. Nothing here chooses a build system, writes a build command or installs a tool.

## What is here

| File | What it is for |
| --- | --- |
| `.conclave/toolchain-cpp.json` | The pin file: each tool, the commands that satisfy it, the least version |
| `scripts/toolchain-check.py` | Says which pinned tools are missing or too old; installs nothing |
| `scripts/compdb-check.py` | Says whether `compile_commands.json` can be trusted; runs and writes nothing |
| `scripts/cc-cache.py` | Puts ccache or sccache in front of the compilers a build names |
| `scripts/cache-stats.py` | Prints the cache's hit rate; can fail a rebuild that did not use the cache |
| `.clangd` | clangd's configuration: where the compile database is |

## Are the tools there?

```text
python scripts/toolchain-check.py --pins .conclave/toolchain-cpp.json
python scripts/toolchain-check.py --pins .conclave/toolchain-cpp.json --json
```

One line per tool: `ok` with the path found and its version, or why not (not installed, with the line that installs it; too old, with both versions; or what went wrong when it was run). The word after the tool's name is `ok`, `setup_required` for a required tool that is not ok, or `optional` for a tool the pin marks optional that is not ok: an `optional` line is information, not setup the project needs. It exits 0 when every required tool is ok, 3 when one is not, and 2 when the pin file does not load. A tool marked `optional` is shown and never fails the check.

Run it first when a build fails for a reason the code does not explain: a missing linker or an old compiler looks like a broken build.

When a tool is missing or too old, the answer is to install it, and that is the operator's to do: the check prints the line. Do not lower a `min_version`, mark a tool `optional` or name another binary to make the check pass, unless the project has really changed what it builds with.

### Changing a pin

Edit `.conclave/toolchain-cpp.json`. Each tool has a `name`, an `install` line, an optional `optional`, and `commands` in order of preference; each command has its own `version_args`, `version_pattern` (one group that captures dotted numbers) and `min_version`, because alternatives such as `clang` and `gcc` number their releases independently. `command` is a bare program name, never a path. To pin the binary the project really builds with, name it: `clang-19` in place of `clang`. The check refuses a pin file that breaks these rules and names the file, the tool and the key.

## Is the compile database current?

clangd takes every flag from `compile_commands.json`. One that is out of date gives wrong diagnostics, and they look exactly like right ones.

```text
python scripts/compdb-check.py
python scripts/compdb-check.py --json
python scripts/compdb-check.py --source src/new_file.c
```

It prints one status and exits 0 only for `current`.

| Status | What it means | What to do |
| --- | --- | --- |
| `current` | It parses, its sources exist, no build file is newer | Nothing |
| `missing` | There is no database (so after a fresh install) | Generate it with the project's command |
| `stale` | A build file is newer, a source is gone, or a `--source` has no entry | Regenerate it |
| `invalid` | It cannot be read or is not a compile database | Regenerate it; never repair it by hand |
| `unknown` | A build file or a source cannot be inspected, or it is too large | Fix the file the detail names |

Exit 2 means the pin file does not load: correct what the message names. Never delete the pin file to get past it.

**After adding, moving or removing a source file, regenerate the database whatever the check says.** A build that finds its sources by itself (a wildcard in a Makefile, a glob in CMake) changes no build file when a source is added, so the check still reads `current` while the database lacks the new file. Then prove the file is in it:

```text
python scripts/compdb-check.py --source src/new_file.c --source src/moved_file.c
```

Trust clangd's diagnostics for a file only once that is `current`.

The check never regenerates anything. The pin file can record the project's own command, and the check then prints it for a database that is not current:

```json
{"schema_version": 1, "tools": [],
 "compdb": {"path": "build/compile_commands.json",
            "build_files": ["CMakeLists.txt", "cmake/flags.cmake"],
            "regenerate": ["cmake", "-S", ".", "-B", "build"]}}
```

`path` is the database (`compile_commands.json` when left out), `build_files` the files that change the compile flags (`build.ninja`, `Makefile` and `CMakeLists.txt` where they exist, when left out), and `regenerate` the command as a list of arguments. When the project has a command and the pin file does not record it, record it there, so the next person and the next agent are given it.

## clangd

`.clangd` tells clangd where the database is: `CompilationDatabase: .` is the folder `.clangd` is in. clangd reads only a file named `compile_commands.json`, in the folder it is given first and in a `build` folder of it when that has none. When the database is anywhere else, write its folder in `.clangd` (`CompilationDatabase: out`). Do not change `.clangd`, or the database, to make a diagnostic go away: fix the code, or the build that wrote the flags.

With the Conclave `lsp` kit in the project, clangd is in `.conclave/lsp.json` and `python scripts/lsp-diagnostics.py --all --changed` checks a change with it (`docs/language-servers.md`).

## The compiler cache

A compiler cache cuts the time and memory of a rebuild. `scripts/cc-cache.py` writes a wrapper for each compiler the build names and prints the one statement that puts the wrappers first on `PATH`. A program cannot change its caller's `PATH`, so the build script evaluates that statement itself, after checking that the setup succeeded. Name the compilers the build runs (`cc c++`, `clang`, `gcc g++`).

In `sh`:

```sh
setup=$(python3 scripts/cc-cache.py cc c++) || exit 1
eval "$setup"
```

In PowerShell:

```powershell
$setup = python scripts/cc-cache.py --shell powershell clang
if ($LASTEXITCODE) { exit 1 }
$setup | Invoke-Expression
```

In a batch file:

```bat
set "CC_CACHE_SETUP=%TEMP%\cc-cache-setup-%RANDOM%.cmd"
python scripts\cc-cache.py --shell cmd clang > "%CC_CACHE_SETUP%" || exit /b 1
call "%CC_CACHE_SETUP%"
del "%CC_CACHE_SETUP%"
```

Check the exit before evaluating: a plain `eval "$(...)"` turns a setup that failed into a quiet success. With no cache installed the setup succeeds and changes nothing, so a missing cache never breaks a build. `--cache ccache` or `--cache sccache` chooses one; otherwise the first of the two on `PATH` is used.

`BUILD_NO_CCACHE=1` turns the cache off for one build. It is for measuring, or for a compiler the cache does not understand. It is not a way past a failing build: a build that fails with the cache and passes without it has a fault worth finding.

Did the rebuild use the cache?

```text
python scripts/cache-stats.py --zero
python scripts/cache-stats.py --min-hit-rate 60
python scripts/cache-stats.py --json
```

`--zero` resets the statistics before a build. After it, the second line prints hits, misses and the rate, and exits 3 when the rate is below the figure or no compile went through the cache at all. It reads the cache the setup chose.

## What is left to the project

- The build system and every build command: Make, Ninja, CMake or anything else.
- Generating `compile_commands.json` (CMake's `CMAKE_EXPORT_COMPILE_COMMANDS`, Bear, Ninja's `compdb` tool, or the project's own script).
- Installing and upgrading tools. The check names what is missing and how to install it; a person installs it.
- Target flags, include paths and clang-tidy checks in `.clangd`.

From a Conclave-SDK checkout, `python forge.py doctor <project> toolchain-cpp` reports all of the above in one list: each file, each pinned tool, the compile database, and whether clangd is wired to it.

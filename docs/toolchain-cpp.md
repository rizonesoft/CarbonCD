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
| `scripts/hardening-check.py` | Says whether compile commands and built binaries carry the declared hardening |
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

## Memory safety

A C or C++ program can be built so that the commonest memory errors end the process at once instead of corrupting it: bounds checks in the standard library, a stack protector, control-flow guards and an image the loader can randomise. The project declares the hardening it builds with in the pin file's `hardening` key, and one command says whether the compile commands and the built binaries really carry it:

```text
python scripts/hardening-check.py
python scripts/hardening-check.py --json
```

It reads `compile_commands.json` and the binaries the policy names, and runs and writes nothing. It exits 0 when every requirement holds (`ok`) or nothing is required (`none`), 1 when an answer cannot be told (`unknown`), 3 when a requirement is missing, and 2 when the pin file does not load. The first line is the compile check, then one line per requirement; then the image check, then one line per binary:

```text
hardening:compile: none: no policy declared: add a `hardening` key to the pin file (docs/toolchain-cpp.md, Memory safety)
hardening:compile: ok: compile_commands.json: 3 requirement(s) on 12 first-party entries; 4 excluded, 0 outside the sources
hardening:compile: missing: compile_commands.json: 1 of 3 requirement(s) missing on 12 first-party entries; 4 excluded, 0 outside the sources
hardening:compile: unknown: the compile database is stale, so the flags it holds may not be the build's: ...
```

### The policy

`compile` lists what every compile command of the project's own sources must carry: a definition (`NAME` or `NAME=VALUE`, matched in its `-D` or `/D` form) or an option (starting with `-` or `/`). `sources` are the project's own folders under the pin file's `root` (default `.`), and `exclude` the folders under them that are not (vendored code). `binaries` lists the built files to read (a `*` is allowed in the file name), `image` the properties each must have, and `test_binaries` the built files allowed a sanitizer runtime. A policy declares `compile`, `binaries` or both; a build is shippable only under both.

How a command is read: the last occurrence of an option wins (`-fstack-protector-strong ... -fno-stack-protector` lacks it); `-fno-stack-protector` undoes every stack protector level, and a stronger level satisfies a weaker one (`-fstack-protector-all` carries `-fstack-protector-strong`); `-U` after `-D` removes a definition; a response file (`@flags.rsp`) is read; the `/` spellings are read for `cl` and `clang-cl` only, after a ccache or sccache launcher; arguments for the linker (`-Wl,...`) are not compile flags. An entry whose command cannot be read (a missing response file, an unbalanced quote) is counted and the others are still checked: a requirement a readable entry lacks is `missing` whatever the unreadable one holds.

The check reads what each command passes, not what a compiler turns on by itself. Ubuntu's GCC 15.2.0 enables `-fstack-protector-strong`, `_FORTIFY_SOURCE=3`, PIE and `-z now` without a flag, and Ubuntu's Clang 19.1.7 does not: declare each option the project depends on in its own build, so the binary is the same on any compiler, and the check sees it.

MSVC, with the image read from the linked `.exe`:

```json
{"hardening": {"compile": ["_MSVC_STL_HARDENING=1", "/guard:cf", "/sdl"], "sources": ["src"],
               "binaries": ["build/Release/*.exe"],
               "image": ["DYNAMIC_BASE", "HIGH_ENTROPY_VA", "NX_COMPAT", "GUARD_CF", "CET_COMPAT"]}}
```

Clang with libc++ on llvm-mingw (Windows), where Control Flow Guard and the shadow stack are Windows image properties:

```json
{"hardening": {"compile": ["_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST", "-fstack-protector-strong",
                           "-ftrivial-auto-var-init=zero", "-mguard=cf"], "sources": ["src"],
               "binaries": ["build/*.exe"], "image": ["DYNAMIC_BASE", "NX_COMPAT", "GUARD_CF", "CET_COMPAT"]}}
```

Clang on Linux, with libstdc++ (its default there; with `-stdlib=libc++`, the libc++ macro replaces `_GLIBCXX_ASSERTIONS`). `-mguard=cf` is refused for a Linux target:

```json
{"hardening": {"compile": ["_GLIBCXX_ASSERTIONS", "-fstack-protector-strong", "-ftrivial-auto-var-init=zero",
                           "_FORTIFY_SOURCE=3", "-fPIE"], "sources": ["src"], "binaries": ["build/app"],
               "image": ["PIE", "RELRO", "BIND_NOW", "NX_STACK", "STACK_PROTECTOR"]}}
```

GCC with libstdc++, on Linux:

```json
{"hardening": {"compile": ["_GLIBCXX_ASSERTIONS", "-fstack-protector-strong", "_FORTIFY_SOURCE=3", "-fPIE"],
               "sources": ["src"], "exclude": ["src/third_party"], "binaries": ["build/app"],
               "image": ["PIE", "RELRO", "BIND_NOW", "NX_STACK", "STACK_PROTECTOR"],
               "test_binaries": ["build/tests/*"]}}
```

`_FORTIFY_SOURCE` works only with optimisation (`-O1` or higher); where the compiler predefines it, write `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3` so the build does not warn of a redefinition.

### The options, and where each was measured

Clang rows hold for Clang on Linux and on llvm-mingw unless a row says llvm-mingw only, the libc++ macro only with libc++ (llvm-mingw's default, `-stdlib=libc++` elsewhere); GCC rows hold for GCC with libstdc++ on Linux. Each was proved by the result it changes beside a build without it: a probe that commits the error on purpose ends in the option's own trap, a probe reads the value an initialising option sets, or `hardening-check.py` reads the property from the linked image.

| Family | Option | Effect | Measured on |
| --- | --- | --- | --- |
| MSVC | `/D_MSVC_STL_HARDENING=1` | Library bounds checks; ends the process | MSVC 19.52 (`0xC0000409`) |
| MSVC | `/guard:cf` (compile and link) | Control Flow Guard | MSVC 19.52 (image flag) |
| MSVC | `/CETCOMPAT` (link) | Shadow stack compatible | MSVC 19.52 (image flag) |
| MSVC | `/sdl` | Extra checks; an uninitialised read becomes error C4700 | MSVC 19.52 |
| Clang | `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST` | libc++ bounds checks | llvm-mingw 23.1.1 |
| Clang | `-fstack-protector-strong` | Stack protector | Clang 19.1.7 on Linux; not on llvm-mingw |
| Clang | `-ftrivial-auto-var-init=zero` | Locals start as zero | Clang 19.1.7 on Linux; not on llvm-mingw |
| Clang | `-mguard=cf` | Control Flow Guard; llvm-mingw only | llvm-mingw 23.1.1 (image flag) |
| Clang | `-Wl,-Xlink=-cetcompat` | Shadow stack; llvm-mingw only (not `-Wl,--cetcompat`) | llvm-mingw 23.1.1 |
| GCC | `-D_GLIBCXX_ASSERTIONS` | Library bounds checks; ends with exit 134 | GCC 15.2.0 |
| GCC | `-fstack-protector-strong` | Stack protector (`stack smashing detected`) | GCC 15.2.0, Clang 19.1.7 |
| GCC | `-D_FORTIFY_SOURCE=2` or `=3` | Checked string and memory calls | GCC 15.2.0, Clang 19.1.7 (glibc) |
| GCC | `-Wl,-z,relro,-z,now` | Full RELRO | GCC 15.2.0 (image) |

### The probes

The programs and commands that measured the table, run 2026-11-16 and 2026-11-17, to repeat on another compiler before relying on a row. Each runs beside its control, and the option holds only when the two results differ as shown. `A` stands for a 69-character argument (`AAAA...`); `cc` is `gcc` or `clang`.

`oob.cpp` reads past the end of a `std::vector`, for the library bounds checks:

```cpp
#include <vector>
#include <cstdio>
int main(int argc, char **) { std::vector<int> v(4); std::printf("%d\n", v[argc + 7]); return 0; }
```

```text
cl /nologo /EHsc /O2 oob.cpp /Fe:msvc0.exe                                  # msvc0.exe exits 0
cl /nologo /EHsc /O2 /D_MSVC_STL_HARDENING=1 oob.cpp /Fe:msvc1.exe          # msvc1.exe exits 0xC0000409
x86_64-w64-mingw32-clang++ -O2 oob.cpp -o oob0.exe                          # exits 0
x86_64-w64-mingw32-clang++ -O2 -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST oob.cpp -o oob1.exe   # exits 0xC000001D
g++ -O2 oob.cpp -o g0 && ./g0                                               # exits 0
g++ -O2 -D_GLIBCXX_ASSERTIONS oob.cpp -o g1 && ./g1                         # stl_vector.h assertion, exit 134
```

`smash.c` overruns a stack buffer:

```c
#include <string.h>
#include <stdio.h>
__attribute__((noinline)) void f(const char *s) { char buf[8]; strcpy(buf, s); puts(buf); }
int main(int argc, char **argv) { f(argc > 1 ? argv[1] : "x"); return 0; }
```

```text
cc -O0 -fno-stack-protector smash.c -o s0 && ./s0 A                         # segmentation fault, exit 139
cc -O0 -fstack-protector-strong smash.c -o s1 && ./s1 A                     # *** stack smashing detected ***, exit 134
cc -O2 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 smash.c -o f0 && ./f0 A   # exit 139
cc -O2 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 smash.c -o f2 && ./f2 A   # *** buffer overflow detected ***, exit 134
cc -O2 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 smash.c -o f3 && ./f3 A   # *** buffer overflow detected ***, exit 134
```

`uninit.c` fills the stack, then reads a local it never set:

```c
#include <stdio.h>
__attribute__((noinline)) int g(int c) { int x; if (c > 5) x = 1; return x; }
__attribute__((noinline)) void dirty(void) { volatile int junk[64]; for (int i = 0; i < 64; i++) junk[i] = 0x5a5a5a5a; }
int main(int argc, char **argv) { (void)argv; dirty(); printf("%d\n", g(argc)); return 0; }
```

```text
cc -O0 uninit.c -o u0 && ./u0                                               # stack contents (2067949718 on GCC 15.2.0)
cc -O0 -ftrivial-auto-var-init=zero uninit.c -o u1 && ./u1                  # 0
```

`sdl.c` reads an uninitialised local, for MSVC:

```c
#include <stdio.h>
int main(void) { int x; printf("%d\n", x); return 0; }
```

```text
cl /nologo sdl.c                                                            # warning C4700, and it builds
cl /nologo /sdl sdl.c                                                       # error C4700, and it does not
```

The link properties are read from the image by `scripts/hardening-check.py`, with a pin file that names the binaries built in `build/`:

```text
gcc -O1 -fPIE -pie -Wl,-z,relro,-z,now smash.c -o build/full
gcc -O1 -fPIE -pie -Wl,-z,relro,-z,lazy smash.c -o build/lazy
x86_64-w64-mingw32-clang -O1 smash.c -o build/plain.exe
x86_64-w64-mingw32-clang -O1 -mguard=cf smash.c -o build/cfg.exe
x86_64-w64-mingw32-clang -O1 -Wl,-Xlink=-cetcompat smash.c -o build/cet.exe
x86_64-w64-mingw32-clang -O1 -Wl,--cetcompat smash.c -o build/x.exe         # lld: error: unknown argument: --cetcompat

.conclave/toolchain-cpp.json:
{"schema_version": 1, "tools": [],
 "hardening": {"binaries": ["build/full", "build/lazy"], "image": ["RELRO", "BIND_NOW"]}}
python scripts/hardening-check.py   # build/full ok; build/lazy (ELF) lacks BIND_NOW; exit 3

with "binaries": ["build/plain.exe", "build/cfg.exe", "build/cet.exe"], "image": ["GUARD_CF", "CET_COMPAT"]:
python scripts/hardening-check.py   # plain.exe lacks both; cfg.exe lacks CET_COMPAT; cet.exe lacks GUARD_CF
```

MSVC's `/guard:cf` and `/CETCOMPAT` were read the same way, from images built by the commands in `scripts/tests/fixtures/README.md` (`pe-hardened.exe` and its control `pe-plain.exe`), and confirmed with `dumpbin`.
### What hardening does not cover

Hardening covers bounds and control flow, not object lifetime: a use after free or a double free is found by a sanitizer test build (`-fsanitize=address`) and by fuzzing, and neither ships. Name a sanitizer build in `test_binaries`; a sanitizer runtime in any other binary is a finding. A hardened process that hits a check ends at once and reaches no handler inside the process, so a shipped program needs a way for its next start to say that the last run stopped.

## What is left to the project

- The build system and every build command: Make, Ninja, CMake or anything else.
- Generating `compile_commands.json` (CMake's `CMAKE_EXPORT_COMPILE_COMMANDS`, Bear, Ninja's `compdb` tool, or the project's own script).
- Installing and upgrading tools. The check names what is missing and how to install it; a person installs it.
- Target flags, include paths and clang-tidy checks in `.clangd`.

From a Conclave-SDK checkout, `python forge.py doctor <project> toolchain-cpp` reports all of the above in one list: each file, each pinned tool, the compile database, whether clangd is wired to it, and the hardening policy (`hardening-policy`, then `hardening:compile` and `hardening:image` when one is declared). The doctor passes `--strict` with `unknown` hardening rows, so it is no release gate: a build is shippable when `python scripts/hardening-check.py` exits 0 with status `ok`.

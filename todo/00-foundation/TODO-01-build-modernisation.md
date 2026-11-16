---
schema_version: 1
id: build-modernisation
domain: 00-foundation
status: active
title: "TODO-01 -- Build modernisation"
---

# TODO-01 -- Build modernisation

> **Goal:** CarbonCD builds as a 64-bit Unicode C++20 program from one CMake build, with its disc engine in a library that has no MFC in it and is covered by tests that run without a drive, on every push.

> **Current state:** Measured 2026-11-16. The app is one MFC project, `src/cdm.vcxproj`: Win32 only (`:6-14`), MultiByte (`:27`), static MFC, toolset v143, `WINVER 0x0501` (`src/stdafx.h:9-22`), `_CRT_SECURE_NO_WARNINGS` (`src/stdafx.h:3`). There are no tests and no CI. This machine has Visual Studio 2026 (18.12) without the MFC component and without v143, so `msbuild CarbonCD.sln` stops at MSB8020 and nothing has been compiled since the move to `src/`. `cmake`, `ninja`, `clang` and `clangd` are installed; `compile_commands.json` does not exist, which `python forge.py doctor . toolchain-cpp` reports as `compile-database: setup_required`. Some sources are code page 932 with Japanese text (`src/cdm.rc`, `src/res/cdm.rc2`, comments, and literals at `src/MasteringPage2.cpp:268,318` and `src/TocWindow.cpp:330`) while the project passes `/utf-8`.

## Inputs

- `AGENTS.md` (Goal, Layout, Verify) and `design/SPEC.md` S0 (platform: Windows 10 1809 and later, 64-bit, Unicode).
- `src/cdm.vcxproj`, `src/stdafx.h`, `src/res/cdm.manifest`, `src/CDM.h:9` (`APP_VERSION`): what §2 to §8 replace.
- `.conclave/toolchain-cpp.json`, `.clangd`, `docs/toolchain-cpp.md`: the pins and the compile database §2 wires.
- -> XREF: D01 T01 §1 -- builds on §3 of this file
- -> XREF: D01 T01 §3 -- builds on §4 of this file
- -> XREF: D01 T01 §6 -- builds on §3 of this file
- -> XREF: D01 T01 §7 -- builds on §3 of this file
- -> XREF: D01 T01 §8 -- builds on §3 of this file
- -> XREF: D02 T01 §1 -- builds on §3 of this file
- -> XREF: D03 T01 §1 -- builds on §7 of this file
- -> XREF: D03 T02 §15 -- builds on §9 of this file
- -> XREF: D04 T01 §1 -- builds on §3 of this file
- -> XREF: D04 T01 §3 -- builds on §3 of this file
- -> XREF: D04 T02 §1 -- builds on §4 of this file
- -> XREF: D04 T02 §5 -- builds on §10 of this file
- -> XREF: D01 T02 §11 -- builds on §12 of this file
- -> XREF: D03 T01 §17 -- builds on §11 of this file

## Outcome

- `cmake --preset x64-release` then `cmake --build --preset x64-release` writes `Bin/Release/CarbonCD.exe`, a 64-bit Unicode executable.
- `ctest --preset x64-debug` runs the core library's tests with no optical drive and passes.
- `python scripts/compdb-check.py` prints `current`, and `python scripts/lsp-diagnostics.py --all --changed` runs clangd over changed files.
- A push to `master` runs the build and the tests on GitHub Actions.

**Adjacency:** all=not-applicable (build system, compiler settings and test harness: no record a user lists, exports, is told about or reverses)

**Adjacency rationale:** The only tunables are build presets, consumed by CMake and CI. Refusals are compiler errors and failing tests, each exercised by its section's checkpoint.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | MFC component installed and the 1.0.8 baseline build recorded | -- | [ ] |
| 2 | §2 | CMake build with presets, the core library target and the compile database | -- | [ ] |
| 3 | §3 | Test harness with doctest and CTest | §2 | [ ] |
| 4 | §4 | App target in CMake with static MFC | §1, §2 | [ ] |
| 5 | §5 | 64-bit target and pointer-size fixes | §4 | [ ] |
| 6 | §6 | Source files converted to UTF-8 | §4 | [ ] |
| 7 | §7 | Unicode build of the app | §5, §6 | [ ] |
| 8 | §8 | Secure CRT and warning-clean build | §7 | [ ] |
| 9 | §9 | One version source, version resource and manifest | §4 | [ ] |
| 10 | §10 | Continuous integration on GitHub Actions | §3, §8 | [ ] |
| 11 | §11 | Memory-safety hardening of every target | §8 | [ ] |
| 12 | §12 | AddressSanitizer preset, code analysis and probe | §3, §11 | [ ] |

## 1. MFC component installed and the 1.0.8 baseline build recorded

Nothing that links MFC can be built on this machine until the MFC component is installed. This section is the operator's one setup step, and it records what 1.0.8 builds to before anything changes.

**Phase:** 0
**Needs:** operator
**Surface:** no

**Build order:**

1. Install the MFC component into Visual Studio from an elevated prompt Done when: `Test-Path` finds `atlmfc\lib\x64\nafxcw.lib` under the installed MSVC toolset.
2. Build 1.0.8 as it is and keep the result as the baseline Done when: `Bin\Release\CarbonCD.exe` starts and shows the main dialog.

- [ ] Run, elevated: `& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "$env:ProgramFiles\Microsoft Visual Studio\18\Professional" --add Microsoft.VisualStudio.Component.VC.ATLMFC --passive --norestart` Done when: `Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\18\Professional\VC\Tools\MSVC\*\atlmfc\lib\x64\nafxcw.lib"` lists the file. Cheaper substitute: copying MFC libraries in by hand from another machine.
- [ ] Build `CarbonCD.sln` Release Win32 with the installed toolset through `bosun run -- msbuild CarbonCD.sln -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=<installed>` Done when: the build ends with 0 errors and `Bin\Release\CarbonCD.exe` exists. Cheaper substitute: editing `PlatformToolset` in `src/cdm.vcxproj` just to get this one build through.
- [ ] Record the baseline in `docs/baseline-1.0.8.md`: toolset, warning count, executable size, and a screenshot of the main dialog with the new icons Done when: the file names each figure and the screenshot shows the eight Carbon Copy button bitmaps. Cheaper substitute: writing the figures from memory without running the build.
- [ ] Commit: `docs: baseline build of 1.0.8`

**Test checkpoint:** `Bin\Release\CarbonCD.exe` exists and `(Get-Item Bin\Release\CarbonCD.exe).VersionInfo.FileVersion` prints a version; it fails while the MFC component is missing.

## 2. CMake build with presets, the core library target and the compile database

One build description for the library, the tests and, once MFC is present, the app. It starts with the library and tests only, so work on the engine does not wait for MFC.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Add the root `CMakeLists.txt` and `CMakePresets.json` with Ninja and MSVC Done when: `cmake --preset x64-debug` configures with no error.
2. Point the toolchain pins and clangd at the build's compile database Done when: `python scripts/compdb-check.py` prints `current`.

- [ ] Create `CMakeLists.txt` (CMake 3.28, C++20, `/W4 /permissive- /utf-8`, `UNICODE` and `_UNICODE`) with a static library target `carbon_core` from `src/core/` and `enable_testing()` Done when: `cmake --preset x64-debug` and `cmake --build --preset x64-debug` succeed with an empty `src/core/Version.cpp`. Cheaper substitute: a Visual Studio only `.vcxproj` for the library.
- [ ] Create `CMakePresets.json` with configure, build and test presets `x64-debug` and `x64-release` (generator Ninja, binary dir `build/<preset>`, `CMAKE_EXPORT_COMPILE_COMMANDS` on) Done when: `cmake --list-presets` prints both. Cheaper substitute: hard-coding a generator or a compiler path in `CMakeLists.txt`.
- [ ] Set `compdb` in `.conclave/toolchain-cpp.json` to `build/x64-debug/compile_commands.json` with `build_files` `CMakeLists.txt` and `CMakePresets.json` and `regenerate` `cmake --preset x64-debug`, and set `CompilationDatabase: build/x64-debug` in `.clangd` Done when: `python scripts/compdb-check.py` prints `current` and `python forge.py doctor . toolchain-cpp` (run from the SDK) reports `compile-database` and `clangd-config` ok. Cheaper substitute: copying a `compile_commands.json` to the repository root by hand.
- [ ] Pin the compiler in `.conclave/toolchain-cpp.json` as `cl` (MSVC 19.40 or later) ahead of `clang`, and make `ninja` required Done when: `python scripts/toolchain-check.py --pins .conclave/toolchain-cpp.json` exits 0 and names `cl`. Cheaper substitute: leaving the seed pins, which do not name the compiler the project builds with.
- [ ] Document the commands in `AGENTS.md` (Verify) and `README.md` (Build) Done when: both name `cmake --preset`, `cmake --build --preset` and `ctest --preset`.
- [ ] Commit: `build: CMake presets, carbon_core target and compile database`

**Test checkpoint:** `cmake --preset x64-debug; cmake --build --preset x64-debug; python scripts/compdb-check.py` ends with `current`; deleting `CMakePresets.json` makes the first command fail.

## 3. Test harness with doctest and CTest

Tests for the core library that run with no drive and no window.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Vendor doctest and add a `carbon_tests` executable registered with CTest Done when: `ctest --preset x64-debug` runs one passing test.
2. Prove a failing test fails the run Done when: a deliberately false assertion makes `ctest` exit non-zero.

- [ ] Add `third_party/doctest/doctest.h` (single header, MIT, version noted in `third_party/README.md` with its licence text) Done when: the file is present and `third_party/README.md` names the version and source URL. Cheaper substitute: fetching it at configure time, which needs a network on every build.
- [ ] Add `tests/CMakeLists.txt` with `carbon_tests` linking `carbon_core`, and `tests/core/test_version.cpp` asserting `carbon::version()` returns the string in `src/core/Version.cpp` Done when: `ctest --preset x64-debug` reports 1 test passed. Cheaper substitute: a test that only prints and cannot fail.
- [ ] Add a `test` preset per configure preset in `CMakePresets.json` with `outputOnFailure` Done when: `ctest --preset x64-debug` prints the failing assertion when one is broken on purpose, then passes when restored.
- [ ] Commit: `test: doctest harness for carbon_core`

**Test checkpoint:** `ctest --preset x64-debug` ends `100% tests passed`; changing the expected string in `tests/core/test_version.cpp` makes it fail.

## 4. App target in CMake with static MFC

The MFC app moves from `src/cdm.vcxproj` to the CMake build, still 32-bit character set at this point, so the build system changes before the code does.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Add the `CarbonCD` executable target with `CMAKE_MFC_FLAG 1`, the resource script and the manifest Done when: `cmake --build --preset x64-release` links `Bin/Release/CarbonCD.exe`.
2. Retire the old project files Done when: `CarbonCD.sln` and `src/cdm.vcxproj*` are gone and Visual Studio opens the folder as a CMake project.

- [ ] Add target `CarbonCD` in `src/CMakeLists.txt` listing every `.cpp` now in `src/cdm.vcxproj:150-218`, `cdm.rc`, precompiled header `stdafx.h`, `CMAKE_MFC_FLAG 1`, `WIN32_EXECUTABLE`, link `htmlhelp` and `winmm`, output directory `Bin/$<CONFIG>`, and set `CMAKE_MSVC_RUNTIME_LIBRARY` to `MultiThreaded$<$<CONFIG:Debug>:Debug>` for every target (static MFC needs the static runtime in the library and the tests too) Done when: `cmake --build --preset x64-release` under the Ninja generator produces `Bin/Release/CarbonCD.exe`, and `dumpbin /dependents` lists no `mfc*.dll` or `vcruntime*.dll`. Cheaper substitute: a custom command that shells out to msbuild on the old project.
- [ ] Guard the app target with a check for MFC (`find_package(MFC)`), so a machine without MFC still builds `carbon_core` and the tests and prints one line saying the app was skipped Done when: configuring with MFC hidden prints the skip line and `ctest` still passes. Cheaper substitute: failing the whole configure when MFC is missing.
- [ ] Delete `CarbonCD.sln`, `src/cdm.vcxproj`, `src/cdm.vcxproj.filters`, `src/cdm.vcxproj.user` and the tracked build leftovers under `Bin/Release` (`*.tlog`, `*.pdb`, `*.recipe`, `cdm.log`, `cdm.res`, `*.FileListAbsolute.txt`), and ignore `Bin/**` except `.gitkeep` Done when: `git ls-files Bin` lists only `Bin/.gitkeep`, and `Bin/Release/CarbonCD.ini`, `English.cml`, `Korean.cml` and `CarbonCD.chm` are confirmed identical to or older than their `Distribution/` copies before removal. Cheaper substitute: deleting `Bin/` itself, which the owner asked to keep where it is.
- [ ] Update `AGENTS.md` and `README.md` to the CMake build Done when: neither mentions `msbuild` or `CarbonCD.sln`.
- [ ] Commit: `build: MFC app target in CMake, old project files retired`

**Test checkpoint:** `cmake --build --preset x64-release` exits 0 and `Bin/Release/CarbonCD.exe` starts; removing a source from `src/CMakeLists.txt` breaks the link.

## 5. 64-bit target and pointer-size fixes

The app is built for x64 only. The code has three pointer-to-DWORD casts and one MFC override with the wrong signature that stop or corrupt a 64-bit build.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Make x64 the only platform and fix what the compiler then refuses Done when: the x64 build has 0 errors.
2. Remove every pointer truncation the compiler warns of Done when: `/we4311 /we4312` are errors and the build passes.

- [ ] Change `CCDMDlg::WinHelp` in `src/CDMDlg.h:100` and `src/CDMDlg.cpp:1084` to MFC's `DWORD_PTR` signature Done when: the x64 build compiles `CDMDlg.cpp`. Cheaper substitute: deleting the override, which drops the help redirect.
- [ ] Replace the pointer casts to `DWORD` in `src/PBBuffer.cpp:19` and `src/SptiDriver.cpp:157,186` with `uintptr_t` arithmetic Done when: the alignment tests in `tests/core/test_aligned_buffer.cpp` pass for buffers above 4 GB addresses (allocated with `VirtualAlloc` and `MEM_TOP_DOWN`). Cheaper substitute: silencing C4311 with a pragma.
- [ ] Add `/we4311 /we4312 /we4267` to the app and library targets in `CMakeLists.txt` Done when: the build passes with them on, and reintroducing one cast fails it. Cheaper substitute: a warning level that leaves truncation as a warning.
- [ ] Set `WINVER` and `_WIN32_WINNT` to `0x0A00` in `src/stdafx.h` and remove `_WIN32_WINDOWS` and `_WIN32_IE` Done when: the build passes and `dumpbin /headers Bin\Release\CarbonCD.exe` shows machine `8664`.
- [ ] Commit: `build: x64 only, pointer truncation is an error`

**Test checkpoint:** `dumpbin /headers Bin\Release\CarbonCD.exe | findstr machine` prints `8664 machine (x64)`; `ctest --preset x64-debug -R aligned_buffer` passes.

## 6. Source files converted to UTF-8

Sources are a mix of code page 932 and UTF-8 while the compiler is told `/utf-8`, so Japanese comments and three literals are read wrongly. Converting once, with the bytes checked, ends that.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Convert every source and resource file to UTF-8 and translate the Japanese literals Done when: no file under `src/` fails a strict UTF-8 decode.
2. Lock the encoding in Done when: `.gitattributes` and `.editorconfig` state it.

- [ ] Convert each `src/*.cpp`, `src/*.h`, `src/cdm.rc` and `src/res/cdm.rc2` that is not valid UTF-8 from code page 932 to UTF-8 with a script `scripts/convert-cp932.py` that refuses a file whose round trip does not reproduce the original bytes Done when: `python scripts/convert-cp932.py --check src` exits 0 and lists no file. Cheaper substitute: re-saving files in an editor, which guesses the code page.
- [ ] Replace the Japanese string literals at `src/MasteringPage2.cpp:268,318` and `src/TocWindow.cpp:330` with language-file strings (key names recorded for D04 T01 §5), and translate Japanese comments in the files the plan edits to English Done when: `rg -n "[\x{3040}-\x{30FF}\x{4E00}-\x{9FFF}]" src --glob "*.cpp"` finds no string literal. Cheaper substitute: leaving the literals and hoping the code page matches.
- [ ] Change `#pragma code_page(932)` and `LANGUAGE LANG_JAPANESE` blocks in `src/cdm.rc` to `#pragma code_page(65001)` with neutral language, and fix the mojibake captions (`cdm.rc:108` Cancel button) Done when: `rc.exe` compiles `cdm.rc` and the Read Track dialog's second button reads `Cancel`. Cheaper substitute: keeping two code pages in one resource script.
- [ ] Add `.editorconfig` (`charset = utf-8`, CRLF for `*.rc`) and mark `*.rc` as `working-tree-encoding=UTF-8` text in `.gitattributes` Done when: `git diff --stat` after a no-op save in Visual Studio is empty.
- [ ] Commit: `src: every source file is UTF-8`

**Test checkpoint:** `python scripts/convert-cp932.py --check src` exits 0; saving one file back as code page 932 makes it exit 1 and name the file.

## 7. Unicode build of the app

The app moves from MultiByte to Unicode. About 278 `.Format(` calls, 31 fixed `char` buffers and six Shift-JIS lead-byte tests assume one byte per character.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Switch the character set and fix every narrow string in the app layer Done when: the app builds with `UNICODE` and `_UNICODE` and no `char` path buffer remains.
2. Prove paths with characters outside the system code page work Done when: a test image in a folder named with Korean and emoji characters opens.

- [ ] Define `UNICODE` and `_UNICODE` for the app target in `src/CMakeLists.txt` and wrap string literals in `_T()` or `L""` across `src/*.cpp` Done when: the app compiles with 0 errors. Cheaper substitute: compiling with `/D_MBCS` and converting at the edges.
- [ ] Replace the fixed `char` path buffers named in the inventory (`src/Setting.cpp:23`, `src/CDMDlg.cpp:772-775`, `src/CDWriter.cpp:48`, `src/DirStructure.cpp:303,367,438`, `src/ReadSettingDialog.cpp:81`, `src/SubcodeGeneratorMS.cpp:47`, `src/CreateThread.cpp:564`, `src/MasteringPage1.cpp:196,230`, `src/TocWindow.cpp:441`) with `CString` or `std::wstring` and `std::filesystem::path` Done when: `rg -n "char\s+\w+\[(512|1024|2048|256)\]" src` finds none of them. Cheaper substitute: widening the arrays to `wchar_t[2048]`.
- [ ] Remove the Shift-JIS lead-byte tests (`src/Setting.cpp:243`, `src/ReadThread.cpp:366-388`, and the same pattern in `src/CreateThread.cpp` and `src/DirStructure.cpp`) in favour of `std::filesystem::path` parent and extension calls Done when: `rg -n "0x9f" src` finds no path-splitting code. Cheaper substitute: keeping the tests behind `#ifndef UNICODE`.
- [ ] Take the executable folder from `GetModuleFileNameW` instead of parsing `GetCommandLine()` (`src/Setting.cpp:24-62`) Done when: starting the app through a shortcut with arguments and through a quoted path with spaces both find `CarbonCD.ini` beside the executable. Cheaper substitute: trimming quotes from the command line by hand.
- [ ] Add `tests/core/test_paths.cpp` for the path helpers moved into `src/core/Paths.h` (extension swap, sibling file, parent folder) with non-ASCII names Done when: `ctest --preset x64-debug -R paths` passes and fails if `sibling()` is made to cut at the first `.`.
- [ ] Commit: `src: Unicode build, no narrow path buffers`

**Test checkpoint:** `ctest --preset x64-debug -R paths` passes, and `rg -n "lstrcpy\(|strcpy\(|sprintf\(" src` prints nothing.

## 8. Secure CRT and warning-clean build

`_CRT_SECURE_NO_WARNINGS` hides unsafe calls. With the buffers gone, the define goes and the build is held at warning level 4 with warnings as errors.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Remove the define and fix what appears Done when: the app builds without `_CRT_SECURE_NO_WARNINGS`.
2. Turn warnings into errors Done when: `/WX` is on for both targets.

- [ ] Delete `#define _CRT_SECURE_NO_WARNINGS` from `src/stdafx.h:3` and replace the remaining `fopen` (9 sites), `strncpy` and `lstrcat` calls with `CFile`, `std::ofstream` or `CString` operations Done when: the app compiles with no C4996. Cheaper substitute: re-adding the define per file.
- [ ] Replace the four `GetVersionEx` uses (for example `src/SptiDriver.cpp:101-115`) with nothing: the platform is always NT Done when: `rg -n GetVersionEx src` prints nothing. Cheaper substitute: suppressing the deprecation warning.
- [ ] Fix `new[]` freed with `delete` at `src/CDWriter.cpp:79,86` with `std::vector<BYTE>`, and remove the stray statement `Buffer[2048];` at `src/IsoCreator.cpp:232` Done when: the build is clean under `/analyze` for those two files.
- [ ] Add `/WX` to both targets in `CMakeLists.txt` Done when: the build passes, and an unused variable added on purpose fails it. Cheaper substitute: `/WX` on the library only.
- [ ] Commit: `build: secure CRT, warnings are errors`

**Test checkpoint:** `cmake --build --preset x64-release` exits 0 with `/W4 /WX`; `rg -n _CRT_SECURE_NO_WARNINGS src` prints nothing.

## 9. One version source, version resource and manifest

The version is typed in three places (`src/CDM.h:9` says 1.0.8, the manifest says 1.0.0.8, the About dialog says 0.0.0) and the manifest is x86 only with no DPI or OS declarations.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Generate the version from `CMakeLists.txt` into a header and the resource script Done when: the About text, the file properties and the manifest agree.
2. Replace the manifest Done when: the app declares per-monitor DPI awareness v2, Windows 10 and 11, `asInvoker`, and common controls 6 for any architecture.

- [ ] Set `project(CarbonCD VERSION 2.0.0)` and generate `build/<preset>/generated/Version.h` and a `VERSIONINFO` block included by `src/cdm.rc` from `src/Version.h.in` and `src/Version.rc.in` Done when: `(Get-Item Bin\Release\CarbonCD.exe).VersionInfo.ProductVersion` prints `2.0.0`. Cheaper substitute: editing three files by hand at each release.
- [ ] Remove `APP_VERSION` from `src/CDM.h:9` and use the generated header at `src/CDMDlg.cpp:58,247,986` Done when: `rg -n "1\.0\.8" src` prints nothing.
- [ ] Rewrite `src/res/cdm.manifest`: `processorArchitecture="*"` for common controls, `dpiAwareness` `PerMonitorV2`, `supportedOS` for Windows 10 and 11, `requestedExecutionLevel` `asInvoker`, `longPathAware` and `activeCodePage` UTF-8 Done when: `mt.exe -inputresource:Bin\Release\CarbonCD.exe;#1 -out:R:\scratch\m.xml` shows each element. Cheaper substitute: setting DPI awareness with an API call at start, which runs after the first window metrics are read.
- [ ] Commit: `build: generated version, modern manifest`

**Test checkpoint:** `(Get-Item Bin\Release\CarbonCD.exe).VersionInfo.FileVersion` prints `2.0.0.0`; changing the version in `CMakeLists.txt` and rebuilding changes it.

## 10. Continuous integration on GitHub Actions

Every push builds the app and runs the tests on a clean Windows machine.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Add the workflow Done when: a push shows a green run with the build and test steps.
2. Make the plan's own checks part of it Done when: the run fails when the TODO tree is invalid.

- [ ] Add `.github/workflows/ci.yml` on `windows-latest`: checkout, `cmake --preset x64-release`, `cmake --build --preset x64-release`, `ctest --preset x64-release`, upload `Bin/Release/CarbonCD.exe` as an artifact Done when: `gh run list --workflow ci.yml --limit 1` shows `completed success` for the commit. Cheaper substitute: a workflow that builds only the library.
- [ ] Add a job step running `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` Done when: a commit with a stale `todo/implementation-plan.md` turns the run red.
- [ ] Write `.conclave/ci.json` as `{"workflows": ["ci.yml"]}` Done when: `python scripts/ci_status.py` prints the workflow's latest verdict.
- [ ] Commit: `ci: build, tests and plan checks on every push`

**Test checkpoint:** `python scripts/ci_status.py --workflow ci.yml` exits 0 after the commit's run; it exits 1 when a test is broken on a branch.

## 11. Memory-safety hardening of every target

CarbonCD reads image files and drive responses it does not control, and its build sets no memory-safety option: `src/cdm.vcxproj` has none, and §2 to §8 add `/W4`, `/WX` and the secure CRT only. Measured on 2026-11-16 with this machine's compiler (MSVC 19.52, `/std:c++latest /O2`): an out-of-bounds read of a `std::vector` and of a `std::span` returned garbage and exited 0 in a plain build, and ended the process with exit `0xC0000409` (fast-fail) under `/D_MSVC_STL_HARDENING=1`, with both `/MD` and `/MT`. This section turns that and the control-flow options on for every target. It covers the standard library only: MFC's own containers (`CArray`, `CString`) are not checked by it, and a dangling pointer is §12's job. A fast-fail ends the app at once and reaches no handler inside the process, so the user needs the message D03 T01 §17 plans.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Add one function in `cmake/CarbonHardening.cmake` that every target goes through Done when: each target's compile and link lines carry the options.
2. Prove the trap and the image flags Done when: a probe test and a header check each fail with their option removed.

- [ ] Add `cmake/CarbonHardening.cmake` with `carbon_harden(<target>)` applying `/sdl`, `/guard:cf` and `_MSVC_STL_HARDENING=1` to the compile, and `/guard:cf` and `/CETCOMPAT` to the link, and call it for `carbon_core`, `CarbonCD` and `carbon_tests` Done when: `build/x64-release/compile_commands.json` shows the three compile options on a `src/core/` source. Cheaper substitute: `add_compile_options` at the root, which also reaches doctest and any fetched dependency with `/sdl` diagnostics that are not ours to fix.
- [ ] Add `tests/hardening/oob_probe.cpp`, an executable that reads a `std::vector<int>` six elements past its end, hardened through `carbon_harden` and registered with CTest under the label `hardening` through a `cmake -P` runner that passes only on exit `0xC0000409` Done when: `ctest --preset x64-release -L hardening` passes, and fails when `_MSVC_STL_HARDENING=1` is removed from `cmake/CarbonHardening.cmake`, then it is restored. Cheaper substitute: asserting the definition is on the command line, which proves nothing about a trap.
- [ ] Add `scripts/hardening-check.py <executable>`, which runs `dumpbin /headers /loadconfig` and requires `Dynamic base`, `High Entropy Virtual Addresses`, `NX compatible`, `Control Flow Guard` and `CET compatible`, naming each one missing and exiting 1, with `scripts/tests/test_hardening_check.py` over stored `dumpbin` outputs Done when: it exits 0 on `Bin/Release/CarbonCD.exe` and 1 naming `Control Flow Guard` on a probe linked without `/guard:cf`. Cheaper substitute: reading the options from the compile database, which says what was asked for and not what the linker wrote.
- [ ] Run the probe label and `scripts/hardening-check.py` in `.github/workflows/ci.yml` after the build Done when: the run for the commit is green and its log shows both.
- [ ] Record the options, what each stops and what none of them covers (MFC containers, object lifetime) in `docs/hardening.md`, and link it from `AGENTS.md` under Verify Done when: both exist and name the two commands.
- [ ] Commit: `build: standard library hardening, control-flow guard and shadow stack on every target`

**Test checkpoint:** `ctest --preset x64-release -L hardening` passes and `python scripts/hardening-check.py Bin/Release/CarbonCD.exe` exits 0; the probe fails with `_MSVC_STL_HARDENING=1` removed.

## 12. AddressSanitizer preset, code analysis and probe

§11 stops an out-of-bounds library access in the shipped app and cannot see a use after free, a double free or a raw buffer overrun, which is what the 1.0.8 inventory mostly holds (`new[]` freed with `delete` at `src/CDWriter.cpp:79,86`). AddressSanitizer finds those when the tests run, and `/analyze` finds some without running anything. Both are test builds: the sanitizer's runtime never ships. Neither was measured in this repository yet; Isotone builds with both on this machine (`native/cmake/IsotoneTargets.cmake` there: `/fsanitize=address` needs `/INCREMENTAL:NO`, and `/analyze /analyze:external-` keeps the analysis to first-party code). Whether static MFC links under the sanitizer is unmeasured, so the preset builds the core library and the tests, where the engine lives, and the first step measures the app.

**Phase:** 1
**Needs:** none
**Surface:** no

**Build order:**

1. Add an `x64-asan` preset and prove it is instrumented with a probe Done when: the probe passes there and fails in `x64-debug`.
2. Add an `x64-analyze` preset Done when: the core library builds clean under `/analyze` with `/WX`.

- [ ] Try linking `CarbonCD` (static MFC) with `/fsanitize=address` and record the outcome in `docs/hardening.md` Done when: the document says whether the app can run under the sanitizer, quoting the linker's or the runtime's output.
- [ ] Add `CARBON_SANITIZE` (a cache string, empty or `address`; any other value is a `FATAL_ERROR` naming it) to `cmake/CarbonHardening.cmake`, adding `/fsanitize=address` to the compile and `/INCREMENTAL:NO` to the link Done when: `-DCARBON_SANITIZE=thread` fails the configure naming the value. Cheaper substitute: a boolean, which cannot refuse a sanitizer nobody measured.
- [ ] Add configure, build and test presets `x64-asan` to `CMakePresets.json` (`RelWithDebInfo`, `CARBON_SANITIZE=address`, binary dir `build/x64-asan`) Done when: `cmake --preset x64-asan` and `cmake --build --preset x64-asan` exit 0 from an empty tree.
- [ ] Add `tests/hardening/uaf_probe.cpp`, which reads freed heap memory, built only when `CARBON_SANITIZE` is `address` and registered with `PASS_REGULAR_EXPRESSION "AddressSanitizer: heap-use-after-free"` Done when: `ctest --preset x64-asan -R uaf_probe` passes, and fails when the preset's `CARBON_SANITIZE` is emptied, then it is restored. Cheaper substitute: `WILL_FAIL`, which also passes when the probe fails to start.
- [ ] Run `ctest --preset x64-asan` and fix each report at the line it names in `src/core/` Done when: the preset passes and the commit message counts the reports fixed.
- [ ] Add configure and build presets `x64-analyze` with `/analyze /analyze:external-` on `carbon_core` and `CarbonCD` through `carbon_harden`, and fix or justify each warning in the code it names Done when: `cmake --build --preset x64-analyze` exits 0 with `/WX`. Cheaper substitute: a `/wd` list, which hides the defect the warning names.
- [ ] Add `asan` and `analyze` jobs to `.github/workflows/ci.yml` Done when: the run for the commit shows both green.
- [ ] Make `scripts/hardening-check.py` exit 1 when the executable imports `clang_rt.asan`, read from `dumpbin /imports` Done when: it exits 1 naming the import on the `x64-asan` test executable and 0 on `Bin/Release/CarbonCD.exe`. Cheaper substitute: trusting that nobody ships the sanitizer tree.
- [ ] Commit: `tests: the core suite runs under AddressSanitizer and code analysis`

**Test checkpoint:** `ctest --preset x64-asan` passes with `uaf_probe` in its list, `uaf_probe` fails when `CARBON_SANITIZE` is empty, and `cmake --build --preset x64-analyze` exits 0.

## Verification

- From a clean clone: `cmake --preset x64-release`, `cmake --build --preset x64-release`, `ctest --preset x64-release` all exit 0 and `Bin/Release/CarbonCD.exe` is a 64-bit Unicode executable that starts.
- `python scripts/compdb-check.py` prints `current` and `python scripts/toolchain-check.py --pins .conclave/toolchain-cpp.json` exits 0.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

---
name: cpp-toolchain
description: Use the project's pinned C and C++ build tools, its compiler cache and its compile database. Use when a build fails for no reason the code explains, before trusting clangd's diagnostics, and after adding, moving or removing a source file.
---

# C and C++ toolchain

The project pins its build tools in `.conclave/toolchain-cpp.json`. `docs/toolchain-cpp.md` has every command. The build system and the build commands are the project's own.

## When a build fails for no reason the code explains

Run the check first: `python scripts/toolchain-check.py --pins .conclave/toolchain-cpp.json`. A missing linker or an old compiler looks like a broken build.

- Never install, upgrade or downgrade a pinned tool. Report the tool, the version found and the version wanted, with the install line the check prints, and the operator installs it.
- Never change a pin to get a build through: do not lower a `min_version`, mark a tool `optional` or name another binary so that the check passes.
- Leave the compiler cache on. Never set `BUILD_NO_CCACHE` to get past a failure: a build that fails with the cache and passes without it has a fault to find and report.

## The compile database

clangd takes every flag from `compile_commands.json`. Check it before trusting a diagnostic: `python scripts/compdb-check.py`.

- After adding, moving or removing a source file, regenerate the database with the project's own command whatever the check says: a build that finds its sources by itself leaves the check `current` without the new file.
- Then read it back, naming each file added or moved: `python scripts/compdb-check.py --source <file>`. Trust clangd's diagnostics only once that is `current`.
- `missing` (a fresh install has none): generate it with the project's command. When the pin file gives none, ask for it and record it as `compdb.regenerate`.
- `stale` or `invalid`: regenerate it. Never repair the file by hand.
- `unknown`: make readable the file the detail names, or correct it in the pin file's `compdb.build_files`, or report it.
- Exit 2: the pin file does not load. Correct what the message names; never delete the file.
- Never edit `compile_commands.json` or `.clangd` to silence a diagnostic. Fix the code, or the build that wrote the flags.

## Memory safety

The project declares its hardening in the pin file's `hardening` key; `docs/toolchain-cpp.md` (Memory safety) has the options for each compiler family and where each was measured.

- Prove an option by the result it changes, beside a control build without it, never by reading the flag back. A runtime check: a probe program commits the error on purpose and must end in that check's own trap, where the control build ends otherwise. An initialising option such as `-ftrivial-auto-var-init=zero`: a probe reads the value it sets (0, where the control build reads stack contents). A link property: `python scripts/hardening-check.py` reads it from the linked image.
- Apply hardening to the project's own targets, and a sanitizer only in a test preset: a sanitizer build is a test build, named in `test_binaries`, and never ships.
- The doctor is not a release gate: `python forge.py doctor --strict` passes `unknown` image rows. Call a build shippable only when `python scripts/hardening-check.py` exits 0 with status `ok`, under a policy that declares both `compile` and `binaries` and names every shipped binary: status `none` means nothing was required, and `unknown` is not evidence.
- Plan a crash message with any hardening that ships. A hardened process that hits a check ends at once and reaches no handler inside it, so the program's next start must be able to say that the last run stopped.

## The cache

`scripts/cc-cache.py` is evaluated by the build script (the lines are in the document), and `python scripts/cache-stats.py --min-hit-rate <percent>` says whether a rebuild used it.

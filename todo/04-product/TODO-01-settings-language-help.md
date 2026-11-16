---
schema_version: 1
id: settings-language-help
domain: 04-product
status: active
title: "TODO-01 -- Settings, language and help"
---

# TODO-01 -- Settings, language and help

> **Goal:** Settings live per user in a typed store that survives a crash mid-write and carries 1.0.8's values over; every string comes from a language file keyed by name, so adding a string breaks no translation; and F1 opens help that covers DVDs.

> **Current state:** Measured 2026-11-16. `CSetting` reads and writes `CarbonCD.ini` beside the executable, found by parsing `GetCommandLine()` (`src/Setting.cpp:24-62`), saved in the destructor (`:69`, `:233`). That folder is not writable under `Program Files`. Keys are in sections `[General]`, `[ReadSetting]`, `[WriteSetting]`, `[CopySetting]` and `[MasteringSetting]` with the defaults at `src/Setting.cpp:110-204`. Language files are `.cml` text read by position: 400 slots (`src/Language.h:3`), block offsets `LP_MAINMENU` to `LP_MESSAGE` (`src/Language.h:5-25`), so adding one string shifts every later one; loading stops at the first empty line (`src/Language.cpp:29-32`). `Distribution/` ships `English.cml`, `French.cml` and `Korean.cml` (385 or 386 strings each). Help is `Distribution/CarbonCD.chm`, opened with `HtmlHelp` (`src/CDMDlg.cpp:1078-1082`); it describes 1.0.8 and the HTML Help compiler is not installed. Sounds are played with `PlaySound` on task end (`src/CDMDlg.cpp:557,567`).

## Inputs

- `design/SPEC.md` S14 (settings and where they are stored), S15 (language), S16 (help link), S13 (the message texts), S0 (removed settings).
- `src/Setting.*`, `src/Language.*`, `src/LanguageDialog.*`, `Distribution/*.cml`, `Distribution/CarbonCD.chm`.
- -> XREF: D03 T01 §7 -- builds on §1 of this file
- -> XREF: D03 T01 §14 -- builds on §3 of this file
- -> XREF: D03 T02 §14 -- builds on §1 of this file
- -> XREF: D03 T02 §14 -- builds on §5 of this file
- -> XREF: D03 T02 §15 -- builds on §8 of this file
- -> XREF: D00 T01 §3 -- §1 here builds on it
- -> XREF: D00 T01 §3 -- §3 here builds on it
- -> XREF: D03 T01 §5 -- §5 here builds on it
- -> XREF: D03 T01 §12 -- §6 here builds on it
- -> XREF: D03 T02 §17 -- §7 here builds on it
- -> XREF: D03 T01 §7 -- §8 here builds on it
- -> XREF: D04 T02 §1 -- builds on §4 of this file
- -> XREF: D04 T02 §1 -- builds on §8 of this file
- -> XREF: D04 T02 §4 -- builds on §7 of this file

## Outcome

- Settings are in `%APPDATA%\Rizonesoft\CarbonCD\CarbonCD.ini`, or beside the executable when a file is there; a 1.0.8 file's values appear on first run.
- Switching language relabels the open window at once; a string missing from a translation shows in English.
- F1 opens local help that describes every pane, including DVD tasks.

**Adjacency:** list=not-applicable (settings are one record, not a list); document=not-applicable (nothing a user prints or sends); settings=applicable; reporting=not-applicable (no summaries); notifications=applicable; permissions=applicable; audit=not-applicable (setting changes are not logged; the task log is D03 T01 §13); exchange=applicable; reverse=applicable

**Adjacency rationale:** Settings: §1 defines every key with a named consumer. Notifications: §6 plays the completion sounds. Permissions: §1 refuses an unwritable store location and falls back. Exchange: §2 imports the 1.0.8 file and §4 converts the shipped translations. Reverse: the dialog's Cancel discards (D03 T02 §14), and §2 never modifies the 1.0.8 file it reads.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Typed settings store per user, with portable override | D00 T01 §3 | [ ] |
| 2 | §2 | Values carried over from a 1.0.8 settings file | §1 | [ ] |
| 3 | §3 | Language files keyed by name | D00 T01 §3 | [ ] |
| 4 | §4 | English, French and Korean files converted | §3 | [ ] |
| 5 | §5 | Language dialog with live switching | §3, D03 T01 §5 | [ ] |
| 6 | §6 | Completion sounds and process priority | §1, D03 T01 §12 | [ ] |
| 7 | §7 | Complete English string set with a coverage check | §3, D03 T02 §17 | [ ] |
| 8 | §8 | Help as a local page covering 2.0 | D03 T01 §7 | [ ] |

## 1. Typed settings store per user, with portable override

One place that knows every setting, its type, default and range, and writes the file safely.

**Phase:** 5
**Needs:** none
**Surface:** no

**Build order:**

1. Define the settings and the store Done when: every setting round-trips.
2. Choose the location and write atomically Done when: a crash mid-save leaves the old file intact.

- [ ] Add `src/core/settings/Settings.h` declaring each setting once with section, key, type, default and range: the kept keys of `src/Setting.cpp:110-204` (drive, last file, window placement, read, write, copy and mastering options, subcode method and speed, priority, sounds, log file, transfer buffer, language) and the new ones (`Theme`, `LastPane`, `TemporaryFolder`, `BusyWaitSeconds`, `VerifyAfterRead`, `VerifyAfterWrite`, `SkipUnreadable`) Done when: `tests/core/test_settings.cpp` writes and reads every declared setting and gets defaults from an empty file. Cheaper substitute: string keys scattered through the code.
- [ ] Clamp out-of-range values to the default on load and log each one (in `src/core/settings/Settings.h`) Done when: a file holding `Buffer=900` loads 15 and logs one line naming the key. Cheaper substitute: trusting the file.
- [ ] Resolve the file: `CarbonCD.ini` beside the executable if it exists and is writable (portable use), else `%APPDATA%\Rizonesoft\CarbonCD\CarbonCD.ini`, creating the folder Done when: the test, with a fake known-folder provider, picks each location in the stated order and falls back when the portable file is read-only. Cheaper substitute: writing beside the executable under `Program Files`.
- [ ] Save by writing `CarbonCD.ini.tmp` as UTF-16 with a byte order mark and `ReplaceFileW`, on explicit save points (dialog Save, task end, window close), never in a destructor Done when: killing the test process between write and replace leaves the previous file readable and complete. Cheaper substitute: `WritePrivateProfileString` key by key in a destructor (`src/Setting.cpp:69`).
- [ ] Replace `CSetting` and `theSetting` uses across `src/` with the store Done when: `rg -n "theSetting|CSetting" src` prints nothing.
- [ ] Commit: `core: typed settings store with atomic save`

**Test checkpoint:** `ctest --preset x64-debug -R settings` passes; saving in place instead of replace fails the crash test.

## 2. Values carried over from a 1.0.8 settings file

First run of 2.0 reads the old file once.

**Phase:** 5
**Needs:** none
**Surface:** no

**Build order:**

1. Import the old file Done when: its values appear in the new store.
2. Handle what was removed Done when: removed keys are ignored with a log line.

- [ ] Add `src/core/settings/Migrate.cpp`: when the new store has no file and a 1.0.8 `CarbonCD.ini` exists beside the executable or at a path given by `--import-settings`, read it in the system ANSI code page and copy every key the new store declares Done when: `tests/core/test_migrate.cpp` imports the tracked 1.0.8 sample (`Bin/Release/CarbonCD.ini` at commit 8577359, stored as `tests/data/CarbonCD-1.0.8.ini`) and reads `LastAccessFile` and `MainDlgX` back. Cheaper substitute: asking users to set everything again.
- [ ] Ignore `UseSPTI`, `AspiDLL`, `Skin` and `Engine=2` with one log line each, and map `MainDlgy` (lower case y) to the new window key Done when: the import logs four lines for a file holding all four and the window position is carried. Cheaper substitute: failing on unknown keys.
- [ ] Never write to or delete the old file, and record `MigratedFrom` so the import runs once Done when: the old file's bytes and timestamp are unchanged and a second start imports nothing. Cheaper substitute: renaming the old file, which breaks a side-by-side 1.0.8.
- [ ] Commit: `core: one-time import of 1.0.8 settings`

**Test checkpoint:** `ctest --preset x64-debug -R migrate` passes; running the import twice in the test changes nothing the second time.

## 3. Language files keyed by name

Strings are looked up by key, in UTF-8 files, with English as the fallback.

**Phase:** 5
**Needs:** none
**Surface:** no

**Build order:**

1. Define the format and the loader Done when: a file loads and strings are found by key.
2. Fall back and report Done when: a missing key shows English and is listed once.

- [ ] Define the format in `docs/language-files.md`: UTF-8, `# Language:` and `# Author:` header lines, then `key = text` lines, `\n` for a line break, `{0}` to `{9}` for parameters, `#` comments, blank lines allowed Done when: the document shows a complete small example. Cheaper substitute: extending the positional format with more slots.
- [ ] Add `src/core/lang/Language.cpp` loading a file into a map, with `tr(key)` and `tr(key, args...)`, English compiled in as the fallback from `Distribution/English.cml` at build time Done when: `tests/core/test_language.cpp` loads a file missing one key and gets the English text for it and the translated text for the rest. Cheaper substitute: an empty string for a missing key.
- [ ] Refuse a file with a duplicate key or a malformed line, naming the line number, and keep the current language (in `docs/language-files.md`) Done when: a file with a duplicate key is refused with `line 14: duplicate key read.title`. Cheaper substitute: loading what parses and ignoring the rest.
- [ ] Check parameters: a translation whose placeholders differ from the English string's is reported at load and the English string used (in `docs/language-files.md`) Done when: a translation using `{1}` where English has only `{0}` falls back and is listed. Cheaper substitute: a crash or garbage at format time.
- [ ] Replace every `MSG(i)`, `m_Str[...]` and `LP_*` use with `tr("key")`, and delete `src/Language.h`'s offset table Done when: `rg -n "LP_[A-Z]+|MSG\(|m_Str\[" src` prints nothing.
- [ ] Commit: `core: keyed UTF-8 language files with English fallback`

**Test checkpoint:** `ctest --preset x64-debug -R language` passes; returning an empty string for a missing key fails the fallback test.

## 4. English, French and Korean files converted

The three shipped files move to the keyed format without losing a translation.

**Phase:** 5
**Needs:** none
**Surface:** no

**Build order:**

1. Convert by position to keys Done when: every old string has a key in all three files.
2. Keep the translators' work Done when: no translated string is lost or altered.

- [ ] Add `scripts/convert-cml.py` mapping each positional slot of the 1.0.8 files to a key through a table `scripts/cml-keys.csv` (slot, key), reading French as Windows-1252 and Korean as code page 949, writing UTF-8 Done when: `python scripts/convert-cml.py --check` reports 0 unmapped slots and 0 undecodable bytes for the three files. Cheaper substitute: retyping the files.
- [ ] Write `Distribution/English.cml`, `French.cml` and `Korean.cml` in the new format, keeping `# Author:` lines Done when: the three files load in `tests/core/test_language.cpp` and a sample of 20 keys per file equals the old slot's text. Cheaper substitute: machine-translating the files afresh.
- [ ] Remove the duplicated copies under `Distribution/CarbonCD108/`, which are the 1.0.8 release's files, only if they are byte-identical to the 1.0.8 tag's; otherwise leave them and note it in `todo/findings.md` Done when: `git ls-files Distribution/CarbonCD108` is empty or the finding is recorded.
- [ ] Commit: `lang: shipped language files in the keyed format`

**Test checkpoint:** `python scripts/convert-cml.py --check` exits 0; removing a row from `scripts/cml-keys.csv` makes it report the unmapped slot and exit 1.

## 5. Language dialog with live switching

SPEC S15.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S15
**Job:** The user picks their language and sees it applied at once
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Its own small window with the title bar treatment of S1.1

**Build order:**

1. Build the dialog Done when: it lists the language files found.
2. Switch live Done when: choosing a language relabels the open window.

- [ ] Add `src/ui/dialogs/Language.cpp` listing each `*.cml` in the language folder with its `# Language:` name and `# Author:`, and `Use this language` Done when: `--render` writes `language-light-100.png` listing English, French and Korean. Cheaper substitute: reusing `IDD_LANGUAGE`.
- [ ] Broadcast a `LanguageChanged` notification that every window and pane handles by re-reading its strings and re-running layout Done when: `tests/ui/test_language_switch.cpp` switches to French and reads French captions from the rail through UI Automation without reopening the window. Cheaper substitute: asking for a restart.
- [ ] On first run choose the file matching the Windows display language when one exists, else English, without a prompt (in `src/ui/dialogs/Language.cpp`) Done when: with the test's locale set to `fr-FR` the first-run language is French. Cheaper substitute: the blocking language prompt of `src/Setting.cpp:207-215`.
- [ ] Delete `src/LanguageDialog.*` and `IDD_LANGUAGE` Done when: `rg -n "CLanguageDialog|IDD_LANGUAGE" src` prints nothing.
- [ ] Commit: `ui: language dialog, live switch`

**Test checkpoint:** `ctest --preset x64-debug -R language_switch` passes; a pane that caches its captions fails it.

## 6. Completion sounds and process priority

Two settings with consumers outside any one pane.

**Phase:** 6
**Needs:** none
**Surface:** no

**Build order:**

1. Play the chosen sound at task end Done when: success and failure each play their file.
2. Apply the priority Done when: the process priority follows the setting.

- [ ] Play the success or failure file with `PlaySoundW(..., SND_ASYNC | SND_FILENAME | SND_NODEFAULT)` when a task reaches the result view, only when the setting is on and the file exists Done when: `tests/ui/test_sounds.cpp`, with a fake player, sees the success file for Done and the failure file for Failed and Stopped, and nothing when off. Cheaper substitute: playing the system default sound when the file is missing.
- [ ] Apply `ProcessPriority` at start and on Save with `SetPriorityClass`, offering Normal, Above normal and High only Done when: setting High and saving changes `GetPriorityClass` in the test. Cheaper substitute: offering Realtime, which can freeze the machine during a write.
- [ ] Commit: `app: completion sounds and process priority`

**Test checkpoint:** `ctest --preset x64-debug -R sounds` passes; playing on Stopped as success fails it.

## 7. Complete English string set with a coverage check

Every string the new interface shows exists in English, and nothing is typed into code.

**Phase:** 7
**Needs:** none
**Surface:** no

**Build order:**

1. Check keys used against keys defined Done when: no key is missing and none is unused.
2. Mark what translators still owe Done when: each translation lists its untranslated keys.

- [ ] Add `scripts/check-strings.py` collecting every `tr("...")` key in `src/` and comparing with `Distribution/English.cml`: a key used and not defined fails, a key defined and not used fails Done when: `python scripts/check-strings.py` exits 0; adding `tr("nope")` makes it exit 1 naming the key and file. Cheaper substitute: finding missing strings by clicking through the app.
- [ ] Fail on user-visible string literals in `src/ui/` outside `tr()` (a literal passed to a caption, label or message call) Done when: adding `SetWindowTextW(L"Hello")` in a pane makes the check exit 1.
- [ ] Write `Distribution/translation-status.md` from the script: for French and Korean, the count and list of keys still falling back to English Done when: the file lists the DVD and new-interface keys awaiting translation. Cheaper substitute: shipping silently half-translated files.
- [ ] Run the check in `.github/workflows/ci.yml` Done when: a commit adding an undefined key turns the run red.
- [ ] Commit: `lang: string coverage check and translation status`

**Test checkpoint:** `python scripts/check-strings.py` exits 0; deleting a used key from `Distribution/English.cml` makes it exit 1.

## 8. Help as a local page covering 2.0

The CHM describes 1.0.8 and cannot be rebuilt here. Help becomes Markdown in the repository, built to one local HTML file.

**Phase:** 6
**Needs:** none
**Surface:** no

**Build order:**

1. Write the help and build it Done when: one HTML file is produced from the Markdown.
2. Open it from the app Done when: F1 opens the page at the current pane's section.

- [ ] Write `docs/help/*.md`: one page per pane (Read to image, Write image, Clone disc, Master disc, Read tracks, Subcode, Erase disc, Drive info), Settings, media types and what each allows, messages and what to do about them, and what CarbonCD does not do (copy protection, Blu-ray) Done when: each SPEC S13 message and each S3 to S10 control is explained on some page. Cheaper substitute: linking to a website that may not exist.
- [ ] Add `scripts/build-help.py` (standard library only) joining the pages into `Distribution/CarbonCD-Help.html`: one self-contained file with a contents list, an anchor per pane, the design tokens' colours and both themes Done when: `python scripts/build-help.py` writes the file and `--check` exits 1 when it is stale. Cheaper substitute: requiring the HTML Help compiler.
- [ ] Open the file at the pane's anchor on `F1` and from About, through `ShellExecuteW`; when the file is missing say so with its expected path Done when: `tests/ui/test_help.cpp` sees the URL end in `#write-image` from the Write pane, and the missing-file message when the file is absent. Cheaper substitute: `HtmlHelp` and the `htmlhelp` library.
- [ ] Remove `Distribution/CarbonCD.chm`, the `htmlhelp` link and `OnHelpHelp` Done when: `rg -n -i "htmlhelp|\.chm" src CMakeLists.txt` prints nothing.
- [ ] Commit: `help: local HTML help for 2.0`

**Test checkpoint:** `python scripts/build-help.py --check` exits 0 and `ctest --preset x64-debug -R help` passes; editing a help page without rebuilding makes the check exit 1.

## Verification

- A fresh profile with a 1.0.8 `CarbonCD.ini` beside the executable starts with its values; the old file is unchanged.
- `python scripts/check-strings.py`, `python scripts/convert-cml.py --check` and `python scripts/build-help.py --check` exit 0.
- Switching to French and back relabels the open window each time.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

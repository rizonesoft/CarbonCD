---
schema_version: 1
id: task-panes
domain: 03-interface
status: active
title: "TODO-02 -- Task panes and dialogs"
---

# TODO-02 -- Task panes and dialogs

> **Goal:** Every task of SPEC S3 to S10 is a working pane in the new window, Settings and About are in the new design, and the old dialogs, menus and bitmaps are gone.

> **Current state:** Measured 2026-11-16. Each task is a modal MFC dialog: `IDD_READSETTING` with four option pages (`src/ReadSettingDialog*.cpp`), `IDD_WRITESETTING` (`src/WriteSettingDialog.cpp`), `IDD_MASTERING_FILE` with three pages and its own menu (`src/MasteringFileDialog.cpp`, `src/MasteringPage1-3.cpp`), `IDD_READTRACK` (`src/ReadTrackDialog.cpp`), `IDD_DRIVE_CAPABILITY`, `IDD_CALCMSF` (`src/MsfDialog.cpp`), `IDD_SETTING` with four pages (`src/SettingPage1-4.cpp`), `IDD_ABOUTBOX` (version text `Version 0.0.0`, copyright 2015) and the TOC frame (`src/TocWindow.cpp`). Clone is `OnCdDuplicate` in `src/CDMDlg.cpp`, which reads then writes with two dialogs. Captions say CD throughout (`Write to CD`, `Erase CD-RW`, `CD Mastering`). The Read Track dialog's Cancel button is mojibake (`src/cdm.rc:108`).

## Inputs

- `design/SPEC.md` S3 to S10, S14, S16, and S1 (file drop and keys).
- `design/DESIGN.md` sections 5, 6 and 10.
- The dialogs and classes named in Current state: each section replaces one and deletes it.
- -> XREF: D03 T01 §12 -- §1 here builds on it
- -> XREF: D03 T01 §14 -- §1 here builds on it
- -> XREF: D01 T03 §5 -- §1 here builds on it
- -> XREF: D01 T03 §6 -- §1 here builds on it
- -> XREF: D01 T02 §5 -- §1 here builds on it
- -> XREF: D01 T02 §6 -- §1 here builds on it
- -> XREF: D03 T01 §12 -- §2 here builds on it
- -> XREF: D03 T01 §14 -- §2 here builds on it
- -> XREF: D01 T03 §12 -- §2 here builds on it
- -> XREF: D01 T02 §9 -- §2 here builds on it
- -> XREF: D03 T01 §12 -- §3 here builds on it
- -> XREF: D03 T01 §14 -- §3 here builds on it
- -> XREF: D01 T03 §13 -- §3 here builds on it
- -> XREF: D03 T01 §8 -- §4 here builds on it
- -> XREF: D03 T01 §14 -- §4 here builds on it
- -> XREF: D02 T01 §5 -- §4 here builds on it
- -> XREF: D02 T01 §6 -- §4 here builds on it
- -> XREF: D02 T01 §8 -- §5 here builds on it
- -> XREF: D02 T01 §1 -- §6 here builds on it
- -> XREF: D03 T01 §12 -- §7 here builds on it
- -> XREF: D02 T01 §7 -- §7 here builds on it
- -> XREF: D02 T01 §8 -- §7 here builds on it
- -> XREF: D03 T01 §12 -- §8 here builds on it
- -> XREF: D03 T01 §14 -- §8 here builds on it
- -> XREF: D01 T02 §7 -- §8 here builds on it
- -> XREF: D03 T01 §12 -- §9 here builds on it
- -> XREF: D03 T01 §14 -- §9 here builds on it
- -> XREF: D01 T02 §8 -- §9 here builds on it
- -> XREF: D03 T01 §12 -- §10 here builds on it
- -> XREF: D03 T01 §14 -- §10 here builds on it
- -> XREF: D01 T03 §11 -- §10 here builds on it
- -> XREF: D01 T02 §10 -- §10 here builds on it
- -> XREF: D03 T01 §8 -- §11 here builds on it
- -> XREF: D03 T01 §14 -- §11 here builds on it
- -> XREF: D01 T03 §3 -- §11 here builds on it
- -> XREF: D01 T02 §1 -- §12 here builds on it
- -> XREF: D01 T03 §2 -- §12 here builds on it
- -> XREF: D03 T01 §5 -- §13 here builds on it
- -> XREF: D01 T01 §7 -- §13 here builds on it
- -> XREF: D03 T01 §5 -- §14 here builds on it
- -> XREF: D03 T01 §4 -- §14 here builds on it
- -> XREF: D04 T01 §1 -- §14 here builds on it
- -> XREF: D04 T01 §5 -- §14 here builds on it
- -> XREF: D03 T01 §5 -- §15 here builds on it
- -> XREF: D00 T01 §9 -- §15 here builds on it
- -> XREF: D04 T01 §8 -- §15 here builds on it
- -> XREF: D04 T01 §7 -- builds on §17 of this file
- -> XREF: D04 T02 §4 -- builds on §17 of this file
- -> XREF: D04 T02 §6 -- builds on §17 of this file
- -> XREF: D04 T02 §7 -- builds on §17 of this file

## Outcome

- Each of the eight rail items opens a pane whose every control in SPEC works, on a simulated CD and a simulated DVD.
- Settings and About match the contract; the MSF calculator converts as 1.0.8 does.
- `src/cdm.rc` holds no retired dialog, menu or bitmap.

**Adjacency:** list=not-applicable (panes hold a session's entries and no stored records); document=applicable @ D03 T02 §12; settings=applicable @ D03 T02 §14; reporting=not-applicable (no summaries over stored data); notifications=not-applicable (task end is announced by the shell, D03 T01 §12 and §15); permissions=applicable; audit=not-applicable (the log is owned by D03 T01 §13); exchange=applicable @ D03 T02 §12; reverse=applicable

**Adjacency rationale:** Document and exchange: the disc information export of §12 is what a user carries away. Settings: §14 is the dialog over the store. Permissions: each pane disables its primary button with a reason and exercises its S13 refusals. Reverse: every pane's started task is stopped from the run view, and Erase asks before it destroys.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Read to image pane | D03 T01 §12, D03 T01 §14, D01 T03 §5, D01 T03 §6, D01 T02 §5, D01 T02 §6 | [ ] |
| 2 | §2 | Write image pane | D03 T01 §12, D03 T01 §14, D01 T03 §12, D01 T02 §9 | [ ] |
| 3 | §3 | Clone disc pane | D03 T01 §12, D03 T01 §14, D01 T03 §13 | [ ] |
| 4 | §4 | Master disc pane: type, label, capacity and options | D03 T01 §8, D03 T01 §14, D02 T01 §5, D02 T01 §6 | [ ] |
| 5 | §5 | Master disc pane: track list | §4, D02 T01 §8 | [ ] |
| 6 | §6 | Master disc pane: folder tree and file list | §4, D02 T01 §1 | [ ] |
| 7 | §7 | Master disc pane: create image and write to disc | §5, §6, D03 T01 §12, D02 T01 §7, D02 T01 §8 | [ ] |
| 8 | §8 | Read tracks pane | D03 T01 §12, D03 T01 §14, D01 T02 §7 | [ ] |
| 9 | §9 | Subcode pane | D03 T01 §12, D03 T01 §14, D01 T02 §8 | [ ] |
| 10 | §10 | Erase disc pane | D03 T01 §12, D03 T01 §14, D01 T03 §11, D01 T02 §10 | [ ] |
| 11 | §11 | Drive info pane: drive page | D03 T01 §8, D03 T01 §14, D01 T03 §3 | [ ] |
| 12 | §12 | Drive info pane: disc page and export | §11, D01 T02 §1, D01 T03 §2 | [ ] |
| 13 | §13 | MSF calculator | D03 T01 §5, D01 T01 §7 | [ ] |
| 14 | §14 | Settings dialog | D03 T01 §5, D03 T01 §4, D04 T01 §1, D04 T01 §5 | [ ] |
| 15 | §15 | About dialog | D03 T01 §5, D00 T01 §9, D04 T01 §8 | [ ] |
| 16 | §16 | File drop on the window | §1, §2, §6 | [ ] |
| 17 | §17 | 1.0.8 dialogs, menus and resources removed | §3, §7, §8, §9, §10, §12, §13, §14, §15, §16 | [ ] |

## 1. Read to image pane

SPEC S3, for CDs and DVDs.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/mockups/main-read.png`; `design/SPEC.md` S3.1 to S3.9 and the CD options list
**Job:** The user saves a disc as an image file, choosing where, in what format and how careful to be
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane and its CD options Done when: every S3 control is present and wired.
2. Start the right task Done when: a DVD reads to ISO and a CD by the chosen engine.

- [ ] Add `src/ui/panes/ReadPane.cpp` with Save as and Browse (S3.1), Format (S3.2), Speed (S3.3), Verify (S3.4), Skip unreadable sectors (S3.5), Options (S3.6), the cards (S3.7, S3.8) and `Read disc` (S3.9) Done when: `--render` writes `main-read-light-100.png` matching `design/mockups/main-read.png`. Cheaper substitute: reusing `IDD_READSETTING`.
- [ ] Constrain Format by the disc: ISO only for a DVD; all three for a CD, with ISO offered only for a single data track; and set the Save as extension to match (in `src/ui/panes/ReadPane.cpp`) Done when: `tests/ui/test_read_pane.cpp` sees the segments enabled as stated for a simulated DVD, a data CD and an audio CD. Cheaper substitute: offering CUE for a DVD and failing later.
- [ ] Add the CD options dialog `src/ui/dialogs/CdReadOptions.cpp` holding every option listed under S3.6 (engine, test mode, the error options, subcode and pregap analysis, swap channels, command detection, read command, audio and data speed), saved through the settings store Done when: each option changes its setting and the started task receives it, asserted per option. Cheaper substitute: dropping the advanced options.
- [ ] Fill the cards from the disc and the path: type, drive, sectors, file system and layer break; file name, size and free space (in `src/ui/panes/ReadPane.cpp`) Done when: the simulated DVD+R DL shows `3 080 112 sectors  UDF 2.50  layer break at 1 540 056` and the copy shows the free space of the target volume.
- [ ] Disable `Read disc` with the inline reason for no disc, no path, too little space, CSS and unsupported media; on start run the DVD read task, or the CD single-session, multi-session or compare task, in the run view, then the verify pass if ticked Done when: each refusal shows its reason; a simulated DVD read ends in the result view with `Done`.
- [ ] Delete `src/ReadSettingDialog.*`, `src/ReadSettingDialog1-4.*`, `src/ReadProgressDialog.*` and their dialog resources Done when: `rg -n "CReadSettingDialog|IDD_READSETTING|IDD_READPROGRESS" src` prints nothing.
- [ ] Commit: `ui: Read to image pane`

**Test checkpoint:** `ctest --preset x64-debug -R read_pane` passes; enabling CUE for a DVD fails the format test.

## 2. Write image pane

SPEC S4, for CD and DVD images.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S4.1 to S4.12
**Job:** The user writes an image file to a blank disc with the options that disc allows
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane Done when: every S4 control is present and wired.
2. Start the right task Done when: an ISO writes to a DVD and a CUE to a CD.

- [ ] Add `src/ui/panes/WritePane.cpp` with Image file (S4.1), Speed (S4.2), Write method (S4.3), Test mode (S4.4), Buffer underrun protection (S4.5), Power calibration (S4.6), Verify (S4.7), Eject when done (S4.8), Layer break (S4.9), the cards (S4.10, S4.11) and `Write image` (S4.12) Done when: `--render` writes `write-light-100.png` with every control. Cheaper substitute: reusing `IDD_WRITESETTING`.
- [ ] Fill Speed and Write method from the drive and the disc, and disable Test mode, Power calibration and Layer break where the media has none, each with its reason (in `src/ui/panes/WritePane.cpp`) Done when: a simulated DVD+R shows Test mode disabled with `DVD+R has no test write`; Layer break appears only for dual-layer media. Cheaper substitute: showing CD write modes for a DVD.
- [ ] Validate the layer break entry as typed (multiple of 16, layer 1 not longer than layer 0) (in `src/ui/panes/WritePane.cpp`) Done when: typing 1 540 060 shows the reason inline and disables `Write image`.
- [ ] Disable `Write image` with the reason for `no-disc`, `not-blank`, `too-large`, `wrong-media`; offer `Erase disc` as a link when a rewritable disc is not blank Done when: each refusal renders its reason, and the link opens the Erase pane. Cheaper substitute: a message box after the click.
- [ ] Start the DVD or CD write task with the chosen options in the run view, with verify as its second phase (in `src/ui/panes/WritePane.cpp`) Done when: a simulated write ends in the result view with `Done` and the verify result.
- [ ] Delete `src/WriteSettingDialog.*`, `src/WriteProgressDialog.*` and their dialog resources Done when: `rg -n "CWriteSettingDialog|IDD_WRITESETTING|IDD_WRITEPROGRESS" src` prints nothing.
- [ ] Commit: `ui: Write image pane`

**Test checkpoint:** `ctest --preset x64-debug -R write_pane` passes; leaving Test mode enabled for DVD+R fails its test.

## 3. Clone disc pane

SPEC S5.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S5.1 to S5.7
**Job:** The user copies one disc to another, with one drive or two
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane Done when: every S5 control is present and wired.
2. Run the clone Done when: two simulated drives clone, and one drive asks for the blank disc.

- [ ] Add `src/ui/panes/ClonePane.cpp` with the source (S5.1), Target drive (S5.2), Temporary folder with space needed and free (S5.3), read and write speeds (S5.4), Verify, Eject when done and Skip unreadable sectors (S5.5), the cards (S5.6) and `Clone disc` (S5.7) Done when: `--render` writes `clone-light-100.png` with every control. Cheaper substitute: the two chained dialogs of `OnCdDuplicate`.
- [ ] Explain the same-drive case in the Copy card: `Same drive: CarbonCD will ask for a blank disc` Done when: choosing the source as target shows that text.
- [ ] Disable `Clone disc` with the reason for no source disc, protected or unsupported source, and too little temporary space Done when: each refusal renders its reason.
- [ ] Start the clone task; show the `NeedsDisc` prompt between phases for the same-drive case Done when: the two-drive simulated clone ends `Done`; the one-drive clone prompts, accepts the swapped disc and ends `Done`. Cheaper substitute: leaving the temporary image behind.
- [ ] Commit: `ui: Clone disc pane`

**Test checkpoint:** `ctest --preset x64-debug -R clone_pane` passes; skipping the temporary-space check fails its refusal test.

## 4. Master disc pane: type, label, capacity and options

The frame of SPEC S6: what kind of disc is being built and how full it is.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S6.1, S6.2, S6.5, S6.6, S6.7
**Job:** The user sets what disc they are building and sees how much room is left
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane's frame Done when: S6.1, S6.2, S6.5, S6.6 and S6.7 are present and wired.
2. Keep the capacity bar live Done when: adding files moves the bar.

- [ ] Add `src/ui/panes/MasterPane.cpp` with Disc type (S6.1) following the disc in the drive, Volume label (S6.2) refusing bad characters as typed, the File system line (S6.6) and the options (S6.7) Done when: `--render` writes `master-light-100.png`; typing `*` in the label shows the reason inline. Cheaper substitute: the three-tab `IDD_MASTERING_FILE`.
- [ ] Add the capacity bar (S6.5) fed by `Tree::layout()` and `Capacity`, showing used and free in sectors and GB, and `MM:SS:FF` for a CD, in `error` when over Done when: a project 10 MB over capacity shows the bar in `error` with `10.00 MB over`. Cheaper substitute: a text field with a number.
- [ ] Carry over Always on top and Tell me when a file name is shortened, and the CD data protection area list for CD data projects only (in `src/ui/panes/MasterPane.cpp`) Done when: each option changes its setting; the protection list is hidden for a Data DVD.
- [ ] Commit: `ui: Master disc pane frame and capacity bar`

**Test checkpoint:** `ctest --preset x64-debug -R master_pane` passes; feeding the bar from the file count instead of the layout fails the over-capacity test.

## 5. Master disc pane: track list

SPEC S6.3: the CD track list.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S6.3
**Job:** The user arranges the data and audio tracks of a CD
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the track list Done when: tracks are added, removed and reordered.
2. Refuse bad audio Done when: a WAV that is not CD audio is refused with the reason.

- [ ] Add the track list to `MasterPane` for CD disc types: columns number, type, source and length; `Add data track`, `Add ISO as data track`, `Add audio track`, `Remove`, `Move up`, `Move down`, and drag and drop of `.wav`, `.iso`, `.bin`, `.img` Done when: `tests/ui/test_master_tracks.cpp` adds three tracks, reorders them and reads the project's order. Cheaper substitute: the menu-only commands of `IDR_MASTERING_FILE`.
- [ ] Show the WAV format refusal inline under the list and keep the list unchanged (in `src/ui/panes/MasterPane.cpp`) Done when: dropping a 48 000 Hz file shows `x.wav is 48 000 Hz. CD audio is 44 100 Hz, 16 bit, stereo.`. Cheaper substitute: a message box per file when many are dropped.
- [ ] Hide the track list for Data DVD (in `src/ui/panes/MasterPane.cpp`) Done when: the list is absent for a Data DVD project.
- [ ] Commit: `ui: Master disc track list`

**Test checkpoint:** `ctest --preset x64-debug -R master_tracks` passes; accepting the 48 000 Hz file fails it.

## 6. Master disc pane: folder tree and file list

SPEC S6.4: the files of the data track.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S6.4
**Job:** The user chooses and arranges the files that go on the disc
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the tree and the list Done when: files and folders are added, renamed and removed.
2. Handle large drops without freezing Done when: dropping a folder of 50 000 files keeps the window responsive.

- [ ] Add the folder tree and file list to `MasterPane`: `Add files`, `Add folder`, `New folder`, `Rename` (in place, `F2`), `Remove` (`Delete`), and drag and drop from Explorer onto either, bound to `Tree` Done when: `tests/ui/test_master_files.cpp` performs each operation and reads the tree back. Cheaper substitute: the fixed 2048-character `DragQueryFile` buffers of `src/MasteringPage1.cpp:196,230`.
- [ ] Scan dropped folders on a background task with a count shown and a `Stop` link, then apply the result in one step Done when: a scripted 50 000 file folder adds with the window answering input throughout, and Stop leaves the tree unchanged. Cheaper substitute: recursing on the UI thread.
- [ ] Show sizes in the list in Data type with 64-bit values, and report shortened names once per operation through `name-shortened` Done when: a 5 GB file shows `5.37 GB`, and adding two long-named files shows one message naming both.
- [ ] Refuse a file over 4 GB in a CD project with `file-over-4gb-cd` inline Done when: the file is not added and the reason shows.
- [ ] Commit: `ui: Master disc folder tree and file list`

**Test checkpoint:** `ctest --preset x64-debug -R master_files` passes; scanning on the UI thread fails the responsiveness assertion.

## 7. Master disc pane: create image and write to disc

SPEC S6.8 to S6.10: the two outputs.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S6.8, S6.9, S6.10
**Job:** The user turns the project into an image file or a finished disc
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Wire the two actions Done when: a project becomes an image or a disc.
2. Refuse what cannot be built Done when: empty and oversize projects are refused with reasons.

- [ ] Add `Create image` (S6.8): a save dialog offering ISO for a data disc and CUE with BIN for audio or mixed, starting the mastering image task in the run view Done when: a simulated Data DVD project produces an ISO and the result view offers `Open folder`.
- [ ] Add `Write to disc` (S6.9) with the Write image options (S4.2 to S4.8) in a compact row, starting mastering to disc Done when: a simulated project masters to a simulated DVD+R and verifies. Cheaper substitute: writing a temporary image without saying so.
- [ ] Fill the cards (S6.10): the file set with count and total size, and the disc or image it becomes (in `src/ui/panes/MasterPane.cpp`) Done when: the original card reads `1 204 files  3.82 GB`.
- [ ] Disable both actions with the reason for an empty project and an oversize one (in `src/ui/panes/MasterPane.cpp`) Done when: each refusal renders its reason.
- [ ] Delete `src/MasteringFileDialog.*`, `src/MasteringPage1-3.*`, `src/CreateProgressDialog.*`, `IDD_MASTERING_FILE`, `IDD_MASTERING_1` to `_3`, `IDD_CREATPROGRESS`, `IDR_MASTERING_FILE`, `IDB_FILE_TREE` and `IDB_FILE_LIST` Done when: `rg -n "CMasteringFileDialog|IDD_MASTERING|IDR_MASTERING_FILE" src` prints nothing.
- [ ] Commit: `ui: Master disc outputs`

**Test checkpoint:** `ctest --preset x64-debug -R master_outputs` passes; enabling `Write to disc` for an oversize project fails its refusal test.

## 8. Read tracks pane

SPEC S7.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S7.1 to S7.4
**Job:** The user pulls chosen tracks off a CD as WAV or ISO files
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane Done when: every S7 control is present and wired.
2. Handle DVDs Done when: a DVD shows the explanation and the link to Read to image.

- [ ] Add `src/ui/panes/TracksPane.cpp` with the track list and check boxes (S7.1), Folder (S7.2), the options (S7.3) and `Read tracks` (S7.4) Done when: `--render` writes `tracks-light-100.png` with a simulated 12 track CD. Cheaper substitute: reusing `IDD_READTRACK`.
- [ ] For a DVD, replace the controls with `DVDs have no tracks to read one by one.` and a `Read to image` link Done when: the simulated DVD shows the text and the link selects the Read pane. Cheaper substitute: a disabled pane with no explanation.
- [ ] Refuse no ticked track inline; start the read tracks task in the run view (in `src/ui/panes/TracksPane.cpp`) Done when: with nothing ticked the reason shows; a simulated read of two tracks ends `Done` with two files.
- [ ] Delete `src/ReadTrackDialog.*` and `IDD_READTRACK` Done when: `rg -n "CReadTrackDialog|IDD_READTRACK" src` prints nothing.
- [ ] Commit: `ui: Read tracks pane`

**Test checkpoint:** `ctest --preset x64-debug -R tracks_pane` passes; starting with no track ticked fails the refusal test.

## 9. Subcode pane

SPEC S8.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S8.1 to S8.3
**Job:** The user finds a CD's true track layout and saves it as a cue sheet
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane Done when: the analysis runs and shows its result.
2. Handle DVDs Done when: a DVD shows why there is nothing to analyse.

- [ ] Add `src/ui/panes/SubcodePane.cpp` showing the method and speed from Settings (S8.1) with a `Change` link, and `Analyse subcode` (S8.2) starting the task in the run view Done when: a simulated CD analysis ends and returns to the pane with its result. Cheaper substitute: reusing `IDD_SUBQPROGRESS`.
- [ ] Show the accurate TOC as a list and add `Save as cue sheet` (S8.3) Done when: the scripted pregap appears as index 0 of track 2 and the saved sheet equals the expected file.
- [ ] For a DVD, show `DVDs have no subcode.` and disable the action Done when: the simulated DVD shows the text.
- [ ] Delete `src/SubQProgressDialog.*` and `IDD_SUBQPROGRESS` Done when: `rg -n "CSubQProgressDialog|IDD_SUBQPROGRESS" src` prints nothing.
- [ ] Commit: `ui: Subcode pane`

**Test checkpoint:** `ctest --preset x64-debug -R subcode_pane` passes; omitting index 0 from the list fails the result test.

## 10. Erase disc pane

SPEC S9.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S9.1 to S9.3; S13 `confirm-erase`
**Job:** The user wipes a rewritable disc, knowing what will be lost
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the pane Done when: the methods follow the media.
2. Confirm before destroying Done when: nothing is erased without the confirmation.

- [ ] Add `src/ui/panes/ErasePane.cpp` with Method (S9.1): `Quick` and `Full` for CD-RW and DVD-RW, `Format` for DVD+RW and DVD-RAM, and for other media the text `This disc cannot be erased. Only rewritable discs can.` Done when: each simulated media type shows the stated choices. Cheaper substitute: the caption `Erase CD-RW` for every disc.
- [ ] Show what will be lost (S9.2): label, size in use and sessions (in `src/ui/panes/ErasePane.cpp`) Done when: a simulated DVD-RW labelled `PHOTOS` with 3.10 GB shows both.
- [ ] On `Erase disc` (S9.3) ask `confirm-erase` with `Cancel` as default, then run the CD or DVD erase task in the run view with `Stop` disabled and explained for a full erase Done when: cancelling sends no command to the simulated drive; confirming ends with a blank disc and `Done`. Cheaper substitute: erasing on the first click.
- [ ] Delete `src/EraseDialog.*`, `IDD_ERASE` and the four erase menu commands Done when: `rg -n "CEraseDialog|IDD_ERASE|ID_CD_ERASE" src` prints nothing.
- [ ] Commit: `ui: Erase disc pane`

**Test checkpoint:** `ctest --preset x64-debug -R erase_pane` passes; removing the confirmation fails the cancel test.

## 11. Drive info pane: drive page

SPEC S10.1.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S10.1
**Job:** The user checks what their drive can read and write, and how fast
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the drive page Done when: identity and capabilities are shown.
2. Cover CD write modes as 1.0.8 does Done when: each mode and its test write show supported or not.

- [ ] Add `src/ui/panes/DriveInfoPane.cpp` with two pages, `Drive` and `Disc`; the drive page shows vendor, model, firmware, letter and buffer size, and a table of media types with reads, writes and write speeds, from the profile list and feature descriptors Done when: a simulated writer lists DVD+R DL as read and write with its speeds. Cheaper substitute: reusing `IDD_DRIVE_CAPABILITY`, which knows only CD modes.
- [ ] Show the CD write modes SAO, RAW+16, RAW+96 and packed RAW+96, each with test write, probed as `src/DriveCapabilityDialog.cpp` does Done when: the simulated drive with RAW+96 disabled shows it as not supported.
- [ ] Delete `src/DriveCapabilityDialog.*` and `IDD_DRIVE_CAPABILITY` Done when: `rg -n "CDriveCapabilityDialog|IDD_DRIVE_CAPABILITY" src` prints nothing.
- [ ] Commit: `ui: Drive info, drive page`

**Test checkpoint:** `ctest --preset x64-debug -R drive_info_drive` passes; reading capabilities from mode page 2Ah only drops the DVD rows and fails it.

## 12. Drive info pane: disc page and export

SPEC S10.2: what is on the disc, and saving it.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S10.2
**Job:** The user reads a disc's layout and saves it to a file
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the disc page Done when: a CD shows its TOC and a DVD its layout.
2. Export Done when: each format saves a correct file.

- [ ] Add the disc page: for a CD the TOC list (session, track, type, start as `MM:SS:FF` and LBA, length, ISRC where read); for a DVD book type, layers, layer break, sessions, capacity and free space Done when: the simulated mixed-mode CD lists its tracks and the simulated DVD+R DL shows `Layer break  1 540 056`. Cheaper substitute: the separate TOC frame of `src/TocWindow.cpp`.
- [ ] Add `Export` saving text, HTML, CSV, or a cue sheet (CD only), as UTF-8, through the core exporters Done when: `tests/ui/test_disc_export.cpp` saves each and they equal the expected files; the cue sheet choice is absent for a DVD.
- [ ] Refuse export with no disc inline, and report a file that cannot be written with its path and the Windows reason (in `src/ui/panes/DriveInfoPane.cpp`) Done when: exporting to a read-only folder shows the reason and writes nothing. Cheaper substitute: a silent failure.
- [ ] Delete `src/TocWindow.*`, `IDR_TOCFRAME` menu and its `Window` menu commands Done when: `rg -n "CTocWindow|ID_VIEW_TOCWINDOW" src` prints nothing.
- [ ] Commit: `ui: Drive info, disc page and export`

**Test checkpoint:** `ctest --preset x64-debug -R disc_export` passes; writing the CSV in the system code page fails the UTF-8 comparison.

## 13. MSF calculator

SPEC S10.3.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S10.3
**Job:** The user converts a CD address between its forms
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Its own small window with the title bar treatment of S1.1

**Build order:**

1. Rebuild the calculator in the new controls Done when: each conversion matches 1.0.8.
2. Refuse bad input Done when: out-of-range values are flagged in place.

- [ ] Add `src/ui/dialogs/MsfCalculator.cpp`: fields for MSF, MSF+150, LBA and LBA in hex, converting on each edit through `src/core/Address.h`, opened by `Ctrl+M` and from the Subcode and Read tracks panes Done when: `tests/ui/test_msf.cpp` enters 00:02:00 and reads MSF+150 00:04:00, LBA 0 and hex `0`. Cheaper substitute: the four `Calc` buttons of `IDD_CALCMSF`.
- [ ] Mark a field invalid in place for seconds over 59, frames over 74 or an LBA beyond the CD range (in `src/ui/dialogs/MsfCalculator.cpp`) Done when: entering frame 75 marks the field and leaves the others unchanged.
- [ ] Delete `src/MsfDialog.*` and `IDD_CALCMSF` Done when: `rg -n "CMsfDialog|IDD_CALCMSF" src` prints nothing.
- [ ] Commit: `ui: MSF calculator`

**Test checkpoint:** `ctest --preset x64-debug -R msf` passes; converting without the 150 offset fails it.

## 14. Settings dialog

SPEC S14: four pages over the settings store.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S14.1 to S14.4
**Job:** The user sets how CarbonCD looks, sounds and talks to the drive
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Its own small window with the title bar treatment of S1.1

**Build order:**

1. Build the dialog Done when: every S14 control reads and writes its setting.
2. Save and cancel Done when: Save applies at once and Cancel changes nothing.

- [ ] Add `src/ui/dialogs/Settings.cpp` with the pages General (S14.1), Sounds (S14.2), Drive (S14.3) and Subcode (S14.4) Done when: `--render` writes one PNG per page with every control. Cheaper substitute: reusing `IDD_SETTING`.
- [ ] Bind each control to its typed setting: Language (opens the language dialog), Theme, Temporary folder, log file, Process priority; the two sounds with test buttons; Transfer size and the busy-drive wait; subcode method and speed (in `src/ui/dialogs/Settings.cpp`) Done when: `tests/ui/test_settings_dialog.cpp` changes every control, saves, and reads each value back from the store. Cheaper substitute: controls that write nothing.
- [ ] Apply on `Save` without a restart (theme, language, priority) and discard on `Cancel` and `Esc` Done when: changing Theme to Dark and saving recolours the open window; cancelling a changed dialog leaves the store's file byte-identical.
- [ ] Validate paths as typed: a temporary folder that does not exist or cannot be written shows the reason and disables `Save` Done when: a path on a missing drive shows the reason. Cheaper substitute: accepting it and failing at the next clone.
- [ ] Delete `src/SettingDialog.*`, `src/SettingPage1-4.*` and `IDD_SETTING`, `IDD_SETTING_1` to `_4` Done when: `rg -n "CSettingDialog|IDD_SETTING" src` prints nothing.
- [ ] Commit: `ui: Settings dialog`

**Test checkpoint:** `ctest --preset x64-debug -R settings_dialog` passes; a control not bound to the store fails the read-back.

## 15. About dialog

SPEC S16.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S16; `design/DESIGN.md` section 9
**Job:** The user finds the version, the licence and where to get help
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Its own small window with the title bar treatment of S1.1

**Build order:**

1. Build the dialog Done when: it shows the banner, version, copyright and licence.
2. Wire the links Done when: each opens what it names.

- [ ] Add `src/ui/dialogs/About.cpp` with the About banner (`design/brand/about-banner.svg`, rasterised by `design/build.py` at each scale), the version and build from the generated header, the copyright line with the current build year, `MIT licence`, and the line `Please do not use this program to make illegal copies of copyrighted work.` Done when: `--render` writes `about-light-100.png` showing `Version 2.0.0`. Cheaper substitute: the hard-coded `Version 0.0.0` and `2015` of `IDD_ABOUTBOX`.
- [ ] Add `Rizonesoft Online` and `Help` links and a `Copy version` button putting the version, build and Windows version on the clipboard Done when: the test invokes each and observes the URL, the help call and the clipboard text.
- [ ] Delete `IDD_ABOUTBOX`, `IDB_ABOUT` and `src/res/AboutImage.bmp` Done when: `rg -n "IDD_ABOUTBOX|IDB_ABOUT" src` prints nothing.
- [ ] Commit: `ui: About dialog`

**Test checkpoint:** `ctest --preset x64-debug -R about` passes; a version typed into the dialog instead of read from the header fails after a version bump in the test.

## 16. File drop on the window

SPEC S1: dropping a file starts the obvious task.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S1 (files dropped on the window)
**Job:** The user drops a file on CarbonCD and lands in the right task with it filled in
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** The main window as a whole is the drop target

**Build order:**

1. Route dropped files Done when: an image opens Write and other files open Master.
2. Refuse during a task Done when: a drop while a task runs is ignored with a status message.

- [ ] Register the main window as a drop target; route `.iso`, `.cue`, `.ccd`, `.cdm` to the Write pane with the path filled, and any other files or folders to the Master pane's file list Done when: `tests/ui/test_drop.cpp` drops each kind and observes the pane and its entries. Cheaper substitute: accepting drops only on the mastering list.
- [ ] While a task runs, refuse the drop (no-drop cursor) and say why in the status bar (in `src/ui/MainWindow.cpp`) Done when: a drop during a simulated task changes no pane and the status reads `A task is running.`.
- [ ] Commit: `ui: file drop routing`

**Test checkpoint:** `ctest --preset x64-debug -R drop` passes; routing an `.iso` to Master fails it.

## 17. 1.0.8 dialogs, menus and resources removed

What is left of the 1.0.8 interface once every pane has replaced its dialog.

**Phase:** 6
**Needs:** none
**Surface:** no

**Build order:**

1. Delete the old main dialog and what only it used Done when: the resource script holds nothing unused.
2. Prove nothing dangles Done when: the build has no unused resource identifier.

- [ ] Delete `src/CDMDlg.*`, `IDD_MAIN_DIALOG`, `IDR_MAINMENU`, `IDR_HIDE_MENU`, `IDD_SELECT_DRIVE`, `src/DeviceList.*`, `src/res/icon1.ico` with `IDI_ICON1`, and the eight button bitmaps in `src/res/` Done when: `rg -n "CCDMDlg|IDR_MAINMENU|IDI_ICON1" src` prints nothing and `src/res/` holds only files `src/cdm.rc` references. Cheaper substitute: leaving dead dialogs in the resource script.
- [ ] Remove from `design/build.py` the export of the eight button bitmaps and `AboutImage.bmp`, which nothing loads any more Done when: `python design/build.py` writes no `.bmp` and `design/build.py --check` passes.
- [ ] Add `scripts/check-resources.py` listing identifiers defined in `src/resource.h` and not used by any `.cpp`, `.h` or `.rc`, and run it in CI Done when: it prints nothing and exits 0; adding an unused `#define` makes it exit 1 naming it.
- [ ] Change every remaining caption that says CD where a DVD is as likely: the app is `CarbonCD`, discs are `disc` Done when: `rg -n "Write to CD|Erase CD-RW|CD Mastering|Read From CD" src Distribution` prints nothing.
- [ ] Commit: `ui: 1.0.8 dialogs, menus and bitmaps removed`

**Test checkpoint:** `python scripts/check-resources.py` exits 0 and `cmake --build --preset x64-release` exits 0; restoring `IDD_MAIN_DIALOG` to the script makes the check exit 1.

## Verification

- `ctest --preset x64-debug -R "_pane|master_|disc_export|msf|settings_dialog|about|drop"` passes.
- With the simulated provider, each of the eight rail items runs its task to the result view on a simulated CD and, where SPEC allows, a simulated DVD.
- `python scripts/check-resources.py` exits 0.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

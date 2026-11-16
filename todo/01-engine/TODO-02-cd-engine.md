---
schema_version: 1
id: cd-engine
domain: 01-engine
status: active
title: "TODO-02 -- CD engine on the new foundations"
---

# TODO-02 -- CD engine on the new foundations

> **Goal:** Everything CarbonCD 1.0.8 does with CDs works on the new transport, image and task foundations, lives in the core library, and is tested against the simulated drive.

> **Current state:** Measured 2026-11-16. CD logic is spread over MFC-bound classes: `CMMCReader` (`src/MMCReader.cpp`, READ CD variants and vendor reads D4h, D5h, D8h), `CMMCSubcodeReader`, `CMMCWriter`, `CCDWriter`, the read engines `src/ReadThreadSS.cpp` (CUE with IMG) and `src/ReadThreadMS.cpp` (CCD or CDM with `.img`, `.sub`, `.pre`), `src/ReadThreadCompare.cpp`, `src/SubQThread.cpp`, `src/WriteThread.cpp` (1 800 lines), `src/CueSheetParser.cpp` and `src/SubcodeGeneratorMS.cpp`. `src/ReadThreadAlpha.cpp` is an empty stub. None of it has a test. Write modes are SAO, RAW+16, RAW+96 and packed RAW+96 (`src/CDWriter.h:10-13`), auto-detected in the order RAW+96, RAW+16, SAO (`src/WriteThread.cpp:1817-1838`).

## Inputs

- `design/SPEC.md` S3 (CD read options), S4 (CD write methods), S7, S8, S9: the behaviours kept from 1.0.8.
- The source files named in Current state: each section moves one behaviour and cites the lines it carries over.
- -> XREF: D01 T01 §3 -- builds on §5 of this file
- -> XREF: D01 T01 §3 -- builds on §6 of this file
- -> XREF: D01 T01 §3 -- builds on §7 of this file
- -> XREF: D01 T01 §3 -- builds on §9 of this file
- -> XREF: D01 T01 §3 -- builds on §10 of this file
- -> XREF: D00 T01 §12 -- §11 here builds on it
- -> XREF: D01 T01 §5 -- §1 here builds on it
- -> XREF: D01 T01 §7 -- §1 here builds on it
- -> XREF: D01 T01 §6 -- §3 here builds on it
- -> XREF: D01 T01 §7 -- §3 here builds on it
- -> XREF: D01 T01 §8 -- §4 here builds on it
- -> XREF: D01 T01 §8 -- §7 here builds on it
- -> XREF: D01 T01 §8 -- §8 here builds on it
- -> XREF: D01 T01 §8 -- §9 here builds on it
- -> XREF: D01 T01 §9 -- §9 here builds on it
- -> XREF: D01 T01 §5 -- §10 here builds on it
- -> XREF: D01 T01 §8 -- §10 here builds on it
- -> XREF: D01 T01 §9 -- §10 here builds on it
- -> XREF: D01 T03 §13 -- builds on §5 of this file
- -> XREF: D01 T03 §13 -- builds on §9 of this file
- -> XREF: D02 T01 §6 -- builds on §1 of this file
- -> XREF: D02 T01 §8 -- builds on §9 of this file
- -> XREF: D02 T01 §8 -- builds on §3 of this file
- -> XREF: D03 T01 §8 -- builds on §1 of this file
- -> XREF: D03 T02 §1 -- builds on §5 of this file
- -> XREF: D03 T02 §1 -- builds on §6 of this file
- -> XREF: D03 T02 §1 -- builds on §7 of this file
- -> XREF: D03 T02 §2 -- builds on §9 of this file
- -> XREF: D03 T02 §8 -- builds on §7 of this file
- -> XREF: D03 T02 §9 -- builds on §8 of this file
- -> XREF: D03 T02 §10 -- builds on §10 of this file
- -> XREF: D03 T02 §12 -- builds on §1 of this file

## Outcome

- A simulated audio and data CD is read to CUE with IMG and to CCD with subcode, written back to a simulated blank CD-R in each write mode, and the two discs compare equal.
- `src/` holds no `ReadThread*.cpp`, `WriteThread.cpp`, `SubQThread.cpp`, `MMCReader.cpp`, `MMCWriter.cpp` or `CDWriter.cpp`: their behaviour is in `src/core/cd/`.

**Adjacency:** all=not-applicable (engine behaviour carried over from 1.0.8; its user-facing options and refusals are owned by the panes of D03 T02)

**Adjacency rationale:** Every read and write option of 1.0.8 keeps a consumer here and a control in D03 T02 §1, §2 and §7.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | CD table of contents, ATIP and CD-TEXT | D01 T01 §5, D01 T01 §7 | [ ] |
| 2 | §2 | CD sector reader with read command detection | §1 | [ ] |
| 3 | §3 | CUE, CCD and CDM image descriptions with 64-bit sizes | D01 T01 §6, D01 T01 §7 | [ ] |
| 4 | §4 | Single-session CD read to CUE with IMG | §2, §3, §8, D01 T01 §8 | [ ] |
| 5 | §5 | Multi-session CD read to CCD or CDM with subcode | §4 | [ ] |
| 6 | §6 | CD compare engine | §4 | [ ] |
| 7 | §7 | Read CD tracks to WAV and ISO | §2, D01 T01 §8 | [ ] |
| 8 | §8 | Subcode analysis | §2, D01 T01 §8 | [ ] |
| 9 | §9 | CD writing in SAO, RAW+16 and RAW+96 | §1, §3, D01 T01 §8, D01 T01 §9 | [ ] |
| 10 | §10 | CD-RW erase | D01 T01 §5, D01 T01 §8, D01 T01 §9 | [ ] |
| 11 | §11 | Fuzz harness for CUE, CCD and CDM descriptions | §3, D00 T01 §12 | [ ] |

## 1. CD table of contents, ATIP and CD-TEXT

Reading what is on a CD: sessions, tracks, their types and addresses.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Move TOC reading into the core library Done when: a simulated three-track CD yields three tracks with the right starts.
2. Refuse malformed drive answers Done when: a truncated TOC returns an error, not garbage.

- [ ] Add `src/core/cd/Toc.cpp` reading the full TOC (READ TOC format 2), ATIP and CD-TEXT into `struct Toc { std::vector<Session>; std::vector<Track>; }` with LBA starts, carrying over the parsing at `src/MMCReader.cpp:27-140,771-830` Done when: `tests/core/test_cd_toc.cpp` reads a simulated mixed-mode CD: two sessions, track types Audio and Mode1, lead-out at the scripted LBA. Cheaper substitute: leaving the 4400-byte `m_RawTOC` array and 99-track fixed arrays (`src/CDType.h:38-39`).
- [ ] Validate every length field against the bytes received (in `src/core/cd/Toc.cpp`) Done when: a TOC whose data length exceeds the transfer returns `Failed` with `malformed TOC`.
- [ ] Export the TOC as text, HTML, CSV and a cue sheet in `src/core/cd/TocExport.cpp`, carrying over `src/TocWindow.cpp:330-460` Done when: the test compares each export with a stored expected file. Cheaper substitute: building the strings in the window class.
- [ ] Commit: `core: CD TOC, ATIP, CD-TEXT and exports`

**Test checkpoint:** `ctest --preset x64-debug -R cd_toc` passes; dropping the session flag parse fails the two-session assertion.

## 2. CD sector reader with read command detection

Reading 2352-byte sectors with optional 16 or 96 bytes of subcode, using whichever command the drive supports.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Move the read commands into one reader Done when: audio and data sectors read correctly from the simulated CD.
2. Detect the working command Done when: a drive that refuses READ CD falls back and the choice is logged.

- [ ] Add `src/core/cd/SectorReader.cpp` with READ CD (BEh), READ CD MSF (B9h), READ(10) and the vendor reads D4h, D5h and D8h from `src/MMCReader.cpp:156-496` and `src/MMCSubcodeReader.cpp:55-375`, selected by `enum class ReadCommand` Done when: `tests/core/test_cd_reader.cpp` reads sector 0 of a data track with sync header and an audio sector, by each command the simulated drive enables. Cheaper substitute: one function per command with duplicated buffers.
- [ ] Add automatic detection trying the commands in the 1.0.8 order and remembering the first that returns valid data (in `src/core/cd/SectorReader.cpp`) Done when: with BEh disabled in the simulated drive the reader settles on the next command and logs it.
- [ ] Add the error options: ignore read errors, fast error skip, the bad sector scanner (burst count 5, skip 100 from `src/Setting.cpp:18-19`), swap audio channels, and error recovery mode page 01h (`src/MMCReader.cpp:562-608`) Done when: a scripted unreadable range of 300 sectors is skipped in bursts and every skipped LBA is in the result. Cheaper substitute: skipping silently.
- [ ] Commit: `core: CD sector reader with command detection and error options`

**Test checkpoint:** `ctest --preset x64-debug -R cd_reader` passes; removing the fallback fails the disabled-command test.

## 3. CUE, CCD and CDM image descriptions with 64-bit sizes

Parsing and writing the three CD image descriptions.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Move the parsers and writers into the core library Done when: each format round-trips.
2. Refuse what cannot be written Done when: an unsupported track mode is named in the error.

- [ ] Add `src/core/cd/CueSheet.cpp` parsing `FILE ... BINARY`, `TRACK NN AUDIO|MODE1/2352|MODE2/2352|MODE1/2048`, `INDEX`, `PREGAP`, carrying over `src/CueSheetParser.cpp:69-205` with 64-bit file sizes and paths relative to the cue file Done when: `tests/core/test_cue.cpp` parses a stored three-track sheet and writes an identical one. Cheaper substitute: keeping `m_ImageSize` as `DWORD` (`src/CueSheetParser.h:21`).
- [ ] Add `src/core/cd/CloneImage.cpp` for CCD and CDM descriptions with their `.img`, `.sub` and `.pre` files, carrying over `src/SubcodeGeneratorMS.cpp:47-101` and `src/ReadThreadMS.cpp:997-1004` Done when: the test round-trips a stored `.ccd` and reads the `[CloneCD] Version` check.
- [ ] Report unsupported content with the line number and the reason (in `src/core/cd/CueSheet.cpp`) Done when: a cue sheet with `TRACK 01 CDG` fails with `line 2: track mode CDG is not supported`. Cheaper substitute: treating unknown modes as MODE1.
- [ ] Commit: `core: CUE, CCD and CDM parsers and writers`

**Test checkpoint:** `ctest --preset x64-debug -R "cue|clone_image"` passes; reverting to a 32-bit size fails the large-file case.

## 4. Single-session CD read to CUE with IMG

The standard CD read as a task.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port the engine as a task Done when: a simulated CD reads to `.cue` and `.img`.
2. Cover stop and failure Done when: stopping leaves no partial files.

- [ ] Add `src/core/cd/ReadSingleSession.cpp` carrying over `src/ReadThreadSS.cpp` and `src/ReadThread.cpp:358-377`: TOC, optional subcode analysis for an accurate TOC, optional pregap analysis (`src/PregapAnalyzer.cpp`), sector loop with progress and surface map, cue sheet written last Done when: `tests/core/test_cd_read_ss.cpp` reads the simulated mixed-mode CD and the `.img` equals the backing data. Cheaper substitute: calling the old thread class from a wrapper.
- [ ] Add test mode (read without writing a file) (in `src/core/cd/ReadSingleSession.cpp`) Done when: the test asserts no file is created and the progress still reaches the end.
- [ ] Delete the partial `.img` and `.cue` on stop or failure, unless errors were set to be ignored Done when: the stop test finds no output file. Cheaper substitute: leaving a truncated image that looks complete.
- [ ] Commit: `core: single-session CD read task`

**Test checkpoint:** `ctest --preset x64-debug -R cd_read_ss` passes; writing the cue sheet before the data makes the stop test find a stray `.cue` and fail.

## 5. Multi-session CD read to CCD or CDM with subcode

The clone-grade CD read: every session, with 96 bytes of subcode per sector.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port the engine as a task Done when: a two-session simulated CD reads to `.ccd`, `.img` and `.sub`.
2. Keep the 90-minute and pregap handling Done when: the edge cases from 1.0.8 have tests.

- [ ] Add `src/core/cd/ReadMultiSession.cpp` carrying over `src/ReadThreadMS.cpp` (session loop, subcode read by the chosen method, `.pre` pregap file, the over-90-minute lead-out handling at `:1099-1114`) Done when: `tests/core/test_cd_read_ms.cpp` reads two sessions and the `.sub` file is 96 bytes per sector. Cheaper substitute: reading only the first session.
- [ ] Carry the subcode read methods of `src/MMCSubcodeReader.cpp` (raw 96, packed 96, 16-byte Q) behind `enum class SubcodeMethod` Done when: each method the simulated drive enables returns the scripted Q channel.
- [ ] Delete `src/ReadThreadAlpha.cpp` and the `Engine=2` value: an old INI with it falls back to single session with a log line Done when: `rg -n Alpha src` prints nothing and the settings test loads `Engine=2` as 0. Cheaper substitute: keeping the empty stub.
- [ ] Commit: `core: multi-session CD read task with subcode`

**Test checkpoint:** `ctest --preset x64-debug -R cd_read_ms` passes; reading 16-byte subcode where 96 was asked fails the size assertion.

## 6. CD compare engine

Compares a disc with an existing image sector by sector, which is also the CD verify pass.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port the compare engine as a task Done when: equal disc and image report no difference.
2. Report differences usefully Done when: each differing sector is listed, up to a limit, with a count.

- [ ] Add `src/core/cd/Compare.cpp` carrying over `src/ReadThreadCompare.cpp`, marking differing sectors as failed cells Done when: `tests/core/test_cd_compare.cpp` finds exactly the three sectors altered in the image. Cheaper substitute: comparing file sizes only.
- [ ] Return the first 100 differing LBAs and the total (in `src/core/cd/Compare.cpp`) Done when: the result holds 3 addresses and count 3; with 500 altered it holds 100 and count 500.
- [ ] Commit: `core: CD compare task`

**Test checkpoint:** `ctest --preset x64-debug -R cd_compare` passes; skipping the last sector fails the tail-difference case.

## 7. Read CD tracks to WAV and ISO

Reading chosen tracks to one file each.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port track reading as a task Done when: an audio track becomes a valid WAV and a data track an ISO.
2. Refuse an empty selection Done when: no track chosen returns an error before the drive is touched.

- [ ] Add `src/core/cd/ReadTracks.cpp` carrying over `src/ReadThread.cpp:126-132` and `src/ImageFile.cpp:117-137`: audio to `NN.wav` with a 44-byte header, data to `NN.iso` with 2048-byte user data taken from each 2352-byte sector Done when: `tests/core/test_cd_tracks.cpp` checks the WAV header fields and that the ISO's sector 16 starts with `CD001`. Cheaper substitute: writing raw 2352-byte sectors into the `.iso`.
- [ ] Write the WAV data length when the track ends, and delete the file on stop (in `src/core/cd/ReadTracks.cpp`) Done when: a stopped read leaves no file. Cheaper substitute: a header with a zero length.
- [ ] Commit: `core: read tracks task`

**Test checkpoint:** `ctest --preset x64-debug -R cd_tracks` passes; writing a wrong `data` chunk size fails the header assertion.

## 8. Subcode analysis

Scans the Q subchannel to find the true track and index positions.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port the analysis as a task Done when: the scripted Q channel yields the scripted index points.
2. Return the result as data Done when: the accurate TOC is a value the interface can show and save.

- [ ] Add `src/core/cd/SubcodeAnalysis.cpp` carrying over `src/SubQThread.cpp` with the method and speed settings (`SubQMethod`, `SubQSpeed`) Done when: `tests/core/test_subq.cpp` finds a pregap of 150 sectors scripted before track 2. Cheaper substitute: reading only the TOC and calling it analysed.
- [ ] Return `struct AccurateToc` and a cue sheet built from it Done when: the test compares the cue sheet with a stored expected file.
- [ ] Commit: `core: subcode analysis task`

**Test checkpoint:** `ctest --preset x64-debug -R subq` passes; ignoring index 0 fails the pregap assertion.

## 9. CD writing in SAO, RAW+16 and RAW+96

Writing a CD image to a blank CD-R or CD-RW in each mode 1.0.8 supports.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port the write path as a task Done when: each mode writes the simulated disc and it reads back equal.
2. Port mode detection and the write options Done when: the detected mode and each option reach the drive.

- [ ] Add `src/core/cd/Write.cpp` carrying over `src/CDWriter.cpp` and `src/WriteThread.cpp`: write parameters page 05h per mode (`src/MMCWriter.cpp:95-166`), SEND CUE SHEET for SAO, software scrambling and subcode generation for raw modes (`src/SubCodeGenerator.cpp`, `src/SubcodeGeneratorMS.cpp`, `src/CheckSector.cpp`, `src/ECCEDC.cpp`), lead-in and lead-out, SYNCHRONIZE CACHE Done when: `tests/core/test_cd_write.cpp` writes the reference image in SAO, RAW+16 and RAW+96 and the compare task reports no difference. Cheaper substitute: supporting SAO only.
- [ ] Carry over automatic mode detection in the order RAW+96, RAW+16, SAO (`src/WriteThread.cpp:1817-1838`) and the drive capability check of page 2Ah (`src/MMCWriter.cpp:578-617`) Done when: with RAW+96 disabled in the simulated drive the task chooses RAW+16 and logs it.
- [ ] Carry over the options: test mode, buffer underrun protection, power calibration (SEND OPC INFORMATION), write speed, eject when done, and the transfer size setting (in `src/core/cd/Write.cpp`) Done when: each option is visible in the simulated drive's recorded mode page or command list. Cheaper substitute: accepting the option and not sending it.
- [ ] Refuse before writing: no disc, not blank, not writable, image larger than the disc (ATIP capacity) (in `src/core/cd/Write.cpp`) Done when: each refusal returns its SPEC S13 key and the simulated disc is untouched. Cheaper substitute: starting and failing at the first write.
- [ ] Commit: `core: CD write task in three modes`

**Test checkpoint:** `ctest --preset x64-debug -R cd_write` passes; sending SAO parameters in RAW+96 mode fails the read-back compare.

## 10. CD-RW erase

Quick and full blanking of a CD-RW as a task with progress.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Port erase as a task Done when: a simulated CD-RW is blank afterwards.
2. Show progress from the drive Done when: progress follows the sense progress indication.

- [ ] Add `src/core/cd/Erase.cpp` sending BLANK (A1h) with the immediate bit, quick or full (`src/MMCWriter.cpp:674-678`), then polling REQUEST SENSE for the progress indication until ready Done when: `tests/core/test_cd_erase.cpp` sees progress rise and READ DISC INFORMATION report blank. Cheaper substitute: a blocking BLANK with no progress and a dialog that cannot be closed (`src/EraseDialog.cpp:36-47`).
- [ ] Refuse media that cannot be erased with the `not-erasable` key Done when: a simulated CD-R returns the refusal and no BLANK is sent.
- [ ] Commit: `core: CD-RW erase task with progress`

**Test checkpoint:** `ctest --preset x64-debug -R cd_erase` passes; sending BLANK to a CD-R in the test is caught by the simulated drive's command list assertion.

## 11. Fuzz harness for CUE, CCD and CDM descriptions

An image description is a text file from anywhere: a download, another program, a damaged disk. §3's parsers are the one place CarbonCD reads untrusted input without a drive in between, and a round-trip test only feeds them files that are well formed. A fuzzer feeds them everything else, under the sanitizer of D00 T01 §12, so a parser that reads past a buffer on a malformed line is found here and not on a user's machine.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Add one libFuzzer target per parser under `tests/fuzz/` in an `x64-fuzz` preset Done when: each runs for 60 seconds with no report.
2. Replay the stored inputs in the ordinary test run Done when: `ctest --preset x64-debug -R fuzz` runs every corpus and regression file once.

- [ ] Add configure and build presets `x64-fuzz` to `CMakePresets.json` compiling `carbon_core` with `/fsanitize=address /fsanitize=fuzzer` Done when: `cmake --build --preset x64-fuzz` exits 0. Cheaper substitute: fuzzing an uninstrumented library, which finds only crashes and guides nothing.
- [ ] Add `tests/fuzz/cue_sheet/fuzz_cue_sheet.cpp` with `LLVMFuzzerTestOneInput` passing the bytes to the parser of `src/core/cd/CueSheet.cpp` through an in-memory source, with a `corpus/` folder seeded from the stored sheets of `tests/core/test_cue.cpp` Done when: `build\x64-fuzz\fuzz_cue_sheet.exe -max_total_time=60 tests\fuzz\cue_sheet\corpus` ends with no report. Cheaper substitute: writing each input to a temporary file, which makes the run measure the disk.
- [ ] Add `tests/fuzz/clone_image/fuzz_clone_image.cpp` the same way for the CCD and CDM descriptions of `src/core/cd/CloneImage.cpp` Done when: its 60-second run ends with no report.
- [ ] Fix each report at the line it names in `src/core/cd/` and store its input under the target's `regressions/` folder Done when: both 60-second runs are clean and every stored input has its fix in the same commit.
- [ ] Add `tests/fuzz/fuzz_replay_main.cpp`, a `main` that calls `LLVMFuzzerTestOneInput` once for every file under `corpus/` and `regressions/`, and register one replay executable per target with CTest as `fuzz.<name>` in every preset that is not `x64-fuzz` Done when: `ctest --preset x64-debug -R fuzz` passes, and fails when a stored regression's fix is reverted.
- [ ] Bound each fuzz run: `-max_total_time=60 -max_len=65536 -rss_limit_mb=2048`, artifacts written under `build/x64-fuzz/artifacts/` and never the source tree Done when: a run leaves `git status --short` empty.
- [ ] Add a `fuzz` job to `.github/workflows/ci.yml` on a nightly schedule running both targets for 60 seconds and uploading any artifact Done when: one scheduled or manually started run is green.
- [ ] Commit: `tests: fuzzers for the CUE, CCD and CDM parsers, replayed in every test run`

**Test checkpoint:** `ctest --preset x64-debug -R fuzz` passes, and both `-max_total_time=60` runs in `x64-fuzz` end with no report; reverting one regression's fix fails the replay.

## Verification

- `ctest --preset x64-debug -R "^cd_"` passes: read, compare, tracks, subcode, write in three modes and erase on simulated media.
- `rg -n "class CReadThread|class CWriteThread|class CMMCReader|class CCDWriter" src` prints nothing.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

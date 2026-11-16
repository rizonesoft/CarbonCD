---
schema_version: 1
id: dvd-engine
domain: 01-engine
status: active
title: "TODO-03 -- DVD engine"
---

# TODO-03 -- DVD engine

> **Goal:** CarbonCD recognises every DVD type, reads a DVD to an ISO image, writes an ISO to recordable and rewritable DVDs including dual-layer discs, verifies, erases, formats and clones them.

> **Current state:** Measured 2026-11-16. There is no DVD code. The engine never sends GET CONFIGURATION, READ CAPACITY, READ DVD STRUCTURE, RESERVE TRACK, CLOSE TRACK SESSION or FORMAT UNIT. Media are assumed to be CDs: capacity comes from ATIP (`src/MasteringFileDialog.cpp:302-308`), writing is gated by a CD-R check of mode page 2Ah (`src/MMCWriter.cpp:578-600`), speed is computed as multiples of 176.4 kB/s (`src/MMCWriter.cpp:633`), and the generic write sends a 16-bit block count (`src/MMCWriter.cpp:320-321`).

## Inputs

- `design/SPEC.md` S0 (media matrix, what is refused), S2 (chips), S3, S4, S5, S9, S13 (messages).
- MMC-5 (INCITS 430-2007): features and profiles (5.3), GET CONFIGURATION (6.6), GET PERFORMANCE (6.8), READ DVD STRUCTURE (6.29), RESERVE TRACK (6.34), SEND DVD STRUCTURE (6.39), SET STREAMING (6.45), CLOSE TRACK SESSION (6.3), FORMAT UNIT (6.5), BLANK (6.2).
- -> XREF: D01 T01 §1 -- §1 here builds on it
- -> XREF: D01 T01 §5 -- §1 here builds on it
- -> XREF: D01 T01 §7 -- §4 here builds on it
- -> XREF: D01 T01 §6 -- §5 here builds on it
- -> XREF: D01 T01 §8 -- §5 here builds on it
- -> XREF: D01 T01 §9 -- §5 here builds on it
- -> XREF: D01 T01 §8 -- §11 here builds on it
- -> XREF: D01 T01 §9 -- §11 here builds on it
- -> XREF: D01 T01 §8 -- §12 here builds on it
- -> XREF: D01 T02 §5 -- §13 here builds on it
- -> XREF: D01 T02 §9 -- §13 here builds on it
- -> XREF: D02 T01 §6 -- builds on §2 of this file
- -> XREF: D02 T01 §7 -- builds on §12 of this file
- -> XREF: D03 T01 §8 -- builds on §2 of this file
- -> XREF: D03 T02 §1 -- builds on §5 of this file
- -> XREF: D03 T02 §1 -- builds on §6 of this file
- -> XREF: D03 T02 §2 -- builds on §12 of this file
- -> XREF: D03 T02 §3 -- builds on §13 of this file
- -> XREF: D03 T02 §10 -- builds on §11 of this file
- -> XREF: D03 T02 §11 -- builds on §3 of this file
- -> XREF: D03 T02 §12 -- builds on §2 of this file

## Outcome

- A simulated disc of each DVD type is recognised with its capacity, layers and state.
- A simulated DVD reads to an ISO that equals its backing data; an ISO writes to each recordable and rewritable simulated DVD and verifies.
- CSS-protected and Blu-ray media are refused with the SPEC S13 messages.

**Adjacency:** all=not-applicable (engine behaviour; the options, messages and results a user sees are owned by D03 T01 and D03 T02)

**Adjacency rationale:** Each refusal here returns a SPEC S13 key that D03 T01 §13 turns into a message. Stop and failure paths delete partial images, which is the reverse of each create.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Media profile detection | D01 T01 §1, D01 T01 §5 | [ ] |
| 2 | §2 | DVD disc information: capacity, layers, state and copy protection | §1 | [ ] |
| 3 | §3 | Read and write speed lists | §1 | [ ] |
| 4 | §4 | DVD sector reader | §2, D01 T01 §7 | [ ] |
| 5 | §5 | Read DVD to ISO task | §4, D01 T01 §6, D01 T01 §8, D01 T01 §9 | [ ] |
| 6 | §6 | Verify pass for DVD | §4 | [ ] |
| 7 | §7 | DVD-R and DVD-RW sequential writing | §2, §3 | [ ] |
| 8 | §8 | DVD+R writing | §2, §3 | [ ] |
| 9 | §9 | Dual-layer writing and the layer break | §7, §8 | [ ] |
| 10 | §10 | Rewritable DVD writing: DVD+RW, DVD-RW overwrite and DVD-RAM | §2, §3 | [ ] |
| 11 | §11 | DVD erase and format | §2, D01 T01 §8, D01 T01 §9 | [ ] |
| 12 | §12 | Write ISO to DVD task | §6, §7, §8, §9, §10, D01 T01 §8 | [ ] |
| 13 | §13 | Clone pipeline for CD and DVD | §5, §12, D01 T02 §5, D01 T02 §9 | [ ] |

## 1. Media profile detection

What kind of disc is in the drive, from the drive's current profile.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Read the profile list and the current profile Done when: each simulated profile maps to the right media type.
2. Name unsupported media Done when: Blu-ray and HD DVD profiles are recognised and marked unsupported.

- [ ] Add `src/core/drive/Media.cpp` sending GET CONFIGURATION (46h, RT 00b) and mapping the current profile to `enum class MediaType`: 08h CD-ROM, 09h CD-R, 0Ah CD-RW, 10h DVD-ROM, 11h DVD-R, 12h DVD-RAM, 13h DVD-RW restricted overwrite, 14h DVD-RW sequential, 15h and 16h DVD-R DL, 1Ah DVD+RW, 1Bh DVD+R, 2Bh DVD+R DL, 40h to 43h BD, 50h to 53h HD DVD, 0 no disc (MMC-5 (INCITS 430-2007) table 92) Done when: `tests/core/test_media.cpp` asserts every mapping against simulated discs. Cheaper substitute: guessing from mode page 2Ah as `IsCDR()` does (`src/MMCWriter.cpp:578-600`).
- [ ] Fall back to READ DISC INFORMATION and the TOC when a drive returns no current profile for a CD (in `src/core/drive/Media.cpp`) Done when: a simulated drive with GET CONFIGURATION disabled still reports CD-ROM, CD-R and CD-RW.
- [ ] Return `media_name(MediaType)` with the names of SPEC S2.2 and a `supported` flag Done when: the test asserts `DVD+R DL` and that BD is unsupported. Cheaper substitute: building names in the interface.
- [ ] Commit: `core: media type from the current profile`

**Test checkpoint:** `ctest --preset x64-debug -R media` passes; mapping 1Bh to DVD-R fails it.

## 2. DVD disc information: capacity, layers, state and copy protection

Everything the interface and the tasks need to know about a DVD before touching it.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Read capacity, layer layout and disc state Done when: a simulated DVD+R DL reports two layers and its layer break.
2. Detect CSS Done when: a simulated protected disc is flagged.

- [ ] Add `src/core/dvd/DiscInfo.cpp` returning `struct DvdInfo { MediaType type; int layers; Lba layer0_end; uint32_t used_sectors, free_sectors, capacity_sectors; bool blank, appendable, rewritable, formatted; int sessions; bool css; std::wstring book_type; }` from READ CAPACITY (25h), READ DISC INFORMATION (51h), READ TRACK INFORMATION (52h) and READ DVD STRUCTURE (ADh) format 00h for each layer Done when: `tests/core/test_dvd_info.cpp` reads a simulated DVD+R DL: 2 layers, layer 0 ends at 1 540 055, 3 080 112 sectors used. Cheaper substitute: using ATIP, which only CD-R has.
- [ ] Read copyright information with READ DVD STRUCTURE format 01h and set `css` when the protection type is 01h Done when: the simulated CSS disc has `css` true and an unprotected one false. Cheaper substitute: probing by reading a sector and looking for an error.
- [ ] Take free space on recordable media from READ TRACK INFORMATION of the invisible track, and on DVD+RW and DVD-RAM from READ FORMAT CAPACITIES (23h) (in `src/core/dvd/DiscInfo.cpp`) Done when: blank DVD-R reports 2 297 888 free and blank DVD+R 2 295 104, as scripted.
- [ ] Commit: `core: DVD capacity, layers, state and CSS flag`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_info` passes; reading only layer 0's descriptor fails the layer count assertion.

## 3. Read and write speed lists

The speeds a drive offers for the disc in it, and setting one.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Read the speed lists Done when: the simulated drive's write speeds for DVD-R appear in order.
2. Set a speed Done when: the chosen speed reaches the drive, with a fallback for older drives.

- [ ] Add `src/core/drive/Speed.cpp` reading write speeds with GET PERFORMANCE (ACh) type 03h and, where that fails, mode page 2Ah write speed descriptors, in kB/s Done when: `tests/core/test_speed.cpp` gets 5 540, 8 310 and 11 080 kB/s for the simulated DVD-R. Cheaper substitute: a fixed list of 1x to 16x.
- [ ] Convert to multiples by media: 176.4 kB/s for CD and 1 385 kB/s for DVD, and format as SPEC S3.3 shows (in `src/core/drive/Speed.cpp`) Done when: 11 080 kB/s is `8x` on a DVD and 1 764 kB/s is `10x` on a CD. Cheaper substitute: the CD-only `Speed*2352*75/1000` (`src/MMCWriter.cpp:633`).
- [ ] Set speed with SET STREAMING (B6h) and fall back to SET CD SPEED (BBh) when it is refused (in `src/core/drive/Speed.cpp`) Done when: the simulated drive records the request by either command and the result names which was used.
- [ ] Commit: `core: drive speed lists and speed setting`

**Test checkpoint:** `ctest --preset x64-debug -R speed` passes; using the CD multiple for a DVD fails the `8x` assertion.

## 4. DVD sector reader

Reading 2048-byte sectors in chunks, with retries and an account of what could not be read.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Read chunks with READ(12) Done when: a simulated DVD's sectors come back in order.
2. Handle unreadable sectors Done when: a bad range is narrowed to the exact sectors.

- [ ] Add `src/core/dvd/SectorReader.cpp` reading up to the transfer-size setting (default 32 sectors) per READ(12) (A8h), falling back to READ(10) Done when: `tests/core/test_dvd_reader.cpp` reads 100 000 sectors and they equal the backing file. Cheaper substitute: the CD reader's one-block READ(10) into a 2352-byte buffer (`src/MMCReader.cpp:361-365`).
- [ ] On a failed chunk, retry it sector by sector, up to three tries each, and return the failed LBAs (in `src/core/dvd/SectorReader.cpp`) Done when: a scripted bad range of 5 sectors inside a chunk returns exactly those 5 and reads the other 27. Cheaper substitute: failing the whole chunk.
- [ ] With skip on, zero-fill failed sectors and continue; with skip off, stop at the first with the LBA in the reason (in `src/core/dvd/SectorReader.cpp`) Done when: both behaviours are asserted, and the zero-filled image has zeros only at the 5 sectors.
- [ ] Commit: `core: chunked DVD sector reader with retry`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_reader` passes; removing the per-sector retry fails the 27-good-sectors assertion.

## 5. Read DVD to ISO task

The whole read, as SPEC S3 describes it for a DVD.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Assemble the task Done when: a simulated DVD reads to an ISO equal to its data.
2. Refuse before reading and clean up after stopping Done when: each refusal returns its key and a stopped read leaves no file.

- [ ] Add `src/core/dvd/ReadToIso.cpp`: disc info, refusals, free-space check, speed, sector loop writing through `ImageFile`, progress and surface map with one block per layer, finish Done when: `tests/core/test_dvd_read.cpp` reads a simulated 6.31 GB DVD+R DL (sparse) and the ISO's sampled sectors and size match. Cheaper substitute: copying file by file from the mounted volume.
- [ ] Refuse with SPEC S13 keys: `no-disc`, `css`, `unsupported-media`, `no-space` Done when: each case returns its key and no file is created. Cheaper substitute: starting the read and failing at the first protected sector.
- [ ] On stop or failure delete the `.part` file unless skip was on; on success with skipped sectors return DoneWithErrors and the failed LBAs Done when: the stop test finds no file, and the skip test gets DoneWithErrors with 5 addresses.
- [ ] Read the volume label from the ISO 9660 or UDF descriptor at sector 16 for the suggested file name (in `src/core/dvd/ReadToIso.cpp`) Done when: the simulated disc labelled `BACKUP_2026` suggests `BACKUP_2026.iso`.
- [ ] Commit: `core: read DVD to ISO task`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_read` passes; skipping the CSS check makes the protected-disc test fail.

## 6. Verify pass for DVD

Compares a disc with an image, used after reading and after writing.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Add the verify task Done when: an identical disc and image verify clean.
2. Report differences Done when: altered sectors are listed and counted.

- [ ] Add `src/core/dvd/Verify.cpp` reading the disc in chunks and comparing with the image, marking differing sectors as failed cells and returning up to 100 LBAs and a total Done when: `tests/core/test_dvd_verify.cpp` finds the 3 altered sectors. Cheaper substitute: comparing a checksum of the first megabyte.
- [ ] Wait for the drive to become ready after a write and re-read the disc information before verifying (in `src/core/dvd/Verify.cpp`) Done when: verify after a simulated write passes without a `not ready` failure. Cheaper substitute: sleeping a fixed ten seconds.
- [ ] Commit: `core: DVD verify task`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_verify` passes; comparing only the first chunk fails the tail case.

## 7. DVD-R and DVD-RW sequential writing

Disc-at-once and incremental writing on the dash formats.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Write disc at once Done when: a simulated DVD-R holds the image afterwards.
2. Write incrementally and with a test write Done when: both variants send the right write parameters.

- [ ] Add `src/core/dvd/WriteDash.cpp`: write parameters page 05h with write type 02h (disc at once) and BUFE as chosen, RESERVE TRACK (53h) for the image's sector count, WRITE(12) in chunks from LBA 0, SYNCHRONIZE CACHE, CLOSE TRACK SESSION (5Bh) (MMC-5 (INCITS 430-2007) 4.3.3) Done when: `tests/core/test_dvd_write_dash.cpp` writes a simulated DVD-R and reading it back equals the image. Cheaper substitute: the CD path's SEND CUE SHEET and 2352-byte frames.
- [ ] Add incremental writing (write type 00h, packet size variable) for DVD-R and DVD-RW sequential (in `src/core/dvd/WriteDash.cpp`) Done when: the simulated drive records write type 00h and the read-back matches.
- [ ] Add the test write bit, and power calibration by SEND OPC INFORMATION (in `src/core/dvd/WriteDash.cpp`) Done when: with test mode on the simulated disc is still blank afterwards and the commands were sent. Cheaper substitute: skipping the writes in the engine instead of asking the drive.
- [ ] Report the drive buffer fill from READ BUFFER CAPACITY (5Ch) into `Progress::buffer_percent` Done when: the test sees buffer values between 0 and 100.
- [ ] Commit: `core: DVD-R and DVD-RW sequential write`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_write_dash` passes; omitting RESERVE TRACK makes the simulated drive refuse the disc-at-once write.

## 8. DVD+R writing

The plus format has no write parameters page: it is written sequentially and closed.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Write and close a DVD+R Done when: a simulated DVD+R holds the image and is closed.
2. Cover the refusals Done when: a non-blank DVD+R is refused.

- [ ] Add `src/core/dvd/WritePlus.cpp`: no mode page 05h, WRITE(12) from LBA 0, SYNCHRONIZE CACHE, CLOSE TRACK SESSION with function 001b for the track then 010b for the session, finalising the disc (MMC-5 (INCITS 430-2007) 4.3.6) Done when: `tests/core/test_dvd_write_plus.cpp` writes a simulated DVD+R; it reads back equal and reports one closed session. Cheaper substitute: reusing the dash path and ignoring the refused MODE SELECT.
- [ ] Disable test mode for this media in the result of a capability query `can_test_write(MediaType)` Done when: the query is false for DVD+R, DVD+R DL, DVD+RW and DVD-RAM, as SPEC S4.4 states.
- [ ] Commit: `core: DVD+R write`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_write_plus` passes; sending MODE SELECT page 05h is caught by the simulated DVD+R refusing it.

## 9. Dual-layer writing and the layer break

Writing 8.5 GB discs, with the layer break chosen automatically or by the user.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Set the layer break on DVD+R DL Done when: the simulated disc's layer 0 ends where asked.
2. Write DVD-R DL Done when: a simulated DVD-R DL holds an image larger than one layer.

- [ ] Add `src/core/dvd/LayerBreak.cpp`: default break at half the image rounded up to 16 sectors; validate a requested break (multiple of 16, layer 1 not longer than layer 0, within layer 0's capacity) Done when: `tests/core/test_layer_break.cpp` accepts 1 540 064 for a 3 080 112 sector image and refuses 1 540 060 and 1 000 000 with the reason. Cheaper substitute: letting the drive choose and reporting nothing.
- [ ] Send the break to a DVD+R DL with SEND DVD STRUCTURE (BFh) format 20h before the first write (in `src/core/dvd/LayerBreak.cpp`) Done when: the simulated disc reports layer 0 end 1 540 063 after the write.
- [ ] Write DVD-R DL (profile 15h) in disc-at-once with the drive's layer capacity, refusing layer-jump profile 16h writing with a clear message (in `src/core/dvd/LayerBreak.cpp`) Done when: an image of 3 000 000 sectors writes and verifies on the simulated DVD-R DL. Cheaper substitute: treating DVD-R DL as a large DVD-R.
- [ ] Commit: `core: dual-layer write and layer break`

**Test checkpoint:** `ctest --preset x64-debug -R "layer_break|dvd_write_dl"` passes; accepting a break that is not a multiple of 16 fails the validation test.

## 10. Rewritable DVD writing: DVD+RW, DVD-RW overwrite and DVD-RAM

Rewritable media are written in place, after a format where one is needed.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Write to formatted rewritable media Done when: each simulated rewritable type holds the image.
2. Format first when needed Done when: an unformatted DVD+RW is formatted, then written.

- [ ] Add `src/core/dvd/WriteRewritable.cpp`: for DVD+RW, format in the background with FORMAT UNIT (04h) format type 26h if unformatted, write with WRITE(12), then CLOSE TRACK SESSION function 010b to stop the background format Done when: `tests/core/test_dvd_write_rw.cpp` writes an unformatted simulated DVD+RW and reads it back. Cheaper substitute: refusing unformatted discs.
- [ ] Write DVD-RW in restricted overwrite (profile 13h) and DVD-RAM (12h) with plain WRITE(12) (in `src/core/dvd/WriteRewritable.cpp`) Done when: both simulated types hold the image afterwards.
- [ ] Warn through the refusal `not-blank` when a rewritable disc holds data, with an `overwrite` option the task accepts Done when: without the option the task returns `not-blank`; with it the old data is replaced. Cheaper substitute: overwriting without asking.
- [ ] Commit: `core: rewritable DVD write with format when needed`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_write_rw` passes; skipping the format makes the simulated unformatted DVD+RW refuse the write.

## 11. DVD erase and format

Blanking DVD-RW and formatting DVD+RW and DVD-RAM, with progress.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Blank DVD-RW quick and full Done when: the simulated disc is blank afterwards.
2. Format DVD+RW and DVD-RAM Done when: the simulated disc reports formatted.

- [ ] Add `src/core/dvd/Erase.cpp`: BLANK (A1h) blanking type 000b full or 001b minimal for DVD-RW, immediate, with progress from REQUEST SENSE Done when: `tests/core/test_dvd_erase.cpp` sees progress rise and the disc report blank. Cheaper substitute: reusing the CD erase without checking the profile.
- [ ] Add FORMAT UNIT for DVD+RW (type 26h) and DVD-RAM (type 00h), waiting for completion with progress (in `src/core/dvd/Erase.cpp`) Done when: both simulated discs report formatted and empty.
- [ ] Refuse other media with `not-erasable`, and report that a full erase cannot be stopped (SPEC S9.3) through a `stoppable` flag on the task Done when: DVD-R is refused; the full erase task reports `stoppable` false. Cheaper substitute: killing the task mid-blank, which can ruin the disc.
- [ ] Commit: `core: DVD-RW erase, DVD+RW and DVD-RAM format`

**Test checkpoint:** `ctest --preset x64-debug -R dvd_erase` passes; sending BLANK to a DVD+RW is caught by the simulated drive.

## 12. Write ISO to DVD task

The whole write, as SPEC S4 describes it for a DVD.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Assemble the task over the per-media writers Done when: an ISO writes to each simulated DVD type and verifies.
2. Refuse before writing Done when: each refusal returns its key and the disc is untouched.

- [ ] Add `src/core/dvd/WriteIso.cpp` choosing the writer by media type, running refusals, speed, the write with progress and surface map, the optional verify pass as a second phase of the same progress, and eject Done when: `tests/core/test_dvd_write.cpp` writes and verifies on simulated DVD-R, DVD-RW, DVD+R, DVD+R DL, DVD-R DL, DVD+RW and DVD-RAM. Cheaper substitute: one code path with media-type `if` statements scattered through it.
- [ ] Refuse with SPEC S13 keys: `no-disc`, `not-blank`, `too-large` (with both sizes), `wrong-media` (a CD image for a DVD or an ISO over 870 MB for a CD), `unsupported-media`, `access-denied` Done when: each case returns its key and the simulated disc's state is unchanged. Cheaper substitute: letting the drive fail the first write.
- [ ] Accept only images whose size is a multiple of 2048 bytes; pad the last write to the 16-sector ECC block (in `src/core/dvd/WriteIso.cpp`) Done when: a 2 049 byte file is refused with the reason, and an image of 17 sectors writes 32.
- [ ] On stop, send SYNCHRONIZE CACHE and close what was opened so the drive is left usable, and return Stopped with `the disc is unusable` for write-once media Done when: the stop test leaves the simulated drive ready and unlocked. Cheaper substitute: abandoning the drive mid-track.
- [ ] Commit: `core: write ISO to DVD task with verify`

**Test checkpoint:** `ctest --preset x64-debug -R "dvd_write$"` passes; removing the `too-large` check makes the oversize test write and fail.

## 13. Clone pipeline for CD and DVD

Disc to disc: read, then write, through a temporary image.

**Phase:** 3
**Needs:** none
**Surface:** no

**Build order:**

1. Assemble clone from the read and write tasks Done when: a simulated source disc and target disc end up equal.
2. Handle one drive and clean up Done when: the same-drive path asks for the blank disc, and the temporary image is always removed.

- [ ] Add `src/core/Clone.cpp` running the right read task (DVD to ISO; CD multi-session to CCD) into the temporary folder, then the right write task, as one task with phases Reading, Writing, Verifying Done when: `tests/core/test_clone.cpp` clones a simulated DVD-ROM to a DVD+R and a simulated mixed-mode CD to a CD-R, and each pair compares equal. Cheaper substitute: streaming drive to drive with no image, which cannot recover from a slow source.
- [ ] When source and target are the same drive, raise a `NeedsDisc` progress phase after reading, eject, and wait for a blank disc of a suitable type with the stop token honoured Done when: the test swaps the simulated disc on `NeedsDisc` and the clone completes; a wrong disc repeats the request.
- [ ] Check the temporary folder's free space before reading, and delete the temporary image on every outcome (in `src/core/Clone.cpp`) Done when: after Done, Stopped and Failed the folder holds no clone file. Cheaper substitute: leaving multi-gigabyte files in the temporary folder.
- [ ] Commit: `core: clone task for CD and DVD`

**Test checkpoint:** `ctest --preset x64-debug -R clone` passes; skipping the cleanup fails the leftover-file assertion.

## Verification

- `ctest --preset x64-debug -R "media|dvd_|layer_break|speed|clone"` passes with no optical drive attached.
- Every SPEC S0 media type has a simulated read test and, where SPEC S0 lists it for writing, a write-and-verify test.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

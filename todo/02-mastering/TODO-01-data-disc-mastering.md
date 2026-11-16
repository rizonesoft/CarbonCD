---
schema_version: 1
id: data-disc-mastering
domain: 02-mastering
status: active
title: "TODO-01 -- Disc mastering for CD and DVD"
---

# TODO-01 -- Disc mastering for CD and DVD

> **Goal:** A user builds a data DVD of up to 8.5 GB, with files larger than 4 GB, as an ISO image or straight to disc, readable on Windows, macOS and Linux; and every CD mastering feature of 1.0.8 still works.

> **Current state:** Measured 2026-11-16. Mastering is CD only. `CDirStructure::m_FileSize` is a `DWORD` and the high half of the file size is discarded (`src/DirStructure.h:9`, `src/MasteringPage2.cpp:262`), so a file over 4 GB is silently truncated. The writer builds ISO 9660 with a Joliet supplementary descriptor by hand with about 66 unaligned typed writes (`src/IsoCreator.cpp:270-372`), wraps every 2048-byte sector as MODE1/2352 (`src/IsoWriter.cpp:54-76`) and always ends in a `.bin` and `.cue` pair (`src/CreateThread.cpp:581,600`). There is no UDF, no multi-extent file, and no boot record. Short names are cut to 8 characters with `~N` (`src/DirStructure.cpp:257-298`) and long names to 64 (`:301-309`). Capacity comes from ATIP (`src/MasteringFileDialog.cpp:302-308`) and the whole header is rebuilt on every change (`:333-338`). WAV files are accepted on a RIFF, WAVE and data check only (`src/CreateThread.cpp:755-798`). A CD-only protection area inserts unreadable sectors (`src/IsoCreator.cpp:1035-1055`, `COPY_PROTECTION` at `src/Setting.h:5`).

## Inputs

- `design/SPEC.md` S0 (DVD mastering scope and what is not in 2.0), S6 (the Master disc pane), S13 (`file-over-4gb-cd`, `name-shortened`, `too-large`).
- `src/DirStructure.*`, `src/FileList.*`, `src/IsoCreator.*`, `src/IsoWriter.*`, `src/CreateThread.*`, `src/MasteringFileDialog.cpp`: the code these sections move into `src/core/master/`.
- ECMA-119 (ISO 9660) and the Joliet specification for §2 and §3; OSTA UDF 1.02 for §4.
- -> XREF: D00 T01 §3 -- §1 here builds on it
- -> XREF: D01 T01 §6 -- §2 here builds on it
- -> XREF: D01 T03 §2 -- §6 here builds on it
- -> XREF: D01 T02 §1 -- §6 here builds on it
- -> XREF: D01 T03 §12 -- §7 here builds on it
- -> XREF: D01 T01 §8 -- §7 here builds on it
- -> XREF: D01 T02 §9 -- §8 here builds on it
- -> XREF: D01 T02 §3 -- §8 here builds on it
- -> XREF: D03 T02 §4 -- builds on §5 of this file
- -> XREF: D03 T02 §4 -- builds on §6 of this file
- -> XREF: D03 T02 §5 -- builds on §8 of this file
- -> XREF: D03 T02 §6 -- builds on §1 of this file
- -> XREF: D03 T02 §7 -- builds on §7 of this file
- -> XREF: D03 T02 §7 -- builds on §8 of this file

## Outcome

- A file set of 8 GB holding one 5 GB file is written as an ISO; Windows mounts it and every file's SHA-256 matches its source.
- The same file set is mastered to a simulated DVD+R DL and verifies.
- A 1.0.8 audio and data CD project produces a CUE and BIN equal to what the 1.0.8 writer produced for the same input.

**Adjacency:** all=not-applicable (image building in the core library; the project's list, options and refusals are owned by D03 T02 §4 to §7)

**Adjacency rationale:** The mastering options (always on top, notify on shortened names, protection area) keep their consumers here and their controls in D03 T02 §4.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Mastering file tree with 64-bit sizes | D00 T01 §3 | [ ] |
| 2 | §2 | ISO 9660 and Joliet writer producing 2048-byte sectors | §1, D01 T01 §6 | [ ] |
| 3 | §3 | Files over 4 GB through ISO 9660 multi-extent records | §2 | [ ] |
| 4 | §4 | UDF 1.02 bridge | §2 | [ ] |
| 5 | §5 | Incremental project size | §1 | [ ] |
| 6 | §6 | Capacity from the disc in the drive | D01 T03 §2, D01 T02 §1 | [ ] |
| 7 | §7 | Data DVD to image and to disc | §3, §4, §5, §6, D01 T03 §12, D01 T01 §8 | [ ] |
| 8 | §8 | CD mastering on the new engine | §2, D01 T02 §9, D01 T02 §3 | [ ] |
| 9 | §9 | Produced images checked with Windows' own reader | §7 | [ ] |

## 1. Mastering file tree with 64-bit sizes

The in-memory folder and file tree a disc is built from.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Move the tree into the core library with 64-bit sizes Done when: a 5 GB file keeps its size.
2. Keep the naming rules and report shortened names Done when: each shortened name is returned to the caller.

- [ ] Add `src/core/master/Tree.cpp` with `struct Node { std::wstring name; std::filesystem::path source; uint64_t size; FILETIME modified; std::vector<Node> children; }` and add, remove, rename, move and new-folder operations, carrying over `src/DirStructure.cpp` and `src/FileList.cpp` Done when: `tests/core/test_master_tree.cpp` adds a sparse 5 GB file and reads back 5 368 709 120 bytes. Cheaper substitute: widening `m_FileSize` and keeping the MFC classes.
- [ ] Generate ISO 9660 level 1 names (8.3, `~N` on collision, carried over from `src/DirStructure.cpp:257-298`) and Joliet names (up to 64 UTF-16 units, `:301-309`), returning the list of names that were changed Done when: the test gets `LONGFI~1.TXT` and `LONGFI~2.TXT` for two colliding names and both are in the changed list. Cheaper substitute: shortening silently.
- [ ] Refuse names Joliet cannot hold (the characters `* / : ; ? \`), a path deeper than 8 levels on ISO 9660 without Joliet lookup, and a duplicate name in one folder, each with the offending path Done when: each refusal is asserted with its path.
- [ ] Follow no symbolic link or junction when adding a folder, and skip files that cannot be opened with a list of what was skipped (in `src/core/master/Tree.cpp`) Done when: adding a folder holding a junction to itself terminates and lists the junction. Cheaper substitute: recursing until the stack overflows.
- [ ] Commit: `core: mastering tree with 64-bit sizes and naming rules`

**Test checkpoint:** `ctest --preset x64-debug -R master_tree` passes; storing the size in 32 bits fails the 5 GB assertion.

## 2. ISO 9660 and Joliet writer producing 2048-byte sectors

A stream of plain 2048-byte sectors: the image a DVD needs, and the data track a CD wraps.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Write the volume descriptors, path tables and directories Done when: the image mounts and lists every file.
2. Stream file data without holding it in memory Done when: an 8 GB image is produced with flat memory use.

- [ ] Add `src/core/master/Iso9660.cpp` writing the primary and Joliet supplementary volume descriptors, both-endian path tables and directory records with serialisation helpers `put_u32_both`, `put_u16_both`, `put_date` (ECMA-119 (ISO 9660) 8.4, 9.1) Done when: `tests/core/test_iso9660.cpp` parses the produced descriptors and directory tree back and finds every name and size. Cheaper substitute: the typed pointer writes of `src/IsoCreator.cpp:270-372`.
- [ ] Expose the image as `class IsoStream { uint64_t sector_count(); void read(uint64_t first, uint32_t count, span) }` reading file data from the sources on demand Done when: the test reads sectors from the middle of a file and they equal the source bytes; peak memory in the 8 GB test stays under 64 MB. Cheaper substitute: building the image in memory or always writing a file first.
- [ ] Fail the stream when a source file changed size or vanished since it was added, naming the file (in `src/core/master/Iso9660.cpp`) Done when: truncating a source mid-read returns the error with its path. Cheaper substitute: padding with zeros and reporting success.
- [ ] Set the volume label, creation time and application identifier `CARBONCD 2` Done when: the test reads them back from the descriptors.
- [ ] Commit: `core: streaming ISO 9660 and Joliet writer`

**Test checkpoint:** `ctest --preset x64-debug -R iso9660` passes; writing a path table in one byte order only fails the parse-back.

## 3. Files over 4 GB through ISO 9660 multi-extent records

ISO 9660 holds a file's size in 32 bits per extent. Larger files are written as several extents.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Split large files into extents Done when: a 5 GB file is one file with two extents.
2. Refuse them where they cannot go Done when: a CD project refuses the file.

- [ ] Write files of 4 GiB minus 2048 bytes or more as consecutive directory records with the multi-extent flag set on all but the last (ECMA-119 (ISO 9660) 9.1.6), in both the ISO 9660 and Joliet trees (in `src/core/master/Iso9660.cpp`) Done when: `tests/core/test_iso_large.cpp` parses two records for the sparse 5 GB file whose lengths sum to its size. Cheaper substitute: storing the size modulo 4 GB as `src/IsoCreator.cpp:949-950` would.
- [ ] Refuse a file over 4 GiB when the disc type is a CD, with the SPEC S13 key `file-over-4gb-cd` Done when: adding the file to a CD project returns the key and the tree is unchanged.
- [ ] Commit: `core: multi-extent files in ISO 9660`

**Test checkpoint:** `ctest --preset x64-debug -R iso_large` passes; clearing the multi-extent flag fails the parse-back.

## 4. UDF 1.02 bridge

DVDs carry UDF beside ISO 9660 so every system reads large files and long names.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Write the UDF structures over the same file data Done when: the image holds valid UDF and ISO 9660 views of the same files.
2. Check it with an independent reader Done when: Windows mounts the image and reads through UDF.

- [ ] Add `src/core/master/UdfBridge.cpp` writing the volume recognition sequence (BEA01, NSR02, TEA01), the anchor volume descriptor pointer at sector 256 and at the last sector, the main and reserve volume descriptor sequences (primary, implementation use, partition, logical volume, unallocated space, terminator), the file set descriptor, and a file entry with file identifier descriptors per folder and a file entry with short allocation descriptors per file (OSTA UDF 1.02 2 and 3) Done when: `tests/core/test_udf.cpp` walks the structures from the anchor to every file, checks each descriptor tag checksum and CRC, and finds each file's extents at the same sectors the ISO 9660 records name. Cheaper substitute: an ISO 9660 only image labelled UDF.
- [ ] Describe files over 1 GiB with several allocation descriptors of at most 1 073 739 776 bytes each (in `src/core/master/UdfBridge.cpp`) Done when: the 5 GB file's descriptors sum to its size. Cheaper substitute: one descriptor with a length that overflows 30 bits.
- [ ] Select the file system by disc type: ISO 9660 with Joliet for a CD, the bridge for a DVD (SPEC S6.6) (in `src/core/master/UdfBridge.cpp`) Done when: the same tree builds 2 volume descriptors for a CD and the UDF sequence as well for a DVD.
- [ ] Commit: `core: UDF 1.02 bridge over the ISO 9660 image`

**Test checkpoint:** `ctest --preset x64-debug -R udf` passes; corrupting a descriptor tag checksum fails the walk.

## 5. Incremental project size

The size of the disc being built, kept current as files are added, without rebuilding the image headers each time.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Compute size from the tree Done when: the computed sector count equals the written image's.
2. Keep it cheap Done when: adding one file to a 100 000 file project updates the size in under 10 ms.

- [ ] Add `Tree::layout()` in `src/core/master/Layout.cpp` computing the sector count of descriptors, path tables, directories (per folder) and file data for the chosen file system, cached per folder and invalidated upward on change Done when: `tests/core/test_layout.cpp` asserts the computed count equals `IsoStream::sector_count()` for ten generated trees. Cheaper substitute: calling the full writer on every change (`src/MasteringFileDialog.cpp:333-338`).
- [ ] Benchmark the incremental update in the test with a 100 000 file tree (in `src/core/master/Layout.cpp`) Done when: the test fails if one add takes over 10 ms.
- [ ] Commit: `core: incremental mastering size`

**Test checkpoint:** `ctest --preset x64-debug -R layout` passes; removing a folder's cache invalidation fails the equality check.

## 6. Capacity from the disc in the drive

How much the disc in the drive holds, for the capacity bar.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Return capacity for CD and DVD Done when: each simulated media type returns its capacity.
2. Provide defaults with no disc Done when: a chosen disc type gives its nominal capacity.

- [ ] Add `src/core/master/Capacity.cpp` returning sectors free for a new session from `DvdInfo` for DVDs and from ATIP and READ TRACK INFORMATION for CDs Done when: `tests/core/test_capacity.cpp` gets 2 297 888 for blank DVD-R, 2 295 104 for DVD+R, 4 173 824 for DVD+R DL and 359 849 for an 80-minute CD-R. Cheaper substitute: ATIP only (`src/MasteringFileDialog.cpp:302-308`).
- [ ] Return nominal capacities by disc type when no disc is present: Data CD 359 849 sectors, Data DVD 2 295 104, with dual layer 4 171 712 (in `src/core/master/Capacity.cpp`) Done when: the test asserts each nominal value and that a disc in the drive overrides it.
- [ ] Commit: `core: mastering capacity for CD and DVD`

**Test checkpoint:** `ctest --preset x64-debug -R capacity` passes; reading ATIP for a DVD fails the DVD-R case.

## 7. Data DVD to image and to disc

The two outputs of a data DVD project.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Create an ISO from a project Done when: the ISO equals the stream.
2. Master straight to disc Done when: a simulated DVD holds the project and verifies.

- [ ] Add `src/core/master/CreateImage.cpp`: a task writing `IsoStream` to an `.iso` through `ImageFile` with progress, free-space check and partial-file cleanup Done when: `tests/core/test_master_image.cpp` builds an image from a generated tree and its SHA-256 equals the stream's. Cheaper substitute: building a `.bin` and `.cue` for a DVD.
- [ ] Extend the DVD write task to take an `IsoStream` as its source, so mastering to disc needs no temporary image, with verify reading the stream again Done when: `tests/core/test_master_disc.cpp` masters to a simulated DVD+R DL and verify passes. Cheaper substitute: always writing a temporary ISO first.
- [ ] Refuse a project larger than the disc with `too-large`, and return the shortened-name list for the `name-shortened` message Done when: an oversize project returns the key with both sizes before any write.
- [ ] Commit: `core: data DVD mastering to image and disc`

**Test checkpoint:** `ctest --preset x64-debug -R "master_image|master_disc"` passes; dropping the UDF anchor from the stream fails the SHA-256 comparison with the reference.

## 8. CD mastering on the new engine

Data, audio and mixed CDs, as 1.0.8 builds them.

**Phase:** 4
**Needs:** none
**Surface:** no

**Build order:**

1. Build CD projects from tracks Done when: an audio and data project produces the 1.0.8 CUE and BIN.
2. Validate audio input Done when: a WAV that is not CD audio is refused with the reason.

- [ ] Add `src/core/master/CdProject.cpp`: an ordered track list (data from the tree, an existing ISO as a data track, audio from WAV), wrapping data sectors as MODE1/2352 with EDC and ECC (`src/IsoWriter.cpp:54-76`, `src/CheckSector.cpp`), carrying over `src/CreateThread.cpp:560-700` Done when: `tests/core/test_cd_project.cpp` builds a two-track project and the `.bin` and `.cue` equal stored files produced by 1.0.8 from the same input. Cheaper substitute: dropping mixed-mode discs.
- [ ] Check each WAV: PCM, 44 100 Hz, 16 bit, 2 channels, and pad the last sector with silence (in `src/core/master/CdProject.cpp`) Done when: a 48 000 Hz file is refused naming its format, and a file of 1 000 000 bytes becomes 426 sectors. Cheaper substitute: accepting any RIFF file (`src/CreateThread.cpp:755-798`).
- [ ] Carry over the CD data protection area (`src/IsoCreator.cpp:1035-1055`, sizes from `CopyProtectionSize`) for CD data projects only, off by default Done when: the test finds the area's sectors where 1.0.8 puts them, and a DVD project ignores the option.
- [ ] Master to disc through the CD write task in SAO (in `src/core/master/CdProject.cpp`) Done when: the project writes to a simulated CD-R and the compare task reports no difference.
- [ ] Commit: `core: CD mastering for data, audio and mixed discs`

**Test checkpoint:** `ctest --preset x64-debug -R cd_project` passes; changing the EDC polynomial fails the byte comparison with the 1.0.8 files.

## 9. Produced images checked with Windows' own reader

An image our parser accepts could still be wrong. Windows mounts it and the files are compared.

**Phase:** 4
**Needs:** windows-host
**Surface:** no

**Build order:**

1. Mount a produced ISO and compare every file Done when: all hashes match.
2. Clean up the mount Done when: no image stays mounted after the test.

- [ ] Add `tests/system/Test-MasteredIso.ps1`: build an image of 200 generated files including one sparse 5 GB file and names in Korean with `tools/master-cli` (a small console target over the core library), `Mount-DiskImage` it, compare each file's SHA-256 and size with its source, then `Dismount-DiskImage` in a `finally` block Done when: the script exits 0 and prints `200 files match`. Cheaper substitute: trusting the unit tests' own parser.
- [ ] Register it with CTest under the label `system`, excluded from the default preset and run in CI Done when: `ctest --preset x64-release -L system` runs it and passes.
- [ ] Commit: `test: mastered ISO mounts in Windows and every file matches`

**Test checkpoint:** `ctest --preset x64-release -L system` passes; writing a wrong extent location makes a hash differ and the script exit 1.

## Verification

- `ctest --preset x64-debug -R "master|iso|udf|layout|capacity|cd_project"` and `ctest --preset x64-release -L system` pass.
- An 8 GB project with a 5 GB file masters to a simulated DVD+R DL and verifies.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

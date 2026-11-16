---
schema_version: 1
id: transport-and-io
domain: 01-engine
status: active
title: "TODO-01 -- Transport, I/O and task foundations"
---

# TODO-01 -- Transport, I/O and task foundations

> **Goal:** The disc engine talks to drives through one small interface with a real SPTI implementation and a simulated drive for tests, handles images and addresses of DVD size, and runs every task on a worker that can be stopped cleanly and reports progress as events.

> **Current state:** Measured 2026-11-16. Commands are built by hand into `SRB_ExecSCSICmd`, a packed 32-bit ASPI structure that embeds pointers (`src/Aspi.h:3-94`) and is reused by SPTI. `CSptiDriver` redefines the pass-through structures locally (`src/SptiDriver.h:3-50`), never checks the drive handle (`src/SptiDriver.cpp:289-290`), sets a timeout of 0xffff seconds (`:153,256`), reports only success or not (`:180-183`) and loses the last error of an asynchronous write (`:215-220`). ASPI is still selectable (`src/CDController.cpp:31-38`). Image offsets are 32-bit (`src/ImageFile.h` `m_ImgPos`, `src/CueSheetParser.h:21`; `GetFileSize(h, nullptr)` at 8 sites). `MSFAddress` holds the minute in a `BYTE` (`src/CDType.h:9-11`). Workers write into dialog members and are killed with `TerminateThread` after 3 seconds (`src/ReadThread.cpp:241-248`); cancel is a plain `bool`. `CLogWindow::AddMessage` inserts into a list control from worker threads (`src/LogWindow.cpp:81-101`).

## Inputs

- `design/SPEC.md` S0 (SPTI only), S11 (stop at a safe point; `not-responding` after 30 seconds), S17 (log).
- `src/Aspi.*`, `src/AspiDriver.*`, `src/SptiDriver.*`, `src/CDController.*`, `src/DeviceList.*`, `src/ImageFile.*`, `src/CDType.*`, `src/PBBuffer.*`, `src/LogWindow.*`, the four `*Thread.*` pairs: the code §1 to §11 replace.
- MMC-5 (INCITS 430-2007) for command descriptor blocks and sense data; `ntddscsi.h` for `SCSI_PASS_THROUGH_DIRECT`.
- -> XREF: D00 T01 §3 -- §1 here builds on it
- -> XREF: D00 T01 §4 -- §3 here builds on it
- -> XREF: D00 T01 §3 -- §6 here builds on it
- -> XREF: D00 T01 §3 -- §7 here builds on it
- -> XREF: D00 T01 §3 -- §8 here builds on it
- -> XREF: D01 T02 §1 -- builds on §5 of this file
- -> XREF: D01 T02 §1 -- builds on §7 of this file
- -> XREF: D01 T02 §3 -- builds on §6 of this file
- -> XREF: D01 T02 §3 -- builds on §7 of this file
- -> XREF: D01 T02 §4 -- builds on §8 of this file
- -> XREF: D01 T02 §7 -- builds on §8 of this file
- -> XREF: D01 T02 §8 -- builds on §8 of this file
- -> XREF: D01 T02 §9 -- builds on §8 of this file
- -> XREF: D01 T02 §9 -- builds on §9 of this file
- -> XREF: D01 T02 §10 -- builds on §5 of this file
- -> XREF: D01 T02 §10 -- builds on §8 of this file
- -> XREF: D01 T02 §10 -- builds on §9 of this file
- -> XREF: D01 T03 §1 -- builds on §1 of this file
- -> XREF: D01 T03 §1 -- builds on §5 of this file
- -> XREF: D01 T03 §4 -- builds on §7 of this file
- -> XREF: D01 T03 §5 -- builds on §6 of this file
- -> XREF: D01 T03 §5 -- builds on §8 of this file
- -> XREF: D01 T03 §5 -- builds on §9 of this file
- -> XREF: D01 T03 §11 -- builds on §8 of this file
- -> XREF: D01 T03 §11 -- builds on §9 of this file
- -> XREF: D01 T03 §12 -- builds on §8 of this file
- -> XREF: D02 T01 §2 -- builds on §6 of this file
- -> XREF: D02 T01 §7 -- builds on §8 of this file
- -> XREF: D03 T01 §8 -- builds on §4 of this file
- -> XREF: D03 T01 §10 -- builds on §8 of this file
- -> XREF: D03 T01 §11 -- builds on §9 of this file
- -> XREF: D03 T01 §13 -- builds on §10 of this file
- -> XREF: D03 T02 §13 -- builds on §7 of this file

## Outcome

- `src/core/` holds the transport interface, the SPTI transport and a simulated drive, with no MFC include.
- A 9 GB image file is written, read back and compared in a test without a 32-bit offset anywhere.
- A task stops within one command of a stop request and never through `TerminateThread`; a drive that stops answering is reported after 30 seconds.

**Adjacency:** all=not-applicable (engine plumbing with no user-facing record; its settings, log and refusals surface through D03 T01 and D04 T01)

**Adjacency rationale:** The transfer size and busy-wait settings are consumed here and owned by D04 T01 §1. The log sink of §10 is the audit trail shown by D03 T01 §12.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Transport interface and command result | D00 T01 §3 | [ ] |
| 2 | §2 | SPTI transport on the Windows SDK structures | §1 | [ ] |
| 3 | §3 | ASPI removed | §2, D00 T01 §4 | [ ] |
| 4 | §4 | Drive enumeration and arrival or removal | §2 | [ ] |
| 5 | §5 | Simulated drive for tests | §1 | [ ] |
| 6 | §6 | 64-bit image file reading and writing | D00 T01 §3 | [ ] |
| 7 | §7 | Sector address types for CD and DVD | D00 T01 §3 | [ ] |
| 8 | §8 | Task runner with stop requests and progress events | D00 T01 §3 | [ ] |
| 9 | §9 | Bounded waits and no thread termination | §2, §8 | [ ] |
| 10 | §10 | Thread-safe log with a UTF-8 file | §8 | [ ] |
| 11 | §11 | Memory and handle defects from the 1.0.8 inventory | §2 | [ ] |

## 1. Transport interface and command result

One interface every drive command goes through: a command descriptor block, a data direction, a buffer, a timeout, and a result carrying the SCSI status and sense data.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Define the types in `src/core/scsi/` Done when: the header compiles into `carbon_core` with no Windows or MFC include.
2. Test the sense decoding Done when: known sense buffers decode to the right key, code and qualifier.

- [ ] Add `src/core/scsi/Transport.h` with `struct Cdb` (up to 16 bytes), `enum class Direction { None, In, Out }`, `struct Result { bool transport_ok; uint8_t status; Sense sense; uint32_t transferred; }` and `class Transport { virtual Result execute(const Cdb&, Direction, std::span<std::byte>, std::chrono::milliseconds) = 0; }` Done when: `carbon_core` builds with the header and `rg -n "afx|windows.h" src/core/scsi/Transport.h` prints nothing. Cheaper substitute: keeping `SRB_ExecSCSICmd` as the carrier with wider fields.
- [ ] Add `src/core/scsi/Sense.h` decoding fixed and descriptor format sense into key, ASC, ASCQ and the progress indication (MMC-5 (INCITS 430-2007) section 4.1.9, sense key specific bytes) Done when: `tests/core/test_sense.cpp` decodes `70 00 02 00 00 00 00 0A 00 00 00 00 04 04 00 80 40 00` as NOT READY, 04h 04h, progress 25 percent. Cheaper substitute: returning only a success flag, as `src/SptiDriver.cpp:180-183` does.
- [ ] Add `Result::describe()` returning the standard text for the common pairs (no medium 3Ah, write protected 27h, incompatible medium 30h, becoming ready 04h 01h, long write in progress 04h 08h, medium changed 28h) Done when: the test asserts each text and that an unknown pair prints its hex.
- [ ] Commit: `core: SCSI transport interface and sense decoding`

**Test checkpoint:** `ctest --preset x64-debug -R sense` passes; swapping the ASC and ASCQ offsets in `Sense.h` makes it fail.

## 2. SPTI transport on the Windows SDK structures

The real transport. It opens the drive, sends pass-through commands with a bounded timeout, and returns status and sense for every command.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Implement `SptiTransport` against `ntddscsi.h` Done when: INQUIRY against a real or virtual optical drive returns vendor and product.
2. Handle open failures as results, not crashes Done when: opening a missing drive letter returns a transport error with the Windows error text.

- [ ] Add `src/core/scsi/SptiTransport.cpp` using `SCSI_PASS_THROUGH_DIRECT` from `<ntddscsi.h>` with a 32-byte sense buffer, an aligned buffer from `_aligned_malloc` to the device's alignment mask (`IOCTL_STORAGE_QUERY_PROPERTY`), and the caller's timeout in seconds rounded up Done when: `rg -n "TimeOutValue = 0xffff|define IOCTL_SCSI" src` prints nothing. Cheaper substitute: copying the local structure definitions from `src/SptiDriver.h:3-50`.
- [ ] Open with `CreateFileW(L"\\\\.\\E:", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, ...)`, falling back to `GENERIC_READ` for read-only tasks, and return `ERROR_ACCESS_DENIED` and `ERROR_SHARING_VIOLATION` as distinct results Done when: `tests/core/test_spti_open.cpp` gets a transport error, not an exception, for `Z:` with no such drive, and the two errors map to SPEC S13 `access-denied` and `drive-busy`. Cheaper substitute: leaving the handle unchecked as `src/SptiDriver.cpp:289-290` does.
- [ ] Make writes synchronous on the task's own thread and delete the asynchronous path whose last error is lost (`src/SptiDriver.cpp:207-220`) Done when: a write that fails on the simulated drive's last block fails the task in `tests/core/test_write_last_error.cpp`. Cheaper substitute: keeping the worker and adding a flush call.
- [ ] Add a manual probe `tools/drive-probe.cpp` (target `drive-probe`) printing INQUIRY data and the current profile for a drive letter Done when: `Bin\Debug\drive-probe.exe E:` prints the drive's vendor and model on a machine with an optical drive, and `no such drive` otherwise.
- [ ] Commit: `core: SPTI transport with bounded timeouts and sense data`

**Test checkpoint:** `ctest --preset x64-debug -R "spti_open|write_last_error"` passes; restoring a fixed 0xffff timeout fails the timeout assertion in `test_spti_open.cpp`.

## 3. ASPI removed

No 64-bit ASPI layer exists, so the ASPI transport, its setting and its structures go.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Delete the ASPI code and route the app through `SptiTransport` Done when: the app builds and lists drives with no ASPI source in the tree.
2. Drop the settings Done when: an old INI with `UseSPTI=0` starts the app on SPTI.

- [ ] Delete `src/Aspi.cpp`, `src/Aspi.h`, `src/AspiDriver.cpp`, `src/AspiDriver.h` and remove them from `src/CMakeLists.txt` Done when: `rg -n "WNASPI32|CAspiDriver|SRB_ExecSCSICmd" src` prints nothing. Cheaper substitute: leaving the files in the tree but out of the build.
- [ ] Change `CCDController::InitializeAspi` (`src/CDController.cpp:31-38`) to construct the SPTI transport only, and rename it `InitializeTransport` Done when: the main window lists the machine's optical drives.
- [ ] Remove the `SPTI` check box, `Set ASPI manager` and its path field from the settings page (`src/cdm.rc` `IDD_SETTING_3`, `src/SettingPage3.cpp`) and stop reading `UseSPTI` and `AspiDLL` in `src/Setting.cpp:122` Done when: the page shows only the transfer size, and an INI holding `UseSPTI=0` and `AspiDLL=x.dll` loads without error. Cheaper substitute: hiding the controls and keeping the keys.
- [ ] Commit: `engine: ASPI transport and settings removed`

**Test checkpoint:** `cmake --build --preset x64-release` exits 0 and `rg -n -i aspi src --glob "*.cpp" --glob "*.h"` prints nothing.

## 4. Drive enumeration and arrival or removal

Drives are found by device interface, not by walking 26 letters, and the list follows a USB drive being plugged in or pulled.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Enumerate optical drives with their letters and identity Done when: the list matches Device Manager.
2. Notify listeners of changes Done when: plugging in a USB drive adds it within two seconds without a restart.

- [ ] Add `src/core/drive/DriveList.cpp` enumerating `GUID_DEVINTERFACE_CDROM` with SetupAPI, resolving each to its drive letter and INQUIRY vendor, product and revision Done when: `tests/core/test_drive_list.cpp` gets the simulated provider's two drives in letter order, and `drive-probe --list` matches Device Manager on a real machine. Cheaper substitute: looping `A:` to `Z:` with `GetDriveType`, which misses drives without letters and limits the count (`src/SptiDriver.cpp:87-95`).
- [ ] Add `DriveWatcher` registering `CM_Register_Notification` for device interface arrival and removal and for media change, calling back on its own thread Done when: the unit test's fake notifier produces `Added`, `Removed` and `MediaChanged` events in order. Cheaper substitute: polling every second.
- [ ] Keep the selected drive by device instance path, and fix the selection being reset to the first drive (`src/DeviceList.cpp:42-43`) Done when: selecting the second drive, removing the first and re-adding it leaves the second selected, in `test_drive_list.cpp`.
- [ ] Commit: `core: device-interface drive list with change notifications`

**Test checkpoint:** `ctest --preset x64-debug -R drive_list` passes; removing the instance-path match makes the selection test fail.

## 5. Simulated drive for tests

A `Transport` that behaves like an MMC drive holding a chosen disc, backed by a file. It is how every engine task is tested end to end without hardware.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Implement the simulated drive's read side for CD and DVD Done when: a test reads a simulated DVD-ROM's sectors and gets the backing file's bytes.
2. Implement its write side and fault injection Done when: a test writes a blank simulated DVD-R and reads it back, and a scripted unreadable sector returns a medium error.

- [ ] Add `src/core/sim/SimDrive.cpp` answering TEST UNIT READY, INQUIRY, GET CONFIGURATION, READ CAPACITY, READ TOC, READ DISC INFORMATION, READ TRACK INFORMATION, MODE SENSE and MODE SELECT (pages 01h, 05h, 2Ah), READ(10), READ(12), READ CD, START STOP UNIT and PREVENT ALLOW MEDIUM REMOVAL from a `SimDisc` description (profile, layers, layer break, tracks, backing file) Done when: `tests/core/test_sim_drive.cpp` reads sector 16 of a simulated DVD-ROM and finds `CD001` at byte 1. Cheaper substitute: mocking each engine function instead of the transport.
- [ ] Add the write side: WRITE(10), WRITE(12), SEND CUE SHEET, RESERVE TRACK, CLOSE TRACK SESSION, SYNCHRONIZE CACHE, BLANK, FORMAT UNIT, SEND DVD STRUCTURE, GET PERFORMANCE and SET STREAMING, enforcing each profile's rules (sequential media refuse out-of-order writes; write-once media refuse a second write; blank status changes) (in `src/core/sim/SimDrive.cpp`) Done when: writing LBA 32 before LBA 0 on a simulated DVD-R returns ILLEGAL REQUEST, and a simulated DVD+RW accepts it. Cheaper substitute: accepting every write regardless of media.
- [ ] Add a fault script: unreadable LBA ranges, a not-ready period, a command that never answers, a write failure at an LBA, a media change (in `src/core/sim/SimDrive.cpp`) Done when: each fault is exercised by one test that asserts the sense data or the timeout the engine sees.
- [ ] Bound every simulated disc's backing file: sparse files under the test's temporary directory, removed on teardown (in `src/core/sim/SimDrive.cpp`) Done when: after `ctest`, `R:\scratch` holds no `carbon-sim-*` file and no test wrote more than the size it declared. Cheaper substitute: leaving multi-gigabyte files behind.
- [ ] Commit: `core: simulated MMC drive with fault injection`

**Test checkpoint:** `ctest --preset x64-debug -R sim_drive` passes; making the simulated DVD-R accept out-of-order writes fails the sequential-write test.

## 6. 64-bit image file reading and writing

One class for reading and writing image files of any size, with explicit sector sizes.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Add the image store Done when: a file larger than 4 GB is written and read back by sector number.
2. Report the disk running out as a result Done when: a write to a full volume returns `no-space` with the bytes needed and free.

- [ ] Add `src/core/image/ImageFile.cpp` with `open`, `create`, `read_sectors(uint64_t first, uint32_t count, span)`, `write_sectors`, `size_bytes()` and `sector_size` (2048, 2352 or 2448), using `SetFilePointerEx` and `GetFileSizeEx` Done when: `tests/core/test_image_file.cpp` writes sector 2 200 000 of a sparse 2048-byte image (offset above 4 GB) and reads the same bytes back. Cheaper substitute: widening `m_ImgPos` to 64 bits in the old class and keeping `SetFilePointer`.
- [ ] Add `free_space_for(path, bytes)` using `GetDiskFreeSpaceExW` and a `preallocate` that fails early Done when: creating a 100 GB image on a volume with less free space returns an error naming both figures, before any write. Cheaper substitute: discovering the full disk at the failing write.
- [ ] Write through a temporary name `<name>.part` and rename on success; delete the part file on failure or stop unless the caller keeps partial images Done when: a stopped write leaves no `.part` file and no final file, asserted in the test. Cheaper substitute: writing straight to the final name.
- [ ] Replace every `GetFileSize(h, nullptr)` call (`src/CueSheetParser.cpp:205`, `src/WriteThread.cpp:208,436,1663`, `src/CreateThread.cpp:666`, `src/CDWriter.cpp:77`, `src/MasteringFileDialog.cpp:346,367`) with the 64-bit size Done when: `rg -n "GetFileSize\(" src` prints nothing.
- [ ] Commit: `core: 64-bit image files with atomic finish`

**Test checkpoint:** `ctest --preset x64-debug -R image_file` passes; changing `SetFilePointerEx` to a 32-bit seek fails the 4 GB test.

## 7. Sector address types for CD and DVD

LBA is the engine's address. MSF is a CD display and cue-sheet format, converted at the edges, and refused where it cannot represent the value.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Add the address types Done when: conversions round-trip over the whole CD range.
2. Remove MSF from DVD paths Done when: no DVD code path constructs an MSF value.

- [ ] Add `src/core/Address.h` with `using Lba = int32_t`, `struct Msf { uint8_t m, s, f; }`, `to_msf(Lba)` returning `std::optional<Msf>` (empty above 99:59:74), `to_lba(Msf)`, and the lead-in offset 450000 and 90-minute threshold 404849 as named constants Done when: `tests/core/test_address.cpp` round-trips LBA -150 to 449 849 and gets no value for 2 295 104. Cheaper substitute: keeping `MSFAddress` with a `BYTE` minute that wraps at 255 (`src/CDType.h:9-11`).
- [ ] Replace the literals 450000 and 404849 (`src/CDWriter.cpp:111,369,410`, `src/MMCWriter.cpp:69`, `src/SubCodeGenerator.cpp:38,172`, `src/SubcodeGeneratorMS.cpp:436,438,583,585,765`, `src/ReadThreadMS.cpp:1099,1114`) with the named constants Done when: `rg -n "450000|404849" src --glob "!core/Address.h"` prints nothing.
- [ ] Add `format_lba` (grouped in threes) and `format_msf` (`MM:SS:FF`) in `src/core/Format.h`, with sizes as decimal GB and MB to two places, per `design/DESIGN.md` section 4 Done when: the test asserts `3 080 112`, `74:00:00`, `6.31 GB` and `11.08 MB/s`. Cheaper substitute: formatting in each dialog.
- [ ] Commit: `core: LBA and MSF types with checked conversion`

**Test checkpoint:** `ctest --preset x64-debug -R address` passes; making `to_msf` wrap instead of returning empty fails it.

## 8. Task runner with stop requests and progress events

Every long operation is a task: it runs on its own thread, checks a stop token between commands, and publishes progress as values the interface reads, never by touching a window.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Add the task and event types Done when: a test task reports progress and ends with a result.
2. Make stopping cooperative and prompt Done when: a stop request ends a task within one command.

- [ ] Add `src/core/task/Task.h`: `class Task` run on `std::jthread`, `std::stop_token` passed to the body, a `Result { Outcome outcome; std::wstring reason; }` with outcomes Done, DoneWithErrors, Stopped, Failed Done when: `tests/core/test_task.cpp` runs a task to each outcome. Cheaper substitute: a `bool m_StopFlag` shared without synchronisation (`src/ReadProgressDialog.cpp:111`).
- [ ] Add `struct Progress { Phase phase; int64_t done, total; Lba lba; double bytes_per_second; int failed_cells; uint8_t buffer_percent; }` and a `ProgressSink` the task calls at most 10 times a second Done when: the test's sink receives monotonic `done` values and no more than 10 a second. Cheaper substitute: posting `ID_UPDATE_DIALOG` and having workers write dialog members (`src/ReadThread.cpp:72-89`).
- [ ] Add `SurfaceMap` in `src/core/task/SurfaceMap.h`: a fixed number of cells over a sector range, each Pending, Active, Done or Failed, with the failed sectors of a cell kept up to ten plus a count Done when: the test marks sectors and reads back cell states, including a cell that is Failed because one of its 7 000 sectors failed. Cheaper substitute: computing cells in the interface from the progress percentage.
- [ ] Add a completion callback delivered once, after the thread has finished and the drive is unlocked (in `src/core/task/Task.h`) Done when: the test asserts the callback runs exactly once for each outcome, including when the body throws.
- [ ] Commit: `core: task runner, progress and surface map model`

**Test checkpoint:** `ctest --preset x64-debug -R "task|surface_map"` passes; removing the stop check from the test task makes the stop test time out and fail.

## 9. Bounded waits and no thread termination

A hung drive must not hang CarbonCD, and a thread is never killed in the middle of a command.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Replace every unbounded wait and every `TerminateThread` Done when: none remains in the source.
2. Surface a drive that stops answering Done when: the task reports `not-responding` after 30 seconds and can keep waiting or stop.

- [ ] Add `wait_until_ready(transport, stop_token, limit)` in `src/core/drive/Ready.cpp` polling TEST UNIT READY once a second, honouring the stop token and the limit, and returning the last sense Done when: `tests/core/test_ready.cpp` sees it return after the simulated drive's 3 second not-ready period, stop at once on a stop request, and time out at the limit. Cheaper substitute: `while (TestUnitReady()==2) Sleep(1000)` (`src/CDWriter.cpp:163-166,176-179`).
- [ ] Delete every `TerminateThread` call (`src/ReadThread.cpp:241-248`, `src/CreateThread.cpp:61-68`, `src/SubQThread.cpp:56-63`, `src/EraseDialog.cpp:36-47`) and every `WaitForSingleObject(..., INFINITE)` on a worker Done when: `rg -n "TerminateThread|INFINITE" src` prints nothing. Cheaper substitute: raising the 3000 ms wait to a larger number.
- [ ] Give each command class a timeout in `src/core/scsi/Timeouts.h` (10 s for queries, 60 s for reads and writes, 30 min for blank, format and close) and raise a `NotResponding` progress phase when a command passes 30 s Done when: the simulated never-answering command produces `NotResponding` at 30 s and a transport timeout at its limit, under a mocked clock. Cheaper substitute: one timeout for every command.
- [ ] Always release the drive at task end: PREVENT ALLOW MEDIUM REMOVAL (allow) and close the handle, on every outcome (in `src/core/drive/Ready.cpp`) Done when: the test asserts the simulated drive is unlocked after Done, Stopped, Failed and after an exception.
- [ ] Commit: `engine: bounded waits, cooperative stop, drive always released`

**Test checkpoint:** `ctest --preset x64-debug -R ready` passes and `rg -n "TerminateThread|INFINITE" src` prints nothing.

## 10. Thread-safe log with a UTF-8 file

Tasks log through a queue; the interface drains it. The log can be saved after each task.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Add the log sink Done when: lines logged from two threads arrive whole and in order per thread.
2. Add the file writer Done when: the saved file is UTF-8 and appended, not rewritten.

- [ ] Add `src/core/log/Log.h`: `enum class Level { Info, Warning, Error }`, `struct Line { system_clock::time_point at; Level level; std::wstring text; }`, a bounded queue of 5 000 lines dropping the oldest, and `log_command(name, lba, length, result)` for drive commands Done when: `tests/core/test_log.cpp` logs 6 000 lines and reads the last 5 000. Cheaper substitute: inserting into a list control from the worker (`src/LogWindow.cpp:81-101`).
- [ ] Add `LogFile` appending lines as UTF-8 with a byte order mark on creation, flushing at task end Done when: the test file decodes as UTF-8 with Korean text intact and grows by exactly the new lines. Cheaper substitute: `fopen("w")` rewriting the whole file after each job (`src/LogWindow.cpp:134-160`).
- [ ] Log every failed command with its CDB bytes and decoded sense, and every skipped sector with its LBA (in `src/core/log/Log.h`) Done when: a simulated medium error produces one Error line holding `03 11 05` decoded text and the LBA.
- [ ] Commit: `core: queued log and appended UTF-8 log file`

**Test checkpoint:** `ctest --preset x64-debug -R log` passes; removing the queue bound fails the 6 000 line test.

## 11. Memory and handle defects from the 1.0.8 inventory

Four defects found while reading the engine, each small and each a crash or a silent failure.

**Phase:** 2
**Needs:** none
**Surface:** no

**Build order:**

1. Fix each defect with a test Done when: each test fails before its fix and passes after.
2. Sweep for the same pattern Done when: no other instance remains.

- [ ] Bound the CD-TEXT copy whose length comes from the drive (`src/MMCReader.cpp:816`, 5000-byte buffer at `:801`) Done when: `tests/core/test_cdtext.cpp` feeds a reported length of 65 535 and gets a truncated, safe result. Cheaper substitute: enlarging the buffer.
- [ ] Fix thread creation checks that compare with `INVALID_HANDLE_VALUE` where the API returns null (`src/SptiDriver.cpp:43`, `src/CreateThread.cpp:39`, `src/EraseDialog.cpp:94`, `src/ReadThread.cpp:219`): these threads become tasks of §8 Done when: `rg -n "CreateThread\(" src` prints nothing.
- [ ] Size the READ(10) buffer by the block length the drive reports, not a fixed 2352 bytes (`src/MMCReader.cpp:361-365`) Done when: reading 32 blocks of 2048 bytes from the simulated drive fills 65 536 bytes and overruns nothing under AddressSanitizer. Cheaper substitute: reading one block at a time.
- [ ] Build the tests once with `/fsanitize=address` in a `x64-asan` preset and run them Done when: `ctest --preset x64-asan` passes.
- [ ] Commit: `engine: bounds, handle and buffer defects fixed`

**Test checkpoint:** `ctest --preset x64-asan` passes; restoring the unbounded `memcpy` makes `test_cdtext` fail under AddressSanitizer.

## Verification

- `ctest --preset x64-debug` and `ctest --preset x64-asan` pass with no optical drive attached.
- `rg -n -i "aspi|TerminateThread|INFINITE|GetFileSize\(" src` prints nothing.
- `drive-probe --list` lists the machine's optical drives on a machine that has one.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

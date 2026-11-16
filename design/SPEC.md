# CarbonCD 2 interface specification

Every window, control, state and message of CarbonCD 2. The look is fixed by [DESIGN.md](./DESIGN.md); this file fixes what exists and what it does. Section numbers (`S1`, `S2.3`) are stable addresses that the TODO files cite.

Reference renders: every screen has one, in light and dark, listed in [mockups/README.md](./mockups/README.md). A screen is built to match its render 1:1; the HTML and CSS the renders are drawn from are in [sheet.html](./sheet.html), where every size, gap and colour can be measured. Where a render and this text disagree, fix the design first (`design/build.py`), then the code.

## S0. Scope of version 2.0

- **Platform.** Windows 10 version 1809 and later and Windows 11, 64-bit, Unicode, no administrator rights needed for reading
- **Drive access.** SPTI only. ASPI is removed: no 64-bit ASPI layer exists
- **CD.** Everything 1.0.8 does: read to CUE with IMG, to CCD or CDM with subcode, write SAO, RAW+16, RAW+96, clone, master, read tracks, subcode analysis, erase
- **DVD read.** DVD-ROM, DVD-R, DVD-R DL, DVD-RW, DVD+R, DVD+R DL, DVD+RW and DVD-RAM, to ISO (2048-byte sectors)
- **DVD write.** ISO to DVD-R, DVD-R DL, DVD-RW, DVD+R, DVD+R DL, DVD+RW and DVD-RAM, with verify
- **DVD clone.** Drive to drive, or one drive through a temporary image
- **DVD erase.** DVD-RW quick and full blank; DVD+RW and DVD-RAM format
- **DVD mastering.** Data discs: ISO 9660 with Joliet and a UDF 1.02 bridge, files over 4 GB, up to 8.5 GB

Not in 2.0, and said so in the interface where a user would look for it:

- **Copy protection is not removed.** A CSS-protected DVD-Video disc is recognised and refused with the message in S13. CarbonCD holds no decryption code.
- **Blu-ray and HD DVD** media are recognised, named in the media chip, and refused for every task.
- **DVD-Video authoring.** A `VIDEO_TS` folder is mastered as ordinary files; CarbonCD does not build or check a DVD-Video structure.
- **Removed from 1.0.8:** the ASPI manager setting, external skins (the design replaces them), and the "Read Alpha-ROM engine" page, which was never finished.

## S1. Main window

One resizable window, 820 by 500 px minimum, titled `CarbonCD`. While a task runs the title adds what it is doing: `CarbonCD  Reading to Backup-2026.iso`.

- **S1.1 Title bar.** `primary` ground, the small app icon, the title, the system buttons
- **S1.2 Task rail.** Six tasks: Read to image, Write image, Clone disc, Master disc, Read tracks, Subcode. Then two tools: Erase disc, Drive info
- **S1.3 Rail footer.** Two text buttons: `Settings` (S14) and `About` (S16)
- **S1.4 Drive strip.** Drive list, media chips, `Recognise disc` link. See S2
- **S1.5 Pane.** The selected task. One of S3 to S10, or the run view S11, or the result view S12
- **S1.6 Status bar.** Left: the last drive or task message. Right: `Log` toggle (S17)

- Selecting a rail item swaps the pane and keeps each pane's entries for the session.
- While a task runs, every rail item except the running one is disabled, and closing the window asks to stop first (S13, `close-while-running`).
- The window's position, size and last pane are saved on exit and restored on start. A saved position that is off every monitor is discarded.
- Keys: `Ctrl+1` to `Ctrl+8` select the rail items in order, `F5` recognises the disc, `Ctrl+L` toggles the log, `F1` opens help, `Ctrl+,` opens Settings, `Ctrl+M` opens the MSF calculator (S10.3), `Esc` in the run view asks to stop.
- Files dropped on the window: an image (`.iso`, `.cue`, `.ccd`, `.cdm`) opens Write image with it; anything else opens Master disc and adds it.

## S2. Drive strip

- **S2.1 Drive list.** Every optical drive as `E:  VENDOR MODEL`. The choice is saved. Drives arriving or leaving update the list without a restart
- **S2.2 Media type chip.** Canary. The profile the drive reports: `CD-ROM`, `CD-R`, `CD-RW`, `DVD-ROM`, `DVD-R`, `DVD-R DL`, `DVD-RW`, `DVD+R`, `DVD+R DL`, `DVD+RW`, `DVD-RAM`, `BD`, `No disc`
- **S2.3 Detail chips.** For a DVD: layers, then used or free space. For a CD: sessions and tracks, then the length as `MM:SS:FF`
- **S2.4 Recognise disc.** Reads the disc again and refreshes the chips and the pane. Also runs when the drive reports a media change

States: `No drive found` (the list is empty and disabled, every task's action is disabled with the reason beside it), `No disc`, `Reading disc` (chips show a busy mark), and a recognised disc.

## S3. Read to image

- **S3.1 Save as.** The image path, with `Browse`. Remembers the last folder. Suggests the volume label as the name
- **S3.2 Format.** `ISO`, `CUE / BIN`, `CCD`. A DVD allows ISO only; a CD allows all three, and ISO only when it is a single data track
- **S3.3 Speed.** The speeds the drive reports for this disc, and `Maximum`
- **S3.4 Verify after reading.** Reads the disc a second time and compares it with the image
- **S3.5 Skip unreadable sectors.** Off: stop at the first unreadable sector. On: write zeros, mark the cell failed, list the sectors in the log
- **S3.6 Options.** Opens the CD read options below. Disabled for a DVD
- **S3.7 Original card.** The disc: type, drive, sector count, file system, and the layer break of a dual-layer disc
- **S3.8 Copy card.** The file name, its size, and the free space on that volume
- **S3.9 Read disc.** Starts. Disabled, with the reason beside it, when there is no disc, no path, or too little free space

CD read options (S3.6), kept from 1.0.8: Engine (`Single session`: CUE with IMG; `Multi-session`: CCD or CDM with subcode; `Compare`: compare the disc with an existing image), Test mode (read without writing a file), Ignore read errors, Fast error skip, Intelligent bad sector scanner, Analyse subcode for an accurate TOC, Analyse pregap, Swap audio channels, Detect read command automatically, Read command, Audio speed, Data speed.

## S4. Write image

- **S4.1 Image file.** Path with `Browse`: `.iso`, `.cue`, `.ccd`, `.cdm`
- **S4.2 Speed.** The write speeds the drive reports for the disc in it
- **S4.3 Write method.** CD: `Automatic`, `SAO`, `RAW+16`, `RAW+96`, `RAW+96 packed`. DVD: `Automatic` (disc at once where the media has it) and `Incremental` on DVD-R and DVD-RW
- **S4.4 Test mode.** Runs the write with the laser off. Disabled where the media has no test write (DVD+R, DVD+RW, DVD-RAM)
- **S4.5 Buffer underrun protection.** On by default
- **S4.6 Power calibration.** On by default. CD and DVD-R/RW
- **S4.7 Verify after writing.** On by default. Reads the disc back and compares it with the image
- **S4.8 Eject when done.** On by default
- **S4.9 Layer break.** Dual-layer media only: `Automatic`, or a sector number. Refused if it is not a multiple of 16 or leaves layer 1 longer than layer 0
- **S4.10 Original card.** The image: name, size, sector count, track layout of a CD image
- **S4.11 Copy card.** The disc: type, capacity, and how much of it the image needs
- **S4.12 Write image.** Starts. Disabled with the reason when the image does not fit, the disc is not blank, or the image type does not suit the disc

## S5. Clone disc

- **S5.1 Source drive.** The drive strip's drive
- **S5.2 Target drive.** Any writer. Choosing the source drive itself clones through a temporary image and asks for the blank disc when the read is done
- **S5.3 Temporary folder.** Where the temporary image goes; shows the space needed and the space free
- **S5.4 Speeds.** Read speed and write speed
- **S5.5 Verify, Eject when done, Skip unreadable sectors.** As S3 and S4
- **S5.6 Original and Copy cards.** The source disc and the target disc or drive
- **S5.7 Clone disc.** Starts. The temporary image is deleted when the clone ends, however it ends

## S6. Master disc

- **S6.1 Disc type.** `Data CD`, `Audio CD`, `Mixed CD`, `Data DVD`. Follows the disc in the drive, and can be set by hand
- **S6.2 Volume label.** Up to 32 characters; refused characters are said at once
- **S6.3 Tracks.** CD only: the track list (data, ISO as data, audio from WAV), with add, remove, move up, move down
- **S6.4 Files.** A folder tree and a file list for the data track: add files, add folder, new folder, rename, remove, drag and drop from Explorer
- **S6.5 Capacity bar.** Used and free against the disc's capacity, in sectors and GB (and `MM:SS:FF` for a CD). Turns `error` when over
- **S6.6 File system.** `ISO 9660 + Joliet` for a CD; `ISO 9660 + Joliet + UDF 1.02` for a DVD. A file over 4 GB needs UDF and is refused on a CD
- **S6.7 Options.** Always on top, Tell me when a file name is shortened, CD data protection area (CD only, as 1.0.8)
- **S6.8 Create image.** Writes the disc as an image file: ISO for a data disc, CUE with BIN for audio or mixed
- **S6.9 Write to disc.** Masters straight to the disc with the Write image options (S4.2 to S4.8)
- **S6.10 Original and Copy cards.** The file set (count, total size) and the disc or image it becomes

## S7. Read tracks

CD only. On a DVD the pane says so and offers Read to image.

- **S7.1 Track list.** Every track with its number, type, start and length; check boxes choose which to read
- **S7.2 Folder.** Where the files go: audio as `NN.wav`, data as `NN.iso`
- **S7.3 Options.** Ignore read errors, Intelligent bad sector scanner, Swap audio channels, Detect read command automatically, Read command, Audio speed, Data speed
- **S7.4 Read tracks.** Starts; refused when no track is ticked

## S8. Subcode

CD only. On a DVD the pane says DVDs have no subcode.

| Id | Control | Behaviour |
| -- | ------- | --------- |
| S8.1 | Method and speed | The subcode read method and speed, from Settings |
| S8.2 | Analyse subcode | Starts the analysis in the run view |
| S8.3 | Result | The accurate TOC it found, with `Save as cue sheet` |

## S9. Erase disc

- **S9.1 Method.** CD-RW and DVD-RW: `Quick` and `Full`. DVD+RW and DVD-RAM: `Format`. Other media: the pane says the disc cannot be erased
- **S9.2 What will be lost.** The disc's label, size in use and session count
- **S9.3 Erase disc.** Asks once (S13, `confirm-erase`), then runs in the run view. A full erase can be stopped only by the drive finishing; `Stop` is disabled and says so

## S10. Drive info

- **S10.1 Drive.** Vendor, model, firmware, drive letter, buffer size, and for each media type whether the drive reads and writes it, with the write speeds. CD write modes (SAO, RAW+16, RAW+96, RAW+96 packed, each with test write) as 1.0.8
- **S10.2 Disc.** CD: the TOC (session, track, type, start, length, ISRC where read). DVD: book type, layers, layer break, sessions, capacity, free space. `Export` saves it as text, HTML, CSV, or a cue sheet (CD)
- **S10.3 MSF calculator.** Converts between MSF, MSF+150, LBA and LBA in hex, as 1.0.8. Opens in its own small window

## S11. Run view

Replaces the pane for every long operation. See `mockups/progress-surface-map.png`.

- **S11.1 Title.** What is running: `Reading disc`, `Writing image`, `Verifying`, `Erasing disc`, `Creating image`, `Analysing subcode`
- **S11.2 Original and Copy cards.** As the task set them; the Copy card counts up: `3.91 of 6.31 GB written`
- **S11.3 Surface map.** DESIGN.md section 7. One block per layer
- **S11.4 Progress bar.** The whole operation, including a verify pass, as one bar with the pass named in the title
- **S11.5 Readouts.** `LBA`, `RATE` (and the speed multiple), `ELAPSED`, `REMAINING`, and `UNREADABLE` or `FAILED` when not zero. A write adds `BUFFER`
- **S11.6 Stop.** Asks once when stopping would spoil a disc (S13, `confirm-stop-write`), then stops at the next safe point and leaves the drive unlocked
- **S11.7 Show log.** Opens the log drawer

- The readouts update at most four times a second. The task never waits on the interface.
- A drive that stops answering is reported after 30 seconds as `The drive is not responding`, with `Keep waiting` and `Stop`.

## S12. Result view

Shown when a task ends.

- **S12.1 Outcome.** `Done`, `Done with unreadable sectors`, `Stopped`, or `Failed`, with one sentence saying why
- **S12.2 Cards.** The Original and the Copy as they ended: sizes, the verify result, the number of unreadable sectors
- **S12.3 Surface map.** Kept as it ended, so the failed cells stay visible
- **S12.4 Actions.** `Open folder` (after a read or image creation), `Eject`, `Save log`, `Do it again`, `Close` (back to the task pane)

A stopped or failed read deletes its partial image unless `Skip unreadable sectors` was on; the result says which happened.

## S13. Messages

Every message has a title, one or two sentences, and buttons that name the action.

- **`no-drive`** (start, no optical drive): No disc drive was found. Connect one and CarbonCD will find it.
- **`no-disc`** (task needs a disc): There is no disc in drive E:.
- **`not-blank`** (write to a used disc): This disc is not blank. Erase it first, or insert a blank disc.
- **`not-erasable`** (erase on write-once media): This disc cannot be erased. Only rewritable discs can.
- **`too-large`** (image over capacity): The image needs 4.38 GB and this disc holds 4.37 GB.
- **`wrong-media`** (image type and disc differ): This image is for a CD and the disc is a DVD. Insert a CD-R or CD-RW.
- **`css`** (cSS-protected DVD): This disc is copy protected (CSS). CarbonCD does not remove copy protection.
- **`unsupported-media`** (blu-ray, HD DVD): CarbonCD works with CDs and DVDs. This is a Blu-ray disc.
- **`no-space`** (target volume too small): Drive D: has 2.10 GB free and the image needs 6.31 GB.
- **`file-over-4gb-cd`** (mastering a CD): This file is over 4 GB and cannot go on a CD.
- **`access-denied`** (drive cannot be opened for writing): Windows did not let CarbonCD write to drive E:. Close other disc software and try again.
- **`drive-busy`** (another program holds the drive): Drive E: is in use by another program.
- **`not-responding`** (no answer for 30 seconds): The drive is not responding. Keep waiting, or stop.
- **`confirm-erase`** (before erasing): Erase this disc? Everything on it will be lost. Buttons: `Erase disc`, `Cancel`
- **`confirm-stop-write`** (stop during a write): Stop writing? The disc will be unusable. Buttons: `Stop writing`, `Keep writing`
- **`close-while-running`** (closing during a task): A task is running. Stop it and close? Buttons: `Stop and close`, `Keep running`
- **`verify-failed`** (verify found a difference): The copy differs from the original in 3 sectors. The log lists them.
- **`name-shortened`** (mastering shortened a name): 2 file names were shortened to fit ISO 9660. The log lists them.

## S14. Settings

A dialog with four pages. `Save` applies and closes; `Cancel` discards.

- **S14.1 General.** Language (opens S15), Theme (`Follow Windows`, `Light`, `Dark`), Temporary folder for cloning, Save the log to a file after each task (with the path), Process priority
- **S14.2 Sounds.** Play a sound when a task succeeds (file), Play a sound when a task fails (file), each with a test button
- **S14.3 Drive.** Transfer size per command (slider, 1 to 27 CD frames, shown in KB), Wait for a busy drive (seconds)
- **S14.4 Subcode.** Subcode read method, Subcode read speed

Settings are stored per user in `%APPDATA%\Rizonesoft\CarbonCD\CarbonCD.ini`. A `CarbonCD.ini` beside the executable is used instead when it exists (portable use). On first run of 2.0 the 1.0.8 file beside the executable is read once and its values carried over.

## S15. Language

A list of the language files found, each with its language name and author, and `Use this language`. The change applies at once, with no restart. A missing string falls back to English.

## S16. About

The About banner, the version and build, the copyright line, the licence (MIT), `Rizonesoft Online`, `Help`, and the line: "Please do not use this program to make illegal copies of copyrighted work."

## S17. Log

A drawer above the status bar, opened by `Log`, `Show log` or `Ctrl+L`.

- **S17.1 Lines.** Time, a mark for information, warning or error, and the text, in Data type. Drive commands are logged by name with their address and result
- **S17.2 Filter.** `All`, `Warnings and errors`
- **S17.3 Save log.** Saves the lines as UTF-8 text
- **S17.4 Clear.** Empties the view, not the saved file

The view keeps the last 5 000 lines.

## S18. Across every screen

- **Keyboard.** Every control is reachable with Tab in reading order, has an access key where it has a label, and shows the focus ring. Enter runs the pane's primary button.
- **Screen readers.** Every control has a name; the surface map reports its summary (`62 percent read, 2 cells unreadable`), and task end is announced.
- **Scale.** Per-monitor DPI awareness v2; crisp at 100, 125, 150, 175 and 200 percent, and when the window moves between monitors.
- **Themes.** Light, dark, and Windows high contrast, switching live.
- **Language.** Every string comes from the language file; layouts leave room for text 40 percent longer than English.
- **Refusals come first.** A task's primary button is disabled with the reason beside it, so a message box is the exception, not the way a user learns what is missing.

---
schema_version: 1
id: interface-shell
domain: 03-interface
status: active
title: "TODO-01 -- Interface shell and design system"
---

# TODO-01 -- Interface shell and design system

> **Goal:** CarbonCD opens as the Carbon Copy window of the design contract: the task rail, the drive strip, a pane, the run view with the disc surface map, the result view, the log drawer and the messages, in light, dark and high contrast, at every scale, usable from the keyboard.

> **Current state:** Measured 2026-11-16. The interface is a fixed-size MFC dialog (`IDD_MAIN_DIALOG`, `src/cdm.rc:503`) with a menu bar and eight bitmap buttons loaded by `CCDMDlg::InitializeButtons` (`src/CDMDlg.cpp:828-905`), set in Verdana 9. Each task opens a modal dialog and each long operation a modal progress dialog with one bar and a percentage (`IDD_READPROGRESS`, `IDD_WRITEPROGRESS`, `IDD_CREATPROGRESS`, `IDD_SUBQPROGRESS`, `IDD_ERASE`). The TOC and the log are separate frame windows. There is no DPI awareness, no dark theme and no accessibility naming. The approved design exists as `design/DESIGN.md`, `design/SPEC.md`, `design/tokens.json`, the masters in `design/icons/` and `design/brand/`, and two reference renders in `design/mockups/`; the app's icons and eight button bitmaps already carry the new art.

## Inputs

- `design/DESIGN.md` (binding: sections 1 to 11) and `design/tokens.json`.
- `design/SPEC.md` S1, S2, S11, S12, S13, S17, S18.
- `design/mockups/main-read.png` and `design/mockups/progress-surface-map.png`: the reference renders.
- `design/build.py`: the generator §1 and §6 extend.
- -> XREF: D00 T01 §7 -- §1 here builds on it
- -> XREF: D04 T01 §1 -- §7 here builds on it
- -> XREF: D01 T01 §4 -- §8 here builds on it
- -> XREF: D01 T03 §2 -- §8 here builds on it
- -> XREF: D01 T02 §1 -- §8 here builds on it
- -> XREF: D01 T01 §8 -- §10 here builds on it
- -> XREF: D01 T01 §9 -- §11 here builds on it
- -> XREF: D01 T01 §10 -- §13 here builds on it
- -> XREF: D04 T01 §3 -- §14 here builds on it
- -> XREF: D03 T02 §1 -- builds on §12 of this file
- -> XREF: D03 T02 §1 -- builds on §14 of this file
- -> XREF: D03 T02 §2 -- builds on §12 of this file
- -> XREF: D03 T02 §2 -- builds on §14 of this file
- -> XREF: D03 T02 §3 -- builds on §12 of this file
- -> XREF: D03 T02 §3 -- builds on §14 of this file
- -> XREF: D03 T02 §4 -- builds on §8 of this file
- -> XREF: D03 T02 §4 -- builds on §14 of this file
- -> XREF: D03 T02 §7 -- builds on §12 of this file
- -> XREF: D03 T02 §8 -- builds on §12 of this file
- -> XREF: D03 T02 §8 -- builds on §14 of this file
- -> XREF: D03 T02 §9 -- builds on §12 of this file
- -> XREF: D03 T02 §9 -- builds on §14 of this file
- -> XREF: D03 T02 §10 -- builds on §12 of this file
- -> XREF: D03 T02 §10 -- builds on §14 of this file
- -> XREF: D03 T02 §11 -- builds on §8 of this file
- -> XREF: D03 T02 §11 -- builds on §14 of this file
- -> XREF: D03 T02 §13 -- builds on §5 of this file
- -> XREF: D03 T02 §14 -- builds on §5 of this file
- -> XREF: D03 T02 §14 -- builds on §4 of this file
- -> XREF: D03 T02 §15 -- builds on §5 of this file
- -> XREF: D04 T01 §5 -- builds on §5 of this file
- -> XREF: D04 T01 §6 -- builds on §12 of this file
- -> XREF: D04 T01 §8 -- builds on §7 of this file

## Outcome

- The main window matches `design/mockups/main-read.png` at 100, 150 and 200 percent in light and dark.
- A simulated read shows the run view with a live surface map matching `design/mockups/progress-surface-map.png`, then the result view.
- Every control is reachable by keyboard and named for a screen reader; Windows high contrast is honoured.

**Adjacency:** list=not-applicable (the shell stores no records); document=not-applicable (exports are owned by the panes of D03 T02); settings=applicable; reporting=not-applicable (no summaries over stored data); notifications=applicable; permissions=applicable; audit=applicable; exchange=not-applicable (no import or export in the shell); reverse=applicable

**Adjacency rationale:** Settings: the theme and window placement are consumed in §4 and §7. Notifications: task end is announced in §12 and §15. Permissions: every refusal is shown through §14. Audit: the log drawer of §13 shows the engine's log. Reverse: Stop in §11 is the undo of a started task, with its confirmation in §14.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Design tokens generated into code | D00 T01 §7 | [ ] |
| 2 | §2 | Bundled fonts loaded for the process | §1 | [ ] |
| 3 | §3 | Per-monitor scaling | §1 | [ ] |
| 4 | §4 | Light, dark and high contrast themes, switching live | §1 | [ ] |
| 5 | §5 | Owner-drawn controls | §2, §3, §4, §16 | [ ] |
| 6 | §6 | Icons from the masters at every scale | §1, §3 | [ ] |
| 7 | §7 | Main window frame, task rail and status bar | §5, §6, D04 T01 §1 | [ ] |
| 8 | §8 | Drive strip with media chips | §7, D01 T01 §4, D01 T03 §2, D01 T02 §1 | [ ] |
| 9 | §9 | Original and Copy cards | §5 | [ ] |
| 10 | §10 | Disc surface map control | §5, D01 T01 §8 | [ ] |
| 11 | §11 | Run view | §8, §9, §10, D01 T01 §9 | [ ] |
| 12 | §12 | Result view | §11 | [ ] |
| 13 | §13 | Log drawer | §7, D01 T01 §10 | [ ] |
| 14 | §14 | Messages and confirmations | §5, D04 T01 §3 | [ ] |
| 15 | §15 | Keyboard and screen reader support | §7, §8, §11, §12, §13 | [ ] |
| 16 | §16 | Review renders of every screen | §1 | [ ] |

## 1. Design tokens generated into code

The contract says no colour or size is typed into code. The generator that draws the art also writes the header the interface reads.

**Phase:** 6
**Needs:** none
**Surface:** no

**Build order:**

1. Emit a C++ header from `design/tokens.json` Done when: the app builds against generated values.
2. Fail the build when they drift Done when: editing the header by hand is caught.

- [ ] Extend `design/build.py` to write `src/ui/Tokens.g.h`: `struct Palette` with a member per colour role and `kLight`, `kDark` instances, spacing, radius, stroke and size constants, and the map and icon geometry Done when: `python design/build.py --no-raster` writes the header and it compiles in a test translation unit. Cheaper substitute: a hand-written header copied from the tokens.
- [ ] Add `python design/build.py --check`, exiting 1 when any generated output differs from what the tokens produce, and run it in `.github/workflows/ci.yml` Done when: changing one colour in `src/ui/Tokens.g.h` by hand makes the check exit 1 and name the file.
- [ ] Add `src/ui/Theme.h` with `const Palette& palette()` and a `ThemeChanged` notification Done when: `tests/ui/test_theme.cpp` reads `palette().primary` as `#1E2A78` in light and `#AAB6FF` in dark. Cheaper substitute: calling `GetSysColor` in each control.
- [ ] Commit: `ui: design tokens generated into Tokens.g.h`

**Test checkpoint:** `python design/build.py --check` exits 0; editing `design/tokens.json` without rebuilding makes it exit 1.

## 2. Bundled fonts loaded for the process

Bricolage Grotesque and IBM Plex Mono ship inside the executable.

**Phase:** 6
**Needs:** none
**Surface:** no

**Build order:**

1. Embed and load the fonts Done when: text draws in both faces on a machine without them installed.
2. Fall back safely Done when: a load failure uses Segoe UI and Consolas and is logged.

- [ ] Have `design/build.py` write a static ExtraBold instance `src/res/fonts/BricolageGrotesque-ExtraBold.ttf` from the variable font, and add it with the two IBM Plex Mono files as `RCDATA` in `src/cdm.rc` Done when: the three resources are in the executable (`dumpbin /resources` or a resource enumeration test). Cheaper substitute: installing the fonts on the user's system.
- [ ] Add `src/ui/Fonts.cpp` loading them with `AddFontMemResourceEx` and creating the Display, Body and Data fonts at the point sizes of `tokens.json` for a given DPI Done when: `tests/ui/test_fonts.cpp` creates each font and `GetTextFace` returns `Bricolage Grotesque`, `Segoe UI Variable Text` or `Segoe UI`, and `IBM Plex Mono`. Cheaper substitute: naming the faces and hoping they are installed.
- [ ] Add `third_party/README.md` entries and ship `design/fonts/*/OFL.txt` in the install layout Done when: the licence files are listed in the install manifest of D04 T02 §1.
- [ ] Commit: `ui: bundled Bricolage Grotesque and IBM Plex Mono`

**Test checkpoint:** `ctest --preset x64-debug -R fonts` passes; removing a font resource fails the face-name assertion.

## 3. Per-monitor scaling

Every size is in device-independent pixels and follows the monitor the window is on.

**Phase:** 6
**Needs:** none
**Surface:** no

**Build order:**

1. Add the scaling helpers and react to scale changes Done when: moving the window between monitors re-lays it out crisply.
2. Remove dialog-unit layouts from new windows Done when: new windows compute layout from tokens.

- [ ] Add `src/ui/Scale.h` with `int px(int dip, UINT dpi)` and a `Scaled` base handling `WM_DPICHANGED` and `WM_DPICHANGED_AFTERPARENT` (fonts, icons and layout recreated, suggested rectangle applied) Done when: `tests/ui/test_scale.cpp` asserts `px(30, 144)` is 45 and that a test window receives one relayout per DPI change. Cheaper substitute: bitmap-stretching the window.
- [ ] Lay out new windows in code from tokens rather than `.rc` dialog units Done when: `rg -n "DIALOGEX" src/cdm.rc` lists only the dialogs D03 T02 §17 has yet to retire, and none of the new windows. Cheaper substitute: scaling dialog units, which rounds unevenly at 125 and 175 percent.
- [ ] Commit: `ui: per-monitor DPI scaling helpers`

**Test checkpoint:** `ctest --preset x64-debug -R scale` passes; rounding down in `px` fails the 175 percent case.

## 4. Light, dark and high contrast themes, switching live

The window follows Windows, or the user's choice in Settings.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/DESIGN.md` section 3; both palettes of `design/tokens.json`
**Job:** The user works in the theme their system or their setting asks for, with no restart
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** The window's own title bar follows the theme (dark title bar attribute)

**Build order:**

1. Follow the system setting and the user's override Done when: switching Windows to dark recolours the open window.
2. Honour high contrast Done when: with a high contrast theme on, system colours are used and the offset shadow is dropped.

- [ ] Add `src/ui/ThemeWatcher.cpp` reading `AppsUseLightTheme`, reacting to `WM_SETTINGCHANGE` (`ImmersiveColorSet`) and to the Theme setting, and setting the dark title bar with `DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` Done when: toggling the test hook switches `palette()` and every open test window repaints once. Cheaper substitute: requiring a restart.
- [ ] In high contrast (`SPI_GETHIGHCONTRAST`) map every role to a system colour (`COLOR_WINDOW`, `COLOR_WINDOWTEXT`, `COLOR_HIGHLIGHT`, `COLOR_HOTLIGHT`, `COLOR_GRAYTEXT`, `COLOR_BTNTEXT`) and disable the canary copy Done when: `tests/ui/test_theme.cpp` asserts the mapping with the high contrast hook on. Cheaper substitute: keeping brand colours, which can be unreadable for these users.
- [ ] Delete `src/ThemeController.cpp` and `.h` Done when: `rg -n CThemeController src` prints nothing.
- [ ] Commit: `ui: live light, dark and high contrast themes`

**Test checkpoint:** `ctest --preset x64-debug -R theme` passes; ignoring the high contrast flag fails the mapping assertion.

## 5. Owner-drawn controls

The contract's controls: primary button with its canary copy, button, text field, list, segmented choice, check box, chip, link, and the focus ring.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/DESIGN.md` section 6, every row of the component list
**Job:** The user presses, toggles, types and reads state in controls that look like the contract and behave like Windows
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Draw each control from tokens in every state Done when: each control renders rest, hover, pressed, focused and disabled.
2. Keep native behaviour Done when: keyboard, mouse and accessibility behave as the standard controls do.

- [ ] Add `src/ui/controls/` with `PrimaryButton`, `Button`, `TextField`, `DropList`, `Segmented`, `CheckBox`, `Chip`, `Link`, each a subclass of the standard control painting with Direct2D or GDI+ from `palette()` and the token sizes (30 px height, 5 px radius, 1 px or 1.5 px strokes) Done when: `--render` writes `controls-light.png` and `controls-dark.png` showing every control in its five states, matching `design/DESIGN.md` section 6. Cheaper substitute: bitmaps of buttons.
- [ ] Draw the primary button's canary copy 4 px down and right, and the 2 px focus ring 2 px outside every focused control (in `src/ui/controls/`) Done when: the renders show both, and the copy is absent in high contrast.
- [ ] Keep the standard window classes underneath so UI Automation patterns, access keys, Space and Enter work unchanged (in `src/ui/controls/`) Done when: `tests/ui/test_controls.cpp` clicks and keys each control through UI Automation and gets its invoke, toggle or selection event. Cheaper substitute: custom windows that swallow keyboard input.
- [ ] Disabled controls draw at 40 percent opacity and take no focus; a disabled primary button shows its reason text beside it through `set_reason()` Done when: the render shows a disabled `Read disc` with `There is no disc in drive E:` beside it. Cheaper substitute: a tooltip only, which keyboard users never see.
- [ ] Commit: `ui: owner-drawn control set`

**Test checkpoint:** `ctest --preset x64-debug -R controls` passes and `Bin\Release\CarbonCD.exe --render R:\scratch\carboncd-renders` writes `controls-light.png`; drawing with a literal colour is caught by `rg -n "RGB\(" src/ui` printing a match.

## 6. Icons from the masters at every scale

The ten graphics and the app icon, sharp at each scale and in each theme.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/DESIGN.md` section 8; masters in `design/icons/`
**Job:** The user recognises each task by its graphic, at any scale and in either theme
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Generate the raster sets Done when: each graphic exists at each size and theme.
2. Load the right one Done when: the rail shows 28 px icons at 100 percent and 56 px at 200.

- [ ] Extend `design/build.py` to write `src/res/icons/<name>-<theme>-<size>.png` for the ten graphics at 16, 20, 24, 28, 32, 40, 48 and 56 px in light and dark, and list them in a generated `src/res/icons.rc2` Done when: the 160 files exist and `design/build.py --check` covers them. Cheaper substitute: scaling one bitmap at run time.
- [ ] Add `src/ui/Icons.cpp` returning the image for a name, theme and DPI, choosing the nearest size at or above the target Done when: `tests/ui/test_icons.cpp` gets the 56 px image for 28 dip at 192 DPI and the dark variant in dark theme.
- [ ] Remove the `[General] Skin` loader and the tile-cutting code (`src/CDMDlg.cpp:828-905`) with the eight `IDB_B_*` bitmaps once the rail of §7 replaces the buttons Done when: `rg -n "IDB_B_|m_SkinFile" src` prints nothing. Cheaper substitute: keeping skins beside the design system.
- [ ] Commit: `ui: generated icon sets for every scale and theme`

**Test checkpoint:** `ctest --preset x64-debug -R icons` passes and `python design/build.py --check` exits 0; deleting one PNG fails the check.

## 7. Main window frame, task rail and status bar

The window of SPEC S1: no menu bar, the rail on the left, a pane host, the status bar.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/mockups/main-read.png`; `design/SPEC.md` S1.1 to S1.6; `design/DESIGN.md` section 5
**Job:** The user picks a disc task from the rail and always sees where they are
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** The top-level window: `primary` title bar with the small app icon, system buttons, no menu bar

**Build order:**

1. Build the frame and the rail Done when: the window matches the mockup's frame.
2. Host panes and remember placement Done when: selecting rail items swaps panes, and the window reopens where it was.

- [ ] Add `src/ui/MainWindow.cpp`: a resizable frame, 820 by 500 dip minimum, a title bar drawn by the app in `primary` on every supported Windows (client area extended over the frame with `WM_NCCALCSIZE`, hit testing in `WM_NCHITTEST`, system buttons and snap layouts kept; `DWMWA_CAPTION_COLOR` exists only from Windows 11 build 22000), the 168 dip rail with six task items, a spacer, two tool items and the `Settings` and `About` footer buttons (S1.2, S1.3), the pane host, and the status bar with the message and the `Log` toggle (S1.6) Done when: `--render` writes `main-read-light-100.png` whose frame, rail and status bar match `design/mockups/main-read.png`. Cheaper substitute: keeping `IDD_MAIN_DIALOG` and restyling its buttons.
- [ ] Add `Pane` as the base of every task pane (create, show, hide, `primary_action()`, `can_leave()`), keeping each pane's entries for the session Done when: switching from Read to Write and back keeps the path typed in Read, in `tests/ui/test_main_window.cpp`.
- [ ] Disable every rail item but the running one while a task runs, and show the task in the title (S1) (in `src/ui/MainWindow.cpp`) Done when: with a simulated task running, the UI Automation tree reports the other items disabled and the title reads `CarbonCD  Reading to x.iso`. Cheaper substitute: leaving the rail live and letting two tasks start.
- [ ] Save and restore position, size and last pane through the settings store, discarding a position that is on no monitor (`MonitorFromRect` with `MONITOR_DEFAULTTONULL`) Done when: a saved position of -5000, -5000 opens centred on the primary monitor. Cheaper substitute: clamping at 0 only, as 1.0.8 does.
- [ ] Handle `WM_CLOSE` in `src/ui/MainWindow.cpp` while a task runs: ask `close-while-running` (SPEC S13), and on `Stop and close` request the stop, wait for the task's completion callback, then close Done when: with a simulated write running, closing shows the question; `Keep running` leaves the task untouched and `Stop and close` exits only after the simulated drive is unlocked. Cheaper substitute: destroying the window and leaving the worker to die with the process.
- [ ] Replace `CCDMDlg` as the app's main window in `src/CDM.cpp` and delete `src/ButtonEx.*` Done when: the app starts into the new window and `rg -n CButtonEx src` prints nothing.
- [ ] Commit: `ui: main window, task rail and status bar`

**Test checkpoint:** `ctest --preset x64-debug -R main_window` passes and `Bin\Release\CarbonCD.exe --render R:\scratch\carboncd-renders` writes `main-read-light-100.png`; removing the off-screen check fails the placement test.

## 8. Drive strip with media chips

SPEC S2: which drive, what disc, and Recognise disc.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S2.1 to S2.4; top of `design/mockups/main-read.png`
**Job:** The user chooses a drive and sees at a glance what disc is in it
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Show drives and follow changes Done when: the list matches the machine and updates when a drive arrives.
2. Show the disc as chips and refresh on media change Done when: inserting a disc updates the chips without a click.

- [ ] Add `src/ui/DriveStrip.cpp` with the drive list (`E:  VENDOR MODEL`), fed by `DriveList` and `DriveWatcher`, saving the choice (S2.1) Done when: with the simulated provider, adding a drive adds a row and the selection is kept. Cheaper substitute: refreshing only at start.
- [ ] Add the media type chip (canary) and detail chips per S2.2 and S2.3: layers and space for a DVD; sessions, tracks and `MM:SS:FF` length for a CD Done when: a simulated DVD+R DL shows `DVD+R DL`, `2 layers`, `6.31 GB used`; a simulated audio CD shows `CD-ROM`, `1 session, 12 tracks`, `52:14:33`. Cheaper substitute: one text line with everything.
- [ ] Run recognition on a background task at start, on `Recognise disc`, on `F5` and on the watcher's media-change event, showing the busy state meanwhile (S2.4) Done when: the UI stays responsive during a simulated 3 second not-ready period and then shows the chips. Cheaper substitute: blocking the window while the drive spins up.
- [ ] Show the states `No drive found`, `No disc` and unsupported media, and publish the current drive and disc to the panes Done when: each state renders, and a pane's primary button is disabled with the matching reason.
- [ ] Commit: `ui: drive strip and media chips`

**Test checkpoint:** `ctest --preset x64-debug -R drive_strip` passes; removing the media-change subscription fails the insert-disc test.

## 9. Original and Copy cards

The pair that carries the identity: what is read on white, what is written on canary.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/DESIGN.md` sections 2 and 6; the cards of both mockups
**Job:** The user checks, before starting, exactly what will be read and what will be written
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Build the card pair Done when: both cards render with a title line and a data line.
2. Let tasks update them live Done when: the Copy card counts up during a task.

- [ ] Add `src/ui/CopyCards.cpp`: two cards joined by an arrow, `ORIGINAL` on `surface` and `COPY` on `accent`, each with a 1.5 px `primary` border, a caption in Data type, a bold title and a Data line Done when: `--render` writes `cards-light.png` matching the cards of the mockup. Cheaper substitute: two group boxes.
- [ ] Add `set_original(title, detail)` and `set_copy(title, detail)` that repaint only the changed card Done when: updating the copy detail 100 times repaints the copy card only, counted in `tests/ui/test_cards.cpp`.
- [ ] Elide long file names in the middle and expose the full text to UI Automation and as a tooltip (in `src/ui/CopyCards.cpp`) Done when: a 200 character path shows start and end with an ellipsis and UI Automation reads the whole. Cheaper substitute: letting the text clip.
- [ ] Commit: `ui: Original and Copy cards`

**Test checkpoint:** `ctest --preset x64-debug -R cards` passes and `Bin\Release\CarbonCD.exe --render R:\scratch\carboncd-renders` writes `cards-light.png`.

## 10. Disc surface map control

The grid of cells the owner asked to keep from direction B, drawn in Carbon Copy's colours.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/DESIGN.md` section 7; `design/mockups/progress-surface-map.png`
**Job:** The user watches where on the disc the task is, and sees at once where it failed
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Draw the map from the surface map model Done when: the control matches the mockup.
2. Repaint only what changed and describe itself Done when: a cell change repaints one cell, and a screen reader hears a summary.

- [ ] Add `src/ui/SurfaceMapView.cpp` drawing 44 columns by 5 rows per layer with 2 px gaps and 1.5 px radius, one block per layer labelled `L0`, `L1`, with the header `DISC SURFACE  1 CELL = N SECTORS` and the four-state legend (`design/DESIGN.md` section 7) Done when: `--render` writes `surface-map-light.png` matching the map of `design/mockups/progress-surface-map.png` for the same scripted state. Cheaper substitute: a progress bar drawn as blocks from the percentage.
- [ ] Bind it to `SurfaceMap`: invalidate only the rectangles of cells whose state changed Done when: `tests/ui/test_surface_map.cpp` counts one invalidated cell rectangle per state change. Cheaper substitute: repainting the whole control four times a second.
- [ ] Show a tooltip per cell with its first and last LBA and state, and for a failed cell its failed sectors (up to ten, then a count) (in `src/ui/SurfaceMapView.cpp`) Done when: hovering the scripted failed cell shows `LBA 497 000 to 503 999, unreadable: 497 112, 497 113`.
- [ ] Expose a UI Automation name and value: `Disc surface`, `62 percent read, 2 cells unreadable`, updated at most once a second Done when: the test reads that value from the automation element. Cheaper substitute: an unnamed custom control.
- [ ] Commit: `ui: disc surface map control`

**Test checkpoint:** `ctest --preset x64-debug -R surface_map_view` passes and `Bin\Release\CarbonCD.exe --render R:\scratch\carboncd-renders` writes `surface-map-light.png`; drawing a failed cell as done fails the state-colour assertion.

## 11. Run view

SPEC S11: what replaces the pane while a task runs.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/mockups/progress-surface-map.png`; `design/SPEC.md` S11.1 to S11.7
**Job:** The user watches a task, knows how long is left, and can stop it safely
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Compose the view and bind it to a task Done when: a simulated read drives every readout.
2. Stop and the not-responding state Done when: Stop asks where it must and the stalled drive state appears.

- [ ] Add `src/ui/RunView.cpp`: title by phase (S11.1), the cards (S11.2), the surface map (S11.3), one progress bar across all phases (S11.4), the readouts `LBA`, `RATE`, `ELAPSED`, `REMAINING`, `BUFFER` for writes and `UNREADABLE` or `FAILED` when not zero (S11.5), `Stop` and `Show log` (S11.6, S11.7) Done when: `--render` writes `run-read-light-100.png` matching `design/mockups/progress-surface-map.png`. Cheaper substitute: the old modal progress dialogs.
- [ ] Marshal `Progress` to the window with a coalescing timer so the view updates at most four times a second and the task never waits Done when: with a task emitting 1 000 events a second the view repaints at most 4 times a second and the task's run time is unchanged, in `tests/ui/test_run_view.cpp`. Cheaper substitute: `SendMessage` from the worker.
- [ ] Compute `REMAINING` from a moving average of the rate over 10 seconds, shown as `mm:ss` or `h:mm:ss`, and blank until 3 seconds have passed Done when: the scripted constant-rate task shows `03:36` at 62 percent. Cheaper substitute: elapsed divided by fraction done, which jumps.
- [ ] On `Stop` or `Esc`: ask `confirm-stop-write` when stopping spoils a disc, request the stop, show `Stopping`, and disable `Stop` when the task reports it cannot be stopped (S9.3) Done when: the test sees the confirmation for a write, none for a read, and a disabled Stop with its reason for a full erase.
- [ ] Show the `not-responding` state with `Keep waiting` and `Stop` when the task raises it, and the `NeedsDisc` prompt for a same-drive clone Done when: both scripted phases render their prompt and each button resumes or stops the task.
- [ ] Commit: `ui: run view bound to tasks`

**Test checkpoint:** `ctest --preset x64-debug -R run_view` passes and `Bin\Release\CarbonCD.exe --render R:\scratch\carboncd-renders` writes `run-read-light-100.png`; using `SendMessage` from the task thread fails the task-time assertion.

## 12. Result view

SPEC S12: what the user sees when a task ends.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S12.1 to S12.4; `design/DESIGN.md` sections 3 and 10
**Job:** The user learns whether it worked, why if not, and what to do next
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Show the outcome with the final cards and map Done when: each outcome renders with its sentence.
2. Offer the follow-up actions Done when: each action works.

- [ ] Add `src/ui/ResultView.cpp` showing `Done`, `Done with unreadable sectors`, `Stopped` or `Failed` with the task's reason (S12.1), the cards as they ended with the verify result (S12.2) and the map as it ended (S12.3) Done when: `--render` writes one PNG per outcome and the failed cells stay visible. Cheaper substitute: a message box saying `Complete`.
- [ ] Add the actions of S12.4: `Open folder` (selecting the file in Explorer), `Eject`, `Save log`, `Do it again` (back to the pane with the same entries) and `Close` Done when: `tests/ui/test_result_view.cpp` invokes each and observes the shell call, the eject command, the saved file and the pane.
- [ ] Say what happened to a partial image: deleted, or kept because skipping was on (in `src/ui/ResultView.cpp`) Done when: the stopped-read result reads `The partial image was deleted.`. Cheaper substitute: leaving the user to look in the folder.
- [ ] Commit: `ui: result view`

**Test checkpoint:** `ctest --preset x64-debug -R result_view` passes; mapping DoneWithErrors to `Done` fails the outcome assertion.

## 13. Log drawer

SPEC S17: the engine's log, inside the window.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S17.1 to S17.4; `design/DESIGN.md` sections 4 and 11
**Job:** The user reads what the drive was asked and what it answered, and saves it to send to someone
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Show the log queue in a drawer Done when: lines appear as tasks run.
2. Filter, save and clear Done when: each control does what S17 says.

- [ ] Add `src/ui/LogDrawer.cpp`: a virtual list in Data type above the status bar, opened by `Log`, `Show log` and `Ctrl+L` over 150 ms (at once when Windows animations are off), each line with time, a level mark and text (S17.1) Done when: with 5 000 lines the drawer opens in under 100 ms and scrolls smoothly, measured in `tests/ui/test_log_drawer.cpp`. Cheaper substitute: a separate frame window as `src/LogWindow.cpp`.
- [ ] Add the filter `All` or `Warnings and errors` (S17.2), `Save log` as UTF-8 (S17.3) and `Clear` (S17.4) Done when: the test filters to the two scripted warnings, saves a file that decodes as UTF-8, and clears the view while the saved file keeps its lines.
- [ ] Show the last line in the status bar and mark the `Log` toggle when a warning or error arrived while the drawer was closed Done when: a scripted error sets the mark; opening the drawer clears it. Cheaper substitute: requiring the user to open the log to learn something went wrong.
- [ ] Delete `src/LogWindow.cpp`, `.h`, `IDR_LOGFRAME` and `IDB_LOGTYPE` Done when: `rg -n "CLogWindow|IDR_LOGFRAME" src` prints nothing.
- [ ] Commit: `ui: log drawer`

**Test checkpoint:** `ctest --preset x64-debug -R log_drawer` passes; inserting lines one window message each fails the 5 000 line timing.

## 14. Messages and confirmations

SPEC S13: every refusal and confirmation, with buttons that name the action.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S13, every row; `design/DESIGN.md` section 10
**Job:** The user is told what is wrong and what to do, and confirms only what cannot be undone
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Its own small window with the title bar treatment of S1.1

**Build order:**

1. Build the message dialog and the table of messages Done when: each S13 key shows its text.
2. Prefer inline reasons Done when: pane buttons show reasons without a dialog.

- [ ] Add `src/ui/Message.cpp`: a themed dialog with a title, one or two sentences and named buttons, the safe choice as default for destructive questions, `Esc` as the safe choice Done when: `--render` writes `message-confirm-erase-light.png` with `Erase disc` and `Cancel`, `Cancel` default. Cheaper substitute: `AfxMessageBox` with OK and Cancel.
- [ ] Add `src/ui/Messages.cpp` mapping each S13 key to its language strings and parameters (sizes, drive letter, counts) Done when: `tests/ui/test_messages.cpp` formats every key in the S13 table and finds no unformatted placeholder. Cheaper substitute: building sentences by concatenation in each pane.
- [ ] Add `reason_for(key)` used by panes for the inline text beside a disabled primary button Done when: the Read pane with no disc shows `There is no disc in drive E:` and opens no dialog.
- [ ] Delete `src/MessageDialog.*`, `IDD_MESSAGE` and every `MessageBox(nullptr, ...)` raised from a worker thread (`src/WriteThread.cpp:253-262`) Done when: `rg -n "MessageBox\(" src` finds calls only in `src/ui/Message.cpp`. Cheaper substitute: leaving worker-thread message boxes that appear behind the window.
- [ ] Commit: `ui: messages and confirmations`

**Test checkpoint:** `ctest --preset x64-debug -R messages` passes; a key missing from the map fails the table test.

## 15. Keyboard and screen reader support

SPEC S18: the whole window from the keyboard, and named for assistive technology.

**Phase:** 6
**Needs:** none
**Surface:** yes
**Fidelity:** `design/SPEC.md` S1 (keys) and S18
**Job:** A keyboard or screen reader user runs any task without the mouse
**Treatment:** Tokens and components of `design/DESIGN.md` sections 3 to 6; no literal colour or size in code
**Chrome:** Inside the main window's pane (S1.5); no window frame of its own

**Build order:**

1. Wire the keys and the tab order Done when: every S1 shortcut works and Tab follows reading order.
2. Name everything and announce task end Done when: the automation tree has no unnamed control.

- [ ] Add the accelerators of S1: `Ctrl+1` to `Ctrl+8`, `F5`, `Ctrl+L`, `F1`, `Ctrl+,`, `Ctrl+M`, `Esc` in the run view, and Enter for the pane's primary button Done when: `tests/ui/test_keyboard.cpp` sends each and observes the pane, action or dialog. Cheaper substitute: mouse-only controls.
- [ ] Set tab order to reading order in the frame and every shell view, with access keys on labelled controls (in `tests/ui/test_automation.cpp`) Done when: the test tabs through the main window and the order equals the stored expected list.
- [ ] Add `tests/ui/test_automation.cpp` walking the UI Automation tree of the main window, run view and result view and failing on any control without a name Done when: the test passes, and removing one control's name fails it naming the control. Cheaper substitute: checking by eye.
- [ ] Raise a UI Automation notification at task end with the outcome sentence (in `tests/ui/test_automation.cpp`) Done when: the test's event handler receives `Done. 6.31 GB read.`.
- [ ] Commit: `ui: keyboard access and automation names`

**Test checkpoint:** `ctest --preset x64-debug -R "keyboard|automation"` passes; deleting an accelerator fails the keyboard test.

## 16. Review renders of every screen

The `--render` mode the other sections use: it writes each registered screen to a PNG so a person or a reviewer can compare it with the contract. It is built before the controls and the windows; each later section registers its own screens.

**Phase:** 6
**Needs:** windows-host
**Surface:** no

**Build order:**

1. Add the render mode Done when: one command writes every screen.
2. Cover themes and scales Done when: each screen exists in light and dark at 100, 150 and 200 percent.

- [ ] Add `--render <folder>` to `src/CDM.cpp`: with the simulated drive provider and scripted data, create each registered screen offscreen, draw it with `WM_PRINT` at each DPI and theme, and write `<screen>-<theme>-<scale>.png`, starting no real drive access and showing no window Done when: `Bin\Release\CarbonCD.exe --render R:\scratch\carboncd-renders` writes the files and exits 0 with no window shown. Cheaper substitute: manual screenshots.
- [ ] Add a registry `register_render(name, factory)` each screen section calls, and a CTest test that runs the mode and asserts every registered screen produced six files of non-zero size Done when: `ctest --preset x64-release -R renders` passes.
- [ ] Upload the folder as a CI artifact named `renders` Done when: the workflow run lists the artifact.
- [ ] Commit: `ui: render mode for review`

**Test checkpoint:** `ctest --preset x64-release -R renders` passes; a screen that fails to paint leaves a zero-size file and fails it.

## Verification

- `ctest --preset x64-debug -R "theme|fonts|scale|controls|icons|main_window|drive_strip|cards|surface_map_view|run_view|result_view|log_drawer|messages|keyboard|automation"` passes.
- The renders `main-read-light-100.png` and `run-read-light-100.png` match `design/mockups/main-read.png` and `design/mockups/progress-surface-map.png`.
- `python design/build.py --check` exits 0.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

# CarbonCD design contract

Direction: **Carbon Copy**, with the disc surface map. Approved by the owner on 2026-11-16 ("A. Carbon Copy, But I like the sector map from B"), on C++ and MFC.

This file is binding. The interface is built to it and to [SPEC.md](./SPEC.md), which lists every window, control and state. Where the code and this contract disagree, the code is wrong, unless the contract is changed first.

## 1. How the contract is held

- `design/tokens.json` is the only source of colours, type roles, spacing, radii and icon geometry. Code reads generated values, never a literal colour or size typed into a `.cpp` or `.rc` file.
- `design/build.py` draws every asset from the tokens: `python design/build.py` writes the SVG masters, the PNG renders, `design/sheet.html`, and the resources the app compiles in (`src/res/cdm.ico`, `src/res/idr_main.ico`, `src/res/AboutImage.bmp` and the button bitmaps). Art is changed in `build.py` or the tokens and rebuilt, never by editing an output.
- A visual change lands in this order: tokens or `build.py`, then this file and `SPEC.md`, then code. A code review that finds a value not in the tokens sends it back.
- The mockups in `design/mockups/` are the reference renders. A screen is done when it matches its mockup at 100, 150 and 200 percent scale in the light and dark themes, with real data.
- Every control in `SPEC.md` works or is absent. A control that is drawn but does nothing is a defect.

## 2. The idea

Carbon paper made copies before discs did. CarbonCD's mark is an original disc in carbon-paper indigo with its copy, in canary yellow, sliding out from behind it. The same pair runs through the interface: the **Original** is always shown on white, the **Copy** on canary, side by side, before anything is read or written.

Three things carry the identity and nothing else competes with them:

1. **Ink indigo** for structure: title bar, borders of the two cards, primary buttons, pane titles.
2. **Canary** for the copy: the Copy card, the media chip, the offset shadow of the primary button, the cell being read or written.
3. **The offset copy**: a canary shape 4 px down and right of an indigo one. It appears on the primary button, the icons and the wordmark, and nowhere as decoration.

## 3. Colour

Values are in `tokens.json`. The roles:

| Role | Light | Dark | Used for |
| ---- | ----- | ---- | -------- |
| `window` | `#F6F7FB` | `#10132A` | Window and pane ground |
| `surface` | `#FFFFFF` | `#181C3A` | Inputs, the Original card, the active rail item |
| `rail` | `#ECEEF8` | `#141833` | Task rail and status bar |
| `line` | `#B9C0E0` | `#3A4280` | Input borders, the dashed rule under the drive strip |
| `line-soft` | `#D3D8EE` | `#272D5C` | Rail and status bar edges |
| `text` | `#14172B` | `#E8EAFB` | All running text |
| `text-muted` | `#4A5180` | `#A4ABD6` | Captions, hints, the status bar |
| `primary` | `#1E2A78` | `#AAB6FF` | Title bar, primary button, card borders, pane titles |
| `on-primary` | `#FFFFFF` | `#10132A` | Text and glyphs on `primary` |
| `link` | `#3B4CCA` | `#AAB6FF` | Text links |
| `accent` | `#FFD23F` | `#FFD23F` | The Copy card, media chip, offset shadow |
| `on-accent` | `#14172B` | `#14172B` | Text on `accent` |
| `ok` | `#1B7F47` | `#5FD08C` | Verified, finished |
| `warn` | `#8A6000` | `#FFD23F` | Recoverable trouble, retries |
| `error` | `#C8233B` | `#FF7A8A` | Failure, unreadable sectors |

Rules:

- Canary never carries meaning by itself: the Copy card is also labelled COPY, the active cell is also outlined.
- Status colours (`ok`, `warn`, `error`) are never used for decoration, and never as the only signal: each comes with a word.
- Text on `window`, `surface`, `rail`, `primary` and `accent` meets WCAG AA (4.5 to 1) in both themes. Measured on 2026-11-16: the lowest pair is `ok` on `window` in the light theme at 4.70 to 1, and every other pair is above 5 to 1. A token change measures the changed pairs again before it lands.
- The app follows the Windows light or dark setting and switches live. Windows high contrast mode replaces every token with the system colours and drops the offset shadow.

## 4. Type

- **Display Bricolage Grotesque ExtraBold (bundled, OFL).** The wordmark and the pane title, 17 pt. Nowhere else
- **Body Segoe UI Variable Text, falling back to Segoe UI (system).** Every control, label and message, 9 pt
- **Data IBM Plex Mono Regular and Medium (bundled, OFL).** Field captions, chips, sector counts, addresses, sizes, speeds, times, the log

- Field captions are Data, 8 pt, upper case, letter-spaced, in `text-muted`: `SAVE AS`, `FORMAT`, `SPEED`.
- Every number the drive or the disc gives is set in Data with tabular figures, grouped in threes with a thin space: `3 080 112`.
- Sizes are decimal gigabytes and megabytes with two decimals (`6.31 GB`), speeds as the multiple and the rate (`8x`, `11.08 MB/s`), times as `mm:ss`, or `h:mm:ss` from one hour.
- CD addresses are shown as `MM:SS:FF` and as LBA. DVD addresses are LBA only.
- The fonts ship in the executable as resources and are loaded for the process only. Their licences are in `design/fonts/`.

## 5. Layout

- One window, 820 by 500 px at 100 percent, resizable, with a minimum of that size. It has no menu bar.
- Left: the **task rail**, 168 px, holding the six disc tasks, a spacer, then the two tools. Rail items are 40 px high with a 28 px icon.
- Right: the **pane**. From the top: the drive strip (drive list, media chips, Recognise disc), a dashed rule, the pane title, the task's fields, the Original and Copy cards, the action row.
- Bottom: the status bar, with the last drive message on the left and the Log toggle on the right. The log opens as a drawer above the status bar.
- Spacing comes from the 4 px scale in the tokens. Controls are 30 px high with a 5 px radius. Cards have square corners and a 1.5 px `primary` border.
- All sizes are in device-independent pixels and scale per monitor (per-monitor DPI awareness v2).

## 6. Components

| Component | Rule |
| --------- | ---- |
| Primary button | `primary` fill, `on-primary` text, a canary copy 4 px down and right. One per pane |
| Button | `surface` fill, 1 px `primary` border, `primary` text |
| Text field and list | `surface` fill, 1 px `line` border, 5 px radius |
| Segmented choice | 1 px `primary` border; the chosen segment is `primary` with `on-primary` text |
| Check box | 15 px, 1.5 px `primary` border; checked is a `primary` square inset by 2.5 px |
| Chip | Data type, 1 px `primary` border, 3 px radius; the media type chip is filled canary |
| Original card | `surface`, labelled ORIGINAL: what is read, with its sector count and file system |
| Copy card | `accent`, labelled COPY: what is written, with its size and the free space |
| Rail item | Icon and label; the active item is `surface` with a 3 px `primary` bar on its left edge |
| Surface map | See section 7 |
| Progress bar | 6 px, `primary` on `map-pending` |
| Focus | A 2 px `primary` ring, 2 px outside the control, on every focusable control |

States every control has: rest, hover, pressed, focused, disabled. Disabled is 40 percent opacity and takes no focus. While a task runs, the rail items other than the running one are disabled.

## 7. The disc surface map

Every long operation (read, write, clone, verify, erase, format) shows the disc as a grid of cells.

- 44 columns, 5 rows per layer, 2 px gaps, 1.5 px cell radius. A single-layer disc and a CD show one block; a dual-layer disc shows two, labelled `L0` and `L1`.
- A cell stands for an equal share of the disc's sectors; the header states it: `1 CELL = 7 000 SECTORS`.
- Cell states: pending (`map-pending`), done (`map-done`), in progress (`map-active` with a `primary` outline), unreadable or failed (`map-bad`). A legend names all four beside the header.
- A cell is failed if any sector in it failed. Hovering a cell shows its first and last LBA and its state; a failed cell also lists the failed sectors, up to ten, then a count.
- Under the map: the progress bar, then the readouts in Data type: `LBA`, `RATE`, `ELAPSED`, `REMAINING`, and the failure count in `error` when it is not zero.
- The map never animates beyond the cells changing state, and it repaints only the cells that changed.

## 8. Icons

- Drawn on a 64 unit grid: a disc of radius 20 at (26, 26) with a 3 unit `primary` outline, its canary copy offset by (4, 4), and a `primary` badge of radius 13.5 at (46, 46) holding the task's glyph in a 2.3 unit round stroke.
- The ten graphics: Read to image, Write image, Clone disc, Read tracks, Master disc, Subcode, Recognise disc, Erase disc, Drive info, Exit. Each has a light and a dark master in `design/icons/`.
- The app icon is the two discs alone: canary behind, indigo in front. Below 48 px the track rings and the highlight arc are left out.
- A new graphic uses the same disc, offset and badge, and adds only a glyph.

## 9. Brand assets

| Asset | File | Use |
| ----- | ---- | --- |
| App icon | `brand/icon.svg`, `brand/icon-small.svg`, `brand/icon-512.png` | Executable, windows, installer |
| Wordmark | `brand/wordmark-light.svg`, `brand/wordmark-dark.svg` | Documentation and web, on light and dark grounds |
| GitHub banner | `brand/github-banner.svg` and `.png` (1280 by 320) | Top of `README.md` |
| Social share banner | `brand/social-preview.svg` and `.png` (1280 by 640) | The repository's social preview |
| About banner | `brand/about-banner.svg` and `.png` (600 by 150) | The About dialog |

- The wordmark is set once, in `build.py`, and is never retyped: lettering in every SVG is outlined, so no asset depends on an installed font.
- Clear space around the wordmark and the icon is the height of the wordmark's `C`. The wordmark is never recoloured, outlined, stretched or set on a photograph.
- The product is written **CarbonCD**, one word, in running text.

## 10. Words

- Controls say what happens: `Read disc`, `Write image`, `Stop`. A dialog's buttons name the action, never `OK` for something that starts work.
- Messages say what went wrong and what to do: "This disc is not blank. Erase it first, or insert a blank disc."
- A disc is a `disc`, the file is an `image`, the two sides are the `original` and the `copy`. Media are named as the drive reports them: `DVD+R DL`, `CD-RW`.
- All text is in the language file; none is typed into code.

## 11. Motion and sound

- No animation except the surface map's cells, the progress bar and the log drawer (150 ms). With Windows animations off, the drawer opens at once.
- The success and failure sounds are the user's own files, off by default.

## 12. Not in this contract

Installer art, a website and printed material are not designed yet. When one is needed it is added here first.

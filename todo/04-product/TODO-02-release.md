---
schema_version: 1
id: release
domain: 04-product
status: active
title: "TODO-02 -- Packaging and release of 2.0"
---

# TODO-02 -- Packaging and release of 2.0

> **Goal:** CarbonCD 2.0 is built, packaged and published from a tag, after it has read and written real CDs and DVDs on real drives.

> **Current state:** Measured 2026-11-16. There is no packaging. `Distribution/` holds the help file and language files by hand, with a `CarbonCD108/` copy of the 1.0.8 release's files. `.gitignore` ignores `*.exe`. Releases have been made by hand. Nothing is code signed. The repository's social preview is not set; the image is `design/brand/social-preview.png`.

## Inputs

- `design/SPEC.md` S0 (platform and media matrix): what the real-drive checks must cover.
- `CMakeLists.txt`, `.github/workflows/ci.yml` (D00 T01), `Distribution/`.
- -> XREF: D00 T01 §4 -- §1 here builds on it
- -> XREF: D04 T01 §4 -- §1 here builds on it
- -> XREF: D04 T01 §8 -- §1 here builds on it
- -> XREF: D03 T02 §17 -- §4 here builds on it
- -> XREF: D04 T01 §7 -- §4 here builds on it
- -> XREF: D00 T01 §10 -- §5 here builds on it
- -> XREF: D03 T02 §17 -- §6 here builds on it
- -> XREF: D03 T02 §17 -- §7 here builds on it

## Outcome

- `cmake --install` produces the folder a user runs from; a ZIP and an installer are built from it.
- Pushing a tag `v2.0.0` produces a draft GitHub release with both packages and their SHA-256 sums.
- A signed-off record shows each media type of SPEC S0 read and written on a real drive.

**Adjacency:** all=not-applicable (packaging and release mechanics; no record a user lists, exports or reverses inside the app)

**Adjacency rationale:** The installer's uninstall is the reverse of install and is exercised in §3. Real-drive results are recorded in `docs/release/` by §6 and §7.

## Implementation Order

| Order | Section | Deliverable | Depends On | Status |
| ----- | ------- | ----------- | ---------- | ------ |
| 1 | §1 | Install layout from the build | D00 T01 §4, D04 T01 §4, D04 T01 §8 | [ ] |
| 2 | §2 | Portable ZIP | §1 | [ ] |
| 3 | §3 | Installer | §1 | [ ] |
| 4 | §4 | Changelog, README and version 2.0.0 | D03 T02 §17, D04 T01 §7 | [ ] |
| 5 | §5 | Release workflow on tags | §2, §3, §4, D00 T01 §10 | [ ] |
| 6 | §6 | Real drive checks: CD | D03 T02 §17 | [ ] |
| 7 | §7 | Real drive checks: DVD | D03 T02 §17 | [ ] |
| 8 | §8 | Code signing | §3 | [ ] |
| 9 | §9 | Social preview image set on GitHub | -- | [ ] |

## 1. Install layout from the build

One command lays out exactly what ships.

**Phase:** 7
**Needs:** none
**Surface:** no

**Build order:**

1. Add install rules Done when: the folder holds the executable and everything it needs.
2. Check it runs from there Done when: the app starts from the installed folder with no other file.

- [ ] Add `install()` rules in `CMakeLists.txt` for `CarbonCD.exe`, `Distribution/*.cml`, `Distribution/CarbonCD-Help.html`, `LICENSE` and the font licences from `design/fonts/*/OFL.txt` as `licences/` Done when: `cmake --install build/x64-release --prefix Distribution/CarbonCD-2.0.0` produces the folder with those files and no others. Cheaper substitute: copying files by hand as `Distribution/CarbonCD108` was made.
- [ ] Write `install_manifest.json` listing each file with its SHA-256, and ignore `Distribution/CarbonCD-*/` in `.gitignore` Done when: the manifest lists every file in the folder and `git status` stays clean after an install.
- [ ] Add a CTest test starting the installed executable with `--render` into a temporary folder Done when: `ctest --preset x64-release -R installed_runs` passes, proving the installed folder is self-sufficient. Cheaper substitute: assuming it runs because the build tree's does.
- [ ] Commit: `release: install layout with manifest`

**Test checkpoint:** `ctest --preset x64-release -R installed_runs` passes; leaving the language files out of the install rules fails it.

## 2. Portable ZIP

The folder, zipped, with an empty settings file so it stays portable.

**Phase:** 7
**Needs:** none
**Surface:** no

**Build order:**

1. Package the folder Done when: the ZIP unpacks to a working portable CarbonCD.
2. Record its checksum Done when: a `.sha256` file sits beside it.

- [ ] Add `scripts/package.py zip` building `Distribution/CarbonCD-2.0.0-portable.zip` from the install folder plus an empty `CarbonCD.ini`, with fixed timestamps and sorted entries so the same input gives the same bytes Done when: running it twice gives identical SHA-256 sums. Cheaper substitute: zipping by hand in Explorer.
- [ ] Write `CarbonCD-2.0.0-portable.zip.sha256` Done when: `certutil -hashfile` of the ZIP equals the file's content.
- [ ] Commit: `release: reproducible portable ZIP`

**Test checkpoint:** `python scripts/package.py zip --check` exits 0; adding a stray file to the install folder changes the sum and the check names it.

## 3. Installer

A per-user or per-machine installer with a clean uninstall.

**Phase:** 7
**Needs:** windows-host
**Surface:** no

**Build order:**

1. Write the installer script Done when: it installs, starts and uninstalls cleanly.
2. Keep user settings on uninstall unless asked Done when: settings survive an upgrade.

- [ ] Add `installer/CarbonCD.iss` (Inno Setup 6): installs the install folder, Start menu shortcut, optional desktop shortcut, `.iso` and `.cue` `Open with` entries (not default handlers), per-user by default with an all-users choice, using `design/brand/icon.svg` renders and the About banner Done when: `iscc installer/CarbonCD.iss` writes `Distribution/CarbonCD-2.0.0-setup.exe`. Cheaper substitute: an MSI project nobody on the team maintains.
- [ ] Pin Inno Setup in a new `.conclave/toolchain-release.json` checked by `python scripts/toolchain-check.py --pins .conclave/toolchain-release.json` Done when: the check names `iscc` and its version, or `setup_required` with `winget install JRSoftware.InnoSetup`.
- [ ] Add `tests/system/Test-Installer.ps1`: silent install to a temporary folder (`/VERYSILENT /DIR=`), run the installed executable with `--render`, silent uninstall, assert the folder is gone and the per-user settings file is kept Done when: the script exits 0 and prints `install, run, uninstall: ok`. Cheaper substitute: testing the installer by hand once.
- [ ] Commit: `release: Inno Setup installer with tested uninstall`

**Test checkpoint:** `ctest --preset x64-release -L installer` passes; removing the uninstall of the Start menu shortcut leaves a file and fails it.

## 4. Changelog, README and version 2.0.0

What changed, said plainly, and the version set.

**Phase:** 7
**Needs:** none
**Surface:** no

**Build order:**

1. Write the changelog and update the README Done when: both describe 2.0 as built.
2. Set the version Done when: the executable reports 2.0.0.

- [ ] Add `CHANGELOG.md` with a 2.0.0 entry: 64-bit and Unicode, the new interface, DVD read, write, clone, erase and mastering, per-user settings, keyed language files, and what was removed (ASPI, skins, the CHM) with the reason for each Done when: every item of `design/SPEC.md` S0, including the removals, appears. Cheaper substitute: a list of commit subjects.
- [ ] Update `README.md`: what CarbonCD does, the media table of S0, how to install, how to build, with a screenshot from the review renders Done when: the README's build commands run as written from a clean clone.
- [ ] Confirm `project(CarbonCD VERSION 2.0.0)` and that About, file properties and the installer agree Done when: all three read `2.0.0`.
- [ ] Commit: `docs: changelog and README for 2.0.0`

**Test checkpoint:** `rg -n "1\.0\.8" README.md` prints nothing and `(Get-Item Bin\Release\CarbonCD.exe).VersionInfo.ProductVersion` prints `2.0.0`.

## 5. Release workflow on tags

A tag builds everything and drafts the release.

**Phase:** 7
**Needs:** none
**Surface:** no

**Build order:**

1. Add the workflow Done when: a tag produces a draft release with both packages.
2. Keep publishing a human step Done when: the release stays a draft until someone publishes it.

- [ ] Add `.github/workflows/release.yml` on tags `v*`: build, run every test, install, package the ZIP and the installer, compute SHA-256 sums, and create a draft GitHub release with the changelog entry as its notes Done when: pushing a test tag `v2.0.0-rc1` produces a draft release holding the ZIP, the installer and their sums. Cheaper substitute: building release packages on a developer's machine.
- [ ] Fail the workflow when the tag and `project(... VERSION ...)` differ Done when: a tag `v2.0.1` on a 2.0.0 tree fails with both versions named.
- [ ] Commit: `ci: release workflow producing a draft release`

**Test checkpoint:** `gh release view v2.0.0-rc1 --json isDraft,assets` shows `isDraft` true and four assets; a tag that does not match the version fails the run.

## 6. Real drive checks: CD

The simulated drive proves the logic. Real drives and real discs prove the product.

**Phase:** 99
**Needs:** windows-host, attended, operator
**Surface:** no

**Build order:**

1. Run each CD task on real hardware Done when: each row of the record is filled in.
2. File what fails Done when: each failure has a section or a finding.

- [ ] With a real writer, run and record in `docs/release/real-drive-cd.md` (drive model and firmware, disc brand, result, log file): read a pressed data CD to CUE, read an audio CD to CCD with subcode, read tracks to WAV, subcode analysis, write CUE in SAO and RAW+96 to CD-R with verify, erase CD-RW quick and full, clone a mixed-mode CD, master an audio and data CD Done when: every row has a result and a saved log under `docs/release/logs/`. Cheaper substitute: signing off from simulated runs.
- [ ] Compare the audio read against a second rip of the same disc by SHA-256 of the audio data (in `docs/release/real-drive-cd.md`) Done when: the two sums match, or the differing sectors are listed in the record.
- [ ] For each failure, add a section through `add-todo` or an entry in `todo/findings.md` with the log's lines Done when: no row reads failed without a reference beside it.
- [ ] Commit: `release: CD tasks checked on real drives`

**Test checkpoint:** `docs/release/real-drive-cd.md` has a result and a log for each of its ten rows; `rg -n "TODO|pending" docs/release/real-drive-cd.md` prints nothing.

## 7. Real drive checks: DVD

Every DVD type of SPEC S0, on a real drive.

**Phase:** 99
**Needs:** windows-host, attended, operator
**Surface:** no

**Build order:**

1. Run each DVD task on each media type Done when: each row of the record is filled in.
2. File what fails Done when: each failure has a section or a finding.

- [ ] With a real writer, run and record in `docs/release/real-drive-dvd.md`: read a pressed DVD-ROM and a dual-layer DVD-ROM to ISO; write an ISO with verify to DVD-R, DVD-RW, DVD+R, DVD+RW, DVD+R DL, DVD-R DL and DVD-RAM; erase DVD-RW quick and full; format DVD+RW; clone a DVD with one drive; master a data DVD holding a file over 4 GB Done when: every row has a result and a saved log under `docs/release/logs/`. Cheaper substitute: testing DVD+R only and assuming the rest.
- [ ] Read each written disc in a second drive, and mount it in Windows, comparing the files' SHA-256 with the sources (in `docs/release/real-drive-dvd.md`) Done when: each written disc's row records the second drive and `match`.
- [ ] Insert a CSS-protected DVD-Video disc and a Blu-ray disc and record the message shown (in `docs/release/real-drive-dvd.md`) Done when: the record quotes the `css` and `unsupported-media` messages of SPEC S13.
- [ ] For each failure, add a section through `add-todo` or an entry in `todo/findings.md` with the log's lines Done when: no row reads failed without a reference beside it.
- [ ] Commit: `release: DVD tasks checked on real drives`

**Test checkpoint:** `docs/release/real-drive-dvd.md` has a result and a log for each row; `rg -n "TODO|pending" docs/release/real-drive-dvd.md` prints nothing.

## 8. Code signing

Unsigned installers meet a SmartScreen warning. Signing needs a certificate, which is the owner's decision and the owner's credential.

**Phase:** 99
**Needs:** owner-decision, operator
**Surface:** no

**Build order:**

1. Decide and obtain the certificate Done when: the decision is recorded here.
2. Sign in the release workflow Done when: the released files carry a valid signature.

- [ ] Record the owner's decision in this section of `todo/04-product/TODO-02-release.md`: sign with which certificate (an OV or EV certificate, or Azure Trusted Signing), or ship unsigned, with the date Done when: the decision line is written here and the Need `owner-decision` is removed. Cheaper substitute: choosing for the owner.
- [ ] If signing: add the signing step to `.github/workflows/release.yml` using the credential from repository secrets, for the executable and the installer, with a timestamp server Done when: `Get-AuthenticodeSignature Distribution\CarbonCD-2.0.0-setup.exe` reports `Valid`. Cheaper substitute: committing a certificate or a password.
- [ ] Commit: `release: code signing decision and step`

**Test checkpoint:** `Get-AuthenticodeSignature Distribution\CarbonCD-2.0.0-setup.exe` reports `Valid`, or this section records the decision to ship unsigned.

## 9. Social preview image set on GitHub

GitHub has no API for the social preview; it is set by hand in the repository settings.

**Phase:** 99
**Needs:** operator
**Surface:** no

**Build order:**

1. Upload the image Done when: shared links to the repository show the banner.
2. Confirm it Done when: the Open Graph image of the repository page is the banner.

- [ ] In the repository's Settings, General, Social preview, upload `design/brand/social-preview.png` Done when: the settings page shows the Carbon Copy banner. Cheaper substitute: leaving GitHub's generated card.
- [ ] Confirm with `gh api repos/rizonesoft/CarbonCD --jq .html_url` and the page's `og:image` no longer pointing at `opengraph.githubassets.com` Done when: the `og:image` URL is a `repository-images.githubusercontent.com` address.
- [ ] Commit: `release: repository social preview set`

**Test checkpoint:** `curl -s https://github.com/rizonesoft/CarbonCD | findstr og:image` prints a `repository-images.githubusercontent.com` URL.

## Verification

- A tag produces a draft release with the portable ZIP, the installer and their SHA-256 sums.
- `docs/release/real-drive-cd.md` and `docs/release/real-drive-dvd.md` are complete, with every failure filed.
- `python scripts/todo-graph.py validate` and `python scripts/todo-graph.py plan --check` pass.

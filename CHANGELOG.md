# Changelog

All notable changes to this project. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [SemVer](https://semver.org/).

## [Unreleased]

### Added
- P0 reconnaissance: the Steam 1.2 build (app 7900 + 7910) picked as the
  target over the retail discs and the archive.org repack; `MoviesSE.exe`
  identified as MSVC 7.1 and packed. `docs/RECON.md`.
- All three executables unpacked headlessly with pcrecomp's new
  `drm/emu_unpack.py` (PECompact 2.x + the Steam2 ownership check).
  `docs/unpacking.md`. Needs pcrecomp `feat/emu-unpack`.
- RTTI recovery: 1,992 classes, 2,530 vtables, 9,227 virtual methods.
- `Setup.cmd` / `tools/setup.ps1`: the Quick start, up to the function catalog.

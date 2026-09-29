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
  `docs/unpacking.md`. Needs pcrecomp#5.
- RTTI recovery: 1,992 classes, 2,530 vtables, 9,227 virtual methods.
- First function catalog, scored against IDA: F1 57.9% (22,355 invented
  starts from the merged section). Recorded, not yet fixed.
- `run_lift.py`: closure-limited lift from the OEP over pcrecomp's lift32,
  with extents by branch walking and every RTTI vtable slot as an entry.
- The host: 32-bit, on pcrecomp's new `runtime/native32` (pcrecomp#7), with
  `--headless`. The lifted CRT runs from the OEP. `docs/host.md`.
- `Setup.cmd` / `tools/setup.ps1`: the Quick start, up to the function catalog.

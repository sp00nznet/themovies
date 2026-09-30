# Changelog

All notable changes to this project. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [SemVer](https://semver.org/).

## [Unreleased]

### Added
- Scripted input for headless runs: `--move x,y@s`, `--click x,y@s`,
  `--key vk@s`. DirectInput 8 devices and the Win32 cursor/key calls answer
  from the script. The game's cursor is steered closed-loop from its own
  accumulator. It now plays from the main menu into a new studio.

### Fixed
- Moving to pcrecomp `main` pinned the cursor to the top-left corner: every
  x87 compare (`fnstsw ax; test ah, N; jp`) in a function with an unresolved
  indirect jump took one fixed branch, 274 sites. Fixed in pcrecomp #22; lift
  with it.
- DirectInput never started: `DirectInput8Create` got the guest image base
  (0x400000) as its `HINSTANCE` and failed with `E_INVALIDARG`. The host passes
  its own.

### Fixed
- The main-menu backdrop and the intro videos (Activision logo, title) were
  black or missing. The lift now uses lift32's precise carry: 64-bit adds and
  subtracts had a wrong high word, so DirectShow judged every video frame late
  and dropped it (DoRenderSample 4 -> 1,508 calls in 100 s). And a headless
  Yes/No/Cancel box answers No rather than OK, which is none of its buttons.

### Added
- `--probe VA`: count indirect calls to a VA and show the first five.
- A conformance milestone for video frames rendered (7/7).
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
- **The recompiled game boots and renders its Lionhead intro**, headless:
  CRT, WinMain, localized text, hidden window, D3D9 device, worker threads,
  6,000+ frames in four minutes. `docs/bringup.md` logs each wall and fix.
- `--headless --record out.mp4 --frames N`: every frame read back and piped
  to ffmpeg; the D3D9 device is forced windowed so no display mode changes.
- Guest identity shims (`GetModuleFileNameA`, `GetModuleHandleA(NULL)`,
  `GetCommandLineA`) and SIMD hiding (`CPUID`, `IsProcessorFeaturePresent`;
  `--simd` to allow) in the host.
- `tools/conformance.py` and `conformance.json`: 6/6 boot milestones, lift
  health (0 errors, 10 unresolvable ITAILs); fails on regression.
- `Setup.cmd` / `tools/setup.ps1`: the Quick start, up to the function catalog.

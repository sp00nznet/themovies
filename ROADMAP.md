# Roadmap

## Next

1. **Fix the catalog before lifting it.** The first pass scores F1 57.9% against
   IDA, and most of the error is 22,355 *invented* starts: data decoded as
   code, because the packer merged `.rdata` and `.data` into one executable
   section. Either restore the section split from the original layout, or
   drive `disasm32` from IDA's code map (`ida_export.py`). Re-score after.
2. **Check the unpacked executable natively.** Run `work\MoviesSE.unpacked.exe`
   from the game folder on a console session under offstage and record the
   main menu. That proves the dump is whole, and it is the reference every
   later run is compared against.
3. **Grow the closure to `WinMain`.** The lift driver and 32-bit host work
   (docs/host.md). The lifted CRT runs until it leaves the closure; widen it
   with the fixed catalog until the CRT reaches `WinMain` and then
   `CreateWindowExA`.
4. **An offscreen present.** D3D9 already reaches real Windows through the
   native bridge. What `--headless` needs is a window that never shows and
   frames captured to ffmpeg, so a run over RDP can record.
5. **Upstream the lift driver's closure and extent walk** into pcrecomp's
   `generate.py`: `run_lift.py` is now the second copy of forcecommander's.
6. **Headless mode**, `--headless --record out.mp4`, as soon as there is a frame
   to record, and the conformance harness once there is a lift to measure.

## Deferred

- `Movies.exe` (the base game without S&E) and `StarMaker.exe`. They unpack
  already; `MoviesSE.exe` contains everything `Movies.exe` does.
- Retail DVD installs. Their executables predate the 1.2 patch and use their own
  protection. Supporting them means a second unpack path for a version that is
  strictly older.

## Out of scope

- The Movies website features (film upload, online charts): the servers are gone.
- Distributing the game, its data, the unpacked executables or the lifted C.

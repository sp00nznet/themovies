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
3. **Lift** with `lift32` through a closure-limited `run_lift.py` from the OEP,
   as in forcecommander, so each run says which function to lift next.
4. **Host runtime** on `recomp32`: the 330-import bridge, then `GetProcAddress`
   handing out real D3D9, DInput8 and DirectSound (or OpenAL). The first
   milestone is the lifted CRT reaching `WinMain`.
5. **Headless mode**, `--headless --record out.mp4`, as soon as there is a frame
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

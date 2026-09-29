# The host

`build/themovies.exe` is the program the lifted C runs inside. It is a
**32-bit** process built with MSVC x86 on pcrecomp's `runtime/native32`
(pcrecomp#7), and the game-specific part is one file, `src/runtime/host.c`.

## Why 32-bit

The Movies statically links its C runtime, so the imports are Win32 only:
330 functions from 11 DLLs, plus whatever `LoadLibrary` brings in later:
D3D9, DInput8, DirectSound or OpenAL, WMVCore. A 64-bit host would need a
hand-written shim for each of those, because every pointer, handle and struct
the game passes is 32 bits wide. Force Commander's 64-bit host carries 11,000
lines of that ([STL-GATE.md](https://github.com/sp00nznet/forcecommander/blob/main/docs/STL-GATE.md)
makes the case for a 32-bit host).

In a 32-bit host the guest image sits 1:1 at `0x00400000` and its structs
already have the layout Windows expects. So:

- **Imports.** The host fills the IAT with real function addresses, as the
  Windows loader would. `call [slot]` lifts to `RECOMP_ICALL(MEM32(slot))`,
  which lands in native32's bridge. The bridge copies the argument slots to the
  real stack and reads the stdcall/cdecl purge from how far `esp` moved. There
  is no argument-count table.
- **`GetProcAddress` and COM.** Same path: any address outside the guest image
  is native. This is how D3D9's vtables will be called, with no per-interface
  code.
- **Callbacks.** The guest's `.text` is mapped non-executable. When Windows
  calls a guest WndProc or thread start, the fetch faults and native32 moves
  it onto the lifted function.

The same mechanism was proven in gunman first; pcrecomp#7 moves it into the
toolkit instead of copying it a third time.

## Headless

This machine is often driven over RDP from a phone, where a window lands on
the phone's screen. `--headless` binds three hand shims in front of the real
imports:

- `MessageBoxA`/`W` print to stderr and return `IDOK`.
- `CreateWindowExA` prints what the game asked for and stops the run with exit
  code 5.

That is the stand-in until there is an offscreen present for the rule-10
`--headless --record out.mp4`.

## Running it

```
build\themovies.exe --headless              # dry run: map and bind, execute nothing
build\themovies.exe --headless --run --watchdog 60
```

The first run, with a 300-function closure from the OEP (pass-1 catalog):

```
[bind] 0x00400000: 327 native, 3 shimmed, 0 unresolved
The Movies recomp host
  lifted functions in dispatch: 53834
  mapped work\MoviesSE.unpacked.exe: 0x00400000-0x01106000
  entering 0x00AD2451

ITAIL: unresolved VA 0x00AD583F from 0x00AD578C

[not-lifted] sub_00ACD512  (called from 0x00ACD58D)
  Widen the closure:  py -3 run_lift.py --roots 0x00ACD512  (or --max N, or --all)
last indirect calls (newest first):
  0x761041F0  from 0x00ADA01F  kernel32.dll!GetCurrentThreadId
  0x7610B2E0  from 0x00ADA01F
  0x00AD583F  from 0x00AD578C
  0x776BF650  from 0x00AD578C  kernel32.dll!HeapAlloc
  0x7610D300  from 0x00ADA01F
  0x76106780  from 0x00ADA01F  kernel32.dll!GetProcAddress
```

The lifted CRT runs, calls the real `GetProcAddress` (its `FlsAlloc` probe),
`HeapAlloc` and `GetCurrentThreadId` through the bridge, and stops exactly at
the edge of what was lifted. The unnamed native addresses are the
`GetProcAddress` results. The `ITAIL` is a jump into a function the pass-1
catalog did not know.

Exits go through `TerminateProcess`, not `ExitProcess`. `ExitProcess` runs the
guest CRT's FLS callbacks, which are lifted code, and the first run printed
its `not-lifted` report twice that way.

## Exit codes

| Code | Meaning |
|---:|---|
| 2 | reached a function outside the lifted closure (`[not-lifted]` names it) |
| 3 | fault (the report names the lifted function and the last native call) |
| 4 | `--watchdog` expired |
| 5 | `--headless` stopped at window creation |

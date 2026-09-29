# Bring-up log

Each wall the boot hit, what it looked like, and what fixed it, newest last.
Most were generic lifter or catalog defects, fixed in pcrecomp rather than
here; the PR is named on each. `tools/conformance.py` scores how far the boot
gets (README, Status).

## 1. The CRT's `calloc` lost its epilogue — pcrecomp#11

```
ITAIL: unresolved VA 0x00AD583F from 0x00AD578C
```

`calloc` calls its own `__finally` block (`call 0xad5836`, `_unlock`) in the
middle of its body, then jumps past it to the exit. The block is a catalogued
entry, the catalog clamps `calloc` there, and the lift driver's branch walk was
bounded by that clamp. The walk is now capped by reach instead.

## 2. `memcpy`'s switch arms were outside the body — pcrecomp#11

```
ITAIL: unresolved VA 0x00ACDE33 from 0x00ACDCF0
```

`jmp dword ptr [edx*4 + 0xacde3c]`: nothing walked through the table, so the
arms past it were never lifted and the linear sweep decoded the table bytes as
`pushal`. The walk now follows `jmp [reg*4 + table]`, and the lift decodes
exactly the instructions the walk reached. `memmove` then showed the other
table shape: slot 0 unused, overlapping the preceding `jmp`'s bytes, so reading
had to skip leading garbage.

## 3. `parse_cmdline` spun forever — pcrecomp#11

```
[watchdog] 180 s: in sub_00AE70CA, last native call (none), 86 indirect calls
```

The catalog has three false starts inside `parse_cmdline`. The walk stopped at
the first when code fell through into it, which left `and [ebp-4], 0` followed
directly by `dec eax; jmp 0xae7143`: an infinite loop. Real code only falls
into another function after a `call` (to something noreturn), so that is now
the only fallthrough that ends a path.

## 4. The game looked for its data next to the host — `src/runtime/host.c`

```
[native] kernel32.dll!FindFirstFileA (...) a0="data\text\EN-UK\*.*" -> FFFFFFFF
[native] kernel32.dll!FindFirstFileA (...) a0="Data\Animations\High\fe_m_white_joe.anm" -> FFFFFFFF   (x thousands)
```

`GetModuleFileNameA(NULL)` returned `build\themovies.exe`, so the game set its
data root to `build\`. Nothing was found, and a file-resolver fallback recursed
until the guest stack ran out. The host now answers `GetModuleFileNameA`,
`GetModuleHandleA(NULL)` (the guest image, `0x00400000`) and `GetCommandLineA`
as `game\MoviesSE.exe` would see them.

## 5. `_stat` closed a find handle twice — pcrecomp#11

```
=== fault 0xC0000005 at 0x776E9F56 ===
  write of 0x00000014 (null/low)
  in lifted sub_00AD3A68, last native call kernel32.dll!FindClose
```

`_stat` jumps to its own cleanup at `0x00AD3D45`, which the catalog lists as an
entry, so the walk treated the jump as a tail call and the cleanup ran as a
separate function after the first had already run. A jump to an entry that lies
inside the span the body already reaches is now internal.

## 6. `strchr` never returned a match — pcrecomp#11

```
ITAIL: unresolved VA 0x00ACECC0 from 0x00ACECD0
```

Hand-written: the found-exit is 16 bytes *below* the entry, after `int3`
padding. A backward target within 256 bytes that follows padding is now walked
as part of the function. Without the padding it would be a split, where the
code lands in the middle of the function before.

After that, every direct branch that leaves a body toward an address nothing
catalogued becomes an entry itself (`run_lift.py`, in rounds). Unresolvable
ITAIL labels in the whole lift: **2,838 → 10**, and none of the 10 is in a
function IDA also has.

## 7. The window, and a device that must not go fullscreen — `host.c`

```
[headless] CreateWindowExA("The Movies", 1024x768) from sub_00542B20 -> hidden hwnd 034E0E5E
[headless] CreateDevice adapter 0 type 1 flags 0x44: 1024x768 fmt 25 x1 ... FULLSCREEN -> windowed
[headless] CreateDevice -> 0x8876086A
```

The game asks for an exclusive-fullscreen 16-bit (`A1R5G5B5`) device, which
would change the display mode of whatever session is running it. `--headless`
wraps `Direct3DCreate9` (the game gets it from `GetProcAddress`) and patches
`IDirect3D9::CreateDevice` to force windowed mode. A windowed back buffer has to
match the desktop, so the format becomes `D3DFMT_UNKNOWN`. Without that, the
call fails with `D3DERR_NOTAVAILABLE`:

```
[headless] CreateDevice -> 0x00000000
```

## 8. The first worker thread started in code nobody lifted — pcrecomp#13

```
=== fault 0xC0000005 at 0x00ACF974 ===
  execute of 0x00ACF974 (inside the guest image)
```

`_beginthreadex` passes `_threadstartex` to `CreateThread` as `push offset`.
disasm32 harvests such immediates, but it skipped any address already inside a
decoded body. `__endthreadex` ends in `ExitThread`, which disasm32 did not know
never returns, so its body ran on into `_threadstartex`. A covered callback
immediate is now an alias entry, the same as a jump into a body.

## 9. A directly called function the catalog dropped — pcrecomp#14

```
ICALL: unresolved VA 0x00C10170 from 0x00C0F110
=== fault 0xC0000005 ... write of 0x76120788
```

`call 0xc10170` is a direct call, and IDA has the function. But a data-scan
hit at `0x00C1016D`, inside the jump table that ends the previous function,
decoded over it, and disasm32 dropped the real entry as "mid-instruction". The
target of a decoded `call` is now never dropped. `run_lift.py` also makes
every direct call target an entry, so a catalog gap cannot become an
unresolved call again.

## 10. `memcpy`, again: a table indexed downward — pcrecomp#11

```
ITAIL: unresolved VA 0x00ACDFAC from 0x00ACDCF0
```

The backward copy does `jmp [ecx*4 + 0xacdf88]` with `ecx` from -3 to 0, so
the arms sit below the displacement. The table reader now reads down as well
(memcpy: 214 -> 231 arms).

## 11. Worker threads overflowed their stacks — pcrecomp#7

The process ended with exit code 3 and printed nothing. A guest thread runs
lifted code on its native stack, and a lifted frame is several times the
original's, so the stack size the game asked for was far too small. The report
itself then overflowed too. native32 now gives every guest thread a 16 MB
reserve and a stack guarantee for the fault handler, and the host's fault
report is written with `WriteFile` from a static buffer.

## 12. `fucompp` did nothing — pcrecomp#15

```
[native] kernel32.dll!DebugBreak (...) from sub_00C3A300
```

`sub_00C3A300` asserts "Sensaura is reporting that we are behond the end of
the sample buffer!!" when a position/length ratio equals 1.0, tested with
`fucompp; fnstsw ax; test ah, 44h; jp`. lift32 had no `fucompp`: no compare,
and two x87 slots leaked per call, 3,244 times in this binary. The same count
turned up 13,700 unimplemented SSE/SSE2/3DNow! sites, all in code that picks
its path by `CPUID` and `IsProcessorFeaturePresent`. The host now reports no
SSE or 3DNow!, so that code takes its x87 paths (`--simd` turns them back on).

## 13. The first frames

With the above in place the boot runs straight through: localized text, the
hidden window, the D3D9 device, worker threads, and the render loop.

```
[headless] CreateDevice -> 0x00000000
[headless] frame 1 presented
[record] 1024x768 format 22 -> G:\recomp\pc\themovies\work\rec\first.mp4
[headless] frame 10 presented
[headless] frame 100 presented
[record] 600 frames -> G:\recomp\pc\themovies\work\rec\first.mp4
```

The frames are the Lionhead intro (README, Screenshots). Left to run, it
presents 6,000+ frames in four minutes with no fault.

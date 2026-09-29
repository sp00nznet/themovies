# What is inside The Movies

P0/P1 reconnaissance. Everything here came out of the shipped binaries in the
Steam build (see [unpacking.md](unpacking.md) for how they were opened).
Nothing was guessed. Numbers are from the runs recorded in the README.

---

## Which build, and why that one

| Source | What it is | Used? |
|---|---|---|
| Steam app 7900 / 7910, builds 251862 + 251864 | The final patched game: `version.inf` says **ExtVersion 1.2**, *The Movies Stunts & Effects*, QA 4135 | **Yes** |
| Retail DVD (2005) and S&E DVD (2006) | 1.0 / expansion executables before the 1.2 patch | Not yet |
| MagiPack repack (archive.org `the-movies-expansion`) | Installer-compressed, cracked executable | No |

The Steam depot is the one to build on. It is the last version Lionhead
shipped, it has the expansion, its executables are byte-for-byte what Valve
distributed, and its only protection is a packer that
[unpacks headlessly](unpacking.md). A repack's executable has been modified by
whoever cracked it, and a retail disc's needs its own DRM dealt with first.

The Movies was on Steam from **2007-06-15** (`steam_release_date` 1181944800
in the app info). It is not an orphan in the legal sense: the copyright is
Lionhead's / Activision's, and you need your own copy.

## The binaries

| Binary | Size (packed) | Built | Linker | Role |
|---|---:|---|---|---|
| `MoviesSE.exe` | 3,120,128 | 2006-04-27 | 7.10 | **the target**: the game with Stunts & Effects |
| `Movies.exe` | 2,953,216 | 2005-09-25 | 7.10 | the base game without the expansion |
| `StarMaker.exe` | 1,070,080 | 2006-03-28 | 7.10 | the StarMaker character tool |

`TestAppOne.exe` and `TestAppTwo.exe` are byte-identical copies of
`MoviesSE.exe` and `StarMaker.exe` (same SHA-1). The install ships **no DLLs
at all**: everything beyond the Win32 core is loaded at run time.

MSVC 7.1 (Visual Studio .NET 2003). The unpacked `MoviesSE.exe` is a 13.6 MB
image whose entry point is the stock CRT one:

```
ad2451 push 0x60
ad2453 push 0xd7e098
ad2458 call 0xad7c10          ; __SEH_prolog
ad245d mov edi, 0x94
ad2462 mov eax, edi
ad2464 call 0xad10e0          ; _alloca_probe
ad2469 mov dword ptr [ebp - 0x18], esp
...
ad2471 call dword ptr [0xd16254]   ; GetVersionExA
```

## The import table understates the runtime surface

The rebuilt import table is 330 functions from 11 DLLs: `kernel32` (191),
`user32` (42), `winmm` (32), `ws2_32` (29), `advapi32` (10), `ole32` (9),
`msacm32` (7), `oleaut32` (4), `gdi32` (3), `shell32` (2), `shfolder` (1).

There is no Direct3D in it. The strings name what `LoadLibrary` brings in:

| DLL | For |
|---|---|
| `d3d9.dll` (`d3d9d.dll` for debug) | the renderer; D3DX is statically linked (`#File created by Microsoft (R) D3DX library`) |
| `dinput8.dll` | input |
| `dsound.dll`, `OpenAL32.dll` / `wrap_oal.dll`, `ksuser.dll` | audio: DirectSound or OpenAL |
| `WMVCore.dll` | exporting the player's films as `.wmv` |
| `wininet.dll`, `Iphlpapi.dll`, `Secur32.dll` | uploading films to the (long dead) Movies website |
| `imagehlp.dll` | crash stack traces (`IStackTrace`) |
| `mscoree.dll`, `version.dll`, `imm32.dll` | runtime checks, version queries, IME |

So the host has to provide D3D9, DInput8 and an audio path on top of the
imports. None of them is in the import table, so the bridge has to catch them
at `GetProcAddress`.

## RTTI was left on

`tools/cpp/rtti.py` over the unpacked image:

```
[*] type descriptors : 2,049
[*] complete object locators : 2,530
[*] vtables          : 2,530
[*] classes          : 1,992
[*] virtual methods  : 9,227  (6,119 attributable to one class)
```

By outermost namespace:

| Namespace | Classes | What it is |
|---|---:|---|
| `TM` | 1,312 | **The Movies** itself: facilities, staff, the film editor, awards, UI |
| `MV` | 204 | the engine layer: `Object`, particles, the intrusive `InList<T>` |
| *(templates)* | 260 | instantiations whose outer name is a template |
| *(global)* | 134 | award factories, DirectShow filters (`CWavDestFilter`), camera items |
| `std` | 24 | Dinkumware STL |
| `LSWF` | 18 | Lionhead's SWF (Flash) player: the UI is Flash |
| `FS` | 11 | file system |
| `boost` | 5 | boost |
| `LHA` | 3 | Lionhead audio (`CCaptureOutput`, `CDriverReporter`) |
| `Perforce` | 3 | yes, Perforce |

Surviving `__FILE__` strings name more libraries than RTTI does: `PK*`
(allocators, threads, red-black trees, data streaming), `LLACoda*` (the audio
channel system), `LHACodecs*` (ACM and Ogg Vorbis), `MapAnalysisImp*`, and the
`C*` engine core (`CEngine.cpp`, `CInstanceManager.cpp`, `CGameFileManager.cpp`).
libpng 1.0.5 and a JPEG decoder are linked in.

## The data

2.5 GB under `Data\`. The bulk is 11 `.pak` archives (textures, costumes,
meshes and scenes, the S&E add-on, `PATCH1.pak`), plus loose `.ogg`+`.cue` audio,
`.lug` sound banks, `.lps` lip-sync tracks and `.wmv` intro movies. None of it
needs decoding for a recompilation: the recompiled game reads it the way the
original does.

## The first catalog

`disasm32.py` seeded with the 9,227 RTTI methods, 143 minutes:

```
[*] Successfully disassembled 77517 functions (7 discovery rounds)
[*] Dropped 9871 entries that are not instruction boundaries
[*] Clamped 13103 function extents to the next function start
[*] Functions: 67646  (thunks=117, leaves=35130)
[*] Instructions: 11,950,614
[*] Byte coverage: 8,705,899 / 13,619,200 (63.9% of code range)
```

Scored against IDA 9.1 (`ida_funcs.py`, 38 minutes, 54,665 functions of which
2,881 are FLIRT-identified library code):

```
  true positives      33,614
  false positives     27,888
      split            5,533  (inside a known function)
      invented        22,355  (outside every known function)
  false negatives     21,051
  precision   54.66%
  recall      61.49%
  F1          57.87%
  function ends: exact 29,711 (88.39%)
```

IDA is a second opinion here, not ground truth: there are no symbols. But the
shape of the error is clear. Most of the false positives are **invented**,
which means starts outside any function IDA found: data decoded as code. The unpacked
image has one RWX section spanning code, read-only data and data, so
`disasm32` has no section boundary to stop it. Where it does find a function,
it gets the end right 88% of the time. This catalog is not lifted as it stands
([ROADMAP.md](../ROADMAP.md)).

## Where this points

- The target is one 32-bit MSVC 7.1 executable, so the pipeline is pcrecomp's
  32-bit path: `disasm32` seeded from RTTI, then `lift32` and a `recomp32`
  host, the same shape as Force Commander.
- It is C++-heavy with RTTI on, which makes it the best-named Lionhead target
  in the collection. Black & White (`bw`) had to recover its 569 types by hand.
- The renderer is D3D9 behind `LoadLibrary`, so the first host-side milestone
  is a `GetProcAddress` bridge that hands the lifted code real D3D9, DInput8 and
  DirectSound objects.

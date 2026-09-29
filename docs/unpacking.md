# Unpacking the Steam executables

All three executables in the Steam build are packed, and the packer is also the
DRM. This is how they come apart without running the game. The tool is
pcrecomp's [`tools/drm/emu_unpack.py`](https://github.com/sp00nznet/pcrecomp/blob/main/tools/drm/emu_unpack.py);
the reasoning for its design lives in its docstring. This page is what The
Movies specifically showed.

## What it looks like

`analyze_sections.py` on the file as shipped:

```
name               VA     vsize   rawsize  entropy  flags
------------------------------------------------------------------------------
.text      0x00001000  13619200   3095040   7.9972  EXEC HIGH-ENTROPY ENTRY-POINT
.rsrc      0x00CFE000     24576     24064   5.0526  EXEC

  Protection / packing indicators:
    - entry section '.text' is high-entropy executable (encrypted/packed loader)
```

One section holding 13 MB of image in 3 MB of file at entropy 8.0, and an
import table of exactly one function per DLL (`LoadLibraryA`, `GetProcAddress`,
`VirtualAlloc`, `VirtualFree`, then one token import each from `user32`,
`gdi32` and the rest, so the loader maps them). The entry is:

```
401000 mov eax, 0x1103b9c
401005 jmp eax
```

and the stub it jumps to opens with PECompact 2.x's delta setup:

```
1103b9c mov eax, 0xf10ffffd
...
1103ba7 lea ebx, [eax + 0x10003b7b]
```

## The Steam part

The first run of the emulator stopped here:

```
stopped before the OEP: unhandled import steam.dll!SteamStartup, called from 0x60001644
```

`0x60001644` is in memory the stub allocated for itself, so this is a PECompact
loader plugin, not the game. It is Valve's Steam2-era wrapper (2004-2008): load
`steam.dll`, `SteamStartup`, `SteamIsAppSubscribed(appid)`, `FreeLibrary`, and
only then jump to the game. All three calls are cdecl. Answering "started,
subscribed, not pending" is enough, and the tool now does that for any
Steam2 title.

That is why the file ships with no `steam.dll`: Steam's own client provided it.
It is also why "run it and dump the memory" was not an option here. That
needs a Steam login that owns the app, and a window, and this machine is often
driven over RDP.

## The IAT is not the only table

The first rebuilt import table had 685 entries for 336 distinct imports. The
image holds the resolved table **twice**: the real IAT at `0x00D16000`, the
start of what was `.rdata`, and a second complete copy at `0x00E4AA5C`. It
also holds 23 single cached function pointers (the CRT keeps some
`GetProcAddress` results in globals). The IAT is the block the code calls
through (`call dword ptr [0xd16254]` at the OEP is `GetVersionExA`), and
choosing by that gives:

```
OEP 0x00AD2451; 330 imports in 11 IAT runs from 11 DLLs; 336 thunks handed out
```

330 game imports, plus the 6 the stub resolved for itself, accounts for all
336.

## Results

| Executable | OEP | Imports | Unpacked image |
|---|---|---:|---:|
| `MoviesSE.exe` | `0x00AD2451` | 330 | 13,651,968 |
| `Movies.exe` | `0x00A309D1` | 329 | 12,869,632 |
| `StarMaker.exe` | `0x004C2213` | 230 | 3,688,960 |

`StarMaker.exe`'s OEP is its old entry address: its stub writes the original
bytes back over its own `mov eax / jmp eax` before jumping there. All three
land on the MSVC 7.1 CRT's `push 60h / push scopetable / call __SEH_prolog`.
The output is deterministic: two runs produce the same MD5.

What the unpacked file is **not** yet: a checked native run. That test (run
`work\MoviesSE.unpacked.exe` in the game folder and see the menu) needs a
window, so it waits for a console session under offstage. The disassembly and
RTTI recovery over it are the evidence so far that the dump is whole.

The unpacked image keeps the packer's merged section layout (code, read-only
data and data all in one RWX `.text`). pcrecomp's 32-bit tools do not need the
sections split.

# Contributing to The Movies recomp

This repository is the game-specific half: the lift driver, the host, the
setup script and the bring-up notes. The recompiler itself is
[pcrecomp](https://github.com/sp00nznet/pcrecomp), and most of what this game
has found so far was fixed there. Contributions to either are welcome.

## Where the gaps are

| Area | What is missing | Difficulty |
|---|---|---|
| **The main menu's backdrop** | Black where the original draws a 3D scene ([ROADMAP.md](ROADMAP.md)). | Medium: a diagnosis first |
| **Scripted input** | `--click x,y@s` / `--key vk@s` for headless runs, so recordings and conformance can go past the menu. | Small |
| **SIMD in lift32** | ~13,700 SSE/SSE2/3DNow! sites are unimplemented; the host hides SIMD so the game takes x87 paths. That work belongs in pcrecomp. | Large |
| **A native reference run** | The unpacked executable run on real Windows and recorded, as ground truth for frame comparisons. | Small |

## Ground rules

**No game files, ever.** No executables, DLLs, `.pak`s, movies or audio. No
unpacked or lifted code either: `work/` and `src/recomp/gen/` are generated
from your own copy and gitignored, and must stay that way. Screenshots of
what the recompiled game renders are fine.

**Where your code comes from.** Contributions must be your own work or under
an MIT-compatible licence. The easy mistake is porting a fix you saw in a GPL
project (Wine, DOSBox, a GPL decompilation), which relicenses it by accident
and is very hard to untangle later. If you port anything, say where it came
from in the pull request.

**Generic fixes go upstream.** If the bug is in the lifter, the catalog or the
runtime, it belongs in pcrecomp as its own PR, and this repo only picks it up.
Game-specific values and shims stay here.

**Say what you measured.** Show the boot before and after
(`py -3 tools/conformance.py`), and the log line or frame that changed. Every
wall so far has an entry in [docs/bringup.md](docs/bringup.md) with the real
output; add one for yours.

**AI-assisted contributions** are welcome, provided a human understood and
verified the change.

## Before you open a pull request

With your own copy set up (README, Getting Started):

```
py -3 run_lift.py --all
build.cmd
py -3 tools\conformance.py
```

`conformance.py` fails on a regression against `conformance.json`. Update the
baseline (`--update`) only when the change is meant to move it, and say so.

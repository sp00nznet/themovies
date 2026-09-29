# Roadmap

## Next

1. **The main menu's black backdrop.** The original shows a 3D scene behind the
   menu; find whether it is not rendered, rendered and lost, or waiting on
   something (streaming, a thread, a timer).
2. **Input.** Scripted clicks and keys for headless runs (`--click x,y@s`,
   `--key vk@s`, gunman's shape) so a recording can go past the menu, and
   conformance milestones past it (a new game, the studio lot).
3. **Upstreaming.** pcrecomp #7, #10 to #15 are open; once they merge, drop the
   integration tree from the README. forcecommander and prey carry their own
   copies of the extent walk, now fixed in `generate.py`.
4. **Native reference run.** Run `work\MoviesSE.unpacked.exe` itself on a
   console session under offstage and record the same 100 seconds, as the
   ground truth to compare frames against.
5. **SIMD.** The lift runs with SSE/3DNow! hidden from the guest. Implementing
   the ~13,700 SIMD sites in lift32 (or lifting with lift32_cpu, which has
   them) would let `--simd` run the paths the game uses on real hardware.

## Deferred

- `Movies.exe` (the base game without S&E) and `StarMaker.exe`. They unpack
  already; `MoviesSE.exe` contains everything `Movies.exe` does.
- Retail DVD installs. Their executables predate the 1.2 patch and use their own
  protection. Supporting them means a second unpack path for a version that is
  strictly older.

## Out of scope

- The Movies website features (film upload, online charts): the servers are gone.
- Distributing the game, its data, the unpacked executables or the lifted C.

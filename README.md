<div align="center">

<img src="docs/logo-professor-dazai.png" width="200" alt="Professor Dazai">

# Paldea Recomp

**Pokémon Scarlet and Violet 4.0.0, running natively on Windows PC. No emulator. Static recompilation. 60 fps.**

**English** · [Português (Brasil)](README.pt-BR.md) · **Version 1.0**

Published by **Professor Dazai**

</div>

> [!IMPORTANT]
> This repository ships **no game files, no keys and no Nintendo code**. You bring your own *Pokémon Scarlet* or
> *Pokémon Violet* 4.0.0 files; the game code is generated and compiled on your own machine from them. There are no
> locks or checks in the tools: what you do with them is up to you, on your own account.

## What is it

Paldea Recomp takes the Switch executable of Pokémon Scarlet / Violet, translates its ARM64 code to C ahead of time
(static recompilation), and builds a normal Windows program out of it. The Switch operating system is replaced by a
small runtime, and the graphics API (NVN) is translated to Vulkan. The game then runs like any other PC game:
no emulator, no per-frame translation.

## Where it comes from (credits)

This is a combination of two open projects, plus our own work on top:

| Part | Source |
|---|---|
| Base: the recompiler, runtime, Switch OS layer, NVN to Vulkan renderer, launcher | **ZA Recomp v0.1.27** (MIT, copyright Peiikai; the release used was Hexveil404's). Kept as the first commit of this repo. Its own README is in [docs/](docs/README-ZA-Recomp.md). |
| Pokémon Violet 4.0.0 support, game patches, memory services | **PokeV-Recomp** by [didslima](https://github.com/didslima) (with community contributions by Atterax and others). |
| Scarlet, 4K Shadows, the fixes below, this packaging | This project, by **Professor Dazai**. |

Community mods that Paldea Recomp can use (**not included here**, download them from GameBanana): Brazilian
Portuguese translation, Improved Graphic Quality, 4K Shadows, Shiny Lock Removal, Dark Void Accuracy Fix, No Trade
Evolution. Each belongs to its own author.

## What we did (v1.0)

1. **Merged the two bases.** Violet from PokeV-Recomp ran on top of the newer ZA Recomp runtime (launcher, mod
   loader, better renderer, DLSS/FSR/XeSS upscalers, settings UI).
2. **Pokémon Scarlet support.** Scarlet's executable differs from Violet's by only two words, so it reuses the same
   recompiled system libraries. Its build ID (`421c5411b487eb4d`) is recognised by the launcher, the version tables
   and the patch system; the game library is built for it separately.
3. **Second game module (`subsdk0`).** The game's second executable module was never loaded; it is now mapped,
   relocated and initialised, and its functions are part of the dispatch tables.
4. **Fixed a startup crash** (null thread pointer): every thread now gets a correctly set up thread-local block.
5. **System allocator and services.** Added the allocator the game expects (`malloc`/`free`/`memalign` family, used
   until the game's own heap is ready) and the Bluetooth manager and error-context services the game talks to at boot.
6. **4K Shadows works.** The mod asks for a 128 MB shadow map, but the game's GPU pool is a fixed 268 MB and was
   already 145 MB full, so it crashed. A new patch raises the pool to 400 MB, and the runtime reports the extra
   application memory (`LARECOMP_ARENA_EXTRA_MB`, default 192).
7. **Stable 60 fps with correct game speed.** Frame clock, slot durations and frame pacing patches keep the world,
   animations and walking at normal speed even when a frame takes longer than 16.7 ms.
8. **Widescreen.** Hor+ for ultrawide, Vert+ for tall windows.
9. **Mod-aware library id.** The game library id includes a hash of the enabled code mods (`.pchtxt`), so each mod
   combination gets its own matching library.

## What improved

- Violet and Scarlet start and reach the game's own first-run screen natively (no emulator).
- Measured 60.0 fps, steady, on the test machine with Improved Graphic Quality and 4K
  Shadows on, for the Violet run (98 s) and the Scarlet run (about 2 min).
- Violet with 4K Shadows no longer crashes.
- Resolution follows the window, with FSR 1 and SMAA available.

## Known limits (honest list)

- Only short runs were tested: the first-run screens and about two minutes each. Long play sessions, the open world,
  battles and saves are **not yet verified**. Expect bugs; reports are welcome.
- Scarlet's data mods (Shiny Lock Removal, Dark Void fix, No Trade Evolution) are installed but not yet checked in play.
- Windows only. Tested on one machine.

## Build and run

1. Get your own *Pokémon Scarlet* or *Violet* 4.0.0 files and put the extracted `exefs/` and `romfs.bin` in an
   `extracted/` folder next to the program (they stay on your PC; `extracted/` is git-ignored).
2. Build the runtime with MSYS2 (UCRT64) and `make`, as in [docs/README-ZA-Recomp.md](docs/README-ZA-Recomp.md).
3. Generate and compile the game library with `tools/gen.py` (this uses the Switch main module and `tools/sv_patches.py`).
   If `make` cannot link `lasgame.dll` on Windows, link it by hand with `clang -shared -fuse-ld=lld ... @lasgame.dll.rsp`.
4. Run `lasrecomp.exe --play`.

## License

MIT, as the base project (see [LICENSE](LICENSE)). Pokémon is a trademark of Nintendo, Creatures Inc. and GAME FREAK
inc. This project is unofficial and not affiliated with them.

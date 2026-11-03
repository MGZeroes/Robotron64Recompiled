# Robotron 64: Recompiled

A native port of **Robotron 64** (N64, 1998) for Windows, Linux and Android. The game's code is statically recompiled with
[N64: Recompiled](https://github.com/N64Recomp/N64Recomp), so it runs directly on your system instead of through an emulator.

**Status: 1.0.** Windows, Linux and Android packages have been tested. A Windows play-test ran through wave 26 (35 minutes, including
a bonus stage and a game over). Android is experimental: it has been tested on one phone.

This repository contains **no game code or assets**. You need your own Robotron 64 (USA) ROM, whichever way you play.

## Features
- Twin-stick aiming on one controller, using the game's own two-controller aiming
- Widescreen and ultrawide, with adjustable HUD placement
- High frame rates, higher resolutions and anti-aliasing through [RT64](https://github.com/rt64/rt64)
- Remappable controls for controller and keyboard
- Saving through an emulated Controller Pak
- Two-player alternating, plus an experimental simultaneous co-op mode
- The game's unused first-person camera, as an option
- RT64 texture pack support
- On Android: on-screen controls, controller support, and picking the ROM and mods with the system file picker

## Download and play
Where a release has packages, they're on the [Releases page](../../releases). Unpack the one for your system (on Android, install the
APK), start the game and choose your ROM in the launcher.

The ROM must be Robotron 64 (USA) rev 0, SHA-1 `44d158bc2aeefb111a620b61e043b2703e6c5808`. The packages contain no ROM and no game
assets; they do contain the game's code in recompiled form.

| System | Needs |
|---|---|
| Windows (x64) | Microsoft's Visual C++ Redistributable (x64), which many PCs already have |
| Linux (x86-64) | glibc 2.38 or newer (Ubuntu 24.04 has it), a Vulkan driver, and the system's SDL2, GTK3 and FreeType libraries |
| Android (ARM64) | Tested on a Galaxy S26 Ultra with Adreno 840. Mali GPUs aren't expected to work yet |

## Controls
The **left stick moves** and the **right stick aims and fires** at its exact angle. The D-pad, C buttons and arrow keys fire in 8
directions. You can remap everything under Settings → Controls. [The player guide](docs/PLAYING.md) covers the other options.

## Documentation
**For players**
- [Player guide](docs/PLAYING.md): controls, recommended settings, two players, first person, saves, Android
- [Texture packs](docs/TEXTURE_PACKS.md): making and installing them
- [Levels](docs/LEVELS.md): waves, floors and bonus rounds

**For developers**
- [Building from source](docs/BUILDING.md): every platform, release packages, contribution rules
- [Game internals](docs/GAME.md): what we've learned about the ROM, with sources
- [Decision records](docs/decisions/) and the [progress log](docs/NOTES.md). They mention a `CLAUDE.md`, which held the project's
  working rules and isn't part of the repository
- [Research](docs/RESEARCH.md): notes on the toolchain and the decomp

## License
GPL-3.0 (see [LICENSE](LICENSE)). Built on N64Recomp, N64ModernRuntime, RT64 and RecompFrontend, with symbols from the
frankischilling/robotron64 decompilation: see [CREDITS.md](CREDITS.md). Robotron 64 and its characters belong to their owners; this
project isn't affiliated with or endorsed by them.

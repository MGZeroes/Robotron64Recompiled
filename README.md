# Robotron 64: Recompiled

A native PC port of **Robotron 64** (N64, 1998, NTSC-U) made with the
[N64: Recompiled](https://github.com/N64Recomp/N64Recomp) toolchain: the game's MIPS code is
statically recompiled to C and runs on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime),
rendered by [RT64](https://github.com/rt64/rt64), with menus/input from
[RecompFrontend](https://github.com/N64Recomp/RecompFrontend).

**Status: 1.0. Playable: play-tested through wave 26 (35 min, bonus stage, game over), with saving and analog aiming.** Progress log: [docs/NOTES.md](docs/NOTES.md).

This repository contains **no game code or assets**. You need your own Robotron 64 (USA) ROM, whichever way you play.

## Download and play
Where a release has packages, they're on the Releases page. Unpack the one for your system (on Android, install the APK), start
the game and choose your ROM in the launcher. The packages contain no ROM and no game assets; they do contain the game's code in
recompiled form. The ROM must be Robotron 64 (USA) rev 0, SHA-1 `44d158bc2aeefb111a620b61e043b2703e6c5808`.

To build it yourself, read on.

## Requirements (Windows)
- Git, CMake ≥ 3.20, Ninja, Python 3
- LLVM/Clang (tested: 22.1.8)
- Visual Studio 2022 Build Tools with the C++ workload and a Windows SDK
- Docker Desktop (builds the decomp ELF that supplies symbols)

## ROM
Place the ROM at `baserom.us.z64` and check it:
```powershell
.\tools\check_rom.ps1
```
Expected: Robotron 64 (USA) rev 0, SHA-1 `44d158bc2aeefb111a620b61e043b2703e6c5808`.
`.v64`/`.n64` dumps are converted: `.\tools\check_rom.ps1 -Rom path\to\rom.v64`.

## Clone
```powershell
git clone --recurse-submodules <this repo>
```

## Build
Windows PowerShell blocks scripts by default. Either run
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once, or call each script as
`powershell -ExecutionPolicy Bypass -File .\tools\<script>.ps1`.

### 1. Recompile (needs Docker Desktop)
```powershell
.\tools\build_elf.ps1                          # decomp ELF (make verify must pass), spimdisasm + n64sym -> build-elf/
python tools\gen_syms.py                       # -> syms/robotron64.us.syms.toml
.\tools\build_elf.ps1 -Script tools/recomp.sh  # -> RecompiledFuncs/, rsp/aspMain.cpp
```
All outputs are derived from your ROM and are gitignored. The first `build_elf.ps1` run takes
several minutes (decomp build + a thorough n64sym scan); later runs are incremental.

### 2. Native build (Windows)
```powershell
.\tools\build.ps1        # VS 2022 env + LLVM clang-cl + Ninja -> build-cmake\Robotron64Recompiled.exe
```
The default is RelWithDebInfo with a console window for logs; `-Config Release -NoConsole` for a normal build.

### 2b. Linux build (Docker)
```powershell
.\tools\build_elf.ps1 -Script tools/build_linux.sh   # -> build-linux\Robotron64Recompiled-linux-x64.tar.gz
```
This needs step 1 first. It builds a native x86-64 Linux binary in the Docker build image and packs it with
`assets/` and `recompcontrollerdb.txt`. On a Linux machine, run `tools/build_linux.sh` directly; you need clang,
cmake, ninja, SDL2, Freetype and GTK3 development packages (see `tools/docker/Dockerfile`). The Linux build links,
but **nobody has run it yet** (ADR 005).

### 3. Run
Double-click `build-cmake\Robotron64Recompiled.exe` (or start it from any folder). On first launch, select
`baserom.us.z64` in the launcher.

Saving works through an emulated Controller Pak in controller port 1. It's stored in
`%LOCALAPPDATA%\Robotron64Recompiled\saves\robotron64.us.controller_pak_1.bin`, with a `.bak` copy of the
previous version.

## Controls
Twin-stick works out of the box: the **left stick moves** and the **right stick aims and fires** at its exact
angle. This is the game's own two-controller aiming from the N64, fed from the right stick. Set
Settings → General → Analog Aiming to Off for the one-controller behaviour, where the right stick acts as the
C buttons and fires in 8 directions. The D-pad, C buttons and keyboard (arrow keys) always fire in 8
directions. Remap under Settings → Controls.

## Recommended settings
- **Aspect Ratio:** Expand fills a widescreen window of any width, including 21:9 and 32:9 (background and
  object culling are patched). Original gives the N64's 4:3 picture.
- **HUD Placement:** with Expand, Expand moves the score, lives and WAVE to the window edges, 16:9 stops at
  the 16:9 edges, and Original keeps them where the N64 had them.
- **Framerate:** Display (or Manual) gives smooth output above 60 fps. The renderer interpolates object
  and camera motion; character limb animation stays at the game's 60 fps.
- Resolution and anti-aliasing can be raised freely.

## Two players
Settings → General → **Two Player Mode** picks how TWO PLAYER plays. **Alternating** (default) is the N64
behaviour: players take turns. **Simultaneous** (experimental) uses the game's unused co-op mode: both play at
once, enemies go after the nearest player, a lost life drops the player back into the arena like at wave
start, and both get a high-score entry.

**Controllers:** Settings → General → Controllers. **Shared** (default): every pad and the keyboard drive
player 1 (and player 2 in two-player games). **One per player**: open Settings → Controls → **Assign players**,
press a button on each player's pad (or a key for the keyboard), then Confirm. Assignment is needed once per
session; until then pads only open the menu. The keyboard drives the player it was assigned to, or player 1.
Each pad starts with the default mapping, and each player's keyboard profile with the normal key bindings; both
can be edited on the player's card.

## First person
The game has a finished first-person camera that was left out of the camera list. With Settings → General →
**First-Person Camera** on, L or R cycles to it after Overhead (single player only). **Classic** keeps the
game's own tank controls: the stick turns and walks, and Z looks around. **Modern**: the left stick walks and
strafes, the right stick turns, and A or B fires ahead.

## Texture packs
HUD, font and other textures can be replaced with RT64 texture packs, loaded as mods from the game's mods
folder and switched on and off in the Mods menu. [docs/TEXTURE_PACKS.md](docs/TEXTURE_PACKS.md) explains how
to make one with RT64's in-game inspector and `tools/texture_pack.ps1`. Packs are shared separately; this
repository contains no game assets.

## Android (experimental)
A build for ARM64 phones. It has on-screen controls, which hide while a controller is connected and in the
launcher and settings menus, which work by tapping.
Tested on one phone so far, a Galaxy S26 Ultra with Adreno 840 (docs/decisions/007-android-test-build.md). Mali GPUs are
not expected to work yet.

After the recompile step above and a native build (`RecompiledPatches/` must exist), in PowerShell:

```powershell
.\tools\build_android.ps1                 # runs in Docker; output: build-android\Robotron64-debug.apk
adb install -r build-android\Robotron64-debug.apk
```

Then copy your ROM to the phone and choose it with the launcher's ROM button, which opens Android's file picker. "Install Mods"
works the same way, with a `.rtz` or zip file (a pack folder can't be picked). Pushing the ROM also still works:

```powershell
adb shell mkdir -p /sdcard/Android/data/io.github.mgzeroes.robotron64/files
adb push baserom.us.z64 /sdcard/Android/data/io.github.mgzeroes.robotron64/files/robotron64.us.z64
```

The first build downloads several GB (Android SDK and NDK).

## Release packages
```powershell
.\tools\make_release_key.ps1      # once; back up what it creates
.\tools\package_release.ps1       # needs a clean git tree; output: dist\
```
Packages contain recompiled game code, so never commit them. [ADR 010](docs/decisions/010-first-release.md) covers what's in them
and what to check before sharing them.

## Contributing
Never commit a ROM, extracted assets, generated recompiled code (`RecompiledFuncs/`, `rsp/*.cpp`), or an ELF or map built from the
ROM. CI checks for the common cases, and `.gitignore` covers them. Dependencies are git submodules at pinned commits
([ADR 002](docs/decisions/002-dependency-pins.md)). Older notes and decision records mention a `CLAUDE.md`: those were the project's
working rules, which aren't part of the repository any more.

## License
GPL-3.0 (see [LICENSE](LICENSE)). Credits: [CREDITS.md](CREDITS.md). Robotron 64 and its characters belong to their owners; this
project isn't affiliated with or endorsed by them.

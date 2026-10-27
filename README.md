# Robotron 64: Recompiled

A native PC port of **Robotron 64** (N64, 1998, NTSC-U) made with the
[N64: Recompiled](https://github.com/N64Recomp/N64Recomp) toolchain: the game's MIPS code is
statically recompiled to C and runs on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime),
rendered by [RT64](https://github.com/rt64/rt64), with menus/input from
[RecompFrontend](https://github.com/N64Recomp/RecompFrontend).

**Status: early work in progress — does not build or run yet.** Progress log: [docs/NOTES.md](docs/NOTES.md).

This repository contains **no game code or assets**. You need your own Robotron 64 (USA) ROM.

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
### 1. Recompile (needs Docker Desktop)
```powershell
.\tools\build_elf.ps1                          # decomp ELF (make verify must pass), spimdisasm + n64sym -> build-elf/
python tools\gen_syms.py                       # -> syms/robotron64.us.syms.toml
.\tools\build_elf.ps1 -Script tools/recomp.sh  # -> RecompiledFuncs/, rsp/aspMain.cpp
```
All outputs are derived from your ROM and are gitignored. The first `build_elf.ps1` run takes
several minutes (decomp build + a thorough n64sym scan); later runs are incremental.

### 2. Native build
Not yet available (Phase 3).

## License
GPL-3.0 (see [LICENSE](LICENSE)). Credits: [CREDITS.md](CREDITS.md).

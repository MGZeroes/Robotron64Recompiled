# Building from source

The port is built from your own ROM, so the recompile step always comes first. All commands are PowerShell, run from the repository
folder. Docker runs the Linux-only steps for you.

## Requirements (Windows)
- Git, CMake ≥ 3.20, Ninja, Python 3
- LLVM/Clang (tested: 22.1.8)
- Visual Studio 2022 Build Tools with the C++ workload and a Windows SDK
- Docker Desktop (builds the decomp ELF that supplies symbols)

Windows PowerShell blocks scripts by default. Either run `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` once, or call each
script as `powershell -ExecutionPolicy Bypass -File .\tools\<script>.ps1`.

## Get the code
```powershell
git clone --recurse-submodules <this repo>
```

## Add your ROM
Place the ROM at `baserom.us.z64` and check it:
```powershell
.\tools\check_rom.ps1
```
Expected: Robotron 64 (USA) rev 0, SHA-1 `44d158bc2aeefb111a620b61e043b2703e6c5808`.
`.v64`/`.n64` dumps are converted: `.\tools\check_rom.ps1 -Rom path\to\rom.v64`.

## Recompile (needs Docker Desktop)
```powershell
.\tools\build_elf.ps1                          # decomp ELF (make verify must pass), spimdisasm + n64sym -> build-elf/
python tools\gen_syms.py                       # -> syms/robotron64.us.syms.toml
.\tools\build_elf.ps1 -Script tools/recomp.sh  # -> RecompiledFuncs/, rsp/aspMain.cpp
```
All outputs are derived from your ROM and are gitignored. The first `build_elf.ps1` run takes several minutes (decomp build + a
thorough n64sym scan); later runs are incremental.

## Windows
```powershell
.\tools\build.ps1        # VS 2022 env + LLVM clang-cl + Ninja -> build-cmake\Robotron64Recompiled.exe
```
The default is RelWithDebInfo with a console window for logs; `-Config Release -NoConsole` gives a normal build.

Run `build-cmake\Robotron64Recompiled.exe` from any folder and select `baserom.us.z64` in the launcher on first launch.

## Linux (Docker)
```powershell
.\tools\build_elf.ps1 -Script tools/build_linux.sh   # -> build-linux\Robotron64Recompiled-linux-x64.tar.gz
```
This builds a native x86-64 binary in the Docker build image and packs it with `assets/`, `recompcontrollerdb.txt` and the licences.

On a Linux machine, run `tools/build_linux.sh` directly. You need clang, cmake, ninja, and the SDL2, Freetype and GTK3 development
packages (see `tools/docker/Dockerfile`).

## Android (experimental)
This needs the recompile step and a native build first, because `RecompiledPatches/` must exist. The first build downloads several GB
(Android SDK and NDK). It runs in Docker:
```powershell
.\tools\build_android.ps1                 # output: build-android\Robotron64-debug.apk
adb install -r build-android\Robotron64-debug.apk
```
Besides the launcher's file picker ([player guide](PLAYING.md#android)), you can push the ROM directly:
```powershell
adb shell mkdir -p /sdcard/Android/data/io.github.mgzeroes.robotron64/files
adb push baserom.us.z64 /sdcard/Android/data/io.github.mgzeroes.robotron64/files/robotron64.us.z64
```

## Release packages
```powershell
.\tools\make_release_key.ps1      # once; back up what it creates
.\tools\package_release.ps1       # needs a clean git tree; output: dist\
```
Packages contain recompiled game code, so never commit them. [ADR 010](decisions/010-first-release.md) covers what's in them and what
to check before sharing them.

## Contributing
Never commit a ROM, extracted assets, generated recompiled code (`RecompiledFuncs/`, `rsp/*.cpp`), or an ELF or map built from the
ROM. CI checks for the common cases, and `.gitignore` covers them. Dependencies are git submodules at pinned commits
([ADR 002](decisions/002-dependency-pins.md)).

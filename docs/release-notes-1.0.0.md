# Robotron 64: Recompiled 1.0.0

A native port of Robotron 64 (N64, 1998) for Windows and Linux, plus an experimental Android build. The game's code is statically
recompiled, so it runs natively instead of through an emulator.

**You need your own Robotron 64 (USA) rev 0 ROM,** SHA-1 `44d158bc2aeefb111a620b61e043b2703e6c5808`. The downloads contain no ROM.

## Install
- **Windows:** unzip, run `Robotron64Recompiled.exe`, and choose your ROM in the launcher.
- **Linux (x86-64):** unpack, run `./Robotron64Recompiled` from that folder, and choose your ROM. Needs a Vulkan driver plus the
  system libraries listed in the README.
- **Android (ARM64, experimental):** install the APK and choose your ROM with the launcher's ROM button.

Check a download against `SHA256SUMS.txt` if you want to confirm it's intact.

## Features
- Twin-stick aiming on one controller, using the game's own two-controller aiming.
- Widescreen and ultrawide, with adjustable HUD placement.
- High frame rates, higher resolutions and anti-aliasing through RT64.
- Remappable controls, for controller and keyboard.
- Saving through an emulated Controller Pak.
- Two-player alternating, plus an experimental simultaneous co-op mode.
- The game's unused first-person camera, as an option.
- RT64 texture pack support, loaded from the Mods menu.
- On Android: on-screen controls, controller support, and picking the ROM and mods with the system file picker.

## Known limits
- **Android** has been tested on one phone (Galaxy S26 Ultra with Adreno 840). Mali GPUs aren't expected to work. "Open Mods Folder"
  does nothing there, and in two-player mode the on-screen buttons overlap parts of the HUD.
- **Simultaneous two-player** is experimental.

## Credits
Built on N64Recomp, N64ModernRuntime, RT64 and RecompFrontend, with symbols from the frankischilling/robotron64 decompilation. See
`CREDITS.md` and the `licenses/` folder. Licensed under GPL-3.0. Robotron 64 and its characters belong to their owners; this project
isn't affiliated with or endorsed by them.

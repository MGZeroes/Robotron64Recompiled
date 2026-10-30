# Credits

Robotron 64: Recompiled stands entirely on other people's work.

## Recompilation toolchain
- **[N64Recomp](https://github.com/N64Recomp/N64Recomp)** by Wiseguy (Mr-Wiseguy) and contributors — static recompiler (MIT).
- **[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)** (ultramodern + librecomp) by Wiseguy and contributors — runtime (GPL-3.0).
- **[RT64](https://github.com/rt64/rt64)** by Dario (DarioSamo) and contributors — N64 graphics renderer (MIT).
- **[RecompFrontend](https://github.com/N64Recomp/RecompFrontend)** by the N64Recomp contributors — shared menus and input (GPL-3.0).

## Project template
- **[N64RecompGameTemplate](https://github.com/SrBananaMan/N64RecompGameTemplate)** by SrBananaMan — the base of this repository's host code.
- **[HarvestMoon64Recomp](https://github.com/HarvestMoon64Recomp/HarvestMoon64Recomp)** — the port the template was derived from (GPL-3.0).
- **[Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp)** — the reference port that the whole ecosystem's host code descends from.

## Game research
- **[frankischilling/robotron64](https://github.com/frankischilling/robotron64)** — the matching decompilation of Robotron 64 that supplies symbols, function boundaries and documentation of the game's internals. Used as a git submodule; nothing from it is redistributed here.

## Prior art
- **[WCWvsNWOWorldTourRecomp](https://github.com/jessetbh/WCWvsNWOWorldTourRecomp)** and **[rush-2049-recomp](https://github.com/n64hub/rush-2049-recomp)** — how RecompFrontend's multiplayer mode is used in practice (option switching single/multiplayer mode, the keyboard-profile issue).

## Bundled third-party assets (from the template)
- Inter, Noto Emoji, Suplexmentary Comic NC fonts and PromptFont — see `assets/` (PromptFont license in `assets/promptfont/LICENSE.txt`).
- SlotMap — `lib/SlotMap/`.

## Launcher and icon art
- **[Orbitron](https://github.com/theleagueof/orbitron)** by Matt McInerney and The Orbitron Project Authors — launcher wordmark font, SIL Open Font License 1.1 (`assets/Orbitron-OFL.txt`; from google/fonts commit `8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5`).
- The launcher backdrop (`assets/launcher/shade.svg` and the colour wave in `src/main/launcher_animation.cpp`) is original work made for this project, licensed with the rest of the repository (GPL-3.0). It uses no game assets.
- The hero's head in the launcher (`assets/launcher/hero.svg`) is fan art of the Robotron 64 hero; Robotron and its characters are trademarks and copyrights of their owners (Midway / Warner Bros.). The SVG is drawn for this project and contains no game assets.
- The app icon (`icons/app.svg`, `icons/app-small.svg`) is fan art of the robot from the Robotron 64 box art (chest emblem simplified); Robotron and its characters are trademarks and copyrights of their owners (Midway / Warner Bros.). The SVG is drawn for this project and contains no game assets.

## The game
Robotron 64 © 1998 Crave Entertainment / Player 1; Robotron: 2084 © Williams Electronics.
This project contains no game code or assets; you must supply your own legally obtained ROM.

## Build tooling
- **[spimdisasm](https://github.com/Decompollaborate/spimdisasm)** and **[rabbitizer](https://github.com/Decompollaborate/rabbitizer)** by Decompollaborate — function boundaries (run through the decomp's `make analyze`).
- **[n64sym](https://github.com/shygoo/n64sym)** by shygoo — libultra function identification (MIT).
- The aspMain RSPRecomp dispatch table was cross-checked against **[drmario64_recomp](https://github.com/AngheloAlf/drmario64_recomp)** and **[Quest64-Recomp](https://github.com/Rainchus/Quest64-Recomp)**.

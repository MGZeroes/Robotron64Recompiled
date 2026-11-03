# Player guide

## Controls
Twin-stick works out of the box: the **left stick moves** and the **right stick aims and fires** at its exact angle. This is the game's
own two-controller aiming from the N64, fed from the right stick. Set Settings → General → Analog Aiming to Off for the
one-controller behaviour, where the right stick acts as the C buttons and fires in 8 directions. The D-pad, C buttons and keyboard
(arrow keys) always fire in 8 directions. Remap under Settings → Controls.

## Recommended settings
- **Aspect Ratio:** Expand fills a widescreen window of any width, including 21:9 and 32:9 (background and object culling are patched).
  Original gives the N64's 4:3 picture.
- **HUD Placement:** with Expand, Expand moves the score, lives and WAVE to the window edges, 16:9 stops at the 16:9 edges, and
  Original keeps them where the N64 had them.
- **Framerate:** Display (or Manual) gives smooth output above 60 fps. The renderer interpolates object and camera motion; character
  limb animation stays at the game's 60 fps.
- Resolution and anti-aliasing can be raised freely.

## Two players
Settings → General → **Two Player Mode** picks how TWO PLAYER plays. **Alternating** (default) is the N64 behaviour: players take
turns. **Simultaneous** (experimental) uses the game's unused co-op mode: both play at once, enemies go after the nearest player, a
lost life drops the player back into the arena like at wave start, and both get a high-score entry.

**Controllers:** Settings → General → Controllers. **Shared** (default): every pad and the keyboard drive player 1 (and player 2 in
two-player games). **One per player**: open Settings → Controls → **Assign players**, press a button on each player's pad (or a key
for the keyboard), then Confirm. Assignment is needed once per session; until then pads only open the menu. The keyboard drives the
player it was assigned to, or player 1. Each pad starts with the default mapping, and each player's keyboard profile with the normal
key bindings; both can be edited on the player's card.

## First person
The game has a finished first-person camera that was left out of the camera list. With Settings → General →
**First-Person Camera** on, L or R cycles to it after Overhead (single player only). **Classic** keeps the game's own tank controls:
the stick turns and walks, and Z looks around. **Modern**: the left stick walks and strafes, the right stick turns, and A or B fires
ahead.

## Saves
Saving works through an emulated Controller Pak in controller port 1. The file is `saves/robotron64.us.controller_pak_1.bin`, with a
`.bak` copy of the previous version, in the game's folder:

| System | Folder |
|---|---|
| Windows | `%LOCALAPPDATA%\Robotron64Recompiled` |
| Linux | `~/.config/Robotron64Recompiled` |
| Android | `/sdcard/Android/data/io.github.mgzeroes.robotron64/files` |

On Android, uninstalling the app deletes that folder, so copy your saves off the phone first.

## Texture packs
HUD, font and other textures can be replaced with RT64 texture packs, loaded as mods from the game's mods folder and switched on and
off in the Mods menu. [Texture packs](TEXTURE_PACKS.md) explains how to make one. Packs are shared separately; this repository
contains no game assets.

## Android
An experimental build for ARM64 phones, tested on one phone so far (Galaxy S26 Ultra with Adreno 840). Mali GPUs aren't expected to
work yet.

- **Controls:** on-screen controls hide while a controller is connected. They also hide in the launcher and settings menus, which
  work by tapping. The MENU button opens the settings during touch-only play.
- **ROM:** copy your ROM to the phone and choose it with the launcher's ROM button, which opens Android's file picker.
- **Mods:** Install Mods uses the same picker, with a `.rtz` or zip file. A pack folder can't be picked.
- **Known limits:** "Open Mods Folder" does nothing, and in two-player mode the on-screen buttons overlap parts of the HUD.

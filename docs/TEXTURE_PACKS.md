# Texture packs

Replace the game's textures, such as the HUD, the fonts or the arena floor, with your own images, without
touching the game or this repository. Robotron 64: Recompiled renders with RT64, which supports texture
packs. A pack is loaded as a mod and can be switched on and off in the **Mods** menu, even while you play.

## Why a mod and not part of the port

An improved version of a game texture is still based on the game's art, which belongs to its owners. This
repository never contains game assets, so packs live in your own mods folder and are shared separately as
`.rtz` files. That also lets players choose: anyone who prefers the original look keeps the pack off.

## What a pack is

A pack is a folder or a `.rtz` file, which is a zip, in the game's mods folder:

| | |
|---|---|
| Windows | `%LOCALAPPDATA%\Robotron64Recompiled\mods` |
| Linux | `~/.config/Robotron64Recompiled/mods` (or `$APP_FOLDER_PATH/mods`) |
| Portable install | `mods` next to `portable.txt` |

It contains:

- **`mod.json`:** the mod's name and id, and the game it is for (`"game_id": "robotron64"`). It is required
  for a folder and optional for a `.rtz`.
- **`rt64.json`:** the list of replaced textures. Each entry maps the hash RT64 computes for an original
  texture to an image file in the pack. Keep `"hashVersion": 5` in it: without it RT64 assumes an old hash
  version and its in-game tools refuse to edit the pack.
- **The images:** PNG or DDS, at any size with the same aspect ratio as the original. 2× or 4× works well.

RT64's own format reference is `lib/rt64/TEXTURE-PACKS.md`.

## Set up (once)

In PowerShell, from the repository folder:

```powershell
.\tools\texture_pack.ps1 -Init robotron64-hud -Author "Your Name" -DisplayName "HUD remaster"
.\tools\texture_pack.ps1 -DevMode On
```

If Windows says running scripts is disabled, allow your own scripts once with
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`, or call the script as
`powershell -ExecutionPolicy Bypass -File .\tools\texture_pack.ps1 ...` (as for the build scripts in the
README).

- **`-Init`** creates the pack folder `mods\robotron64-hud` with a `mod.json`, an empty `rt64.json` and a
  short readme. The name is also the mod's id.
- **`-DevMode On`** turns on RT64's developer tools. It is a hidden graphics setting
  (`"developer_mode": true` in `graphics.json`) that is only read when the game starts, so restart the
  game after changing it.

Start the game. The pack is listed in the **Mods** menu and enabled.

## Replace a texture

1. **Open the inspector.** Play to where the texture shows, for example the HUD during a wave. Press **F1**
   to open RT64's inspector, and **F4** inside it to pause the game.
2. **Pick the draw call.** Find the call that draws your texture and select it. The HUD is drawn as small
   textured quads, one per character, icon or digit (`func_8004B590`, see GAME.md). A single glyph texture
   is often reused for every copy of that letter, so replacing it once changes it everywhere.
3. **Dump the original.** Press **Dump PNG** to save it at its original size, so you know the layout and
   the transparent areas. **Copy hash** copies its 16-digit hash, if you want to edit `rt64.json` by hand.
4. **Paint the replacement.** Make the new image at 2× or 4× the size, keeping the aspect ratio and the
   transparency. Save it **inside the pack folder**; subfolders are fine, for example `hud/`.
5. **Assign it.** Press **Replace** next to the texture's hash and choose your image. Replace only works
   when this pack is the only one enabled and is a folder, not a `.rtz`.
6. **Save the list.** Press **Save directory** in the inspector's Textures tab. It writes `rt64.json` and
   keeps the previous version as `rt64.json.old`.

**Reloading:** the game does not watch the folder. To see changes you made to the files, turn the pack off
and on again in the **Mods** menu. A brand-new pack folder only shows up after restarting the game.

**Many textures at once:** the inspector's **Textures** tab can dump everything the game loads ("Start
dumping textures"). RT64's `texture_hasher` tool turns such a dump into an `rt64.json`
(`lib/rt64/TEXTURE-PACKS.md`).

## What the dump files are

"Start dumping textures" writes several files per texture into the folder you pick. Every name starts with
the texture's RT64 hash, e.g. `028ac770e59f6751.v5`; `v5` is RT64's hash version. This comes from
`TextureManager::dumpTexture` in `lib/rt64/src/hle/rt64_rdp_tmem.cpp`.

| File | What it is | What it's for |
|---|---|---|
| `.v5.png` | The texture as it looks, at its original size (Robotron's are 32×32) | Looking at it; the template to draw over |
| `.v5.tmem` | A raw copy of the N64's 4 KB texture memory when the texture was loaded | RT64's `texture_hasher`, to recompute hashes if RT64's hash version changes |
| `.v5.tile.json` | How the game told the N64 to read it: format (CI8 here), size, wrap/mirror, palette | Information; the hasher reads it with `.tmem` |
| `.v5.rice.json`, `.v5.rice.rdram` | The texture's bytes as they were in the game's RAM, and how they were loaded | The "Rice" hash, the older texture-pack format other N64 plugins use (`texture_hasher --rice`) |
| `.v5.rice.palette.json`, `.palette.rdram` | The same for the colour palette (CI textures have one) | The Rice hash of palette textures |

**To make a pack, you only need the PNGs and the hash in their file names.** Keep the other files
anyway: they're small, and they let RT64's tools build or upgrade `rt64.json` automatically.

Keep the dump in a folder called `dump/` in the repository folder. It is gitignored, because these are the
game's own textures and must never be committed. A dump made while the game runs keeps growing until you
stop it or close the game.

## Sorting a dump

```powershell
.\tools\build_elf.ps1 -Script tools/sort_dump.sh     # runs in the Docker image
```

This sorts `dump/` into `texture_work/`, which is also gitignored, using the classification in
`tools/texture_classes.txt`. That file has one line per texture: `<hash> <category> <label>`.

- `texture_work/<category>/<label>__<hash>.png`: each texture enlarged 4× (128×128) without smoothing, so
  every original pixel is a crisp 4×4 block. Use these as templates to draw over. The categories are
  `font`, `cabinet` (the arcade cabinet in the intro), `powerups` (the pickups, named as in the game manual), `icons`, `arena`, `effects`, `models` and `other`.
- `texture_work/sheets/<category>.png`: a labelled overview per category.
- `texture_work/index.csv`: hash, category, label and original size for every texture.

Textures the classification doesn't know yet, for example from a new dump of other screens, end up in
`other/` as `unknown__<hash>.png`. Add a line for them to `tools/texture_classes.txt` and run the sort again.

So far the font is the high-score name-entry alphabet: A–Z, 0–9, `.`, `?` and the END/DEL/SPC keys. The HUD
draws its letters and digits from the same set. Each glyph is its own 32×32 texture, white with a dark
outline, and the game tints it when drawing. A white replacement is therefore tinted the same way.

**From template to pack:** draw your version at 128×128, or 64×64. Keep the glyph in the same place in the
tile and keep the transparency. Save it into the pack folder, for example `font/A.png`. Then either use
**Replace** in the inspector, or add an entry to the pack's `rt64.json` by hand, with the hash from the
template's file name:

```json
{ "path": "font/A.png", "hashes": { "rt64": "<hash>" } }
```

## Art split over several textures

Some pictures are made of several textures drawn side by side. One example is the "2084" side of the arcade
cabinet in the intro: three 32×32 pieces stacked top to bottom (`side_art_top`, `_middle`, `_bottom`). Draw such
art as one picture and export the pieces as slices (in Affinity: Export Persona, one slice per piece). Two
things decide where the slices go:

- **The game may not show a texture's full size.** The cabinet pieces are drawn with texture coordinates 0..31,
  and on the N64 that means texel centres 0 to 31, so only 31 of the 32 rows show. RT64 reproduces this with
  the default shift `half`: in a 1024 px replacement, 16 px at every edge are hidden. With shift `none` the
  hidden part moves to the bottom 32 px, and the top edge blends with the bottom row instead, because these
  textures wrap.
- **So the slices overlap.** Lay the art out on a 1024 × 3008 canvas: 3 × 992 visible rows plus the hidden
  16 px above and below. Put the slices at y 0, 992 and 1984, each 1024 × 1024. Neighbouring slices then share
  32 rows. Each copy of those rows is half hidden, so nothing is shown twice and nothing is missing.

The same applies sideways. All cabinet textures (side art, bezel, marquee, control panel) are 32×32 wrap tiles,
like the side art, so expect 16 px hidden on every side of every piece. For a grid of C columns × R rows, use a
canvas of (C·992 + 32) × (R·992 + 32) with the art 16 px in from each edge. The slice for column i, row j is
1024 × 1024 at x = i·992, y = j·992. Only the vertical direction has been checked in the game so far, on the
side art; check the horizontal one with the first picture whose pieces sit side by side.

Keep the default shift (no `shiftFilters` entry) for such pieces. The font needs `none` for a different reason:
its glyphs reach the edge of their tile, and `half` cut off their tops. If a replacement looks cut off or shows
a thin line from the opposite edge, try the other shift first.

## Share it

```powershell
.\tools\texture_pack.ps1 -Pack robotron64-hud
```

This writes `mods\robotron64-hud.rtz`. Move the folder out of the mods folder before testing the `.rtz`,
otherwise both are loaded. Players drop the `.rtz` into their mods folder. RT64's `texture_packer` can also
build a `.rtz` with a low-resolution preview cache for big packs; small packs like a HUD don't need it.

When you're done, turn the developer tools off again:

```powershell
.\tools\texture_pack.ps1 -DevMode Off
```

## Rules for this repository

- Never commit pack folders, dumped textures or `.rtz` files here (CLAUDE.md rule 1).
- The helper script and the empty template (`tools/texture_pack_template/`) are part of the repository.
- Packs are their authors' work, based on the game's art. Share them separately and credit the original
  game.

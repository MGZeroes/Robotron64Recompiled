# Levels, floors and bonus rounds

What Robotron 64 has in each wave: its floor texture, its background, and where the bonus rounds and bosses
are. Useful for texture packs: it tells you which floor you will see where. Facts come from the decomp, from
the ROM read with `tools/level_table.py`, and from the game manual. Each section says which.

## In short

- **200 waves plus 10 bonus rounds**, 210 level slots in all. A bonus round comes after waves 19, 39, 59, 79,
  99, 119, 139, 159, 179 and 199.
- **Bosses:** wave 100 (the yellow Guardian, `BOSS1.STR`) and wave 200 (the blue Guardian, `BOSS2.STR`).
- **Floors:** each slot uses one of 13 floor textures. Most stay the same for a block of 10 waves.
- **Round arena:** waves 60–69 and 160–169, plus one bonus round. They use a round floor and boundary instead
  of the square grid.
- **Named sections** (from the waves' title cards): the Electrode Forest (30–39, again 130–139), the Beam Maze
  (40–49, again 120–129), the octagon (60–69), the dark waves (70–79, black background).

## Is the green floor a forest?

Nothing supports that. The green floor with bright veins is the ROM file **`swamp.flr`**. It is the floor of 5 of
the 10 bonus rounds and of waves 140–149. The "Electrode Forest" in the manual and on the title cards is waves
30–39. Its floor is **`swirl.flr`**, dark blue. The forest seems to be the obstacles: the manual's giant electrodes
("Giant Electrode, found in the Electrode Forest"). A Robotron X source describes them as shaped like abstract pine
trees; that hasn't been checked on the N64 version. Waves 130–139 ("back into the electrode forest") use
`floor5.flr`, a teal pattern. The other green floor, **`circuit.flr`**, is a single green glow on black and is used
for waves 10–19, 70–79, 90, 110–119 and 190.

Nothing in the manual gives the floors a story meaning. The floor names above are the ROM's file names.

## How the game picks the floor (code)

| Step | What | Source |
|---|---|---|
| Level index | `D_800AD160`, 0–209, set when a game starts (`func_80022528`) | `decomp/src/game/session_scene_start.c` |
| Wave number shown | `LEVELNOS.DAT` → `D_800BA7A8[220]`: wave per level index, 0 = bonus | `session_initialization/initialize.c` |
| Background record | `D_80074C08[level]`, 16 bytes: kind, floor index, colour pair index; +0x0C points to the wave's title text | `session_background_select.c` (`func_80022BFC`), `include/scene_background_internal.h` |
| Floor file | `D_8007CCC0[floor index]` → `<name>.flr`. It is loaded into one 32×32 RGBA16 texture when the index changes | `renderer_texture_file_cache.c` (`func_80042830`), `include/renderer_texture_cache.h` |
| Floor drawing | 5×5 grid of squares (`func_80042E2C`), or the round floor (`func_800428C0`) when the scene's +0xD0C flag is set | `object_runtime_service.c` |
| Background | `kind & 0x7F`. Kinds 1/2/3/6/7/8 are treated as 0. 0 = colour wave (as in the menus), 4 = gradient, 5 = two-colour fill, 10 = black | `session_background_select.c`, `renderer_surfaces/background_dispatch.c` |
| Wave scripts | `LEVEL<wave>.STR` (1–199, no 100), `BOSS1/2.STR`, `BONUS1/2/3/4/9.STR`. +0xCCC is the level index, +0xCD0 the wave, +0xCD4 the bonus type, +0xD0C the round floor | `include/scene_definition.h`; ROM via `level_table.py --scenes` |

Bit 0x80 of the kind is set on exactly the 10 bonus slots. The game masks it off and doesn't use it anywhere
we've found.

## Floor per wave (ROM)

From `python tools/level_table.py`. "Dump hash" is the RT64 hash, matched pixel for pixel with
`--match dump` (`tools/texture_classes.txt`, category `arena`).

| Waves | Floor | Background | Section |
|---|---|---|---|
| 1–9 | lego | colour wave | |
| 10–19 | circuit | colour wave | |
| bonus | swamp | colour wave | |
| 20–29 | plate | gradient | |
| 30–39 | swirl | two colours | Electrode Forest |
| bonus | lego | colour wave | |
| 40–49 | tile2 / tile, alternating | colour wave | Beam Maze |
| 50–59 | check | two colours | |
| bonus | floor5 | colour wave | |
| 60–64 / 65–69 | lego / moirre | two colours, 67–69 colour wave | octagon (round floor) |
| 70–79 | circuit | black | dark waves |
| bonus | swamp | black | |
| 80–89 | square | gradient | |
| 90–99 | a different floor each wave: circuit, tile, plate, moirre, square, lego, check2, spin, plate, swirl | colour wave | |
| bonus | swamp | colour wave | |
| 100 | floor5 | colour wave | Boss 1 |
| 101–109 | lego | colour wave | |
| 110–119 | circuit | colour wave | |
| bonus | plate | gradient | |
| 120–129 | tile / tile2, alternating | two colours | Beam Maze |
| 130–139 | floor5 | gradient | Electrode Forest |
| bonus | check2 | colour wave | round floor (`BONUS2`) |
| 140–149 | swamp | colour wave | |
| 150–159 | check2 | two colours | |
| bonus | floor5 | colour wave | |
| 160–164 / 165–169 | moirre / check2 | colour wave | octagon (round floor) |
| 170–179 | spin | gradient | |
| bonus | swamp | colour wave | |
| 180–189 | square | colour wave | |
| 190–199 | as 90–99 | colour wave | |
| bonus | swamp | colour wave | |
| 200 | floor5 | colour wave | Boss 2 |

## The floor textures

| File | Looks | Dump hash | First seen in |
|---|---|---|---|
| `lego.flr` | blue plate with four round studs | `667ad04e14785b44` | wave 1 |
| `circuit.flr` | green glow on black | `d8a1c8b0a29beed1` | wave 10 |
| `swamp.flr` | bright green veins | `7a228b9bc4ee2f91` | first bonus round |
| `plate.flr` | dark blue with purple rivets | `8976c57c673c072a` | wave 20 |
| `swirl.flr` | dark blue marbled | `a8c31d78be2a0ccc` | wave 30 |
| `tile2.flr` | reddish brown bricks | `e90e89121400f277` | wave 40 |
| `tile.flr` | grey-green tiles | `73efeef8afd71782` | wave 41 |
| `check.flr` | purple with dark circles | not dumped yet | wave 50 |
| `floor5.flr` | teal scales | not dumped yet | third bonus round, wave 100 |
| `moirre.flr` | dark purple checker | not dumped yet | wave 65 |
| `square.flr` | black with a blue frame | not dumped yet | wave 80 |
| `check2.flr` | dark green squares | not dumped yet | wave 96 |
| `spin.flr` | dark green spiral | not dumped yet | wave 97 |

The floors are RGBA16 textures in wrap mode (`dump/*.v5.tile.json`: fmt 0, siz 2, mask 5). The font is I8 and
the cabinet is CI8. Each floor repeats across the arena, so a replacement must tile seamlessly.

The name table also lists `floor2.flr` and 37 `.raw` textures (`floor1-8`, `tile1-20`, `plate1-10`). No level
uses them. `floor2.flr` can't even load: its name has no terminator and runs into the next one.

`python tools/level_table.py --floors texture_work/floors` writes all of them as PNGs into the gitignored
`texture_work/`. That shows the floors you haven't reached yet. A texture pack still needs the hash from an RT64
dump, so play to the listed wave once with dumping on. The level select cheat below can help.

## Bonus rounds

Each bonus slot has its own title card. The scripts (`BONUS*.STR`) set bonus types 0, 6, 7, 8 and 9.

| After wave | Title | Floor |
|---|---|---|
| 19 | meet the beez! | swamp |
| 39 | the weebles | lego |
| 59 | dancing droids | floor5 |
| 79 | the bee hive | swamp |
| 99 | shoot the electrodes | swamp |
| 119 | you rack, i break | plate |
| 139 | destroy them! | check2 |
| 159 | food wave! | floor5 |
| 179 | beez beez beez | swamp |
| 199 | shoot the electrodes | swamp |

Which bonus script runs in which slot is only proven for five of them: `BONUS1` at 19, `BONUS2` at 139,
`BONUS3` at 159, `BONUS4` at 179 and `BONUS9` at 199 (from the scripts' own level index). The rest presumably
reuse these scripts. That would need the script list in `GAME.TOK` decoded.

From reviews (not checked in code): you can't die in a bonus round, and you shoot targets for points.

## Story and names (manual)

Official US manual NUS-NRXE-USA, scanned on archive.org: <https://archive.org/details/robotron-64-nus-nrxe-usa>.

- **Story:** "in 2084, inspired by his never ending quest for progress, man perfects The Robotrons", "a robot
  species so advanced that man is inferior to his own creation". The player: "You are the last hope of
  mankind. Due to a genetic engineering error you possess superhuman powers." The goal: "stop The Robotrons
  and save the last human family." The hero is called Eugene.
- **Enemies:**
  - Grunt 1–4 (Chrome Grunts take 2 hits);
  - Hulk 1–4 (indestructible);
  - Spheroid 1–4 (make Enforcers);
  - Enforcer 1–4;
  - Quark 1–4 (make Tanks);
  - Tank 1–4;
  - Brain 1–4 (turn humans into Progs);
  - Nanobyte, Dynobyte, Ant (the Guardian's minions), Progs, and the Guardian.
- **Electrodes:** basic, giant (Electrode Forest) and transformer ("used in the Beam Maze").
- **Level Gates:** "created when a magical grunt is destroyed. If entered, a Level Gate will randomly warp the
  player forward through levels."
- **Power-ups:** Shield, Three Way, Four Way, Flame Thrower, Speed Up, and a forward-and-behind shot whose
  name is unreadable in the scan. The texture classification uses these names.
- **Options:** one player, or two taking turns; difficulty Easy / Normal / Insane; game speed; 11 music
  tracks; Controller Pak saves and passwords.

## Cheats (unverified)

From cheathappens.com; not tested in this port. Enter them at the setup screen:

| Cheat | Input |
|---|---|
| Level select | Down, Up, C-Left, Down, C-Left, C-Right, Down, C-Right |
| 50 lives | Up, Up, Down, Down, Left, Right, Left, Right, C-Left, C-Right, C-Left, C-Right |
| Monochrome | Up, Down, Right, C-Left, Down, Up, Left, C-Right, Up, Down |

## Not known yet

- Which enemies come in which wave. Each `.STR` has 256 arrival records that haven't been decoded.
- The bonus script in slots 39, 59, 79, 99 and 119 (see above).
- Record bytes +3..+0xB (short values like 70, 50, −70). They are not read by `func_80022BFC`, perhaps a light
  direction.
- Arena models: `MODELS\ARENA*.DAT`, `OCTARENA`, `BOSARENA`, `BONUSOCT` and others are in the ROM. How they are
  picked hasn't been traced.
- Whether the levels match the PlayStation/PC version, Robotron X. Robotron 64 is a port of it, and a Robotron X
  player describes the same Electrode Forest and Beam Maze sections.

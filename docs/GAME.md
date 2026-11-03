# Robotron 64 internals

Facts as we learn them, each with a source. "Decomp" = frankischilling/robotron64 @ `812a75f`.

## ROM

| Fact | Value | Source |
|---|---|---|
| Supported dump | Robotron 64 (USA) rev 0, `NRXE`, 8 MiB, big-endian | `decomp/config/target.json`, local check |
| XXH3-64 (runtime `rom_hash`) | `0x41CAB16167C1368C` | python3-xxhash, 2026-10-27 |
| SHA-1 | `44d158bc2aeefb111a620b61e043b2703e6c5808` | same; verified with `tools/check_rom.ps1` |
| Header CRC1/CRC2 | `AC8E4B32` / `E7B47326` | ROM header 0x10/0x14 |
| Known bad dump | SHA-1 `285c720f642649790faaaa02c33636cf1b33d7db` (same header + CRCs; 227 bytes differ in ROM `0x4726BC`–`0x47ADFF`, asset area — code identical) | local diff, 2026-10-27 |
| PAL release | `NRXP`, SHA-1 `d86a7ed2f203ba9df0ce5b18d9e4e2e2d9b62a3f`, CRC `9FF69D4F`/`195F0059` — not supported | local check |
| Data end | non-zero data ends at ROM `0x78D630`; rest zero-padded | local scan (of the bad dump; identical beyond `0x47ADFF`, so holds for the good one) |

## Memory layout

| Fact | Value | Source |
|---|---|---|
| Entrypoint | `0x80000400` | ROM header, `target.json` |
| VRAM ↔ ROM | VRAM = ROM + `0x7FFFF400` (single flat image) | decomp `docs/rom-map.md` |
| CPU code | VRAM `0x80000400`–`0x8006F440` (ROM `0x1000`–`~0x70040`) | decomp `docs/rom-map.md` |
| BSS | `0x80097290`–`0x80197640` | decomp `linker_scripts/us.ld` |
| Boot | stack set to `0x8013A280`, jumps to `0x80048170` | decomp `docs/rom-map.md` |
| Overlays / compressed code | none | decomp docs |
| Inflate (assets) | `0x8005DA20`–`0x8005FBB0` | decomp `docs/compression-runtime.md` |

## Microcodes

| Fact | Value | Source |
|---|---|---|
| Graphics | F3DEX 1.21 (string @ ROM `0x96E80`) and F3DLP.Rej 1.21 (@ `0x97680`) | decomp issue #6 |
| rspboot | candidate ROM `0x70040`–`0x70110` | decomp issue #6 |
| F3DEX 1.21 | text `0x8006F510` (ROM `0x70110`), data `0x80095FD0` (ROM `0x96BD0`) | decomp `docs/graphics-tasks.md` |
| F3DLP.Rej 1.21 | text `0x80070940` (ROM `0x71540`), data `0x800967D0` (ROM `0x973D0`) | same |
| Graphics ucode select | table at `0x8008D560`, index 0 = F3DEX, 1 = F3DLP.Rej, set by `func_800498E0` | same |
| RT64 support | both listed in `rt64/src/gbi/rt64_gbi.cpp` (F3DLP 1.21.Rej marked "Needs confirmation") | rt64 `d9817c6` |
| Audio | **stock `aspMain`**: text `0x80071D10` (ROM `0x72910`, size `0xE20`), data `0x80096FD0` (ROM `0x97BD0`, `0x800`). Dispatch table at data+0x10 is the standard 16 entries | decomp `docs/audio-generation.md`; ROM check 2026-10-27 |

## Code inventory (Phase 2)

| Fact | Value | Source |
|---|---|---|
| Functions | 1,539 in `0x80000400`–`0x8006F440` (1,467 decomp-known) | `tools/gen_syms.py` |
| libultra block | `0x8005FBB0`–`0x8006F440` (contiguous); 216 functions named via n64sym | n64sym `ccf4600` |
| libultra contents | `osPfs*`, `osMotor*` (rumble), 64DD init/interrupt (unused), classic `al*` audio library | n64sym hits in the block |
| Game calls not provided by runtime | `osPiRawReadIo`, `osPiWriteIo`, `osPfsIsPlug`, pak internals via `func_800617A0` | N64Recomp output check |

## Runtime behaviour

| Fact | Value | Source |
|---|---|---|
| Save | Controller Pak, one 4 KiB file | decomp `docs/save-game.md` |
| Test shortcut | Password `BQJDNQMBHC` (LOAD GAME) starts at wave 21 with 155000 points, just after the wave-20 bonus stage | play-test |
| Runtime Pak support | N64ModernRuntime only has stubs (`librecomp/src/pak.cpp` returns `PFS_ERR_NOPACK`). We replace them with `src/game/controller_pak.cpp`: a file-API-level pak on channel 0 with 16 notes and 123 × 256-byte pages, stored in `%LOCALAPPDATA%\Robotron64Recompiled\saves\robotron64.us.controller_pak_1.bin` (+ `.bak`). | runtime `cdf5abb`; our code |
| Pak use by the game | Channel 0 only. One 0x1000-byte note: company `0x345A`, game code `0x4E525845`. It's created on the first write-open (`func_8004C3AC`) and read or written whole. The pak menu lists up to 16 notes (`osPfsFileState`) and can delete them. Calls: InitPak, IsPlug, FindFile, AllocateFile, DeleteFile, FileState, FreeBlocks, NumFiles, ReadWriteFile, `__osGetId`. | decomp `src/game/pak_file_*.c`, `controller_pak_*.c`; `RecompiledFuncs` call list |
| Threads | main (pri 10), PI manager (150), game loop (10), thread @ `0x80051380` (110), 3 scheduler threads | decomp `docs/startup.md` |
| Odd PI reads | 16 reads from `0x00FFB000`–`0x00FFB03C` at startup, results unused | decomp `docs/startup.md` |

## Input

| Fact | Value | Source |
|---|---|---|
| Direction bits | `func_8003C1A8` turns the stick, D-pad and C buttons into direction bits (stick threshold ±20; an axis is cleared when the other is 8× larger). These bits are 8-way. | decomp `docs/controller-input-mapping.md`, `src/game/controller_input.c` |
| Analog movement | In play, the player moves with the raw stick of their port (`D_8013DBB8/C8[port]`, scaled by speed), which is analog. The direction bits are only used when the stick is idle. | decomp `src/game/player/behavior.c` (movement block) |
| Two-controller aiming | Player 1 uses port 0, or port 2 for player 2 when it's connected. If the **next** port is connected, its analog stick is the aim vector (`secondaryX/Y`, threshold 0x10). The shot angle is `func_8003CD4C(y, x)` in 0x1000 units, not 8-way. Held fire bits (0xF0) override it with 8-way fire. The port uses the right stick; the Analog Aiming option controls this. | decomp `src/game/player/behavior.c` (fire block); `src/main/main.cpp` `get_n64_input` |
| Ports per player | P1 moves/buttons with port 0, aims with port 1; P2 with ports 2 and 3. P2's `port04` is 2 only if port 2 was connected when `func_8001A2C4` ran (boot, options load), else 0 (shares P1's pad); the analog move/aim index follows the live mask `D_8007C334` (bit n = port n, copy of the `osContInit` mask `D_8013D9D0`). Shell menus OR in port `(port+2)&3`. The game never re-queries controllers per frame. Our runtime reports all four ports connected (ADR 006). | decomp `player/behavior.c:53-57,195`, `session_input_initialize.c`, `early_pointer_state.c`, `controller/poll.c` |
| Bit groups | Mode 0 → bits 0–3; other modes → bits 4–7. Start `0x100`, A `0x800`, B `0x1000`. L+R together toggles `D_8007CCA0`; L or R alone cycles the camera view (`func_8001276C`, see Rendering: Camera views). | same |
| Player sampling | `func_8001A1F0` stores the bits in the player state and then calls `func_8001BFB0`. With `D_800AEE9C` set, it ORs in controller `(port+2)&3` in mode 0. | decomp `src/game/early_player_input_sample.c` |

## Two players

| Fact | Value | Source |
|---|---|---|
| Play styles | Session `extra48` (`D_800AD180`): 0 = one player, 1 = alternating (TWO PLAYER), 3 = simultaneous. Style 3 is fully coded but unreachable from the menu. The code only tests `== 3` / `!= 3`. `func_80022528(level, mode, style)` starts a game; our option rewrites style 1 to 3 at its entry. | decomp `session_scene_start.c`, `session_menu_start_alternate.c`; `robotron64.us.toml` |
| Session / players | Session `D_800AD138`: input block at +0, player count `D_800AD164`, `currentPlayer` `D_800AD168`. Players `D_8009B190[2]`, stride 0xDB4: actor +8, score +0x18, lives +0x1C, input block +0x34, wave `valueD70` (`D_8009BF00` / `D_8009CCB4`). Live wave `D_800BA748`. | decomp `include/` structs; RecompiledFuncs |
| Actors | `GameActor`, 0x7C bytes, list head `D_800AA708`: objectIndex +0x0C (s16), timer0E +0x0E, flags +0x14, kind +0x1C (0 enemy, 2 player), anim +0x1F, resource +0x24 (scale at res+0x0C), callback44 +0x44, field48 timer +0x48, behavior +0x5C, position x/y/z +0x60, next +0x78. Flags: 0x40 = call callback44 when the one-shot animation ends; 0x4000 = invulnerable blink; 0x400 = pickup invincibility. Collision skips anim 1 or `flags & 0x4400`. | decomp `player/behavior.c`, actor update `func_80029FD8` |
| Style-3 gaps (patched) | Menus and skip read only player 1's input (the second player's block is merged in, `func_80022D24` @ 0x80022E90). Enemies target `currentPlayer` (swapped to the nearest living player around each behavior call). The revive just stands the player up where they fell, so a crowd keeps them down. Only the last player out got a high-score entry. The HUD wave of the player with no lives stopped updating. | play test 2026-10-29; `src/game/coop.cpp` |
| Wave-start drop-in | The player model's animation track 6, with z = 500. Wave setup hides the players (`func_80035CC8(1)`). Intro state 0 runs `func_80035CC8(0)`: `func_80027AB8(a, 6, 1)`, zero +0x2C/+0x6C/+0x70, `func_80039514(obj, 0)`, `func_80039E5C(obj, 1)`, flags \|= 0x40, callback44 = `func_80029194` (stands up, anim 8, field48 = now), timer0E = 999. The behavior does nothing in anim 6, and 0x4000 stays set until about 1 s after landing. The co-op respawn copies this per-actor body (calling `func_80035CC8` mid-wave would restart every player). | decomp `scene_actor_mode_list.c`; research 2026-10-29 |
| Floor ring and aim pose | The player behavior `func_80033974` leaves "last player" globals: actor `D_800BEF60`, fire angle or -1 `D_800BEF6C`, facing `D_800BEF64`. The draw loop `func_8003A8B0` turns the actor into one object record `D_800BF2C0 = &D_800BF918[objectIndex]` (stride 0x78). That object's shadow (`func_8000AC6C`, per object with `property12`) is drawn as the yellow ring (others black, 0x8003AB7C), and its arm is blended toward the fire angle (`func_8003B2B0`, 0x8003B0A4). The actor list is newest-first, so in style 3 P1 ran last and P2 got neither; patched by saving each player's values at the behavior epilogue (0x80035174) and restoring them per object in the draw loop (0x8003A990). | decomp `object_runtime_update.c`, `ground_disc.c`, `object_runtime_projection.c`; research 2026-10-29 |
| High-score flow | Screen state `D_800AD280`: 12 name entry, 8 TOP5 + save, 4 title; step `D_800AD28C`; the entry is for the player at `0x800AD2F0`. Ranking: `rank++` while `rank < 20` and `table[rank] >= score`, so a tie loses and rank 20 skips the entry. Table `D_80075DA0`, 20 × 0x14 bytes. | decomp `high_score*.c`; RecompiledFuncs |

## Rendering

| Fact | Value | Source |
|---|---|---|
| Background gradient | A 3D quad at Z = 32000 with half-extents `D_8007CCA8`/`D_8007CCAC`, which `func_800400D0` resets every frame to 19200 × 14400 (or 19200 × 11000). Five routines read the extents and load Z with `addiu rX, $zero, 0x7D00`: `func_80040F5C` (0x800410A8), `func_80041180` (0x80041274), `func_80040BA0` (0x80040DE0), `func_800414C4` (0x800418BC), `func_80041B24` (0x80041E30). They draw with the game's 4:3 perspective (`func_800493F4`) and render mode `005049D8` (Z compare, no Z update), so the quad can be moved closer safely. It covers about 96% of the view; on a TV the rest was hidden in overscan. Our hooks grow it, widen it for Expand, and past ~2.15:1 (where X would overflow s16) divide X, Y and Z by the same factor. | decomp `renderer_surfaces/background_dispatch.c`, `color_gradient.c`, `border_gradient.c`, `boot/frame_projection.c`; grep of loads of both globals in RecompiledFuncs |
| Arena floor per level | `func_80022BFC` reads `D_80074C08[level]` (210 × 16 bytes: kind, floor index, colour pair; +0x0C title text). Floor file is `D_8007CCC0[index]`, one of 13 used `<name>.flr` (32×32 RGBA16, wrap), loaded by `func_80042830`. It is drawn as a 5×5 grid or, with scene flag +0xD0C, as a round floor. `LEVELNOS.DAT` maps level index → wave (0 = bonus): 200 waves + 10 bonus rounds. Full table, hashes and bonus rounds: docs/LEVELS.md. | decomp `session_background_select.c`, `renderer_texture_file_cache.c`, `object_runtime_service.c`; ROM via `tools/level_table.py` |
| Menu background | Background kind 0, used by title.scr, menu.scr, CREDITS.SCR and other scripts, is `func_80041B24(r1,g1,b1, r2,g2,b2)`, assembly only. It draws a 4×4 vertex grid over the background quad (vertex k = row × 4 + col; X = ±W, ±W/3, Y = ±H, ±H/3), split into 18 smooth-shaded triangles with every cell cut along the same diagonal. It uses no texture and nothing moves: per vertex, `c = 128 + 127·sins(k · time · 8)`, and the colour is `(c·first + (256−c)·second) >> 8`. `time` advances 750 per second, so vertex k has a period of 10.9/k s, and vertex 0 stays purple. `func_800045E4` sets first = (0x80,0,0) red and second = (0,0,0x80) blue. The launcher's ColorWaveElement draws the same kind of field (our code). | decomp `renderer_surfaces/background_dispatch.c`, `movie_start.c`, `docs/renderer-color-wave.md`, disassembly lines 74831-75196 |
| In-game HUD | `func_8004B590` (15 args, last = player count) draws score, lives, WAVE, power-up icons, ticker text and bonus counter as 3D quads at camera z = 200 (projection `func_800493F4(400)`), so RT64's HUD Placement can't move them. Every position goes through `func_80049DD8(x, y, z)` with literal coordinates; +x is screen-left and the 4:3 half-width at z = 200 is ~125. 1P: score x 10 (centred), lives 'X' −84 / number −96 / head −72 at y 76, WAVE from −40 at y −78, power icons 40, 28, 16, 4, −8 at y −78, ticker from 100 at y −78, bonus counter from −80 at y 60. 2P: scores ±95 at y 76, lives 83/71/95 and −80/−92/−68 at y 64, WAVE 95 and −36 at y −78. The only other caller of the setter is the status text `func_8004B340`; menus use `func_8004A2B4`. The function has a single `jr $ra` at 0x8004BBBC. | decomp `renderer_text/game_hud.c`; play test 2026-10-29 |
| Frame pacing | One retrace per VI (`func_80050440(..., 1)`). The main loop waits for task-done plus the next retrace, so it runs at up to 60 fps and drops to 30/20 when a frame is slow. The timestep is variable: `func_80038598` sets `D_8009EF94` = elapsed ms × 0.75 (clamped to 100), which scales movement and timers. | decomp `boot/startup.c`, `boot/frame_render.c`, `game/scene_frame_tick.c` |
| Matrices | A per-object model→camera matrix from a per-frame pool (`D_80126B90[250]`, index `D_8007D6A8`), built by `func_80047570`, `func_80047A08` and `func_80047D88` (about 36 call sites). The camera is applied on the CPU per object (`func_8004D4B4`), and joint vertices are transformed on the CPU in model space. RT64's automatic interpolation follows the per-object matrices without tagging (measured 120/240 fps); joint animation stays at the game rate. | decomp `renderer_matrix_*.c`, `frame_transform.c`, `model_hierarchy_vertices.c`; play test 2026-10-28 |
| Frame clear | Z and colour buffers are filled (0..319 × 0..239) every frame; there's no full-screen fade rectangle. | decomp `boot/frame_begin.c` |
| Object view culling | `func_8003D43C`, `func_8003D7F4` and `func_8003E220` (model draw routines, binary fallbacks in the decomp) draw an object only if `z+500 >= 2\|x\|` and `z+500 >= 2\|y\|` in camera space: a fixed square cone with tan 0.5, tested at the object's centre. The sibling routines `func_8003DB7C`, `func_8003DF38`, `func_8003E6A0` and `func_8003E8D0` don't cull. Our hooks widen the x test for Expand (plus a 1.2 margin). | decomp disassembly `build/analysis/cpu/baserom_80000400.text.s`; play test 2026-10-28 |
| Camera views | The camera director `func_80012950` (asm only, L20477-23047) reads the view from the current player record (`D_8009B190[cur]+5`). L or R calls `func_8001276C`, which steps through `D_80073864 = {10, 1, 9, 3, 0}` (index `D_8007388C`, wraps at 0). The HUD name comes from `func_800126E0`, which looks the view up in that table and returns `D_80073850[i]`: ARENA, SMART, THIRD, OVERH, FIRST. Five names for four views: FIRST (view 8) was cut from the cycle. View 1 SMART is the default follow camera (zoom 500, fovy = atan(480/500) = 43.8 degrees, adaptive zoom). Pause forces view 10 and restores the view afterwards. | disassembly; Explore pass 2026-10-30 |
| First-person view (cut) | View 8: eye at the player x/z, world height +600, pitch 0, zoom 166.67 (fovy 70.9 degrees), yaw from `D_800BEF68`. It sets `D_8007394C` (cleared at the top of the director each tick), which turns on tank controls (`actor_manual_motion.c`: stick X turns, stick Y walks, D-pad the same, Z free-looks) and facing-relative shots (`func_80036064`). Tank mode reads controller port 0 only, and in style 3 the view follows a mix of both players. View 2 is a similar eye-level camera that turns toward the fire direction. Our hooks put view 8 back after OVERH (option First-Person Camera: Classic keeps these controls, Modern replaces them with left stick walk/strafe, right stick turn, A/B fire via a hook at the func_80035E3C entry; single player), name it on the HUD, skip the player object in the draw loop, widen culling by 0.95/0.5 in x and 0.71/0.5 in y, and set the near plane to 25. | disassembly; `src/game/first_person.cpp` |
| Near plane | `func_80048DDC` (`renderer_projection/setup.c`) builds the projection every frame: fovy = atan(480/zoom), aspect 4/3, near 100, far from `D_FLT_80095460`; near is stored at sp+0x10 (0x80048F20) before the perspective call. Camera-space units are world/2, so near is 200 world units (about 714 actor units). | decomp; disassembly |
| Transition trails | The level-intro screens ("PLAYER 1 UP …") leave smeared or ghost copies of the text and the player model. The N64 footage shows the model smeared too, so this is the game's own effect; the user found it looks right in play (2026-10-29). | same |
| Crave intro | A 3D logo disc carried in by a robot; it matches the N64 | same |
| Intro cabinet side art | The "2084" side of the arcade cabinet in the intro is three 32×32 CI8 textures stacked top to bottom (`79397743d7509ae3`, `d915aa5d6975223f`, `99970cfc39343713`), loaded in wrap mode (`cms`/`cmt` 0, mask 5). Each is drawn with texture coordinates 0..31, so only 31 of its 32 rows show. This is inferred, not read from the model data: with RT64 shift `none`, a 1024 px replacement lost about 32 px at each seam and its top edge picked up the wrapped bottom row. Replacements need shift `half` and overlapping slices (docs/TEXTURE_PACKS.md). | dump `.v5.tile.json`; screenshot measurement 2026-10-30 |

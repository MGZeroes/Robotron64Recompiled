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
| Runtime Pak support | none yet: N64ModernRuntime `librecomp/src/pak.cpp` returns `PFS_ERR_NOPACK` for every `osPfs*` call | runtime `cdf5abb` |
| Threads | main (pri 10), PI manager (150), game loop (10), thread @ `0x80051380` (110), 3 scheduler threads | decomp `docs/startup.md` |
| Odd PI reads | 16 reads from `0x00FFB000`–`0x00FFB03C` at startup, results unused | decomp `docs/startup.md` |

## Input

| Fact | Value | Source |
|---|---|---|
| Direction bits | `func_8003C1A8` turns the stick, D-pad and C buttons into direction bits (stick threshold ±20; an axis is cleared when the other is 8× larger). These bits are 8-way. | decomp `docs/controller-input-mapping.md`, `src/game/controller_input.c` |
| Analog movement | In play, the player moves with the raw stick of their port (`D_8013DBB8/C8[port]`, scaled by speed), which is analog. The direction bits are only used when the stick is idle. | decomp `src/game/player/behavior.c` (movement block) |
| Two-controller aiming | Player 1 uses port 0, or port 2 for player 2 when it's connected. If the **next** port is connected, its analog stick is the aim vector (`secondaryX/Y`, threshold 0x10). The shot angle is `func_8003CD4C(y, x)` in 0x1000 units, not 8-way. Held fire bits (0xF0) override it with 8-way fire. The port uses the right stick; the Analog Aiming option controls this. | decomp `src/game/player/behavior.c` (fire block); `src/main/main.cpp` `get_n64_input` |
| Bit groups | Mode 0 → bits 0–3; other modes → bits 4–7. Start `0x100`, A `0x800`, B `0x1000`. L or R toggles `D_8007CCA0`. | same |
| Player sampling | `func_8001A1F0` stores the bits in the player state and then calls `func_8001BFB0`. With `D_800AEE9C` set, it ORs in controller `(port+2)&3` in mode 0. | decomp `src/game/early_player_input_sample.c` |

## Rendering

| Fact | Value | Source |
|---|---|---|
| Background vs 3D area | The background and full-screen fade are 2D rectangles slightly smaller than the area the 3D scene draws into; the floor sticks out past them at the bottom corners. On a TV that border falls into overscan. Not yet traced to code. | play test 2026-10-28, compared with N64 footage the user supplied |
| Expand aspect ratio | RT64 widens the 3D but not those 2D rectangles, so a 4:3 box shows. Needs a patch to the rectangle draws (RT64 extended GBI). | same |
| Transition trails | The level-intro screens ("PLAYER 1 UP …") leave smeared or ghost copies of the text and the player model. The N64 footage shows the model smeared too, so this is probably the game's own effect. Still to check: whether it looks the same at Framerate Original and Display. | same |
| Crave intro | A 3D logo disc carried in by a robot; it matches the N64 | same |

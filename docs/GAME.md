# Robotron 64 internals

Facts as we learn them, each with a source. "Decomp" = frankischilling/robotron64 @ `812a75f`.

## ROM

| Fact | Value | Source |
|---|---|---|
| Supported dump | Robotron 64 (USA) rev 0, `NRXE`, 8 MiB, big-endian | `decomp/config/target.json`, local check |
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
| Audio | **unknown** — to identify in Phase 2 | — |

## Runtime behaviour

| Fact | Value | Source |
|---|---|---|
| Save | Controller Pak, one 4 KiB file | decomp `docs/save-game.md` |
| Threads | main (pri 10), PI manager (150), game loop (10), thread @ `0x80051380` (110), 3 scheduler threads | decomp `docs/startup.md` |
| Odd PI reads | 16 reads from `0x00FFB000`–`0x00FFB03C` at startup, results unused | decomp `docs/startup.md` |

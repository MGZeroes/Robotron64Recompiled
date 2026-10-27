# Phase 0 research

Looked at on **2026-10-27**. Upstream moves fast; re-check anything here before relying on it.

## 1. Prior art

No Robotron 64 recompilation exists.

- GitHub repo search: "robotron recomp" → 0 results; "robotron64" → only the decomp below.
- https://recompedia.com (91 projects listed) — no Robotron.
- https://recomp.fyi — "Robotron" does not appear.
- General web search — nothing.

## 2. The decomp — [frankischilling/robotron64](https://github.com/frankischilling/robotron64)

Commit `812a75f85ca925d8752a8a92c21acbdeab862d9f` (2026-10-11, "Match random-color background grid").

| Topic | Finding | Source |
|---|---|---|
| Target ROM | Robotron 64 (USA) rev 0, 8 MiB, `NRXE`, entry `0x80000400`, SHA-1 `44d158bc2aeefb111a620b61e043b2703e6c5808`, SHA-256 `91d85baeca4b9517e93b3637b52909cee942b09e2fe44a37df9ded17687faddd`, MD5 `5698757883a6f46fe5b4c9b6e780b480`, CRC `ac8e4b32`/`e7b47326` | `config/target.json` |
| ROM normalization | `tools/rom.py` fixes byte order only, then hard-checks size + SHA-1 + SHA-256 + MD5 | `tools/rom.py` |
| Toolchain | IDO 5.3 (static-recompiled, checksum-pinned, fetched by `make setup` into `.local/toolchain/5.3/cc`), GNU make, `binutils-mips-linux-gnu`, Python ≥ 3.12. Not splat-based: custom `tools/extract.py`. Linux/WSL2 only (only `tools/rom.py` claimed to work on Windows). | README, `Makefile` |
| Build | `python3 tools/rom.py <rom> --output baseroms/us/baserom.z64`; `make setup`; `make -j2`; `make verify` | README |
| Outputs | `build/us/robotron64.elf`, `build/us/robotron64.map` (via `mips-linux-gnu-ld -T linker_scripts/us.ld`), then `objcopy` → `build/us/robotron64.z64` | `Makefile` |
| Layout | One flat image, VRAM = ROM + `0x7FFFF400`. CPU code ROM `0x1000`–`~0x70040` (VRAM `0x80000400`–`0x8006F440`). BSS `0x80097290`–`0x80197640`. No overlays, no compressed code. | `docs/rom-map.md`, `linker_scripts/us.ld` |
| Huffman | DEFLATE-style inflate at `0x8005DA20`–`0x8005FBB0`, used for assets (no evidence its output is executed) | `docs/compression-runtime.md` |
| Progress | 1,437 matched C functions (321,808 B) + 29 asm functions (4,372 B); **128,376 B in 180 unmatched spans** emitted as `.incbin` in `build/us/fallback.o` **without symbols**. ~50 absolute `func_`/`D_` symbols in `us.ld` (size 0). | README, `Makefile`, `us.ld` |
| Symbol sizes | Hand-written asm emits `.type @function` + `.size` explicitly (e.g. `src/game/audio_interrupt_service.s`). IDO objects: expected to carry `st_size` — **to verify on the built ELF**. | source |
| Microcodes | Graphics: F3DEX 1.21 (string at ROM `0x96E80`), F3DLP.Rej 1.21 (`0x97680`); selection logic not traced. rspboot candidate ROM `0x70040`–`0x70110`. Audio ucode not identified ("SDK synthesizer"). libultra version under investigation. | issue #6, `docs/audio-synthesis-startup.md` |
| Save | 4 KiB Controller Pak file; no EEPROM/SRAM mentioned | `docs/controller-pak-menu.md`, `docs/save-game.md` |
| Threads | main (pri 10), PI manager (150), game loop (10), thread 110 @ `0x80051380`, 3 scheduler threads | `docs/startup.md` |
| Oddity | Startup does 16 PI reads from `0x00FFB000`–`0x00FFB03C` (beyond the 8 MiB ROM); results unused | `docs/startup.md` |
| License | No LICENSE file (GitHub API: `null`). `CREDITS.md` asks that component licenses be respected; no credit format given | repo root |
| Issues | 27 open, all function-matching; none about build blockers. #6 = microcodes. | GitHub issues |

## 3. N64Recomp toolchain (heads on 2026-10-27)

| Repo | Commit | Date | License |
|---|---|---|---|
| [N64Recomp](https://github.com/N64Recomp/N64Recomp) | `ffb39cdad1da` | 2026-05-27 | MIT |
| [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) | `cdf5abbd5026` | 2026-08-30 | GPL-3.0 |
| [rt64](https://github.com/rt64/rt64) | `d9817c68ffd7` | 2026-10-23 | MIT |
| [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) | `2ddbfc8adcfe` | 2026-10-16 | GPL-3.0 |

**N64Recomp** (`src/config.cpp` @ `ffb39cd`; README is outdated on several points):
- `[input]`: only `output_func_path` is required. Optional: `entrypoint`, `elf_path`, `symbols_file_path`, `rom_file_path`, `relocatable_sections_path`, `uses_mips3_float_mode`, `bss_section_suffix` (default `.bss`), `single_file_output`, `use_absolute_symbols`, `unpaired_lo16_warnings`, `use_mdebug`, `mdebug_file_mappings`, `recomp_include`, `functions_per_output_file` (50), `trace_mode`, `manual_funcs` (`{name, section, vram, size}`), `function_sizes` (`{name, size}`).
- Patch mode keys: `output_binary_path`, `func_reference_syms_file`, `data_reference_syms_files`, `allow_exports`, `strict_patch_mode`.
- `[patches]`: `stubs`, `ignored`, `renamed`, `instruction` (`{vram, func, value}`), `hook` (`{func, text, before_vram?}`).
- **Metadata without an ELF is supported**: `symbols_file_path` → syms toml (`[[section]]` with `rom`, `vram`, `size`, `name`, `functions[{name,vram,size}]`, `relocs[...]`). `--dump-context` produces it from an ELF.
- **RSPRecomp** (top-level keys): required `text_offset`, `text_size`, `text_address`, `rom_file_path`, `output_file_path`, `output_function_name`; optional `extra_indirect_branch_targets`, `unsupported_instructions`, `overlay_slots`.
- Tools built: `N64Recomp`, `RSPRecomp`, `RecompModTool`, `OfflineModRecomp`, `RecompModMerger`, `LiveRecompTest`. CMake ≥ 3.20, C++20.
- Patch ELF pipeline (Zelda64Recomp, template): clang `-target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls`, `ld.lld --emit-relocs --unresolved-symbols=ignore-all`, then `N64Recomp patches.toml` → `patches.bin` embedded via `file_to_c`.

**N64ModernRuntime** (`librecomp/include/librecomp/game.hpp` @ `cdf5abb`): `recomp::GameEntry { rom_hash, internal_name, display_name, game_id, mod_game_id, save_type, thumbnail_bytes, is_enabled, decompression_routine, has_compressed_code, entrypoint_address, entrypoint, thread_create_callback, on_init_callback }`; `register_config_path`, `register_game`, `start(Configuration)`. Developed with Clang 15+; MSVC/GCC untested.

**RT64**: `src/gbi` contains F3D, F3DEX, F3DEX2, F3DZEX2, L3DEX2, F3DGOLDEN, F3DPD, F3DWAVE, S2DEX, S2DEX2, RDP, extended GBI. No explicit F3DLP.Rej entry (see risks).

**RecompFrontend**: `recompinput` (SDL2 input, mapping, per-GUID profiles) + `recompui` (RmlUi menus, mod menu, UI API). Integrate with `RECOMP_FRONTEND_N64MODERNRUNTIME_PATH` / `RECOMP_FRONTEND_RT64_PATH` + `add_subdirectory`, link `recompui recompinput`.

**Windows prerequisites** (Zelda64Recomp `BUILDING.md`, template `CMakeLists.txt`): VS 2022 (C++ workload; Clang provided separately here), CMake ≥ 3.20, Ninja, `make` for patches, SDL2 2.30.3 VC devel (FetchContent). No minimum LLVM version enforced.

## 4. Template comparison

| Port | Latest commit | Frontend | Symbols source | Runtime pin |
|---|---|---|---|---|
| [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) | `b65c482` 2026-09-25 | own RmlUi (`recomp-frontend` branch exists) | syms submodule | `df7e820` 2025-08 |
| [MarioKart64Recomp](https://github.com/sonicdcer/MarioKart64Recomp) | `3f72d23` 2026-08-11 | own RmlUi | syms repo | `ff5daef` 2025-08 |
| [Goemon64Recomp](https://github.com/klorfmorf/Goemon64Recomp) | `6661abc` 2026-09-19 | own RmlUi | syms repo | fork, 2026-01 |
| [drmario64_recomp](https://github.com/AngheloAlf/drmario64_recomp) | `7892264` 2025-08-12 | own RmlUi | decomp + syms | `df7e820` 2025-08 |
| [HarvestMoon64Recomp](https://github.com/HarvestMoon64Recomp/HarvestMoon64Recomp) | `ddd991c` 2026-09-25 | **RecompFrontend** `b3b7ebb` | syms toml | `ca568b6` 2026-01 |
| [Quest64-Recomp](https://github.com/Rainchus/Quest64-Recomp) | `ea94271` 2026-09-29 | own RmlUi | syms submodule | fork, 2025-11 |
| [N64RecompGameTemplate](https://github.com/SrBananaMan/N64RecompGameTemplate) | `be3271c` 2026-06-19 | **RecompFrontend** `b3b7ebb` | ELF → `--dump-context` | `ca568b6` 2026-01 |
| [body-harvest-recomp](https://github.com/danielgomesvieira2000/body-harvest-recomp) | `711a2b6` 2026-09-17 | RecompFrontend `b1a1477` | decomp ELF via WSL + fixups | `cdf5abb` 2026-08 |

Midway-era: [gauntlet-legends-recomp](https://github.com/Auxnon/gauntlet-legends-recomp) (experimental, Linux-only, TLB emulation); navjack's Cruis'n recomps (macOS). No N64 Rampage recomp.

Decision: see [ADR 001](decisions/001-base-template.md).

## 5. Risks

1. **Unmatched code without symbols** (128 KB, 180 spans) — N64Recomp skips sizeless code. Need a symbol-synthesis step (boundaries from map gaps, `jr ra` scan, `us.ld` absolutes; jump tables need care).
2. **F3DLP.Rej** not explicitly listed in RT64's GBI files — verify in RT64 source (it is F3DEX-family; may map onto F3DEX).
3. **Audio microcode** unidentified — scan ROM for the audio task's ucode pointers.
4. **Out-of-range PI reads** at `0x00FFB000` — runtime may need to tolerate them.
5. **Controller Pak save** — most ports use EEPROM/SRAM/Flash; verify N64ModernRuntime Pak support.
6. **Decomp has no license** — fine for local builds (we commit nothing derived from it), but noted.
7. **Pin bump** — template was validated on Jan-2026 runtime/frontend; we use newer heads (ADR 002), so template code may need API adjustments.

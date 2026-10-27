# 001 — Base template: SrBananaMan/N64RecompGameTemplate

Date: 2026-10-27. Status: accepted (chosen by the user).

## Context
We need a host project (CMake, main, config, input/UI, patch build, CI) around the recompiled
code. Candidates are compared in [RESEARCH.md §4](../RESEARCH.md#4-template-comparison).
Wanted: small, current with upstream, uses the shared RecompFrontend, stock (unforked) runtime.

## Options
1. **N64RecompGameTemplate** (`be3271c`, 2026-06-19) — HarvestMoon64Recomp stripped to
   placeholders, `configure_game.py`, `User Task:` markers, RecompFrontend, stock runtime, CI.
2. **Fork HarvestMoon64Recomp** — proven real port, but more HM64-specific code to remove.
3. **body-harvest-recomp** — newest pins and same decomp→ELF pipeline, but self-described
   "very untested", no CI, patches its submodules in place.
4. Zelda64Recomp / MK64 / Goemon / Quest64 / Dr. Mario — own RmlUi UI (not RecompFrontend),
   older or forked runtimes.

## Decision
Option 1. HarvestMoon64Recomp is the cross-reference for real-game glue; body-harvest-recomp is
a reference for the decomp-ELF → symbols pipeline only.

Applied with:
`configure_game.py "Robotron" --game-id robotron64 --project-name Robotron64Recompiled --rom-stem robotron64.us --app-id io.github.robotron64recomp.robotron64recompiled`,
then display names fixed to "Robotron 64" by hand (the script rejects digits in names).
`configure_game.py` itself and the template `.gitmodules` were not copied.

## Consequences
- Template has no LICENSE file but derives from GPL-3.0 code and links GPL-3.0 libraries →
  this project is GPL-3.0 (`LICENSE`).
- Template's decomp location `lib/<decomp>` is replaced by `decomp/` (CLAUDE.md layout);
  `patches/Makefile` `DECOMP_DIR` must point there.
- Template was validated against older pins; see ADR 002.

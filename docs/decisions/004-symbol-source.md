# 004 — Where the recompiler's function symbols come from

Date: 2026-10-27. Status: accepted.

## Context
N64Recomp only recompiles code covered by a function symbol with a size, and it swaps
libultra functions for runtime versions **by name**. The decomp ELF builds byte-exact, but:
- ~128 KB of code (180 spans) is unmatched `.incbin` in `fallback.o`, with no symbols at all;
- only 18 libultra functions have their real names; the rest are `func_XXXXXXXX` or unmatched.
A libultra function recompiled as plain code pokes hardware registers and breaks.

## Options
1. `N64Recomp --dump-context` from the ELF, then hand-add the missing functions.
2. Generate the symbols file ourselves from: the decomp's exact matched functions
   (`config/functions.json`), the decomp's own spimdisasm pass over the whole CPU range
   (`make analyze`, boundaries for everything), and **n64sym** signature hits for libultra names.

## Decision
Option 2, implemented in `tools/gen_syms.py`. It writes `syms/robotron64.us.syms.toml`, which is
gitignored and regenerated locally:
- **Boundaries.** spimdisasm records, split wherever the decomp knows a function starts that spimdisasm
  merged. That happens 3 times, all in the exception handler.
- **Sizes.** The decomp's exact sizes for matched functions, spimdisasm lengths for the rest.
- **Names.** In priority order: decomp names that aren't `func_*`, then n64sym libultra names, then
  decomp `func_*` names, then spimdisasm.
- **Which n64sym hits count.** Only hits that land on a function start inside the contiguous libultra block
  (`0x8005FBB0`–`0x8006F440`). Hits below that are tiny-function false positives (e.g. `osDestroyThread`
  matched three times). Two-name hits are resolved by uniqueness elsewhere, by an explicit override with
  a reason, or are left unnamed. Every decision goes to `build-elf/gen_syms_report.txt`.
- **Checks.** No overlaps, word alignment, every decomp function present with its size, and every
  uncovered gap is zero padding. The script fails if any check fails.

The ELF and map are kept for debugging and as the proof of a byte-exact build (`make verify`).
They are not the symbol source.

## Consequences
- The script needs no manual symbol edits. When the decomp matches more code, re-run
  `tools/build_elf.ps1` and `gen_syms.py` to pick up the new names and sizes.
- The n64sym pin (`ccf4600`) and the `LIBULTRA_START` cutoff are assumptions. They are
  documented in the script.
- Data symbols for patches (`datasyms.toml`) are not generated yet; that comes with the first patch (Phase 3).

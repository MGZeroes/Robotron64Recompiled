#!/usr/bin/env python3
"""Generate the N64Recomp symbols file for Robotron 64 (ADR 004).

Inputs (all produced by tools/build_elf.sh and n64sym, in build-elf/):
  - decomp/config/functions.json   exact names + sizes of matched/asm functions
  - build-elf/functions.csv         spimdisasm boundaries for the whole CPU range
  - build-elf/n64sym*.txt           libultra signature hits

Output: syms/robotron64.us.syms.toml (one .text section) plus a decision report in
build-elf/gen_syms_report.txt. Exits non-zero if a consistency check fails.
"""

import argparse
import collections
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

ROM_START = 0x1000
VRAM_START = 0x80000400
VRAM_END = 0x8006F440  # rspboot text begins here (decomp config/analysis.json)

# n64sym hits are only trusted inside the contiguous libultra block at the end of code.
# Below it, hits on small game functions were false positives (e.g. osDestroyThread at
# 0x80019E40, leoInitUnit_atten). The decomp's own libultra names all lie above this too.
LIBULTRA_START = 0x8005FBB0

# n64sym names for file-local statics ("contreaddata_text_017C") are not real symbols.
PLACEHOLDER_NAME = re.compile(r"_(text|data|rodata|bss)_[0-9A-F]{4}$")

# Hits where n64sym reports two names for byte-identical code.
OVERRIDES = {
    # Alias pairs: keep the public libultra name (the one N64Recomp's lists use).
    0x80063220: "sqrtf",
    0x80063230: "sinf",
    0x800633F0: "cosf",
    0x800674C0: "bzero",
    # Si vs Sp raw IO: both call 0x8006DBF0 (__osSiDeviceBusy), so they are the SI variants.
    0x800669B0: "__osSiRawReadIo",
    0x80066A00: "__osSiRawWriteIo",
    # n64sym says osPiWriteIo, but the game reads ROM words with it
    # (decomp src/game/rom_files.c: func_80063560(deviceAddress, &word)).
    0x80063560: "osPiReadIo",
    # n64sym matches __osGetId twice. This one is called by game code and its body reads,
    # checks and repairs the pak ID (decomp src/sdk/pfs_repair_id.c); the other has no callers.
    0x800617A0: "__osGetId",
    0x80068F44: None,
}


def load_n64sym(paths):
    """Merge n64sym outputs. Normalizes its 0x70xxxxxx/0x80xxxxxx address quirk."""
    hits = collections.defaultdict(set)
    for path in paths:
        for line in path.read_text().splitlines():
            parts = line.split()
            if len(parts) < 2:
                continue
            vram = (int(parts[0], 16) & 0x00FFFFFF) | 0x80000000
            hits[vram].add(parts[1])
    return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=ROOT / "syms/robotron64.us.syms.toml")
    parser.add_argument("--report", type=Path, default=ROOT / "build-elf/gen_syms_report.txt")
    args = parser.parse_args()

    errors = []
    report = []

    decomp = {f["vram"]: f for f in json.loads((ROOT / "decomp/config/functions.json").read_text())}
    with (ROOT / "build-elf/functions.csv").open(newline="") as stream:
        spim = {int(r["address"], 16): int(r["length"], 16) for r in csv.DictReader(stream)}
    n64sym = load_n64sym(sorted((ROOT / "build-elf").glob("n64sym*.txt")))

    # Function starts: spimdisasm's, split wherever the decomp knows a function starts that
    # spimdisasm merged into its predecessor (e.g. __osException after __osExceptionPreamble).
    starts = sorted({v for v in spim if VRAM_START <= v < VRAM_END}
                    | {v for v in decomp if VRAM_START <= v < VRAM_END})
    for vram in starts:
        if vram not in spim:
            report.append(f"split   {vram:08X} at decomp function {decomp[vram]['name']}")
    next_start = dict(zip(starts, starts[1:] + [VRAM_END]))
    spim = {v: min(spim.get(v, VRAM_END), next_start[v] - v) for v in starts}

    # Libultra names: accept, then resolve multi-name hits and duplicates.
    candidates = {}
    for vram, names in sorted(n64sym.items()):
        names = {n for n in names if not PLACEHOLDER_NAME.search(n)}
        if not names:
            continue
        if vram < LIBULTRA_START or vram >= VRAM_END:
            if VRAM_START <= vram < VRAM_END:
                report.append(f"reject  {vram:08X} {','.join(sorted(names))}: below libultra block")
            continue
        if vram not in spim:
            report.append(f"reject  {vram:08X} {','.join(sorted(names))}: not a function start")
            continue
        candidates[vram] = names

    single = collections.Counter(next(iter(n)) for n in candidates.values() if len(n) == 1)
    resolved = {}
    for vram, names in candidates.items():
        if vram in OVERRIDES:
            report.append(f"override {vram:08X} {','.join(sorted(names))} -> {OVERRIDES[vram] or 'unnamed'}")
            if OVERRIDES[vram] is None:
                continue
            names = {OVERRIDES[vram]}
        if len(names) > 1:
            # Drop alternatives that are uniquely identified at another address.
            narrowed = {n for n in names if single[n] == 0}
            if len(narrowed) == 1:
                report.append(f"resolve {vram:08X} {','.join(sorted(names))} -> {next(iter(narrowed))}")
                names = narrowed
            else:
                report.append(f"ambig   {vram:08X} {','.join(sorted(names))}: left unnamed")
                continue
        resolved[vram] = next(iter(names))

    counts = collections.Counter(resolved.values())
    for vram, name in list(resolved.items()):
        if counts[name] > 1:
            report.append(f"dup     {vram:08X} {name}: found {counts[name]}x, left unnamed")
            del resolved[vram]

    # Build the function list.
    functions = []
    for vram in sorted(spim):
        if not VRAM_START <= vram < VRAM_END:
            continue
        size = spim[vram]
        name = f"func_{vram:08X}"
        if vram in decomp:
            name = decomp[vram]["name"]
            size = decomp[vram]["size"]  # exact; spimdisasm lengths include trailing padding
        if vram in resolved:
            if vram in decomp and not decomp[vram]["name"].startswith("func_") and decomp[vram]["name"] != resolved[vram]:
                report.append(f"keep    {vram:08X} decomp name {decomp[vram]['name']} over n64sym {resolved[vram]}")
            else:
                name = resolved[vram]
        functions.append((vram, size, name))

    # Consistency checks.
    names = collections.Counter(name for _, _, name in functions)
    errors += [f"duplicate function name {n}" for n, c in names.items() if c > 1]
    covered = 0
    gaps = []
    prev_end = VRAM_START
    for vram, size, name in functions:
        if vram % 4 or size % 4 or size == 0:
            errors.append(f"{name} @ {vram:08X} size 0x{size:X} is not word aligned / empty")
        if vram < prev_end:
            errors.append(f"{name} @ {vram:08X} overlaps the previous function (ends {prev_end:08X})")
        elif vram > prev_end:
            gaps.append((prev_end, vram - prev_end))
        covered += size
        prev_end = vram + size
    if prev_end < VRAM_END:
        gaps.append((prev_end, VRAM_END - prev_end))
    if prev_end > VRAM_END:
        errors.append(f"last function ends past the CPU range ({prev_end:08X})")

    rom = (ROOT / "baserom.us.z64").read_bytes()
    nonzero_gaps = []
    for start, length in gaps:
        offset = start - VRAM_START + ROM_START
        if any(rom[offset:offset + length]):
            nonzero_gaps.append((start, length))

    renamed = sum(1 for vram in resolved if vram in {f[0] for f in functions})
    summary = [
        f"functions: {len(functions)} ({sum(1 for f in functions if f[0] in decomp)} from decomp, {renamed} libultra names from n64sym)",
        f"covered: 0x{covered:X} of 0x{VRAM_END - VRAM_START:X} bytes; {len(gaps)} gaps, {len(nonzero_gaps)} with non-zero bytes",
    ]
    report += [f"gap     {s:08X} +0x{l:X} (non-zero)" for s, l in nonzero_gaps]

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text("\n".join(summary + report + [f"ERROR {e}" for e in errors]) + "\n")
    print("\n".join(summary))
    print(f"report: {args.report.relative_to(ROOT)}")
    if errors:
        print("\n".join(f"error: {e}" for e in errors[:20]), file=sys.stderr)
        return 1

    lines = [
        "# Generated by tools/gen_syms.py -- do not edit. Derived from the ROM; not committed.",
        "[[section]]",
        'name = ".text"',
        f"rom = 0x{ROM_START:X}",
        f"vram = 0x{VRAM_START:08X}",
        f"size = 0x{VRAM_END - VRAM_START:X}",
        "",
        "functions = [",
    ]
    lines += [f'    {{ name = "{name}", vram = 0x{vram:08X}, size = 0x{size:X} }},' for vram, size, name in functions]
    lines.append("]")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n")
    print(f"wrote {args.out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

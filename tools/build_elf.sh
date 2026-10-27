#!/usr/bin/env bash
# Builds the decomp's matching ELF and its spimdisasm analysis, then copies the
# outputs we use into build-elf/. Runs inside the Docker build image (ADR 003):
#   docker run --rm -v <repo>:/work robotron64-recomp-build bash tools/build_elf.sh
# Use tools/build_elf.ps1 on Windows.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DECOMP=$ROOT/decomp
OUT=$ROOT/build-elf
JOBS=$(nproc)

# Windows checkouts with core.autocrlf=true can turn scripts into CRLF, which breaks make/python here.
if grep -lq $'\r' "$DECOMP/Makefile" "$DECOMP/tools/rom.py" "$DECOMP/tools/extract.py"; then
    echo "error: decomp files have CRLF line endings; re-checkout the submodule with core.autocrlf=false" >&2
    exit 1
fi

cd "$DECOMP"
echo "== ROM"
mkdir -p baseroms/us
python3 tools/rom.py "$ROOT/baserom.us.z64" --output baseroms/us/baserom.z64

echo "== make setup"
make setup
echo "== make -j$JOBS"
make -j"$JOBS"
echo "== make verify"
make verify

echo "== make analyze"
[ -x .venv/bin/python ] || make analysis-setup
make analyze

mkdir -p "$OUT"
cp build/us/robotron64.elf build/us/robotron64.map "$OUT/"
cp build/analysis/functions.csv build/analysis/context.csv build/analysis/summary.json "$OUT/"

# libultra identification for tools/gen_syms.py (ADR 004). Two passes: a thorough scan and a
# normal scan with the base pinned; gen_syms merges both. The thorough scan takes minutes,
# so skip it when its output is newer than the ROM.
cd "$ROOT"
if [ ! build-elf/n64sym.txt -nt baserom.us.z64 ]; then
    echo "== n64sym (thorough)"
    n64sym baserom.us.z64 -s -t -o build-elf/n64sym.txt > /dev/null
fi
if [ ! build-elf/n64sym.h.txt -nt baserom.us.z64 ]; then
    echo "== n64sym"
    n64sym baserom.us.z64 -s -h 0x7FFFF400 -o build-elf/n64sym.h.txt > /dev/null
fi
echo "== done: outputs in build-elf/"
ls -l "$OUT"

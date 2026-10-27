#!/usr/bin/env bash
# Builds N64Recomp/RSPRecomp from the submodule and runs them. Runs inside the Docker build
# image (ADR 003); on Windows use:  .\tools\build_elf.ps1 -Script tools/recomp.sh
# Needs syms/robotron64.us.syms.toml (tools/gen_syms.py) for the CPU recompilation step.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLS=$ROOT/build-tools/linux
cd "$ROOT"

echo "== build N64Recomp + RSPRecomp"
cmake -S lib/N64ModernRuntime/N64Recomp -B "$TOOLS" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$TOOLS" --target N64Recomp RSPRecomp

echo "== RSPRecomp aspMain.us.toml"
"$TOOLS/RSPRecomp" aspMain.us.toml

echo "== N64Recomp robotron64.us.toml"
rm -rf RecompiledFuncs
"$TOOLS/N64Recomp" robotron64.us.toml
echo "== done: $(ls RecompiledFuncs | wc -l) files in RecompiledFuncs/"

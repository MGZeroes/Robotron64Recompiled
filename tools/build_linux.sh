#!/usr/bin/env bash
# Native Linux build of Robotron64Recompiled. Runs inside the Docker build image (ADR 003/005);
# on Windows use:  .\tools\build_elf.ps1 -Script tools/build_linux.sh
# Needs the recompiler output first (tools/recomp.sh). Output: build-linux/, plus a tarball with the
# binary, assets/, recompcontrollerdb.txt and the licences.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$ROOT/build-linux
cd "$ROOT"

# A Windows checkout with core.autocrlf=true would break this script with CRLF line endings.
if grep -q $'\r' "$0"; then
    echo "error: $0 has CRLF line endings; check out with LF (see .gitattributes)" >&2
    exit 1
fi

if [ ! -d RecompiledFuncs ] || [ ! -f rsp/aspMain.cpp ]; then
    echo "error: RecompiledFuncs/ or rsp/aspMain.cpp missing; run tools/recomp.sh first" >&2
    exit 1
fi

echo "== configure"
cmake -S . -B "$BUILD" -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release

echo "== build"
cmake --build "$BUILD" --target Robotron64Recompiled -j "$(nproc)"

echo "== package"
python3 "$ROOT/tools/collect_licenses.py" "$BUILD/licenses"
tar -czf "$BUILD/Robotron64Recompiled-linux-x64.tar.gz" \
    -C "$BUILD" Robotron64Recompiled licenses \
    -C "$ROOT" assets recompcontrollerdb.txt LICENSE CREDITS.md
file "$BUILD/Robotron64Recompiled"
if ldd "$BUILD/Robotron64Recompiled" | grep -q "not found"; then
    ldd "$BUILD/Robotron64Recompiled" | grep "not found" >&2
    echo "error: unresolved shared libraries" >&2
    exit 1
fi
echo "== done: $BUILD/Robotron64Recompiled-linux-x64.tar.gz"

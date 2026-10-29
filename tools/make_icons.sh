#!/usr/bin/env bash
# Renders the app icon from icons/app.svg (and icons/app-small.svg for 16-32 px) into
# icons/app.png (512 px: Linux window icon, Flatpak, macOS bundle) and icons/app.ico (Windows exe and
# window icon). Runs inside the Docker build image (ADR 003); on Windows use:
#   .\tools\build_elf.ps1 -Script tools/make_icons.sh
set -euo pipefail
cd "$(dirname "$0")/.."

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rsvg-convert -w 512 -h 512 icons/app.svg -o icons/app.png
for size in 16 24 32; do
    rsvg-convert -w $size -h $size icons/app-small.svg -o "$tmp/$size.png"
done
for size in 48 64 128 256; do
    rsvg-convert -w $size -h $size icons/app.svg -o "$tmp/$size.png"
done
convert "$tmp/16.png" "$tmp/24.png" "$tmp/32.png" "$tmp/48.png" "$tmp/64.png" "$tmp/128.png" "$tmp/256.png" icons/app.ico
identify icons/app.ico
echo "== wrote icons/app.png, icons/app.ico"

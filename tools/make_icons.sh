#!/usr/bin/env bash
# Renders the app icon from icons/app.svg (and icons/app-small.svg for 16-32 px) into
# icons/app.png (512 px: Linux window icon, macOS bundle), icons/app.ico (Windows exe and
# window icon) and the Android launcher icon layers. Runs inside the Docker build image (ADR 003); on Windows use:
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

# Android adaptive icon: separate background and foreground layers per screen density (108 dp canvas).
python3 tools/make_android_icon_layers.py icons/app.svg "$tmp"
res=android/app/src/main/res
for density in mdpi:108 hdpi:162 xhdpi:216 xxhdpi:324 xxxhdpi:432; do
    dir=$res/mipmap-${density%:*}
    size=${density#*:}
    mkdir -p "$dir"
    rsvg-convert -w "$size" -h "$size" "$tmp/android_background.svg" -o "$dir/ic_launcher_background.png"
    rsvg-convert -w "$size" -h "$size" "$tmp/android_foreground.svg" -o "$dir/ic_launcher_foreground.png"
done
echo "== wrote icons/app.png, icons/app.ico"

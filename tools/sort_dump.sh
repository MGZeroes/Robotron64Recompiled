#!/usr/bin/env bash
# Sorts an RT64 texture dump (dump/, from the inspector's "Start dumping textures") into texture_work/ by the
# classification in tools/texture_classes.txt. Both folders are gitignored: they hold the game's textures.
# Runs inside the Docker build image (ImageMagick); on Windows use:
#   .\tools\build_elf.ps1 -Script tools/sort_dump.sh
#
# texture_work/<category>/<label>__<hash>.png  4x nearest-neighbour copy (pixel-exact), a template to draw over
# texture_work/index.csv                       hash, category, label, size
# texture_work/sheets/<category>.png           labelled overview per category
# Textures not in the classification go to other/ as unknown__<hash>.png; add them to texture_classes.txt.
set -euo pipefail
cd "$(dirname "$0")/.."

dump=dump
work=texture_work
classes=tools/texture_classes.txt
scale=4

[ -d "$dump" ] || { echo "No $dump/ folder: dump textures with RT64's inspector first (docs/TEXTURE_PACKS.md)." >&2; exit 1; }

declare -A category label
while read -r hash cat lab; do
    [[ -z "$hash" || "$hash" == \#* ]] && continue
    category[$hash]=$cat
    label[$hash]=$lab
done < "$classes"

for dir in font cabinet powerups icons arena effects models other sheets; do
    rm -rf "${work:?}/$dir"
    mkdir -p "$work/$dir"
done
echo "hash,category,label,size" > "$work/index.csv"

count=0
unknown=0
for png in "$dump"/*.v5.png; do
    hash=$(basename "$png" .v5.png)
    cat=${category[$hash]:-other}
    lab=${label[$hash]:-unknown}
    [ "$lab" = unknown ] && unknown=$((unknown + 1))
    size=$(identify -format '%wx%h' "$png")
    convert "$png" -filter point -resize "$((scale * 100))%" "$work/$cat/${lab}__${hash}.png"
    echo "$hash,$cat,$lab,$size" >> "$work/index.csv"
    count=$((count + 1))
done

# Overview sheets: templates on grey (white glyphs and transparency stay visible), labelled with the file name.
for dir in font cabinet powerups icons arena effects models other; do
    shopt -s nullglob
    files=("$work/$dir"/*.png)
    shopt -u nullglob
    [ ${#files[@]} -eq 0 ] && continue
    montage -background '#5a5a6a' -fill white -font assets/InterVariable.ttf -pointsize 10 -label '%t' "${files[@]}" \
        -tile 6x -geometry 128x128+40+8 "$work/sheets/$dir.png"
done

echo "== sorted $count textures into $work/ ($unknown unknown, in other/)"

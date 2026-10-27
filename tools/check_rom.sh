#!/usr/bin/env sh
# Verifies the Robotron 64 (USA) ROM and normalizes it to big-endian .z64.
# Accepts .z64 / .v64 (byte-swapped) / .n64 (word-swapped).
# Usage: tools/check_rom.sh [rom] [output]   (defaults: baserom.us.z64 for both)
# Exit code 0 = ROM matches the expected hash, 1 = mismatch or bad input.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ROM=${1:-$ROOT/baserom.us.z64}
OUT=${2:-$ROOT/baserom.us.z64}

# Robotron 64 (USA), rev 0 -- same values as decomp/config/target.json
EXPECTED_SHA1=44d158bc2aeefb111a620b61e043b2703e6c5808
EXPECTED_SIZE=8388608

[ -f "$ROM" ] || { echo "ROM not found: $ROM" >&2; exit 1; }
size=$(wc -c < "$ROM" | tr -d ' ')
[ "$size" -eq "$EXPECTED_SIZE" ] || { echo "Unexpected size $size (expected $EXPECTED_SIZE)" >&2; exit 1; }

magic=$(od -An -tx1 -N4 "$ROM" | tr -d ' \n')
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
case "$magic" in
    80371240) format=z64; cp "$ROM" "$tmp" ;;
    37804012) format=v64; dd if="$ROM" conv=swab status=none of="$tmp" ;;
    40123780) format=n64
        python3 -c 'import sys; d=bytearray(open(sys.argv[1],"rb").read()); d[0::4],d[1::4],d[2::4],d[3::4]=d[3::4],d[2::4],d[1::4],d[0::4]; open(sys.argv[2],"wb").write(d)' "$ROM" "$tmp" ;;
    *) echo "Not an N64 ROM (magic $magic)" >&2; exit 1 ;;
esac

hash=$(sha1sum "$tmp" | cut -d' ' -f1)
code=$(dd if="$tmp" bs=1 skip=59 count=4 status=none)
crc=$(od -An -tx1 -j16 -N8 "$tmp" | tr -d ' \n')

echo "Input:    $ROM ($format)"
echo "Game ID:  $code, CRC $crc"
echo "SHA-1:    $hash"

if [ "$format" != z64 ]; then
    if [ "$(realpath "$OUT" 2>/dev/null || echo "$OUT")" = "$(realpath "$ROM")" ]; then
        echo "Input is $format; pass an output path for the converted .z64" >&2; exit 1
    fi
    cp "$tmp" "$OUT"
    echo "Wrote big-endian copy to $OUT"
fi

if [ "$hash" = "$EXPECTED_SHA1" ]; then
    echo "OK: Robotron 64 (USA) rev 0 -- matches the decomp's target."
    exit 0
fi
if [ "$crc" = ac8e4b32e7b47326 ]; then
    echo "MISMATCH: header CRCs are correct (code is likely intact) but the full-file hash differs -- bad or modified dump."
else
    echo "MISMATCH: this is not Robotron 64 (USA) rev 0."
fi
exit 1

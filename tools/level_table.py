#!/usr/bin/env python3
"""Print Robotron 64's level -> floor/background table from the ROM (docs/LEVELS.md).

Reads baserom.us.z64 and only prints; with --floors it also writes the arena floor textures as PNGs, which
are the game's art, so point it at a gitignored folder (texture_work/floors/).

Sources (decomp/):
  - src/game/rom_directory.c      file directory at ROM 0x97E90: little-endian count, then 32-byte
                                  RomFileEntry {unknown0, name[23], deviceAddress, size}, words little-endian,
                                  deviceAddress relative to 0x97E90 (include/rom_files.h)
  - src/game/session_background_select.c
                                  func_80022BFC: record D_80074C08[level] {kind, texture, colors, ...},
                                  kind & 0x7F, floor file D_8007CCC0[texture] (include/scene_background_internal.h)
  - src/game/renderer_texture_file_cache.c
                                  func_80042830 loads that file as one 32x32 RGBA16 texture (2048 bytes)
  - src/game/session_initialization/initialize.c
                                  LEVELNOS.DAT -> D_800BA7A8[220], the wave number shown per level index (0 = bonus)
  - include/scene_definition.h    LEVEL*/BOSS*/BONUS* .STR scene files (big-endian in the ROM)

Usage:
  python tools/level_table.py                       level table (210 level indexes)
  python tools/level_table.py --scenes              per scene file: level index, wave, bonus type, round floor
  python tools/level_table.py --floors texture_work/floors --match dump
                                                    write the floor PNGs and match them to RT64 dump hashes
"""

import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

VRAM_TO_ROM = 0x80000400 - 0x1000      # decomp/linker_scripts/us.ld: .entry 0x80000400 : AT(0x1000)
DIRECTORY = 0x97E90
BACKGROUND_RECORDS = 0x80074C08         # D_80074C08, 16 bytes per level index
FLOOR_NAMES = 0x8007CCC0                # D_8007CCC0, pointers to file names, ends with 0xFFFFFFFF
LEVEL_COUNT = 210                       # func_80022D24 only titles 0 <= level < 210
# func_80022BFC turns the stored kinds 1/2/3/6/7/8 into 0; func_800400D0 draws the rest.
SKY_KINDS = {0: "colour wave", 4: "gradient", 5: "two colours", 9: "white", 10: "black"}


def rom_word(rom, vram):
    return struct.unpack_from(">I", rom, vram - VRAM_TO_ROM)[0]


def rom_string(rom, vram):
    start = vram - VRAM_TO_ROM
    return rom[start:rom.index(b"\0", start)].decode("latin-1")


def read_directory(rom):
    count = struct.unpack_from("<I", rom, DIRECTORY)[0]
    files = {}
    for i in range(count):
        entry = DIRECTORY + 4 + i * 32
        name = rom[entry + 1:entry + 24].split(b"\0")[0].decode("latin-1")
        address, size = struct.unpack_from("<Ii", rom, entry + 24)
        files[name.upper()] = rom[DIRECTORY + address:DIRECTORY + address + size]
    return files


def find_file(files, name):
    """Files are stored with folder prefixes (e.g. MODELS\\...); match on the base name."""
    name = name.upper()
    for full, data in files.items():
        if full == name or full.endswith("\\" + name) or full.endswith("/" + name):
            return data
    return None


def floor_names(rom):
    names = []
    vram = FLOOR_NAMES
    while (pointer := rom_word(rom, vram)) != 0xFFFFFFFF:
        names.append(rom_string(rom, pointer))
        vram += 4
    return names


def level_numbers(files):
    data = find_file(files, "LEVELNOS.DAT")
    big = struct.unpack_from(">220i", data)
    little = struct.unpack_from("<220i", data)
    # The file is copied raw into an int array; take the byte order that gives sane wave numbers.
    return list(big if max(big) <= 255 else little)


def rgba16_pixels(data, swap):
    """32x32 RGBA5551 -> list of (r5, g5, b5, a1)."""
    order = "<" if swap else ">"
    return [((v >> 11) & 31, (v >> 6) & 31, (v >> 1) & 31, v & 1)
            for v in struct.unpack_from(order + "1024H", data)]


def write_png(path, pixels, width=32, height=32):
    rows = b""
    for y in range(height):
        rows += b"\0" + bytes(c for p in pixels[y * width:(y + 1) * width] for c in p)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def read_png(path):
    """Minimal reader for RT64's dump PNGs: 8-bit RGB or RGBA, not interlaced. Returns (w, h, [(r,g,b,a)])."""
    data = path.read_bytes()
    pos, idat = 8, b""
    while pos < len(data):
        length, kind = struct.unpack_from(">I4s", data, pos)
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    if depth != 8 or color not in (2, 6) or interlace:
        raise ValueError(f"{path}: unsupported PNG (depth {depth}, colour type {color})")
    bpp = 4 if color == 6 else 3
    raw, stride, out, prev = zlib.decompress(idat), width * bpp, [], bytearray(width * bpp)
    for y in range(height):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if kind == 1:
                line[i] = (line[i] + a) & 255
            elif kind == 2:
                line[i] = (line[i] + b) & 255
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += [tuple(line[x * bpp:x * bpp + bpp]) + ((255,) if bpp == 3 else ()) for x in range(width)]
        prev = line
    return width, height, out


def print_levels(rom, files, names):
    waves = level_numbers(files)
    print(f"{'idx':>3}  {'wave':>5}  {'floor':<8} {'sky':<12} kind")
    for level in range(LEVEL_COUNT):
        kind, texture, colors = rom[BACKGROUND_RECORDS - VRAM_TO_ROM + level * 16:][:3]
        wave = f"{waves[level]:>5}" if waves[level] else "bonus"
        floor = names[texture].split(".")[0] if texture < len(names) else f"?{texture}"
        sky = SKY_KINDS.get(kind & 0x7F, SKY_KINDS[0])
        print(f"{level:>3}  {wave}  {floor:<8} {sky:<12} {kind & 0x7F}{' +0x80' if kind & 0x80 else ''}")


def print_scenes(files):
    # SceneDefinition offsets (include/scene_definition.h): +0xCCC level index, +0xCD0 HUD wave,
    # +0xCD4 bonus type, +0xD0C round floor (D_800BA784, object_runtime_service.c).
    pattern = re.compile(r"(LEVEL|BOSS|BONUS)(\d+)\.STR$")
    scenes = []
    for name, data in files.items():
        match = pattern.search(name)
        if match and len(data) >= 0xD10:
            index, wave, bonus = struct.unpack_from(">iii", data, 0xCCC)
            round_floor = struct.unpack_from(">i", data, 0xD0C)[0]
            scenes.append((match.group(1), int(match.group(2)), name, index, wave, bonus, round_floor))
    print(f"{'file':<16} {'idx':>4} {'wave':>5} {'bonus':>5} round")
    for _, _, name, index, wave, bonus, round_floor in sorted(scenes):
        print(f"{name.split(chr(92))[-1]:<16} {index:>4} {wave:>5} {bonus:>5} {'yes' if round_floor else ''}")


def floors(files, names, out_dir, dump_dir):
    decoded = {}
    for name in dict.fromkeys(names):
        data = find_file(files, name)
        if data is None or len(data) < 2048:
            # floor2.flr: its name in D_8007CCC0 runs into floor1.raw (no terminator); no level uses it.
            print(f"{name!r}: not in the ROM directory")
            continue
        decoded[name] = data[:2048]
    if out_dir:
        out_dir.mkdir(parents=True, exist_ok=True)
    dumps = {}
    if dump_dir:
        for png in sorted(dump_dir.glob("*.v5.png")):
            width, height, pixels = read_png(png)
            if (width, height) == (32, 32):
                dumps[png.name.split(".")[0]] = [(r >> 3, g >> 3, b >> 3, 1 if a >= 128 else 0)
                                                 for r, g, b, a in pixels]
    for name, data in decoded.items():
        matches = []
        for swap in (False, True):
            pixels = rgba16_pixels(data, swap)
            matches += [(h, "little-endian" if swap else "big-endian") for h, p in dumps.items() if p == pixels]
        if out_dir:
            pixels = rgba16_pixels(data, any(order == "little-endian" for _, order in matches))
            write_png(out_dir / (name.replace(".", "_") + ".png"),
                      [((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 255 * a) for r, g, b, a in pixels])
        if dump_dir:
            found = ", ".join(f"{h} ({order})" for h, order in matches) or "not in the dump"
            print(f"{name:<12} {found}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--rom", type=Path, default=Path(__file__).resolve().parent.parent / "baserom.us.z64")
    parser.add_argument("--scenes", action="store_true", help="list the scene files instead of the level table")
    parser.add_argument("--floors", type=Path, help="write the floor textures as PNGs into this (gitignored) folder")
    parser.add_argument("--match", type=Path, help="RT64 dump folder: match floor textures to dump hashes")
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    if rom[:4] != b"\x80\x37\x12\x40":
        sys.exit(f"{args.rom}: not a big-endian .z64 ROM")
    files = read_directory(rom)
    names = floor_names(rom)
    if args.scenes:
        print_scenes(files)
    elif args.floors or args.match:
        floors(files, names, args.floors, args.match)
    else:
        print_levels(rom, files, names)


if __name__ == "__main__":
    main()

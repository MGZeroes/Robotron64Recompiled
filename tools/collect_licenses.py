#!/usr/bin/env python3
"""Copies the licence of everything the release builds contain into one folder (ADR 010).

Usage: collect_licenses.py OUTPUT_DIR
Fails if a listed file is missing, so a moved file after a pin update can't drop a notice silently.
"""
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RT64 = "lib/rt64/src/contrib"
RUNTIME = "lib/N64ModernRuntime"
FRONTEND = "lib/RecompFrontend"

LICENSES = {
    "Robotron64Recompiled-GPL-3.0": "LICENSE",
    "N64ModernRuntime-GPL-3.0": f"{RUNTIME}/COPYING",
    "RecompFrontend-GPL-3.0": f"{FRONTEND}/COPYING",
    "N64Recomp": f"{RUNTIME}/N64Recomp/LICENSE",
    "ELFIO": f"{RUNTIME}/N64Recomp/lib/ELFIO/LICENSE.txt",
    "fmt": f"{RUNTIME}/N64Recomp/lib/fmt/LICENSE",
    "rabbitizer": f"{RUNTIME}/N64Recomp/lib/rabbitizer/LICENSE",
    "sljit": f"{RUNTIME}/N64Recomp/lib/sljit/LICENSE",
    "tomlplusplus": f"{RUNTIME}/N64Recomp/lib/tomlplusplus/LICENSE",
    "miniz": f"{RUNTIME}/thirdparty/miniz/LICENSE",
    "o1heap": f"{RUNTIME}/thirdparty/o1heap/LICENSE",
    "xxHash": f"{RUNTIME}/thirdparty/xxHash/LICENSE",
    "GamepadMotionHelpers": f"{FRONTEND}/lib/GamepadMotionHelpers/LICENSE",
    "FreeType": f"{FRONTEND}/recompui/lib/freetype-windows-binaries/LICENSE.TXT",
    "lunasvg": f"{FRONTEND}/recompui/lib/lunasvg/LICENSE",
    "plutovg": f"{FRONTEND}/recompui/lib/lunasvg/plutovg/LICENSE",
    "RmlUi": f"{FRONTEND}/recompui/lib/RmlUi/LICENSE.txt",
    "RT64": "lib/rt64/LICENSE",
    "ddspp": f"{RT64}/ddspp/LICENSE",
    "hlslpp": f"{RT64}/hlslpp/LICENSE",
    "im3d": f"{RT64}/im3d/LICENSE",
    "imgui": f"{RT64}/imgui/LICENSE.txt",
    "implot": f"{RT64}/implot/LICENSE",
    "nativefiledialog-extended": f"{RT64}/nativefiledialog-extended/LICENSE",
    "plume": f"{RT64}/plume/LICENSE",
    "D3D12MemoryAllocator": f"{RT64}/plume/contrib/D3D12MemoryAllocator/LICENSE.txt",
    "volk": f"{RT64}/plume/contrib/volk/LICENSE.md",
    "Vulkan-Headers": f"{RT64}/plume/contrib/Vulkan-Headers/LICENSE.md",
    "VulkanMemoryAllocator": f"{RT64}/plume/contrib/VulkanMemoryAllocator/LICENSE.txt",
    "re-spirv": f"{RT64}/re-spirv/LICENSE",
    "SPIRV-Cross": f"{RT64}/spirv-cross/LICENSE",
    "stb": f"{RT64}/stb/LICENSE",
    "zstd": f"{RT64}/zstd/LICENSE",
    # SDL's licence text doesn't change between 2.x releases; this copy is the one always in the tree.
    "SDL2": f"{RT64}/mupen64plus-win32-deps/SDL2-2.26.3/COPYING.txt",
    "SlotMap": "lib/SlotMap/README.md",
    "PromptFont-OFL": "assets/promptfont/LICENSE.txt",
    "Orbitron-OFL": "assets/Orbitron-OFL.txt",
}

# Header-only libraries carry their notice at the top of the file: (path, number of lines to copy).
HEADER_NOTICES = {
    "concurrentqueue": (f"{RUNTIME}/thirdparty/concurrentqueue/concurrentqueue.h", 28),
    "nlohmann-json": (f"{RUNTIME}/thirdparty/json/json.hpp", 8),
    "sse2neon": (f"{RUNTIME}/thirdparty/sse2neon/sse2neon.h", 30),
    "utf8conv": (f"{RT64}/utf8conv/utf8conv.h", 8),
    "plainargs": (f"{RT64}/plainargs/plainargs.h", 30),
}

# The font files name their copyright holders and the OFL, but no licence text comes with them.
FONT_NOTICE = """InterVariable.ttf: Copyright 2016 The Inter Project Authors
NotoEmoji-Regular.ttf: Copyright 2013 Google Inc. All Rights Reserved.

Both fonts are licensed under the SIL Open Font License, Version 1.1. The licence text follows,
from assets/Orbitron-OFL.txt, without Orbitron's copyright line.

"""


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = Path(sys.argv[1])
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)

    paths = list(LICENSES.values()) + [path for path, _ in HEADER_NOTICES.values()]
    missing = [path for path in paths if not (ROOT / path).is_file()]
    if missing:
        sys.exit("missing licence files:\n  " + "\n  ".join(missing))
    for name, path in LICENSES.items():
        shutil.copyfile(ROOT / path, out / f"{name}.txt")
    for name, (path, count) in HEADER_NOTICES.items():
        lines = (ROOT / path).read_text(encoding="utf-8", errors="replace").splitlines()
        (out / f"{name}.txt").write_text("\n".join(lines[:count]) + "\n", encoding="utf-8")

    ofl = (ROOT / "assets/Orbitron-OFL.txt").read_text(encoding="utf-8").splitlines()
    (out / "Inter-and-NotoEmoji-OFL.txt").write_text(FONT_NOTICE + "\n".join(ofl[1:]) + "\n", encoding="utf-8")
    print(f"{len(LICENSES) + len(HEADER_NOTICES) + 1} licence files in {out}")


if __name__ == "__main__":
    main()

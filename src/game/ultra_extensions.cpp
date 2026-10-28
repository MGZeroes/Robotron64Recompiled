// libultra functions Robotron 64 calls that N64ModernRuntime does not provide.
// N64Recomp skips these by name (ignored/reimplemented lists), so the recompiled code calls
// the *_recomp versions below. See docs/NOTES.md (Phase 2/3) for how each was identified.

#include "recomp.h"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"

// Reads one big-endian word from cartridge ROM. The game reads 16 words from 0x00FFB000 at
// boot (decomp src/boot/startup.c), past the end of the 8 MiB ROM, and ignores the results,
// so out-of-range reads return 0 instead of the hardware's open-bus value.
static void read_rom_word(uint8_t* rdram, recomp_context* ctx) {
    u32 dev_addr = _arg<0, u32>(rdram, ctx) & 0x0FFFFFFF;
    PTR(u32) data = _arg<1, PTR(u32)>(rdram, ctx);
    std::span<const uint8_t> rom = recomp::get_rom();

    u32 value = 0;
    if (dev_addr + 4 <= rom.size()) {
        value = (rom[dev_addr] << 24) | (rom[dev_addr + 1] << 16) | (rom[dev_addr + 2] << 8) | rom[dev_addr + 3];
    }
    MEM_W(0, data) = value;
    _return<s32>(ctx, 0);
}

// osPiReadIo_recomp, used by the game's ROM file reader, comes from the template (recomp_api.cpp).
extern "C" void osPiRawReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    read_rom_word(rdram, ctx);
}

// The Controller Pak functions (osPfs*, __osGetId) are in controller_pak.cpp.

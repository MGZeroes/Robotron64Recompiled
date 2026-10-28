// libultra functions Robotron 64 calls that N64ModernRuntime does not provide.
// N64Recomp skips these by name (ignored/reimplemented lists), so the recompiled code calls
// the *_recomp versions below. See docs/NOTES.md (Phase 2/3) for how each was identified.

#include <algorithm>

#include "recomp.h"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"
#include "recompui/recompui.h"
#include "ultramodern/config.hpp"

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

// Background quad size, called from the func_800400D0 hook (robotron64.us.toml). The game's quad
// covers ~96% of the 4:3 view; grow it 6% so it fills the frame, and in Expand mode widen it by the
// window's aspect ratio relative to 4:3. Vertex positions are s16, so clamp; that covers windows up
// to about 2.1:1.
constexpr float background_overscan = 1.06f;

// How much wider than 4:3 the view is: 1 for Original, window aspect / (4/3) for Expand.
static float view_widening() {
    if (ultramodern::renderer::get_graphics_config().ar_option == ultramodern::renderer::AspectRatio::Expand) {
        int width, height;
        recompui::get_window_size(width, height);
        if (height > 0) {
            return std::max(1.0f, (static_cast<float>(width) / height) / (4.0f / 3.0f));
        }
    }
    return 1.0f;
}

static int32_t clamp_extent(float value) {
    return static_cast<int32_t>(std::min(value, 32767.0f));
}

extern "C" int32_t robotron64_background_extent_x(int32_t extent) {
    return clamp_extent(extent * background_overscan * view_widening());
}

// Object culling, called from the three draw-routine hooks (robotron64.us.toml) with abs(camera x).
// Dividing it by the widening factor widens the game's horizontal cull cone to match the view. The
// test uses the object's centre, so objects vanished while still half on screen (hidden by TV
// overscan on hardware); the margin keeps them drawn until they have left the view.
constexpr float cull_margin = 1.2f;

extern "C" int32_t robotron64_cull_abs_x(int32_t abs_x) {
    return static_cast<int32_t>(abs_x / (view_widening() * cull_margin));
}

extern "C" int32_t robotron64_background_extent_y(int32_t extent) {
    return clamp_extent(extent * background_overscan);
}

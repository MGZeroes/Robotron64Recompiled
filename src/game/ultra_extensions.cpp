// Host helpers: libultra functions the runtime doesn't provide, which the recompiled code calls as *_recomp because
// N64Recomp skips them by name, and the widescreen hooks (robotron64.us.toml). See docs/NOTES.md.

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
// window's aspect ratio relative to 4:3.
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

// The quad sits at Z = 32000 and its vertex coordinates are s16, so beyond about 2.15:1 the needed X
// no longer fits. What the projection shows depends on X/Z, so for wider windows X, Y and Z are all
// divided by the same factor: same coverage, closer quad. The gradient is drawn right after the depth
// clear with Z compare but no Z update, so moving it closer cannot hide anything.
constexpr float background_max_coord = 32000.0f;
static float background_divisor = 1.0f;

// Called first each frame (func_800400D0 hook), so it also decides the divisor for the frame.
extern "C" int32_t robotron64_background_extent_x(int32_t extent) {
    float needed = extent * background_overscan * view_widening();
    background_divisor = std::max(1.0f, needed / background_max_coord);
    return static_cast<int32_t>(needed / background_divisor);
}

extern "C" int32_t robotron64_background_extent_y(int32_t extent) {
    return static_cast<int32_t>(extent * background_overscan / background_divisor);
}

// Called from the five background draw routines (robotron64.us.toml) right after they load Z = 32000.
extern "C" int32_t robotron64_background_z(int32_t z) {
    return static_cast<int32_t>(z / background_divisor);
}

// HUD placement. The in-game HUD (func_8004B590, decomp renderer_text/game_hud.c) is drawn as 3D
// quads at camera z = 200, every position going through func_80049DD8(x, y, z) with literal
// coordinates (+x is screen-left), so RT64's HUD Placement option can't move it. While the HUD
// function runs, positions are shifted here instead: glyphs are grouped into runs (same y, x stepping
// by exactly 8, as the game lays out words and numbers), and a run that starts at least 36 units off
// centre moves toward its edge by the extra half-width, keeping its distance to the edge. The 1P
// score (x = 10) and the power-up icons (x = 40, 28, 16, 4, -8 at y = -78) stay centred.
constexpr float hud_half_width = 124.9f; // 4:3 half-width at z = 200 (background quad: 19983 at z = 32000)

static bool hud_active = false;
static bool hud_run_valid = false;
static int32_t hud_last_x = 0;
static int32_t hud_last_y = 0;
static float hud_run_offset = 0.0f;

static float hud_widening() {
    float widening = view_widening();
    switch (ultramodern::renderer::get_graphics_config().hr_option) {
        case ultramodern::renderer::HUDRatioMode::Original:
            return 1.0f;
        case ultramodern::renderer::HUDRatioMode::Clamp16x9:
            return std::min(widening, (16.0f / 9.0f) / (4.0f / 3.0f));
        case ultramodern::renderer::HUDRatioMode::Full:
        default:
            return widening;
    }
}

static bool is_power_icon(int32_t x, int32_t y) {
    return y == -78 && (x == 40 || x == 28 || x == 16 || x == 4 || x == -8);
}

extern "C" void robotron64_hud_begin() {
    hud_active = true;
    hud_run_valid = false;
}

extern "C" void robotron64_hud_end() {
    hud_active = false;
}

// func_80049DD8 entry hook: returns the (possibly shifted) x.
extern "C" int32_t robotron64_hud_x(int32_t x, int32_t y) {
    if (!hud_active) {
        return x;
    }
    bool continues_run = hud_run_valid && y == hud_last_y && (x - hud_last_x == 8 || x - hud_last_x == -8);
    if (!continues_run) {
        hud_run_offset = 0.0f;
        if ((x >= 36 || x <= -36) && !is_power_icon(x, y)) {
            float shift = hud_half_width * (hud_widening() - 1.0f);
            hud_run_offset = x > 0 ? shift : -shift;
        }
    }
    hud_run_valid = true;
    hud_last_x = x;
    hud_last_y = y;
    return x + static_cast<int32_t>(hud_run_offset);
}

// Object culling, called from the three draw-routine hooks (robotron64.us.toml) with abs(camera x) and
// abs(camera y). Dividing them widens the game's cull cone to match the view: horizontally by the widening
// factor, and in the first-person view (first_person.cpp) by its wider lens in both directions. The
// test uses the object's centre, so objects vanish half on screen (hidden by TV overscan on hardware); the margin
// keeps them drawn until they leave the view.
constexpr float cull_margin = 1.2f;

float robotron64_first_person_cull_x(uint8_t* rdram);
float robotron64_first_person_cull_y(uint8_t* rdram);

extern "C" int32_t robotron64_cull_abs_x(uint8_t* rdram, int32_t abs_x) {
    return static_cast<int32_t>(abs_x / (view_widening() * robotron64_first_person_cull_x(rdram) * cull_margin));
}

extern "C" int32_t robotron64_cull_abs_y(uint8_t* rdram, int32_t abs_y) {
    float widen = robotron64_first_person_cull_y(rdram);
    return widen == 1.0f ? abs_y : static_cast<int32_t>(abs_y / (widen * cull_margin));
}

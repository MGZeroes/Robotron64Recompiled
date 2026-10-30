// First-person camera (option First-Person Camera, config.cpp). The game has a finished first-person view,
// camera view 8 "FIRST", that was left out of the L/R camera cycle: the name table D_80073850 lists ARENA,
// SMART, THIRD, OVERH, FIRST, but the cycle D_80073864 is {10, 1, 9, 3, 0}. In view 8 the camera director
// (func_80012950) puts the eye at the player at pitch 0 with a wide lens, and sets D_8007394C, which turns on
// the game's tank controls (actor_manual_motion.c) and facing-relative shots. The hooks in robotron64.us.toml
// call these helpers to put the view back in the cycle (single player only: the view follows a mix of both
// players and the tank controls read controller 1), name it on the HUD, hide the player's own model, widen
// the object culling cone to the wider lens and pull the near plane in. See docs/GAME.md (Camera).

#include <cstring>

#include "recomp.h"
#include "robotron64_config.h"

extern "C" bool robotron64_coop_game_active();

namespace {
    constexpr uint32_t tank_mode = 0x8007394C;             // D_8007394C: cleared each tick, set in view 8
    constexpr uint32_t cycle_index = 0x8007388C;           // D_8007388C: position in the cycle D_80073864
    constexpr uint32_t view_names = 0x80073850;            // D_80073850: ARENA, SMART, THIRD, OVERH, FIRST
    constexpr uint32_t current_player = 0x800AD168;        // session currentPlayer
    constexpr uint32_t play_style = 0x800AD180;            // 0 one player, 1 alternating, 3 simultaneous
    constexpr uint32_t players = 0x8009B190;               // D_8009B190[2], stride 0xDB4
    constexpr uint32_t player_stride = 0xDB4;
    constexpr uint32_t player_view = 0x05;                 // camera view (selection05)
    constexpr uint32_t player_actor = 0x08;
    constexpr uint32_t actor_object = 0x0C;                // short objectIndex

    constexpr uint8_t view_smart = 1;
    constexpr uint8_t view_overhead = 3;
    constexpr uint8_t view_first = 8;
    constexpr int32_t overhead_index = 3;                  // OVERH, the last entry before the cycle's 0
    constexpr uint32_t first_name_index = 4;               // FIRST in D_80073850

    // The cut view's lens is wider than the culling cone (tan 0.5, made for the normal zoom): its half-view
    // is about 0.95 wide and 0.71 high at 4:3 (fovy 70.9 degrees at zoom 166.67).
    constexpr float cull_widen_x = 0.95f / 0.5f;
    constexpr float cull_widen_y = 0.71f / 0.5f;
    constexpr float near_plane = 25.0f;                    // the game's is 100 (200 world units)

    int32_t player_object = -1;                            // the active player's object while in view 8

    int32_t& word(uint8_t* rdram, uint32_t addr) {
        return MEM_W(0, (gpr)(int32_t)addr);
    }

    uint32_t player_addr(int32_t index) {
        return players + static_cast<uint32_t>(index) * player_stride;
    }

    uint32_t current_view_addr(uint8_t* rdram) {
        return player_addr(word(rdram, current_player)) + player_view;
    }

    bool allowed(uint8_t* rdram) {
        return robotron64::first_person_enabled() && word(rdram, play_style) == 0 && !robotron64_coop_game_active();
    }
}

bool robotron64_first_person_active(uint8_t* rdram) {
    return word(rdram, tank_mode) != 0;
}

// func_8001276C before it stores the next view (0x8001286C), $v0 = view. When the cycle wraps from OVERH
// back to the start, go to FIRST instead and leave the index on OVERH, so the next press wraps for real.
extern "C" void robotron64_camera_cycle(uint8_t* rdram, recomp_context* ctx) {
    uint8_t previous = MEM_BU(0, (gpr)(int32_t)current_view_addr(rdram));
    if (word(rdram, cycle_index) == 0 && previous == view_overhead && allowed(rdram)) {
        ctx->r2 = view_first;
        word(rdram, cycle_index) = overhead_index;
    }
}

// func_800126E0 entry (the HUD's camera name): view 8 is not in the cycle table it searches.
extern "C" int32_t robotron64_camera_name(uint8_t* rdram) {
    if (MEM_BU(0, (gpr)(int32_t)current_view_addr(rdram)) == view_first) {
        return word(rdram, view_names + first_name_index * 4);
    }
    return 0;
}

// Object draw func_8003A8B0 entry: once per frame. Leaves view 8 if it is no longer allowed (option off,
// two players), and finds the player's object to hide.
extern "C" void robotron64_first_person_frame(uint8_t* rdram) {
    uint32_t view = current_view_addr(rdram);
    if (MEM_BU(0, (gpr)(int32_t)view) == view_first && !allowed(rdram)) {
        MEM_BU(0, (gpr)(int32_t)view) = view_smart;
    }
    player_object = -1;
    if (robotron64_first_person_active(rdram)) {
        gpr actor = (gpr)word(rdram, player_addr(word(rdram, current_player)) + player_actor);
        if (actor != 0) {
            player_object = MEM_H(actor_object, actor);
        }
    }
}

// Object draw loop (0x8003A988, after the enabled check), $s5 = object: the eye is inside the player's
// model, so skip it along with its floor ring and aim pose.
extern "C" int robotron64_first_person_hide(int32_t object) {
    return object == player_object;
}

// Culling (robotron64_cull_abs_x/y in ultra_extensions.cpp): extra widening while in view 8.
float robotron64_first_person_cull_x(uint8_t* rdram) {
    return robotron64_first_person_active(rdram) ? cull_widen_x : 1.0f;
}

float robotron64_first_person_cull_y(uint8_t* rdram) {
    return robotron64_first_person_active(rdram) ? cull_widen_y : 1.0f;
}

// func_80048DDC before the perspective call (0x80048F24): the near plane is at sp+0x10. Enemies right
// next to the player would cut into it in view 8.
extern "C" void robotron64_first_person_near(uint8_t* rdram, recomp_context* ctx) {
    if (robotron64_first_person_active(rdram)) {
        uint32_t bits;
        std::memcpy(&bits, &near_plane, sizeof(bits));
        MEM_W(0x10, ctx->r29) = static_cast<int32_t>(bits);
    }
}

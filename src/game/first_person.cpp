// First-person camera (option First-Person Camera, config.cpp). The game has a finished first-person view,
// camera view 8 "FIRST", that was left out of the L/R camera cycle: the name table D_80073850 lists ARENA,
// SMART, THIRD, OVERH, FIRST, but the cycle D_80073864 is {10, 1, 9, 3, 0}. In view 8 the camera director
// (func_80012950) puts the eye at the player at pitch 0 with a wide lens, and sets D_8007394C, which turns on
// the game's tank controls (actor_manual_motion.c) and facing-relative shots. The hooks in robotron64.us.toml
// call these helpers to put the view back in the cycle (single player only: the view follows a mix of both
// players and the tank controls read controller 1), name it on the HUD, hide the player's own model, widen
// the object culling cone to the wider lens and pull the near plane in. See docs/GAME.md (Camera).
//
// Modern controls (option value Modern) replace the tank controls: the left stick walks and strafes relative
// to the view, the right stick turns, A or B fires ahead. main.cpp (get_n64_input) reshapes the pad while
// robotron64::first_person_modern_active() and hands the turn input over; the func_80035E3C hook moves.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "recomp.h"
#include "robotron64_config.h"

extern "C" bool robotron64_coop_game_active();
extern "C" void func_8003CC88(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8003CC58(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80039514(uint8_t* rdram, recomp_context* ctx);

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

    // Modern controls. Angles are 0x1000 per turn; the game's timestep D_8009EF94 is elapsed ms * 0.75.
    constexpr uint32_t timestep = 0x8009EF94;
    constexpr uint32_t look_angle = 0x800BEF68;            // D_800BEF68: the view's yaw in view 8
    constexpr uint32_t stick_x = 0x8013DBB8;               // D_8013DBB8[4], controller 0 first
    constexpr uint32_t stick_y = 0x8013DBC8;               // D_8013DBC8[4]
    constexpr uint32_t actor_angle = 0x08;                 // short angle08
    constexpr uint32_t actor_velocity = 0x6C;              // int field6C, field70
    constexpr uint32_t input_held = 0x1C;                  // input block held1C: 1 left, 2 right, 4 up, 8 down
    constexpr int32_t stick_dead_zone = 5;                 // as the tank controls
    constexpr float stick_full = 80.0f;
    constexpr float turn_rate = 2048.0f;                   // angle units per second at full turn (180 degrees)
    constexpr int32_t quarter_turn = 0x400;

    int32_t player_object = -1;                            // the active player's object while in view 8
    std::atomic_bool modern_active = false;
    std::atomic<float> turn_input = 0.0f;

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
    bool active = robotron64_first_person_active(rdram);
    if (active) {
        gpr actor = (gpr)word(rdram, player_addr(word(rdram, current_player)) + player_actor);
        if (actor != 0) {
            player_object = MEM_H(actor_object, actor);
        }
    }
    modern_active.store(active && robotron64::first_person_mode() == robotron64::FirstPersonMode::Modern);
}

// For main.cpp (input thread): whether the pad is reshaped for Modern controls, and the turn input (-1..1).
bool robotron64::first_person_modern_active() {
    return modern_active.load();
}

void robotron64::set_first_person_turn(float turn) {
    turn_input.store(turn);
}

namespace {
    int32_t game_trig(uint8_t* rdram, recomp_context* ctx, void (*func)(uint8_t*, recomp_context*), int32_t angle) {
        recomp_context call = *ctx;
        call.r4 = (gpr)angle;
        func(rdram, &call);
        return static_cast<int32_t>(call.r2);
    }

    float stick_axis(int32_t value) {
        if (std::abs(value) < stick_dead_zone) {
            return 0.0f;
        }
        return std::clamp(value / stick_full, -1.0f, 1.0f);
    }
}

// func_80035E3C entry (tank controls; $a0 actor, $a1 walk speed, $a2 input block). Returns 1 when it moved
// the player with Modern controls, and the hook then skips the tank code. Velocity is written the way the
// tank code does: direction (func_8003CC88 / func_8003CC58, 4096 = 1) times the walk speed.
extern "C" int robotron64_first_person_move(uint8_t* rdram, recomp_context* ctx) {
    if (!modern_active.load() || !robotron64_first_person_active(rdram)) {
        return 0;
    }
    gpr actor = ctx->r4;
    int32_t speed = static_cast<int32_t>(ctx->r5);
    gpr input = ctx->r6;

    float dt = word(rdram, timestep) / 750.0f;
    int32_t angle = MEM_H(actor_angle, actor);
    angle = (angle + static_cast<int32_t>(std::lround(turn_input.load() * turn_rate * dt))) & 0xFFF;
    MEM_H(actor_angle, actor) = static_cast<int16_t>(angle);

    float forward = stick_axis(word(rdram, stick_y));
    float strafe = stick_axis(word(rdram, stick_x));
    int32_t held = MEM_W(input_held, input);
    if (held & 4) forward += 1.0f;
    if (held & 8) forward -= 1.0f;
    if (held & 1) strafe -= 1.0f;
    if (held & 2) strafe += 1.0f;
    forward = std::clamp(forward, -1.0f, 1.0f);
    strafe = std::clamp(strafe, -1.0f, 1.0f);

    int32_t side = (angle + quarter_turn) & 0xFFF;
    float x = game_trig(rdram, ctx, func_8003CC88, angle) * forward + game_trig(rdram, ctx, func_8003CC88, side) * strafe;
    float y = game_trig(rdram, ctx, func_8003CC58, angle) * forward + game_trig(rdram, ctx, func_8003CC58, side) * strafe;
    MEM_W(actor_velocity, actor) = static_cast<int32_t>(x * speed / 4096.0f);
    MEM_W(actor_velocity + 4, actor) = static_cast<int32_t>(y * speed / 4096.0f);

    recomp_context call = *ctx;
    call.r4 = (gpr)(int32_t)MEM_H(actor_object, actor);
    call.r5 = (gpr)angle;
    func_80039514(rdram, &call);
    word(rdram, look_angle) = angle;
    return 1;
}

// Object draw loop (0x8003A990, after the enabled check), $s5 = object: the eye is inside the player's
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

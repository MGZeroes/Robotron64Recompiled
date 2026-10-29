// Simultaneous two-player (option Two Player Mode, config.cpp). The game has an unused play style 3
// in which both players are in the arena at once (decomp session_menu_start_alternate.c, player/behavior.c).
// It was never finished: menus stop getting input, every enemy chases one player, a revived player stands
// up inside the crowd that killed them, and the HUD shows a stale wave for one player. The hooks in
// robotron64.us.toml call these helpers; each does nothing outside style 3. See docs/GAME.md (Two players).

#include <cstring>
#include <initializer_list>

#include "recomp.h"

extern "C" void func_80027ED4(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_800290B0(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80039C5C(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_800399E4(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80027AB8(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80039514(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80039E5C(uint8_t* rdram, recomp_context* ctx);

namespace {
    // Session state D_800AD138 (decomp include/save_game.h, SavedSessionState).
    constexpr uint32_t session = 0x800AD138;               // starts with the session input block
    constexpr uint32_t current_player = 0x800AD168;        // currentPlayer
    constexpr uint32_t player_choices = 0x800AD170;        // playerChoices38[2]
    constexpr uint32_t play_style = 0x800AD180;            // extra48: 1 alternating, 3 simultaneous
    constexpr uint32_t menu_latch = 0x800AD15C;            // input block +0x24
    constexpr uint32_t screen_state = 0x800AD280;          // 12 = high-score entry (HIENTRY.SCR)
    constexpr uint32_t screen_step = 0x800AD28C;
    constexpr uint32_t screen_player = 0x800AD2F0;         // D_800AD2D8+0x18: player the screen is for
    constexpr uint32_t actor_list = 0x800AA708;            // D_800AA708, linked through +0x78

    // Players D_8009B190[2] (GamePlayerState, stride 0xDB4).
    constexpr uint32_t players = 0x8009B190;
    constexpr uint32_t player_stride = 0xDB4;
    constexpr uint32_t player_actor = 0x08;                // saved.actor08
    constexpr uint32_t player_lives = 0x1C;                // saved.active
    constexpr uint32_t player_input = 0x34;                // per-player input block

    // Input block (include/early_input_internal.h).
    constexpr uint32_t input_pressed0C = 0x0C;
    constexpr uint32_t input_buttons10 = 0x10;
    constexpr uint32_t input_held1C = 0x1C;
    constexpr uint32_t input_pressed20 = 0x20;

    // GameActor (include/actor.h).
    constexpr uint32_t actor_object = 0x0C;                // short objectIndex
    constexpr uint32_t actor_timer = 0x0E;                 // short timer0E: frame that ends a one-shot anim
    constexpr uint32_t actor_flags = 0x14;                 // 0x40 = call callback44 when the anim ends
    constexpr uint32_t actor_kind = 0x1C;                  // 0 = enemy (held at wave start), 2 = player
    constexpr uint32_t actor_animation = 0x1F;             // 1 = dying, 6 = drop-in
    constexpr uint32_t actor_resource = 0x24;              // resource; +0x0C = model scale
    constexpr uint32_t actor_field2C = 0x2C;
    constexpr uint32_t actor_callback = 0x44;              // callback44
    constexpr uint32_t actor_owner = 0x3C;                 // the player struct, for player actors
    constexpr uint32_t actor_facing = 0x08;                // short angle08
    constexpr uint32_t actor_position = 0x60;              // int x, y, z
    constexpr uint32_t actor_velocity = 0x6C;              // int x, y
    constexpr uint32_t actor_next = 0x78;

    constexpr uint8_t kind_enemy = 0;
    constexpr uint8_t kind_player = 2;
    constexpr uint8_t animation_dying = 1;
    constexpr uint8_t animation_drop_in = 6;
    constexpr uint32_t flag_anim_callback = 0x40;
    constexpr int32_t drop_in_landed = 0x80029194;         // func_80029194: plays anim 8 (standing)
    constexpr int32_t player_height = 500;                 // behavior.c sets position z to 0x1F4
    constexpr int32_t crowd_distance = 4000;               // spawn finder keeps 3000 from every actor

    int32_t& word(uint8_t* rdram, uint32_t addr) {
        return MEM_W(0, (gpr)(int32_t)addr);
    }

    bool simultaneous(uint8_t* rdram) {
        return word(rdram, play_style) == 3;
    }

    uint32_t player_addr(int32_t index) {
        return players + static_cast<uint32_t>(index) * player_stride;
    }
}

// Main tick, after the per-player input sampling (0x80022E90). Style 3 samples only the two player
// blocks, so the session block that the pause menu, menu driver and transition skip read stays frozen.
// Give it the first player's input, with the second player's buttons ORed in so either can pause.
extern "C" void robotron64_coop_merge_input(uint8_t* rdram) {
    if (!simultaneous(rdram)) {
        return;
    }
    uint32_t first = player_addr(word(rdram, player_choices)) + player_input;
    uint32_t second = player_addr(word(rdram, player_choices + 4)) + player_input;
    for (uint32_t offset = input_pressed0C; offset <= input_pressed20; offset += 4) {
        word(rdram, session + offset) = word(rdram, first + offset);
    }
    for (uint32_t offset : {input_pressed0C, input_buttons10, input_held1C, input_pressed20}) {
        word(rdram, session + offset) |= word(rdram, second + offset);
    }
}

// Enemy targeting. The enemy behaviors chase D_8009B190[currentPlayer]. Around each non-player actor's
// behavior call in the actor update loop func_80029FD8, point currentPlayer at the nearest player who
// is alive, then put it back unless the game changed it meanwhile (a death switches it).
static bool target_saved = false;
static int32_t target_original = 0;
static int32_t target_set = 0;

extern "C" void robotron64_coop_target_begin(uint8_t* rdram, gpr actor) {
    if (!simultaneous(rdram) || MEM_BU(actor_kind, actor) == kind_player) {
        return;
    }
    int32_t ax = MEM_W(actor_position, actor);
    int32_t ay = MEM_W(actor_position + 4, actor);
    int32_t nearest = -1;
    int64_t nearest_distance = 0;
    for (int32_t i = 0; i < 2; i++) {
        uint32_t player = player_addr(i);
        int32_t player_actor_ptr = word(rdram, player + player_actor);
        if (player_actor_ptr == 0 || word(rdram, player + player_lives) <= 0) {
            continue;
        }
        gpr pa = (gpr)player_actor_ptr;
        if (MEM_BU(actor_animation, pa) == animation_dying) {
            continue;
        }
        int64_t dx = MEM_W(actor_position, pa) - ax;
        int64_t dy = MEM_W(actor_position + 4, pa) - ay;
        int64_t distance = dx * dx + dy * dy;
        if (nearest < 0 || distance < nearest_distance) {
            nearest = i;
            nearest_distance = distance;
        }
    }
    if (nearest < 0) {
        return;
    }
    target_original = word(rdram, current_player);
    target_set = nearest;
    target_saved = true;
    word(rdram, current_player) = nearest;
}

// Runs at L_8002A1F8, which other paths also reach, so it only acts after a begin.
extern "C" void robotron64_coop_target_end(uint8_t* rdram) {
    if (!target_saved) {
        return;
    }
    target_saved = false;
    if (word(rdram, current_player) == target_set) {
        word(rdram, current_player) = target_original;
    }
}

// Revive (player behavior func_80033974, 0x80033D80): style 3 stands the player up where they died, as a
// shrunken body still attached to a grabbing enemy and often inside the crowd that killed them, which
// kills them again as soon as the blink ends. Instead:
// - if an enemy is close, move them to a free spot from the game's spawn-spot finder func_80027ED4
//   (actor_spawn_position.c: random arena point clear of every actor); otherwise they stay put;
// - restore the body (detach, normal scale as set at creation 0x80028678, no velocity);
// - play the drop-in players do at wave start: the per-actor part of func_80035CC8(0)
//   (scene_actor_mode_list.c). Anim 6 carries the fall; when it ends the frame tick calls
//   func_80029194, which stands the player up (anim 8) and restarts field48. The revive code has already
//   set flag 0x4000 (invulnerable), which the behavior clears 1 s after landing; the behavior does
//   nothing in anim 6, so there is no control during the fall.
// Calls use a copy of ctx so the caller's registers stay intact.
static bool enemy_near(uint8_t* rdram, int32_t x, int32_t y) {
    for (int32_t a = word(rdram, actor_list); a != 0; a = MEM_W(actor_next, (gpr)a)) {
        gpr other = (gpr)a;
        if (MEM_BU(actor_kind, other) != kind_enemy) {
            continue;
        }
        int32_t dx = MEM_W(actor_position, other) - x;
        int32_t dy = MEM_W(actor_position + 4, other) - y;
        if (dx > -crowd_distance && dx < crowd_distance && dy > -crowd_distance && dy < crowd_distance) {
            return true;
        }
    }
    return false;
}

extern "C" void robotron64_coop_respawn(uint8_t* rdram, recomp_context* ctx, gpr actor) {
    gpr position = actor + actor_position;
    gpr object = (gpr)(int32_t)MEM_H(actor_object, actor);
    recomp_context call;

    if (enemy_near(rdram, MEM_W(actor_position, actor), MEM_W(actor_position + 4, actor))) {
        call = *ctx;
        call.r4 = position;
        call.r5 = 0;
        call.r6 = 1;
        func_80027ED4(rdram, &call);  // writes the spot into the actor's position if it finds one
    }
    MEM_W(actor_position + 8, actor) = player_height;
    call = *ctx;
    call.r4 = object;
    call.r5 = position;
    func_800290B0(rdram, &call);

    call = *ctx;
    call.r4 = object;
    call.r5 = (gpr)(int32_t)-1;
    func_80039C5C(rdram, &call);

    float scale = static_cast<float>(MEM_W(0x0C, (gpr)MEM_W(actor_resource, actor)) << 12) / 40960.0f;
    int32_t scale_bits;
    std::memcpy(&scale_bits, &scale, sizeof(scale_bits));
    call = *ctx;
    call.r4 = object;
    call.r5 = (gpr)scale_bits;
    func_800399E4(rdram, &call);

    call = *ctx;
    call.r4 = actor;
    call.r5 = animation_drop_in;
    call.r6 = 1;
    func_80027AB8(rdram, &call);
    MEM_W(actor_field2C, actor) = 0;
    MEM_W(actor_velocity, actor) = 0;
    MEM_W(actor_velocity + 4, actor) = 0;

    call = *ctx;
    call.r4 = object;
    call.r5 = 0;
    func_80039514(rdram, &call);
    call = *ctx;
    call.r4 = object;
    call.r5 = 1;
    func_80039E5C(rdram, &call);

    // Without track 6 the player keeps the revive's anim 8; don't arm a callback that would never fire.
    if (MEM_BU(actor_animation, actor) == animation_drop_in) {
        MEM_W(actor_flags, actor) |= flag_anim_callback;
        MEM_W(actor_callback, actor) = drop_in_landed;
        MEM_H(actor_timer, actor) = 999;
    }
}

// High score. In alternating 2P each player gets a name entry when they run out of lives. In style 3 the
// first player to run out drops out silently (behavior.c:85 switches to style 0, which the game treats
// like alternating with one player out), so only the survivor got an entry. Remember who ran out first
// and give them their entry after the survivor's, before the TOP5 screen.
static int32_t first_out = -1;

// True for the whole of a game started in style 3, also after the first player is out (style 0 then).
static bool coop_game = false;

// Game start (func_80022528 hook), with the play style after robotron64_two_player_style.
extern "C" void robotron64_coop_game_start(int32_t style) {
    first_out = -1;
    coop_game = style == 3;
}

// Style 3 out-of-lives branch (L_80033D90 in func_80033974), before the switch to style 0.
extern "C" void robotron64_coop_player_out(uint8_t* rdram, int32_t player) {
    if (simultaneous(rdram) && first_out < 0) {
        first_out = static_cast<int32_t>((static_cast<uint32_t>(player) - players) / player_stride);
    }
}

// High-score entry finished and nobody has lives left (0x800237A0 in func_80022D24, just before the
// switch to TOP5). Returns true if it started the first-out player's entry instead, the same way style
// 3's own second entry does (0x800237DC-0x80023850).
extern "C" int robotron64_coop_next_entry(uint8_t* rdram) {
    if (first_out < 0) {
        return 0;
    }
    int32_t player = first_out;
    first_out = -1;
    word(rdram, screen_player) = static_cast<int32_t>(player_addr(player));
    word(rdram, menu_latch) = 0xFFFF;
    word(rdram, current_player) = player;
    word(rdram, screen_state) = 12;
    word(rdram, screen_step) = 0;
    return 1;
}

// HUD state (func_800371FC entry): in 2P it shows the inactive player's saved wave, which style 3 never
// updates. Both players are in the same wave, D_800BA748 (D_800B9A78.valueCD0).
extern "C" void robotron64_coop_hud_wave(uint8_t* rdram) {
    if (!simultaneous(rdram)) {
        return;
    }
    int32_t wave = word(rdram, 0x800BA748);
    word(rdram, 0x8009BF00) = wave;  // D_8009B190[0].valueD70
    word(rdram, 0x8009CCB4) = wave;  // D_8009B190[1].valueD70
}

// Floor ring and aim pose. The player behavior func_80033974 leaves "last player" globals behind: its actor
// (D_800BEF60), the fire angle or -1 (D_800BEF6C) and the facing (D_800BEF64). The draw loop func_8003A8B0
// turns the actor into one object record (D_800BF2C0) and draws that object's shadow as the yellow ring and
// blends its arm toward the fire angle (decomp object_runtime_update.c). Only the player whose behavior ran
// last (P1, the actor list is newest-first) got either. Each player's values are saved at the end of their
// behavior and put back when the draw loop reaches that player's object.
namespace {
    constexpr uint32_t aim_actor = 0x800BEF60;             // D_800BEF60
    constexpr uint32_t aim_facing = 0x800BEF64;            // D_800BEF64
    constexpr uint32_t aim_angle = 0x800BEF6C;             // D_800BEF6C
    constexpr uint32_t aim_object = 0x800BF2C0;            // D_800BF2C0
    constexpr uint32_t objects = 0x800BF918;               // D_800BF918, stride 0x78
    constexpr uint32_t object_stride = 0x78;

    int32_t saved_angle[2] = { -1, -1 };
    int32_t saved_facing[2] = { 0, 0 };

    // Index of the player owning this actor, or -1.
    int player_of_actor(uint8_t* rdram, gpr actor) {
        if (actor == 0 || MEM_B(actor_kind, actor) != kind_player) {
            return -1;
        }
        uint32_t owner = static_cast<uint32_t>(MEM_W(actor_owner, actor));
        for (int i = 0; i < 2; i++) {
            if (owner == player_addr(i) && static_cast<uint32_t>(word(rdram, player_addr(i) + player_actor)) == static_cast<uint32_t>(actor)) {
                return i;
            }
        }
        return -1;
    }
}

// End of the player behavior (L_80035174 in func_80033974, every exit passes it), $s0 = actor. D_800BEF64
// is only stored in the return's delay slot, so the facing is read from the actor.
extern "C" void robotron64_coop_save_aim(uint8_t* rdram, gpr actor) {
    if (!coop_game) {
        return;
    }
    int i = player_of_actor(rdram, actor);
    if (i >= 0) {
        saved_angle[i] = word(rdram, aim_angle);
        saved_facing[i] = MEM_H(actor_facing, actor);
    }
}

// Top of the draw loop body in func_8003A8B0 (0x8003A990), $s5 = object index. Selects a player's own
// object and aim before the ring (0x8003AB7C) and pose (0x8003B0A4) checks read them.
extern "C" void robotron64_coop_select_object(uint8_t* rdram, int32_t object) {
    if (!coop_game) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        gpr actor = (gpr)word(rdram, player_addr(i) + player_actor);
        if (player_of_actor(rdram, actor) == i && MEM_H(actor_object, actor) == object) {
            word(rdram, aim_actor) = static_cast<int32_t>(actor);
            word(rdram, aim_object) = static_cast<int32_t>(objects + static_cast<uint32_t>(object) * object_stride);
            word(rdram, aim_angle) = saved_angle[i];
            MEM_H(0, (gpr)(int32_t)aim_facing) = static_cast<int16_t>(saved_facing[i]);
            return;
        }
    }
}

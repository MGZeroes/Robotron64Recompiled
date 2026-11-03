#include "robotron64_config.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "recompinput/recompinput.h"
#include "recompinput/players.h"
#include "recompinput/profiles.h"
#include "librecomp/config.hpp"
#include "util/file.h"
#include <atomic>
#include <filesystem>

static std::atomic_bool analog_aim = true;

bool robotron64::analog_aim_enabled() {
    return analog_aim.load();
}

static std::atomic_bool simultaneous_two_player = false;

static std::atomic<uint32_t> first_person = static_cast<uint32_t>(robotron64::FirstPersonMode::Classic);

robotron64::FirstPersonMode robotron64::first_person_mode() {
    return static_cast<robotron64::FirstPersonMode>(first_person.load());
}

bool robotron64::first_person_enabled() {
    return first_person_mode() != FirstPersonMode::Off;
}

// Called from the func_80022528 hook (robotron64.us.toml) with the session play style.
extern "C" int32_t robotron64_two_player_style(int32_t style) {
    return (style == 1 && simultaneous_two_player.load()) ? 3 : style;
}

static void add_general_options(recomp::config::Config &config) {
    using EnumOptionVector = const std::vector<recomp::config::ConfigOptionEnumOption>;

    // The game aims with the analog stick of the controller after the player's (decomp
    // src/game/player/behavior.c, secondaryX/Y), i.e. the N64 two-controllers-per-player setup.
    // main.cpp feeds the right stick to that port; the right stick then stops producing the
    // C-button direction bits, which would otherwise override the analog aim with 8-way fire.
    static EnumOptionVector analog_aim_options = {
        {0, "Off", "Off"},
        {1, "On", "On"},
    };
    config.add_enum_option(
        robotron64::configkeys::general::analog_aim,
        "Analog Aiming",
        "<recomp-color primary>On</recomp-color>: fires along the exact angle of the right stick, using the game's own "
        "two-controller aiming."
        "<br />"
        "<recomp-color primary>Off</recomp-color>: the right stick acts as the C buttons and fires in 8 directions, as "
        "with one N64 controller."
        "<br /><br />"
        "The D-pad, C buttons and keyboard always fire in 8 directions.",
        analog_aim_options,
        1
    );
    config.add_option_change_callback(robotron64::configkeys::general::analog_aim,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            bool enabled = std::get<uint32_t>(cur_value) != 0;
            analog_aim.store(enabled);
            recompinput::set_right_analog_suppressed(enabled);
        });
    recompinput::set_right_analog_suppressed(analog_aim.load());

    // TWO PLAYER normally alternates turns (play style 1). The game also has an unused simultaneous
    // style 3 (decomp session_menu_start_alternate.c, never linked from the menu), which the
    // func_80022528 hook selects when this option is on. See docs/GAME.md (Two players).
    static EnumOptionVector two_player_mode_options = {
        {0, "Alternating", "Alternating"},
        {1, "Simultaneous", "Simultaneous"},
    };
    config.add_enum_option(
        robotron64::configkeys::general::two_player_mode,
        "Two Player Mode",
        "<recomp-color primary>Alternating</recomp-color>: the original. Players take turns, switching on each death."
        "<br />"
        "<recomp-color primary>Simultaneous</recomp-color> (experimental): the game's unused co-op mode. Both players "
        "are in the arena at once and drop back in after losing a life, until they run out of lives."
        "<br /><br />"
        "Applies when a new game starts.",
        two_player_mode_options,
        0
    );
    config.add_option_change_callback(robotron64::configkeys::general::two_player_mode,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            simultaneous_two_player.store(std::get<uint32_t>(cur_value) != 0);
        });

    // Shared keeps recompinput in single-player mode, where every pad and the keyboard drive player 1.
    // One per player switches it to multiplayer mode: pads are given to players with RecompFrontend's
    // Assign Players screen (Controls tab), and main.cpp maps each player to their two N64 ports.
    // See docs/decisions/006-two-controllers.md.
    static EnumOptionVector controllers_options = {
        {0, "Shared", "Shared"},
        {1, "One per player", "One per player"},
    };
    config.add_enum_option(
        robotron64::configkeys::general::controllers,
        "Controllers",
        "<recomp-color primary>Shared</recomp-color>: every controller and the keyboard control player 1 (and player 2 "
        "in two-player games)."
        "<br />"
        "<recomp-color primary>One per player</recomp-color>: each player uses their own controller or the keyboard. "
        "Assign them under Controls > Assign players."
        "<br /><br />"
        "The assignment is needed once per session. Without one, the keyboard controls player 1.",
        controllers_options,
        0
    );
    config.add_option_change_callback(robotron64::configkeys::general::controllers,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            recompinput::players::set_single_player_mode(std::get<uint32_t>(cur_value) == 0);
        });

    // The game has a finished first-person camera (view FIRST) that was left out of the L/R camera cycle.
    // first_person.cpp puts it back after OVERHEAD. See docs/GAME.md (Camera).
    // Saved values: 1 must stay Classic.
    static EnumOptionVector first_person_options = {
        {0, "Off", "Off"},
        {1, "Classic", "Classic"},
        {2, "Modern", "Modern"},
    };
    config.add_enum_option(
        robotron64::configkeys::general::first_person,
        "First-Person Camera",
        "L or R also cycles to the game's unused first-person view, after Overhead. Single player only."
        "<br />"
        "<recomp-color primary>Classic</recomp-color>: the game's own tank controls. The stick turns and walks, "
        "shots go where you look, and Z looks around."
        "<br />"
        "<recomp-color primary>Modern</recomp-color>: the left stick walks and strafes, the right stick turns, "
        "and A or B fires ahead."
        "<br />"
        "<recomp-color primary>Off</recomp-color>: the original four camera views.",
        first_person_options,
        static_cast<uint32_t>(robotron64::FirstPersonMode::Classic)
    );
    config.add_option_change_callback(robotron64::configkeys::general::first_person,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            first_person.store(std::get<uint32_t>(cur_value));
        });
}

static void set_control_descriptions() {
    // Mapping from decomp/docs/controller-input-mapping.md (func_8003C1A8). L or R alone cycles the camera
    // (docs/GAME.md, Input).
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_POS, "Move up");
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_NEG, "Move down");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_NEG, "Move left");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_POS, "Move right");
    recompinput::set_game_input_description(recompinput::GameInput::A,          "A button");
    recompinput::set_game_input_description(recompinput::GameInput::B,          "B button");
    recompinput::set_game_input_description(recompinput::GameInput::Z,          "Z trigger");
    recompinput::set_game_input_description(recompinput::GameInput::L,          "Change camera (L)");
    recompinput::set_game_input_description(recompinput::GameInput::R,          "Change camera (R)");
    recompinput::set_game_input_description(recompinput::GameInput::START,      "Start / pause");
    // RecompFrontend's default controller mapping also puts the C buttons on the right stick
    // (recompinput/src/input_mapping.cpp); with Analog Aiming on, the right stick aims directly instead.
    recompinput::set_game_input_description(recompinput::GameInput::C_UP,       "Fire up");
    recompinput::set_game_input_description(recompinput::GameInput::C_DOWN,     "Fire down");
    recompinput::set_game_input_description(recompinput::GameInput::C_LEFT,     "Fire left");
    recompinput::set_game_input_description(recompinput::GameInput::C_RIGHT,    "Fire right");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_UP,    "D-pad up");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_DOWN,  "D-pad down");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_LEFT,  "D-pad left");
    recompinput::set_game_input_description(recompinput::GameInput::DPAD_RIGHT, "D-pad right");
}

void robotron64::init_config() {
    std::filesystem::path recomp_dir = recompui::file::get_app_folder_path();

    if (!recomp_dir.empty()) {
        std::filesystem::create_directories(recomp_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;

    auto &general_config = recompui::config::create_general_tab(general_options);
    add_general_options(general_config);

    recompui::config::create_graphics_tab();

    set_control_descriptions();
    recompui::config::create_controls_tab();

    recompui::config::create_sound_tab();

    recompui::config::create_mods_tab();

    recompui::config::finalize();

    // The game has two players. Their input profiles are set in main.cpp (update_player_profiles).
    recompinput::players::set_player_count_range(1, 2);

    // RecompFrontend creates the "Keyboard (Player N)" profiles, used when a player is assigned the keyboard,
    // without bindings. Start them from the normal keyboard bindings; they are saved in controls.json, so
    // edits made on the player cards are kept.
    using namespace recompinput;
    int sp_keyboard = profiles::get_sp_keyboard_profile_index();
    for (int player = 0; player < 2; player++) {
        int profile = profiles::get_or_create_mp_keyboard_profile_index(player);
        bool empty = true;
        for (size_t input = 0; input < static_cast<size_t>(GameInput::COUNT) && empty; input++) {
            for (size_t binding = 0; binding < num_bindings_per_input; binding++) {
                empty &= profiles::get_input_binding(profile, static_cast<GameInput>(input), binding).input_type == InputType::None;
            }
        }
        if (!empty) {
            continue;
        }
        for (size_t input = 0; input < static_cast<size_t>(GameInput::COUNT); input++) {
            for (size_t binding = 0; binding < num_bindings_per_input; binding++) {
                profiles::set_input_binding(profile, static_cast<GameInput>(input), binding,
                    profiles::get_input_binding(sp_keyboard, static_cast<GameInput>(input), binding));
            }
        }
    }

}

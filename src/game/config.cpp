#include "robotron64_config.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "recompinput/recompinput.h"
#include "robotron64_support.h"
#include "ultramodern/config.hpp"
#include "librecomp/files.hpp"
#include "librecomp/config.hpp"
#include "util/file.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>

#if defined(_WIN32)
#include <Shlobj.h>
#elif defined(__linux__)
#include <unistd.h>
#include <pwd.h>
#elif defined(__APPLE__)
#include "apple/rt64_apple.h"
#endif

static std::atomic_bool analog_aim = true;

bool robotron64::analog_aim_enabled() {
    return analog_aim.load();
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
        "<recomp-color primary>On</recomp-color> fires along the exact angle of the right stick, using the game's own "
        "two-controller aiming. <recomp-color primary>Off</recomp-color> maps the right stick to the C buttons, which "
        "fire in 8 directions as with one N64 controller. The D-pad, C buttons and keyboard always fire in 8 directions.",
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
}

template <typename T = uint32_t>
T get_general_config_enum_value(const std::string& option_id) {
    return static_cast<T>(std::get<uint32_t>(recompui::config::get_general_config().get_option_value(option_id)));
}

template <typename T = uint32_t>
T get_general_config_number_value(const std::string& option_id) {
    return static_cast<T>(std::get<double>(recompui::config::get_general_config().get_option_value(option_id)));
}

template <typename T = uint32_t>
T get_graphics_config_enum_value(const std::string& option_id) {
    return static_cast<T>(std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(option_id)));
}

static void add_graphics_options(recomp::config::Config &config) {
    // User Task: Add game-specific graphics options.
}

static void set_control_defaults() {
    using namespace recompinput;

    // TODO default controls overrides
}

static void set_control_descriptions() {
    // Mapping from the decomp (docs/controller-input-mapping.md, func_8003C1A8): the stick, D-pad and
    // C buttons feed the movement/firing direction groups; Start, A and B are separate bits; an L or R
    // press toggles a game flag. Refine as play-testing confirms what each input does.
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_POS, "Move up");
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_NEG, "Move down");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_NEG, "Move left");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_POS, "Move right");
    recompinput::set_game_input_description(recompinput::GameInput::A,          "A button");
    recompinput::set_game_input_description(recompinput::GameInput::B,          "B button");
    recompinput::set_game_input_description(recompinput::GameInput::Z,          "Z trigger");
    recompinput::set_game_input_description(recompinput::GameInput::L,          "Toggle (L)");
    recompinput::set_game_input_description(recompinput::GameInput::R,          "Toggle (R)");
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

    auto &graphics_config = recompui::config::create_graphics_tab();
    add_graphics_options(graphics_config);

    set_control_defaults();
    set_control_descriptions();
    recompui::config::create_controls_tab();

    recompui::config::create_sound_tab();

    recompui::config::create_mods_tab();

    recompui::config::finalize();

}

#ifndef __ROBOTRON64_CONFIG_H__
#define __ROBOTRON64_CONFIG_H__

#include <filesystem>
#include <string>

namespace robotron64 {
    inline const std::u8string program_id = u8"Robotron64Recompiled";
    inline const std::string program_name = "Robotron 64: Recompiled";

    namespace configkeys {
        namespace general {
            inline const std::string analog_aim = "analog_aim";
            inline const std::string two_player_mode = "two_player_mode";
            inline const std::string controllers = "controllers";
            inline const std::string first_person = "first_person";
        }
    }

    // TODO: Move loading configs to the runtime once we have a way to allow per-project customization.
    void init_config();

    // "Analog aiming" option: the right stick drives the game's second-controller aim stick.
    bool analog_aim_enabled();

    // "First-Person Camera" option: L/R camera cycling also reaches the game's cut FIRST view (single player),
    // with the game's tank controls (Classic) or left stick walk / right stick turn / A-B fire (Modern).
    // See first_person.cpp.
    enum class FirstPersonMode { Off = 0, Classic = 1, Modern = 2 };
    FirstPersonMode first_person_mode();
    bool first_person_enabled();
    // While in the first-person view with Modern controls: main.cpp reshapes the pad and passes the turn
    // input (-1 left .. 1 right) to the movement hook.
    bool first_person_modern_active();
    void set_first_person_turn(float turn);
};

#endif

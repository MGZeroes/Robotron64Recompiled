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
        }
    }

    // TODO: Move loading configs to the runtime once we have a way to allow per-project customization.
    void init_config();

    // "Analog aiming" option: the right stick drives the game's second-controller aim stick.
    bool analog_aim_enabled();

    // "Two Player Mode" option: Simultaneous starts TWO PLAYER in the game's unused co-op mode.
    bool simultaneous_two_player_enabled();
};

#endif

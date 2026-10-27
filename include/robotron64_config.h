#ifndef __ROBOTRON64_CONFIG_H__
#define __ROBOTRON64_CONFIG_H__

#include <filesystem>
#include <string>

namespace robotron64 {
    inline const std::u8string program_id = u8"Robotron64Recompiled";
    inline const std::string program_name = "Robotron 64: Recompiled";

    namespace configkeys {
        namespace general {
        }
    }

    // TODO: Move loading configs to the runtime once we have a way to allow per-project customization.
    void init_config();
};

#endif

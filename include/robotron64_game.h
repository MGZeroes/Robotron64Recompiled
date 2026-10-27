#ifndef __ROBOTRON64_GAME_H__
#define __ROBOTRON64_GAME_H__

#include <cstdint>
#include <span>
#include <vector>
#include "recomp.h"

namespace robotron64 {
    void game_on_init(uint8_t* rdram, recomp_context* ctx);
};

#endif

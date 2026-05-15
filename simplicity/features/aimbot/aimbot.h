#pragma once
#include "../game/reader.h"
#include "../../utils/globals.h"

namespace aimbot
{
    void update(const game::GameSnapshot& snap);
    bool is_active();
}
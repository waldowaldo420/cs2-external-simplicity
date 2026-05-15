#pragma once
#include "../../imgui/imgui.h"
#include "../../game/reader.h"
#include "../../utils/math.h"
#include "../../utils/globals.h"

namespace esp
{
    void draw(ImDrawList* dl, const game::GameSnapshot& snap);
}
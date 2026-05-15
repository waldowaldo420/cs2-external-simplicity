#include "rcs.h"
#include "../../utils/globals.h"
#include "../aimbot/aimbot.h"
#include <Windows.h>
#include <cmath>
#include <algorithm>

namespace rcs
{
    static float s_prev_x = 0.f;
    static float s_prev_y = 0.f;
    static float s_smooth_x = 0.f;
    static float s_smooth_y = 0.f;

    void update(const game::GameSnapshot& snap)
    {
        if (!globals::rcs::enabled)
        {
            s_prev_x = s_prev_y = s_smooth_x = s_smooth_y = 0.f;
            return;
        }

        if (globals::aimbot::enabled && aimbot::is_active())
        {
            s_prev_x = snap.local_aim_punch.x;
            s_prev_y = snap.local_aim_punch.y;
            s_smooth_x = 0.f;
            s_smooth_y = 0.f;
            return;
        }

        const float punch_x = snap.local_aim_punch.x;
        const float punch_y = snap.local_aim_punch.y;

        if (snap.local_shots_fired <= 1)
        {
            s_prev_x = punch_x;
            s_prev_y = punch_y;
            s_smooth_x = 0.f;
            s_smooth_y = 0.f;
            return;
        }

        const float sens = snap.local_sensitivity > 0.f
            ? snap.local_sensitivity : 1.f;

        const float delta_x = (punch_x - s_prev_x) * -1.f;
        const float delta_y = (punch_y - s_prev_y) * -1.f;

        const float strength_x = globals::rcs::yaw / 100.f;
        const float strength_y = globals::rcs::pitch / 100.f;

        const float move_x = (delta_y * strength_x * 2.f / sens) / -0.022f;
        const float move_y = (delta_x * strength_y * 2.f / sens) / 0.022f;

        const float smooth = std::clamp(globals::rcs::smooth, 1.f, 100.f);
        s_smooth_x += (move_x - s_smooth_x) / smooth;
        s_smooth_y += (move_y - s_smooth_y) / smooth;

        s_prev_x = punch_x;
        s_prev_y = punch_y;

        const int mx = static_cast<int>(std::round(s_smooth_x));
        const int my = static_cast<int>(std::round(s_smooth_y));

        if (mx != 0 || my != 0)
        {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = mx;
            input.mi.dy = my;
            SendInput(1, &input, sizeof(INPUT));
        }
    }
}
#include "aimbot.h"
#include "../../utils/globals.h"
#include "../../utils/math.h"
#include "../../render/overlay.h"
#include <Windows.h>
#include <cmath>
#include <algorithm>

namespace aimbot
{
    static float s_accum_x = 0.f;
    static float s_accum_y = 0.f;
    static float s_recoil_x = 0.f;
    static float s_recoil_y = 0.f;

    static void reset()
    {
        s_accum_x = 0.f;
        s_accum_y = 0.f;
        s_recoil_x = 0.f;
        s_recoil_y = 0.f;
    }

    static const game::PlayerEntry* find_best_target(const game::GameSnapshot& snap, int sw, int sh)
    {
        const float cx = sw * 0.5f;
        const float cy = sh * 0.5f;
        const float fov_px = (globals::aimbot::fov / 90.f) * (sw * 0.5f);
        const float fov_sq = fov_px * fov_px;

        const game::PlayerEntry* best = nullptr;
        float best_sq = fov_sq;

        for (int i = 0; i < snap.player_count; i++)
        {
            const auto& p = snap.players[i];
            if (!p.alive || p.health <= 0) continue;
            if (globals::aimbot::enemy_only && p.team == snap.local_team) continue;
            if (globals::aimbot::vis_only && !p.visible) continue;

            const int slot = globals::aimbot::bone;
            if (slot < 0 || slot >= game::BONE_COUNT) continue;
            if (!p.bones[slot].valid) continue;

            math::Vector2 screen{};
            if (!math::world_to_screen(p.bones[slot].pos,
                snap.view_matrix, sw, sh, screen)) continue;

            const float dx = screen.x - cx;
            const float dy = screen.y - cy;
            const float dq = dx * dx + dy * dy;

            if (dq < best_sq) { best_sq = dq; best = &p; }
        }
        return best;
    }

    static bool s_has_target = false;

    bool is_active()
    {
        return s_has_target;
    }

    void update(const game::GameSnapshot& snap)
    {
        if (!globals::aimbot::enabled) return;
        if (!(GetAsyncKeyState(globals::aimbot::key) & 0x8000))
        {
            s_has_target = false;
            reset();
            return;
        }

        const int sw = render::g_overlay.width();
        const int sh = render::g_overlay.height();
        const float cx = sw * 0.5f;
        const float cy = sh * 0.5f;

        const int shots = snap.local_shots_fired;
        if (shots <= globals::aimbot::start_after ||
            shots > globals::aimbot::stop_after)
        {
            reset();
            return;
        }

        const game::PlayerEntry* target = find_best_target(snap, sw, sh);
        if (!target)
        {
            s_has_target = false;
            reset();
            return;
        }

        s_has_target = true;

        const int slot = globals::aimbot::bone;
        math::Vector2 bone_screen{};
        if (!math::world_to_screen(target->bones[slot].pos, snap.view_matrix, sw, sh, bone_screen))
        {
            reset();
            return;
        }

        float rel_x = bone_screen.x - cx;
        float rel_y = bone_screen.y - cy;

        if (globals::aimbot::recoil_comp && shots > 1)
        {
            const float sens = snap.local_sensitivity > 0.f ? snap.local_sensitivity : 1.f;
            const float raw_x = (snap.local_aim_punch.y * 2.f / sens) / -0.022f;
            const float raw_y = (snap.local_aim_punch.x * 2.f / sens) / 0.022f;
            const float tgt_x = raw_x * globals::aimbot::recoil_strength;
            const float tgt_y = raw_y * globals::aimbot::recoil_strength;
            const float rsmooth = std::max(1.f, globals::aimbot::recoil_smooth);

            s_recoil_x += (tgt_x - s_recoil_x) / rsmooth;
            s_recoil_y += (tgt_y - s_recoil_y) / rsmooth;

            rel_x -= s_recoil_x;
            rel_y -= s_recoil_y;
        }
        else
        {
            s_recoil_x = 0.f;
            s_recoil_y = 0.f;
        }

        const float dist = std::sqrtf(rel_x * rel_x + rel_y * rel_y);
        const float base_smooth = std::max(globals::aimbot::smooth, 1.f);
        const float dist_scale = std::clamp(dist / 200.f, 0.5f, 2.f);
        const float adapt_smooth = base_smooth * dist_scale;
        const float t = std::clamp(1.f / adapt_smooth, 0.005f, 0.8f);

        s_accum_x += rel_x * t;
        s_accum_y += rel_y * t;

        const int move_x = static_cast<int>(std::round(s_accum_x));
        const int move_y = static_cast<int>(std::round(s_accum_y));

        s_accum_x -= static_cast<float>(move_x);
        s_accum_y -= static_cast<float>(move_y);

        if (move_x != 0 || move_y != 0)
        {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = move_x;
            input.mi.dy = move_y;
            SendInput(1, &input, sizeof(INPUT));
        }
    }
}
#include "triggerbot.h"
#include "../../utils/globals.h"
#include "../../utils/math.h"
#include "../../render/overlay.h"
#include "../../game/reader.h"
#include <Windows.h>
#include <chrono>
#include <unordered_map>
#include <cmath>
#include <algorithm>

#include <iostream>

using namespace game::bone_slot;

namespace triggerbot
{
    struct BodySeg { int bone_a; int bone_b; float radius; };

    static constexpr BodySeg k_segs[] = {
        { pelvis,     spine_0,    6.f },
        { spine_0,    spine_1,    6.f },
        { spine_1,    spine_2,    6.f },
        { spine_2,    neck,       2.5f },
        { neck,       head,       2.5f },
        { neck,       clavicle_r, 2.f },
        { clavicle_r, shoulder_r, 2.f },
        { shoulder_r, elbow_r,    2.f },
        { elbow_r,    hand_r,     2.f },
        { neck,       clavicle_l, 2.f },
        { clavicle_l, shoulder_l, 2.f },
        { shoulder_l, elbow_l,    2.f },
        { elbow_l,    hand_l,     2.f },
        { pelvis,     hip_r,      2.5f },
        { hip_r,      knee_r,     2.5f },
        { knee_r,     foot_r,     2.5f },
        { pelvis,     hip_l,      2.5f },
        { hip_l,      knee_l,     2.5f },
        { knee_l,     foot_l,     2.5f },
    };
    static constexpr int k_seg_count = static_cast<int>(
        sizeof(k_segs) / sizeof(k_segs[0]));

    static bool point_in_capsule(
        float px, float py,
        float ax, float ay,
        float bx, float by,
        float radius)
    {
        const float dx = bx - ax, dy = by - ay;
        const float len_sq = dx * dx + dy * dy;
        float t = 0.f;
        if (len_sq > 1e-6f)
            t = std::clamp(((px - ax) * dx + (py - ay) * dy) / len_sq, 0.f, 1.f);
        const float nx = ax + t * dx - px;
        const float ny = ay + t * dy - py;
        return (nx * nx + ny * ny) <= (radius * radius);
    }

    static float world_radius_to_screen(
        const math::Vector3& bone_world, float world_r,
        const math::Matrix4x4& mat, int sw, int sh)
    {
        const float rx = mat.m[0][0];
        const float ry = mat.m[0][1];
        const float rz = mat.m[0][2];
        const float rlen = std::sqrtf(rx * rx + ry * ry + rz * rz);
        if (rlen < 1e-6f) return -1.f;

        math::Vector2 center{};
        if (!math::world_to_screen(bone_world, mat, sw, sh, center)) return -1.f;

        const math::Vector3 edge = {
            bone_world.x + (rx / rlen) * world_r,
            bone_world.y + (ry / rlen) * world_r,
            bone_world.z + (rz / rlen) * world_r
        };
        math::Vector2 edge_s{};
        if (!math::world_to_screen(edge, mat, sw, sh, edge_s)) return -1.f;

        const float dx = edge_s.x - center.x;
        const float dy = edge_s.y - center.y;
        return std::sqrtf(dx * dx + dy * dy);
    }

    static bool is_on_target(
        const game::PlayerEntry& p,
        const math::Matrix4x4& mat,
        float cx, float cy, int sw, int sh)
    {
        const float head_nudge = 3.f;
        const float scale = globals::triggerbot::capsule_scale;

        if (globals::triggerbot::head_only)
        {
            const auto& hb = p.bones[head];
            if (!hb.valid) return false;

            math::Vector3 head_pos = hb.pos;
            head_pos.z += head_nudge;

            math::Vector2 hs{};
            if (!math::world_to_screen(head_pos, mat, sw, sh, hs)) return false;
            const float r = world_radius_to_screen(head_pos, 4.f * scale, mat, sw, sh);
            const float dx = hs.x - cx, dy = hs.y - cy;
            return (dx * dx + dy * dy) <= (r * r);
        }

        for (int i = 0; i < k_seg_count; i++)
        {
            const auto& ba = p.bones[k_segs[i].bone_a];
            const auto& bb = p.bones[k_segs[i].bone_b];
            if (!ba.valid || !bb.valid) continue;

            math::Vector3 pos_a = ba.pos;
            math::Vector3 pos_b = bb.pos;
            if (k_segs[i].bone_a == head) pos_a.z += head_nudge;
            if (k_segs[i].bone_b == head) pos_b.z += head_nudge;

            math::Vector2 sa{}, sb{};
            if (!math::world_to_screen(pos_a, mat, sw, sh, sa)) continue;
            if (!math::world_to_screen(pos_b, mat, sw, sh, sb)) continue;

            float r = world_radius_to_screen(
                pos_a, k_segs[i].radius * scale, mat, sw, sh);
            if (r < 2.f) r = 2.f;

            if (point_in_capsule(cx, cy, sa.x, sa.y, sb.x, sb.y, r))
                return true;
        }
        return false;
    }

    void update(const game::GameSnapshot& snap)
    {
        if (!globals::triggerbot::enabled) return;
        if (!(GetAsyncKeyState(globals::triggerbot::key) & 0x8000)) return;

        using Clock = std::chrono::steady_clock;
        static auto last_shot = Clock::now() - std::chrono::seconds(10);
        static std::unordered_map<uintptr_t, Clock::time_point> vis_start;

        const auto  now = Clock::now();
        const int   sw = render::g_overlay.width();
        const int   sh = render::g_overlay.height();
        const float cx = sw * 0.5f;
        const float cy = sh * 0.5f;

        for (int i = 0; i < snap.player_count; i++)
        {
            const auto& p = snap.players[i];
            if (!p.alive || p.health <= 0) continue;
            if (globals::triggerbot::enemy_only && p.team == snap.local_team) continue;

            if (p.visible)
            {
                if (vis_start.find(p.pawn) == vis_start.end())
                    vis_start[p.pawn] = now;
            }
            else
            {
                vis_start.erase(p.pawn);
            }
        }

        const auto since_shot = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shot).count();
        if (since_shot < globals::triggerbot::cooldown_ms) return;

        for (int i = 0; i < snap.player_count; i++)
        {
            const auto& p = snap.players[i];
            if (!p.alive || p.health <= 0) continue;
            if (globals::triggerbot::enemy_only && p.team == snap.local_team) continue;
            if (globals::triggerbot::vis_only && !p.visible) continue;

            const auto vis_it = vis_start.find(p.pawn);
            if (vis_it == vis_start.end()) continue;
            const auto vis_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - vis_it->second).count();
            if (vis_ms < globals::triggerbot::reaction_ms) continue;

            if (!is_on_target(p, snap.view_matrix, cx, cy, sw, sh)) continue;

            INPUT down{}, up{};
            down.type = up.type = INPUT_MOUSE;
            down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &down, sizeof(INPUT));
            SendInput(1, &up, sizeof(INPUT));

            last_shot = Clock::now();
            break;
        }
    }
}
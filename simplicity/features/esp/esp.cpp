#include "esp.h"
#include "../../utils/globals.h"
#include "../../utils/math.h"
#include "../../render/overlay.h"
#include "../../game/reader.h"
#include <cmath>
#include <string>
#include <algorithm>

namespace esp
{

    static ImVec2 catmull_rom(
        const ImVec2& p0, const ImVec2& p1,
        const ImVec2& p2, const ImVec2& p3, float t)
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return {
            0.5f * ((2.f * p1.x) + (-p0.x + p2.x) * t + (2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x) * t2 + (-p0.x + 3.f * p1.x - 3.f * p2.x + p3.x) * t3),
            0.5f * ((2.f * p1.y) + (-p0.y + p2.y) * t + (2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y) * t2 + (-p0.y + 3.f * p1.y - 3.f * p2.y + p3.y) * t3)
        };
    }

    static void draw_spline(ImDrawList* dl,
        const std::vector<ImVec2>& pts, ImU32 col, float thick)
    {
        if (pts.size() < 2) return;

        std::vector<ImVec2> p;
        p.push_back(pts.front());
        for (auto& v : pts) p.push_back(v);
        p.push_back(pts.back());

        ImVec2 last = p[1];
        for (size_t i = 0; i + 3 < p.size(); ++i)
        {
            for (float t = 0.f; t <= 1.f; t += 0.08f)
            {
                ImVec2 pt = catmull_rom(p[i], p[i + 1], p[i + 2], p[i + 3], t);
                dl->AddLine(last, pt, col, thick);
                last = pt;
            }
        }
    }

    static void draw_skeleton(ImDrawList* dl,
        const game::PlayerEntry& p,
        const math::Matrix4x4& view,
        int sw, int sh,
        ImU32 color)
    {
        if (!globals::esp::skeleton) return;

        auto w2s = [&](int slot, ImVec2& out) -> bool
            {
                if (slot < 0 || slot >= game::BONE_COUNT) return false;
                const auto& b = p.bones[slot];
                if (!b.valid) return false;
                math::Vector2 s{};
                if (!math::world_to_screen(b.pos, view, sw, sh, s)) return false;
                if (!std::isfinite(s.x) || !std::isfinite(s.y)) return false;
                out = { s.x, s.y };
                return true;
            };

        const float thick = globals::esp::skeleton_thickness;

        {
            ImVec2 head_s{}, neck_s{};
            if (w2s(game::bone_slot::head, head_s) &&
                w2s(game::bone_slot::neck, neck_s))
            {
                const float dy = std::fabsf(head_s.y - neck_s.y);
                const float r = std::max(2.f, dy);

				if (globals::esp::skeleton_head)
                {
                dl->AddCircleFilled(
                    { head_s.x, head_s.y - 2.f },
                    r, color, 24);
                }
            }
        }

        {
            std::vector<ImVec2> spine;
            ImVec2 pt{};
            if (w2s(game::bone_slot::pelvis, pt)) spine.push_back(pt);
            if (w2s(game::bone_slot::spine_0, pt)) spine.push_back(pt);
            if (w2s(game::bone_slot::spine_1, pt)) spine.push_back(pt);
            if (w2s(game::bone_slot::spine_2, pt)) spine.push_back(pt);
            if (w2s(game::bone_slot::neck, pt)) spine.push_back(pt);
            draw_spline(dl, spine, color, thick);
        }

        {
            std::vector<ImVec2> arm;
            ImVec2 pt{};
            if (w2s(game::bone_slot::neck, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::clavicle_l, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::shoulder_l, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::elbow_l, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::hand_l, pt)) arm.push_back(pt);
            draw_spline(dl, arm, color, thick);
        }

        {
            std::vector<ImVec2> arm;
            ImVec2 pt{};
            if (w2s(game::bone_slot::neck, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::clavicle_r, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::shoulder_r, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::elbow_r, pt)) arm.push_back(pt);
            if (w2s(game::bone_slot::hand_r, pt)) arm.push_back(pt);
            draw_spline(dl, arm, color, thick);
        }

        {
            std::vector<ImVec2> leg;
            ImVec2 pt{};
            if (w2s(game::bone_slot::pelvis, pt)) leg.push_back(pt);
            if (w2s(game::bone_slot::hip_l, pt)) leg.push_back(pt);
            if (w2s(game::bone_slot::knee_l, pt)) leg.push_back(pt);
            if (w2s(game::bone_slot::foot_l, pt)) leg.push_back(pt);
            draw_spline(dl, leg, color, thick);
        }

        {
            std::vector<ImVec2> leg;
            ImVec2 pt{};
            if (w2s(game::bone_slot::pelvis, pt)) leg.push_back(pt);
            if (w2s(game::bone_slot::hip_r, pt)) leg.push_back(pt);
            if (w2s(game::bone_slot::knee_r, pt)) leg.push_back(pt);
            if (w2s(game::bone_slot::foot_r, pt)) leg.push_back(pt);
            draw_spline(dl, leg, color, thick);
        }
    }

    static bool get_box(
        const game::PlayerEntry& player,
        const math::Matrix4x4& view,
        int sw, int sh,
        float& out_x, float& out_y,
        float& out_w, float& out_h)
    {
        math::Vector3 feet = player.position;
        math::Vector3 head;

        if (player.bones[game::bone_slot::head].valid)
            head = player.bones[game::bone_slot::head].pos;
        else
        {
            head = player.position;
            head.z += 73.f;
        }

        feet.z -= globals::esp::box_feet_offset;
        head.z += globals::esp::box_head_offset;

        math::Vector2 feet_screen{}, head_screen{};
        if (!math::world_to_screen(feet, view, sw, sh, feet_screen)) return false;
        if (!math::world_to_screen(head, view, sw, sh, head_screen)) return false;

        const float height = feet_screen.y - head_screen.y;
        const float width = height * globals::esp::box_width_scale;

        out_x = head_screen.x - width * 0.5f;
        out_y = head_screen.y;
        out_w = width;
        out_h = height;

        return out_h > 5.f;
    }

    static void draw_box(ImDrawList* dl,
        float x, float y, float w, float h,
        ImU32 color)
    {
        const float t = globals::esp::box_thickness;

        dl->AddRect(
            ImVec2(x - 1.f, y - 1.f),
            ImVec2(x + w + 1.f, y + h + 1.f),
            IM_COL32(0, 0, 0, 180), 0.f, 0, t + 1.f);

        dl->AddRect(
            ImVec2(x, y),
            ImVec2(x + w, y + h),
            color, 0.f, 0, t);


        dl->AddRect(
            ImVec2(x + 1.f, y + 1.f),
            ImVec2(x + w - 1.f, y + h - 1.f),
            IM_COL32(0, 0, 0, 180), 0.f, 0, 1.f);
    }

    static void draw_health_bar(ImDrawList* dl,
        float x, float y, float w, float h,
        int health)
    {
        const float bar_w = globals::esp::health_bar_width;
        const float gap = globals::esp::health_bar_offset;
        const float fill = (float)health / 100.f;
        const float fill_h = h * fill;

        float bar_x, bar_y, bar_x2, bar_y2;

        switch (globals::esp::health_bar_pos)
        {
        case 0:
            bar_x = x - bar_w - gap;
            bar_y = y;
            bar_x2 = bar_x + bar_w;
            bar_y2 = y + h;
            break;
        case 1:
            bar_x = x + w + gap;
            bar_y = y;
            bar_x2 = bar_x + bar_w;
            bar_y2 = y + h;
            break;
        case 2:
            bar_x = x;
            bar_y = y - bar_w - gap;
            bar_x2 = x + w;
            bar_y2 = bar_y + bar_w;
            break;
        case 3:
            bar_x = x;
            bar_y = y + h + gap;
            bar_x2 = x + w;
            bar_y2 = bar_y + bar_w;
            break;
        default:
            bar_x = x - bar_w - gap;
            bar_y = y;
            bar_x2 = bar_x + bar_w;
            bar_y2 = y + h;
            break;
        }

        dl->AddRectFilled(
            ImVec2(bar_x - 1.f, bar_y - 1.f),
            ImVec2(bar_x2 + 1.f, bar_y2 + 1.f),
            IM_COL32(0, 0, 0, 180));

        ImU32 bar_color;
        if (health > 60)      bar_color = IM_COL32(0, 255, 0, 255);
        else if (health > 30) bar_color = IM_COL32(255, 200, 0, 255);
        else                  bar_color = IM_COL32(255, 0, 0, 255);

        if (globals::esp::health_bar_pos == 0 || globals::esp::health_bar_pos == 1)
        {
            dl->AddRectFilled(
                ImVec2(bar_x, bar_y2 - fill_h),
                ImVec2(bar_x2, bar_y2),
                bar_color);
        }
        else
        {
            const float fill_w = (bar_x2 - bar_x) * fill;
            dl->AddRectFilled(
                ImVec2(bar_x, bar_y),
                ImVec2(bar_x + fill_w, bar_y2),
                bar_color);
        }
    }

    static void draw_name(ImDrawList* dl,
        float x, float y, float w, float h,
        const game::PlayerEntry& p)
    {
        char buf[160]{};
        if (p.name[0] && globals::esp::show_health_text)
            snprintf(buf, sizeof(buf), "%s [%dhp]", p.name, p.health);
        else if (p.name[0])
            snprintf(buf, sizeof(buf), "%s", p.name);
        else if (globals::esp::show_health_text)
            snprintf(buf, sizeof(buf), "%dhp", p.health);
        else
            return;

        const ImVec2 text_size = ImGui::CalcTextSize(buf);
        const float  text_x = x + (w - text_size.x) * 0.5f;
        const float  text_y = y - text_size.y - globals::esp::name_offset;

        dl->AddText(ImVec2(text_x + 1.f, text_y + 1.f),
            IM_COL32(0, 0, 0, 255), buf);
        dl->AddText(ImVec2(text_x, text_y),
            IM_COL32(255, 255, 255, 255), buf);
    }

    void draw(ImDrawList* dl, const game::GameSnapshot& snap)
    {
        if (!globals::esp::enabled) return;
        if (!dl) return;

        const int sw = render::g_overlay.width();
        const int sh = render::g_overlay.height();

        if (globals::aimbot::draw_fov)
        {
            const float fov_px = (globals::aimbot::fov / 90.f) * (sw * 0.5f);
            const ImU32 fov_col = ImGui::ColorConvertFloat4ToU32(globals::aimbot::fov_color);
            dl->AddCircle(ImVec2(sw * 0.5f, sh * 0.5f), fov_px, fov_col, 64, 1.f);
        }

        for (int i = 0; i < snap.player_count; i++)
        {
            const game::PlayerEntry& p = snap.players[i];
            if (!p.alive || p.health <= 0) continue;
            if (globals::esp::team_check && p.team == snap.local_team) continue;

            float bx, by, bw, bh;
            if (!get_box(p, snap.view_matrix, sw, sh, bx, by, bw, bh)) continue;

            const bool is_enemy = p.team != snap.local_team;

            ImU32 box_color;
            if (is_enemy)
                box_color = (globals::esp::vis_check && !p.visible)
                ? ImGui::ColorConvertFloat4ToU32(globals::esp::box_color_enemy_hidden)
                : ImGui::ColorConvertFloat4ToU32(globals::esp::box_color_enemy);
            else
                box_color = ImGui::ColorConvertFloat4ToU32(globals::esp::box_color_team);

            if (globals::esp::boxes)
                draw_box(dl, bx, by, bw, bh, box_color);

            if (globals::esp::skeleton)
                draw_skeleton(dl, p, snap.view_matrix, sw, sh, box_color);

            if (globals::esp::health_bar)
                draw_health_bar(dl, bx, by, bw, bh, p.health);

            if (globals::esp::names)
                draw_name(dl, bx, by, bw, bh, p);

        }
    }
}
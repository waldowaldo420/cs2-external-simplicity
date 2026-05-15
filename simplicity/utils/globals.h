#pragma once
#include "../imgui/imgui.h"
#include "../utils/math.h"
#include "../game/reader.h"

namespace globals
{
    inline bool menu_open = false;

    namespace esp
    {
        inline bool enabled = true;
        inline bool team_check = true;
        inline bool vis_check = true;
        inline ImVec4 box_color_enemy_hidden = { 1.f, 0.0f, 0.f, 1.0f };

        inline bool boxes = true;
        inline float box_thickness = 1.5f;
        inline float box_head_offset = 9.0f;
        inline float box_feet_offset = 6.f;
        inline float box_width_scale = 0.6f;
        inline ImVec4 box_color_enemy = { 1.f, 1.0f, 1.0f, 1.0f };
        inline ImVec4 box_color_team = { 0.2f, 1.f, 0.2f, 1.f };
        inline bool health_bar = true;
        inline float health_bar_width = 4.f;
        inline float health_bar_offset = 3.f;
        inline int health_bar_pos = 0;
        inline bool names = true;
        inline float name_offset = 3.f;
        inline bool show_health_text = true;
        inline bool skeleton = true;
        inline bool skeleton_head = true;
        inline float skeleton_thickness = 1.5f;
    }

    namespace menu
    {
        inline int active_tab = 0;
        inline int window_width = 700;
        inline int window_height = 850;
    }

    namespace aimbot
    {
        inline bool enabled = true;
        inline bool vis_only = true;
        inline bool recoil_comp = true;
        inline float recoil_strength = 0.25f;
        inline float recoil_smooth = 10.0f;
        inline int start_after = 1;
        inline int stop_after = 30;
        inline int key = 1;
        inline float fov = 8.f;
        inline float smooth = 50.f;
        inline bool enemy_only = true;
        inline bool vis_check = true;
        inline int bone = game::bone_slot::spine_2;
        inline bool draw_fov = false;
        inline ImVec4 fov_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    namespace rcs
    {
        inline bool enabled = true;
        inline float pitch = 100.f;
        inline float yaw = 100.f;
        inline float smooth = 10.f;
    }

    namespace triggerbot
    {
        inline bool enabled = true;
        inline bool vis_only = true;
        inline int key = 0x56;
        inline int reaction_ms = 50;
        inline int cooldown_ms = 500;
        inline bool enemy_only = true;
        inline bool head_only = false;
        inline float capsule_scale = 1.f;
    }
}
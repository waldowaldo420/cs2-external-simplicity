#include "menu.h"
#include "../utils/globals.h"
#include "../imgui/imgui.h"
#include <cmath>

namespace menu
{
    enum : int { TAB_AIMBOT = 0, TAB_TRIGGERBOT = 1, TAB_RCS = 2, TAB_ESP = 3 };

    static int s_active_tab = 0;

    void apply_style()
    {
        ImGuiStyle& s = ImGui::GetStyle();

        s.WindowRounding = 10.f;
        s.ChildRounding = 8.f;
        s.FrameRounding = 6.f;
        s.GrabRounding = 6.f;
        s.TabRounding = 6.f;
        s.PopupRounding = 6.f;
        s.ScrollbarRounding = 6.f;
        s.WindowPadding = ImVec2(12.f, 12.f);
        s.FramePadding = ImVec2(8.f, 6.f);
        s.ItemSpacing = ImVec2(8.f, 6.f);
        s.ItemInnerSpacing = ImVec2(6.f, 4.f);
        s.IndentSpacing = 18.f;
        s.ScrollbarSize = 10.f;
        s.GrabMinSize = 8.f;
        s.WindowBorderSize = 1.f;
        s.ChildBorderSize = 1.f;
        s.FrameBorderSize = 0.f;

        ImVec4* c = s.Colors;

        const ImVec4 bg_main = ImVec4(0.04f, 0.05f, 0.08f, 0.98f);
        const ImVec4 bg_child = ImVec4(0.06f, 0.07f, 0.11f, 0.92f);
        const ImVec4 bg_frame = ImVec4(0.09f, 0.10f, 0.16f, 1.00f);
        const ImVec4 border = ImVec4(0.20f, 0.24f, 0.45f, 0.55f);
        const ImVec4 accent = ImVec4(0.34f, 0.45f, 1.00f, 1.00f);
        const ImVec4 accent_hover = ImVec4(0.45f, 0.56f, 1.00f, 1.00f);
        const ImVec4 accent_active = ImVec4(0.26f, 0.36f, 0.92f, 1.00f);
        const ImVec4 accent_dim = ImVec4(0.34f, 0.45f, 1.00f, 0.35f);

        c[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.96f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.58f, 1.00f);

        c[ImGuiCol_WindowBg] = bg_main;
        c[ImGuiCol_ChildBg] = bg_child;
        c[ImGuiCol_PopupBg] = bg_child;

        c[ImGuiCol_Border] = border;
        c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);

        c[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.06f, 0.10f, 1.00f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.08f, 0.14f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.06f, 0.10f, 1.00f);

        c[ImGuiCol_FrameBg] = bg_frame;
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.13f, 0.15f, 0.24f, 1.00f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.18f, 0.28f, 1.00f);

        c[ImGuiCol_Button] = bg_frame;
        c[ImGuiCol_ButtonHovered] = accent_hover;
        c[ImGuiCol_ButtonActive] = accent_active;

        c[ImGuiCol_Tab] = ImVec4(0.08f, 0.09f, 0.14f, 1.00f);
        c[ImGuiCol_TabHovered] = accent_hover;
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
        c[ImGuiCol_TabUnfocusedActive] = accent_dim;

        c[ImGuiCol_Header] = accent_dim;
        c[ImGuiCol_HeaderHovered] = accent;
        c[ImGuiCol_HeaderActive] = accent_active;

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accent_active;

        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_SeparatorHovered] = accent;
        c[ImGuiCol_SeparatorActive] = accent_active;

        c[ImGuiCol_ResizeGrip] = accent_dim;
        c[ImGuiCol_ResizeGripHovered] = accent;
        c[ImGuiCol_ResizeGripActive] = accent_active;

        c[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.06f, 0.09f, 1.00f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.22f, 0.36f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.30f, 0.50f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive] = accent;
    }

    void draw()
    {
        if (!globals::menu_open)
            return;

        ImGui::SetNextWindowSize(
            ImVec2(900.f, 580.f),
            ImGuiCond_Once);

        ImGui::Begin(
            "simplicity",
            nullptr,
			ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();

        dl->AddRectFilled(
            wp,
            ImVec2(wp.x + ws.x, wp.y + ws.y),
            IM_COL32(14, 15, 18, 255),
            12.f);

        dl->AddRectFilledMultiColor(
            wp,
            ImVec2(wp.x + ws.x, wp.y + 120.f),
            IM_COL32(70, 110, 255, 40),
            IM_COL32(70, 110, 255, 10),
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 0));

        ImGui::BeginChild(
            "##sidebar",
            ImVec2(170.f, 0.f),
            false);

        ImGui::Dummy(ImVec2(0.f, 20.f));

        ImGui::SetCursorPosX(20.f);

        ImGui::PushFont(ImGui::GetFont());

        ImGui::TextColored(
            ImVec4(1.f, 1.f, 1.f, 1.f),
            "    simplicity");

        ImGui::TextColored(
            ImVec4(0.5f, 0.55f, 0.65f, 1.f),
            "      by waldowaldo");

        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.f, 30.f));

        const float tab_h = 42.f;

        auto sidebar_tab =
            [&](const char* name, int id)
            {
                bool active = s_active_tab == id;

                if (active)
                {
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(0.22f, 0.32f, 0.70f, 0.35f));
                }
                else
                {
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(0.f, 0.f, 0.f, 0.f));
                }

                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    ImVec4(1.f, 1.f, 1.f, 0.04f));

                ImGui::PushStyleColor(
                    ImGuiCol_ButtonActive,
                    ImVec4(1.f, 1.f, 1.f, 0.06f));

                ImGui::SetCursorPosX(10.f);

                if (ImGui::Button(name, ImVec2(150.f, tab_h)))
                    s_active_tab = id;

                ImGui::PopStyleColor(3);
            };

        sidebar_tab("Aimbot", TAB_AIMBOT);
        sidebar_tab("Triggerbot", TAB_TRIGGERBOT);
        sidebar_tab("RCS", TAB_RCS);
        sidebar_tab("ESP", TAB_ESP);

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild(
            "##content",
            ImVec2(0.f, 0.f),
            false);

        ImGui::Dummy(ImVec2(0.f, 10.f));

        auto begin_card =
            [&](const char* title)
            {
                ImGui::BeginChild(
                    title,
                    ImVec2(0.f, 0.f),
                    true);

                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImVec4(0.95f, 0.96f, 0.98f, 1.f));

                ImGui::TextUnformatted(title);

                ImGui::PopStyleColor();

                ImGui::Spacing();
            };

        auto end_card =
            [&]()
            {
                ImGui::EndChild();
            };

        if (s_active_tab == TAB_AIMBOT)
        {
            ImGui::Columns(2, nullptr, false);

            widgets::section("General");
            ImGui::Spacing();
            widgets::checkbox("Enabled", globals::aimbot::enabled);
            ImGui::Spacing();
            widgets::checkbox("Enemy Only", globals::aimbot::enemy_only);
            ImGui::Spacing();
            widgets::checkbox("Visible Only", globals::aimbot::vis_only);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Keybind");
            ImGui::Spacing();
            ImGui::Text("Aimbot Key");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 160.f);
            draw_keybind(globals::aimbot::key, "##kb_aim");
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Targeting");
            ImGui::Spacing();
            widgets::slider_float("FOV", globals::aimbot::fov, 1.f, 45.f, "%.1f");
            ImGui::Spacing();
            widgets::slider_float("Smooth", globals::aimbot::smooth, 1.f, 50.f, "%.1f");
            ImGui::Spacing();
            ImGui::Spacing();

            static const char* bones[] = {
                "Pelvis", "Spine 0", "Spine 1", "Spine 2",
                "Neck",   "Head",
                "Clavicle L", "Shoulder L", "Elbow L", "Hand L",
                "Clavicle R", "Shoulder R", "Elbow R", "Hand R"
            };
            widgets::combo("Bone Target", globals::aimbot::bone, bones, 14);
            ImGui::Spacing();
            widgets::section("Recoil");
            ImGui::Spacing();
            widgets::checkbox("Recoil Compensation", globals::aimbot::recoil_comp);
            ImGui::Spacing();
            if (globals::aimbot::recoil_comp)
            {
                widgets::slider_float("Strength", globals::aimbot::recoil_strength, 0.f, 1.f, "%.2f");
                ImGui::Spacing();
                widgets::slider_float("Recoil Smooth", globals::aimbot::recoil_smooth, 1.f, 10.f, "%.1f");
            }
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::NextColumn();


            widgets::section("Shot Filter");
            ImGui::Spacing();
            widgets::slider_int("Start After Shot", globals::aimbot::start_after, -1, 5);
            ImGui::Spacing();
            widgets::slider_int("Stop After Shot", globals::aimbot::stop_after, 1, 100);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Visuals");
            ImGui::Spacing();
            widgets::checkbox("Draw FOV", globals::aimbot::draw_fov);
            ImGui::Spacing();
            if (globals::aimbot::draw_fov)
                widgets::color_picker("FOV Color", globals::aimbot::fov_color);


            ImGui::Columns(1);
        }

        if (s_active_tab == TAB_TRIGGERBOT)
        {
            ImGui::Columns(2, nullptr, false);

            widgets::section("General");
            ImGui::Spacing();
            widgets::checkbox("Enabled", globals::triggerbot::enabled);
            ImGui::Spacing();
            widgets::checkbox("Enemy Only", globals::triggerbot::enemy_only);
            ImGui::Spacing();
            widgets::checkbox("Head Only", globals::triggerbot::head_only);
            ImGui::Spacing();
            widgets::checkbox("Visible Only", globals::triggerbot::vis_only);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Keybind");
            ImGui::Spacing();
            ImGui::Text("Trigger Key");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 160.f);
            draw_keybind(globals::triggerbot::key, "##kb_tb");

            ImGui::NextColumn();

            widgets::section("Timing");
            ImGui::Spacing();
            widgets::slider_int("Reaction (ms)",
                globals::triggerbot::reaction_ms, 0, 500);
            ImGui::Spacing();
            widgets::slider_int("Cooldown (ms)",
                globals::triggerbot::cooldown_ms, 50, 1000);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Detection");
            ImGui::Spacing();
            widgets::slider_float("Capsule Scale",
                globals::triggerbot::capsule_scale, 0.5f, 3.f, "%.2f");
            ImGui::Spacing();

            ImGui::Columns(1);
        }

        if (s_active_tab == TAB_RCS)
        {
            widgets::section("General");
            ImGui::Spacing();
            widgets::checkbox("Enabled", globals::rcs::enabled);
            ImGui::Spacing();
            ImGui::Spacing();


            widgets::section("Strength");
            ImGui::Spacing();
            widgets::slider_float("Pitch (Vertical)",
                globals::rcs::pitch, 0.f, 100.f, "%.1f%%");
            ImGui::Spacing();
            widgets::slider_float("Yaw (Horizontal)",
                globals::rcs::yaw, 0.f, 100.f, "%.1f%%");
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Smoothing");
            ImGui::Spacing();
            widgets::slider_float("Smooth",
                globals::rcs::smooth, 1.f, 10.f, "%.1f");
        }

        if (s_active_tab == TAB_ESP)
        {
            ImGui::Columns(2, nullptr, false);

            widgets::section("General");
            ImGui::Spacing();
            widgets::checkbox("Enabled", globals::esp::enabled);
            ImGui::Spacing();
            widgets::checkbox("Team Check", globals::esp::team_check);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Box");
            ImGui::Spacing();
            widgets::checkbox("Draw Box", globals::esp::boxes);
            ImGui::Spacing();
            widgets::slider_float("Box Thickness", globals::esp::box_thickness, 0.5f, 5.f, "%.1f");
            ImGui::Spacing();
            widgets::slider_float("Width Scale", globals::esp::box_width_scale, 0.2f, 1.f, "%.2f");
            ImGui::Spacing();
            widgets::slider_float("Head Offset", globals::esp::box_head_offset, 0.f, 20.f, "%.1f");
            ImGui::Spacing();
            widgets::slider_float("Feet Offset", globals::esp::box_feet_offset, 0.f, 20.f, "%.1f");
            ImGui::Spacing();
            widgets::color_picker("Enemy Visible", globals::esp::box_color_enemy);
            ImGui::Spacing();
            widgets::color_picker("Enemy Occluded", globals::esp::box_color_enemy_hidden);
            ImGui::Spacing();
            widgets::color_picker("Team Color", globals::esp::box_color_team);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Names");
            ImGui::Spacing();
            widgets::checkbox("Draw Names", globals::esp::names);
            ImGui::Spacing();
            widgets::checkbox("Show HP Text", globals::esp::show_health_text);
            ImGui::Spacing();
            widgets::slider_float("Name Offset",
                globals::esp::name_offset, 0.f, 20.f, "%.1f");

            ImGui::NextColumn();

            widgets::section("Health Bar");
            ImGui::Spacing();
            widgets::checkbox("Draw Health Bar", globals::esp::health_bar);
            ImGui::Spacing();
            widgets::slider_float("Bar Width",
                globals::esp::health_bar_width, 1.f, 10.f, "%.1f");
            ImGui::Spacing();
            widgets::slider_float("Bar Offset",
                globals::esp::health_bar_offset, 1.f, 10.f, "%.1f");
            ImGui::Spacing();

            static const char* positions[] = { "Left", "Right", "Top", "Bottom" };
            widgets::combo("Position",
                globals::esp::health_bar_pos, positions, 4);
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Skeleton");
            ImGui::Spacing();
            widgets::checkbox("Draw Skeleton", globals::esp::skeleton);
            ImGui::Spacing();
            widgets::checkbox("Head Circle", globals::esp::skeleton_head);
            ImGui::Spacing();
            widgets::slider_float("Skeleton Thickness",
                globals::esp::skeleton_thickness, 0.1f, 5.f, "%.1f");
            ImGui::Spacing();
            ImGui::Spacing();

            widgets::section("Visibility");
            ImGui::Spacing();
            widgets::checkbox("Visibility Check", globals::esp::vis_check);

            ImGui::Columns(1);

        }

        ImGui::EndChild();

        ImGui::End();
    }
}
#pragma once
#include "../imgui/imgui.h"
#include <Windows.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <string>

static void draw_keybind(int& hotkey, const char* changeId)
{
    static bool waitingForKey = false;
    static int* waitingTarget = nullptr;
    static bool prevDown[256] = { false };

    char keyName[64] = {};

    if (hotkey == VK_XBUTTON1) snprintf(keyName, sizeof(keyName), "Mouse 4");
    else if (hotkey == VK_XBUTTON2) snprintf(keyName, sizeof(keyName), "Mouse 5");
    else if (hotkey == VK_LBUTTON)  snprintf(keyName, sizeof(keyName), "LMB");
    else if (hotkey == VK_RBUTTON)  snprintf(keyName, sizeof(keyName), "RMB");
    else if (hotkey == VK_MBUTTON)  snprintf(keyName, sizeof(keyName), "MMB");
    else if (hotkey == VK_MENU)     snprintf(keyName, sizeof(keyName), "Alt");
    else if (hotkey == VK_SHIFT)    snprintf(keyName, sizeof(keyName), "Shift");
    else if (hotkey == VK_CONTROL)  snprintf(keyName, sizeof(keyName), "Ctrl");
    else
    {
        UINT scanCode = MapVirtualKeyA(hotkey, MAPVK_VK_TO_VSC);
        if (scanCode != 0)
        {
            LPARAM lp = (scanCode & 0xFF) << 16;
            if (!GetKeyNameTextA(lp, keyName, sizeof(keyName)) || keyName[0] == '\0')
                snprintf(keyName, sizeof(keyName), "VK 0x%02X", hotkey);
        }
        else
            snprintf(keyName, sizeof(keyName), "VK 0x%02X", hotkey);
    }

    char buttonText[128] = {};

    if (waitingForKey && waitingTarget == &hotkey)
    {
        snprintf(buttonText, sizeof(buttonText), "Press any key...");

        for (int vk = 1; vk < 256; ++vk)
        {
            const bool nowDown = (GetAsyncKeyState(vk) & 0x8000) != 0;

            if (nowDown && !prevDown[vk])
            {
                hotkey = vk;
                waitingForKey = false;
                waitingTarget = nullptr;
                prevDown[vk] = nowDown;
                break;
            }

            prevDown[vk] = nowDown;
        }
    }
    else
    {
        snprintf(buttonText, sizeof(buttonText), "%s", keyName);
    }

    if (ImGui::Button(buttonText, ImVec2(150, 0)))
    {
        waitingForKey = true;
        waitingTarget = &hotkey;

        for (int vk = 1; vk < 256; ++vk)
            prevDown[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
    }
}

namespace widgets
{
    namespace theme
    {
        inline ImVec4 accent = ImVec4(0.34f, 0.45f, 1.00f, 1.00f);
        inline ImVec4 bg = ImVec4(0.1f, 0.1f, 0.1f, 1.f);
        inline ImVec4 bg_hovered = ImVec4(0.2f, 0.2f, 0.2f, 1.f);
        inline ImVec4 text = ImVec4(1.f, 1.f, 1.f, 1.f);
        inline ImVec4 text_dim = ImVec4(0.6f, 0.6f, 0.6f, 1.f);
        inline float  rounding = 4.f;
    }

    inline bool checkbox(const char* label, bool& value)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float size = 16.f;
        const float pad = 3.f;

        ImGui::InvisibleButton(label, ImVec2(ImGui::GetContentRegionAvail().x, size));
        const bool clicked = ImGui::IsItemClicked();
        const bool hovered = ImGui::IsItemHovered();

        if (clicked) value = !value;

        const ImU32 col_outline = hovered
            ? ImGui::ColorConvertFloat4ToU32(theme::accent)
            : IM_COL32(80, 80, 80, 255);

        dl->AddRect(
            pos,
            ImVec2(pos.x + size, pos.y + size),
            col_outline, theme::rounding, 0, 1.5f);

        if (value)
        {
            dl->AddRectFilled(
                ImVec2(pos.x + pad, pos.y + pad),
                ImVec2(pos.x + size - pad, pos.y + size - pad),
                ImGui::ColorConvertFloat4ToU32(theme::accent),
                theme::rounding * 0.5f);
        }

        dl->AddText(
            ImVec2(pos.x + size + 8.f, pos.y + 1.f),
            value
            ? ImGui::ColorConvertFloat4ToU32(theme::text)
            : ImGui::ColorConvertFloat4ToU32(theme::text_dim),
            label);

        return clicked;
    }

    inline bool slider_float(const char* label, float& value,
        float min, float max, const char* fmt = "%.2f")
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float w = ImGui::GetContentRegionAvail().x;
        const float h = 6.f;
        const float th = 12.f;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float label_h = ImGui::GetTextLineHeight();

        char val_buf[64]{};
        snprintf(val_buf, sizeof(val_buf), fmt, value);

        dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(theme::text), label);
        dl->AddText(
            ImVec2(pos.x + w - ImGui::CalcTextSize(val_buf).x, pos.y),
            ImGui::ColorConvertFloat4ToU32(theme::text_dim), val_buf);

        const ImVec2 track_pos = ImVec2(pos.x, pos.y + label_h + 4.f);

        dl->AddRectFilled(
            track_pos,
            ImVec2(track_pos.x + w, track_pos.y + h),
            IM_COL32(50, 50, 50, 255), h * 0.5f);

        const float t = (value - min) / (max - min);
        dl->AddRectFilled(
            track_pos,
            ImVec2(track_pos.x + w * t, track_pos.y + h),
            ImGui::ColorConvertFloat4ToU32(theme::accent), h * 0.5f);

        const float thumb_x = track_pos.x + w * t;
        dl->AddCircleFilled(
            ImVec2(thumb_x, track_pos.y + h * 0.5f),
            th * 0.5f,
            IM_COL32(255, 255, 255, 255));

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y));
        ImGui::InvisibleButton(label,
            ImVec2(w, label_h + 4.f + h + 8.f));

        bool changed = false;
        if (ImGui::IsItemActive())
        {
            const float mouse_x = ImGui::GetIO().MousePos.x;
            const float new_t = std::clamp(
                (mouse_x - track_pos.x) / w, 0.f, 1.f);
            const float new_val = min + new_t * (max - min);
            if (new_val != value) { value = new_val; changed = true; }
        }

        ImGui::SetCursorScreenPos(
            ImVec2(pos.x, track_pos.y + h + 8.f));
        return changed;
    }

    inline bool slider_int(const char* label, int& value,
        int min, int max)
    {
        float f = static_cast<float>(value);
        const bool changed = slider_float(label, f,
            static_cast<float>(min),
            static_cast<float>(max), "%.0f");
        if (changed) value = static_cast<int>(std::roundf(f));
        return changed;
    }

    inline bool combo(const char* label, int& current,
        const char* const* items, int count)
    {
        const float w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float h = 22.f;
        const float label_h = ImGui::GetTextLineHeight();

        dl->AddText(pos,
            ImGui::ColorConvertFloat4ToU32(theme::text), label);

        const ImVec2 box_pos = ImVec2(pos.x, pos.y + label_h + 2.f);

        const bool hovered = ImGui::IsMouseHoveringRect(
            box_pos, ImVec2(box_pos.x + w, box_pos.y + h));

        dl->AddRectFilled(
            box_pos,
            ImVec2(box_pos.x + w, box_pos.y + h),
            hovered
            ? ImGui::ColorConvertFloat4ToU32(theme::bg_hovered)
            : ImGui::ColorConvertFloat4ToU32(theme::bg),
            theme::rounding);

        dl->AddRect(
            box_pos,
            ImVec2(box_pos.x + w, box_pos.y + h),
            IM_COL32(80, 80, 80, 255), theme::rounding);

        if (current >= 0 && current < count)
            dl->AddText(
                ImVec2(box_pos.x + 8.f, box_pos.y + 3.f),
                ImGui::ColorConvertFloat4ToU32(theme::text),
                items[current]);

        const float ax = box_pos.x + w - 16.f;
        const float ay = box_pos.y + h * 0.5f;
        dl->AddTriangleFilled(
            ImVec2(ax, ay - 3.f),
            ImVec2(ax + 8.f, ay - 3.f),
            ImVec2(ax + 4.f, ay + 3.f),
            IM_COL32(180, 180, 180, 255));

        ImGui::SetCursorScreenPos(box_pos);
        ImGui::InvisibleButton(label, ImVec2(w, h));

        bool changed = false;
        if (ImGui::IsItemClicked())
            ImGui::OpenPopup(label);

        ImGui::SetCursorScreenPos(
            ImVec2(pos.x, box_pos.y + h + 4.f));

        ImGui::SetNextWindowPos(
            ImVec2(box_pos.x, box_pos.y + h), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);

        if (ImGui::BeginPopup(label))
        {
            for (int i = 0; i < count; i++)
            {
                const bool selected = (i == current);
                if (ImGui::Selectable(items[i], selected))
                {
                    current = i;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndPopup();
        }

        return changed;
    }

    inline void section(const char* title)
    {
        const float w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float text_w = ImGui::CalcTextSize(title).x;
        const float h = ImGui::GetTextLineHeight();
        const float pad = 6.f;

        dl->AddLine(
            ImVec2(pos.x, pos.y + h * 0.5f),
            ImVec2(pos.x + pad, pos.y + h * 0.5f),
            ImGui::ColorConvertFloat4ToU32(theme::accent), 1.f);

        dl->AddText(
            ImVec2(pos.x + pad * 2.f, pos.y),
            ImGui::ColorConvertFloat4ToU32(theme::accent), title);

        dl->AddLine(
            ImVec2(pos.x + pad * 2.f + text_w + pad, pos.y + h * 0.5f),
            ImVec2(pos.x + w, pos.y + h * 0.5f),
            ImGui::ColorConvertFloat4ToU32(theme::accent), 1.f);

        ImGui::Dummy(ImVec2(w, h + 4.f));
    }

    inline bool color_picker(const char* label, ImVec4& color)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float  w = ImGui::GetContentRegionAvail().x;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float  lh = ImGui::GetTextLineHeight();
        const float  sw = 40.f;
        const float  sh = lh + 2.f;

        dl->AddText(pos,
            ImGui::ColorConvertFloat4ToU32(theme::text), label);

        const ImVec2 sp = ImVec2(pos.x + w - sw, pos.y);
        const ImVec2 se = ImVec2(pos.x + w, pos.y + sh);

        for (int xi = 0; xi < 4; xi++)
            for (int yi = 0; yi < 2; yi++)
            {
                if ((xi + yi) % 2 == 0)
                    dl->AddRectFilled(
                        ImVec2(sp.x + xi * (sw / 4.f), sp.y + yi * (sh / 2.f)),
                        ImVec2(sp.x + (xi + 1) * (sw / 4.f), sp.y + (yi + 1) * (sh / 2.f)),
                        IM_COL32(100, 100, 100, 255));
                else
                    dl->AddRectFilled(
                        ImVec2(sp.x + xi * (sw / 4.f), sp.y + yi * (sh / 2.f)),
                        ImVec2(sp.x + (xi + 1) * (sw / 4.f), sp.y + (yi + 1) * (sh / 2.f)),
                        IM_COL32(60, 60, 60, 255));
            }

        dl->AddRectFilled(sp, se,
            ImGui::ColorConvertFloat4ToU32(color), theme::rounding);
        dl->AddRect(sp, se, IM_COL32(70, 70, 70, 255), theme::rounding);

        ImGui::SetCursorScreenPos(sp);
        ImGui::InvisibleButton(label, ImVec2(sw, sh));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup(label);

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + sh + 4.f));

        bool changed = false;
        if (ImGui::BeginPopup(label))
        {
            changed = ImGui::ColorPicker4("##cp",
                reinterpret_cast<float*>(&color),
                ImGuiColorEditFlags_AlphaBar |
                ImGuiColorEditFlags_NoSidePreview);
            ImGui::EndPopup();
        }

        ImGui::Dummy(ImVec2(0.f, 2.f));
        return changed;
    }

    inline float& anim_state(const char* key)
    {
        static std::unordered_map<std::string, float> s_states;
        return s_states[key];
    }

    inline float lerp_anim(const char* key, float target)
    {
        float& val = anim_state(key);
        val += (target - val) * std::clamp(
            ImGui::GetIO().DeltaTime * 8.f, 0.f, 1.f);
        return val;
    }

}
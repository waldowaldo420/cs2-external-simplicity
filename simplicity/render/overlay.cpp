#include "overlay.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../game/reader.h"
#include "../game/game.h"
#include "../utils/globals.h"
#include "../menu/menu.h"
#include "../features/esp/esp.h"
#include "../features/aimbot/aimbot.h"
#include "../features/rcs/rcs.h"
#include "../features/triggerbot/triggerbot.h"
#include "../features/botwalker/botwalker.h"
#include <dwmapi.h>
#include <iostream>
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace render
{
    c_overlay g_overlay;

    bool c_overlay::create_window()
    {
        m_width = GetSystemMetrics(SM_CXSCREEN);
        m_height = GetSystemMetrics(SM_CYSCREEN);

        m_wc = {};
        m_wc.cbSize = sizeof(WNDCLASSEXW);
        m_wc.style = CS_HREDRAW | CS_VREDRAW;
        m_wc.lpfnWndProc = wnd_proc;
        m_wc.hInstance = GetModuleHandleW(nullptr);
        m_wc.lpszClassName = L"Windows.UI.Composition"; 

        if (!RegisterClassExW(&m_wc))
        {
            std::cout << "[overlay] failed to register window class\n";
            return false;
        }

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST |  
            WS_EX_LAYERED | 
            WS_EX_TRANSPARENT |  
            WS_EX_NOACTIVATE,  
            m_wc.lpszClassName,
            L"Program Manager", 
            WS_POPUP,       
            0, 0,
            m_width, m_height,
            nullptr, nullptr,
            m_wc.hInstance,
            nullptr);

        if (!m_hwnd)
        {
            std::cout << "[overlay] failed to create window\n";
            return false;
        }

        SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        std::cout << "[overlay] window created " << m_width << "x" << m_height << "\n";
        return true;
    }

    bool c_overlay::create_device()
    {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = m_hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, 
            D3D_DRIVER_TYPE_HARDWARE, 
            nullptr,
            0,       
            feature_levels,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &m_swapchain,
            &m_device,
            &feature_level,
            &m_context);

        if (FAILED(hr))
        {
            std::cout << "[overlay] D3D11 device creation failed: 0x" << std::hex << hr << "\n";
            return false;
        }

        std::cout << "[overlay] D3D11 device created\n";
        return create_render_target();
    }

    bool c_overlay::create_render_target()
    {
        ID3D11Texture2D* back_buffer = nullptr;
        m_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        if (!back_buffer) return false;

        m_device->CreateRenderTargetView(back_buffer, nullptr, &m_render_target);
        back_buffer->Release();
        return m_render_target != nullptr;
    }

    void c_overlay::destroy_render_target()
    {
        if (m_render_target)
        {
            m_render_target->Release();
            m_render_target = nullptr;
        }
    }

    void c_overlay::init_imgui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(m_hwnd);
        ImGui_ImplDX11_Init(m_device, m_context);

        menu::apply_style();

        std::cout << "[overlay] imgui initialized\n";
    }

    void c_overlay::begin_frame()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void c_overlay::end_frame()
    {
        ImGui::Render();

        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
        m_context->OMSetRenderTargets(1, &m_render_target, nullptr);
        m_context->ClearRenderTargetView(m_render_target, clear_color);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_swapchain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    }

    static void set_click_through(HWND hwnd, bool click_through)
    {
        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (click_through)
        {
            ex |= WS_EX_TRANSPARENT;
            ex |= WS_EX_NOACTIVATE;
        }
        else
        {
            ex &= ~WS_EX_TRANSPARENT;
            ex &= ~WS_EX_NOACTIVATE;
        }
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    }

    void c_overlay::run()
    {
        if (!create_window()) return;
        if (!create_device()) return;
        init_imgui();

        m_running = true;

        std::cout << "[overlay] running - INSERT to toggle menu, END to exit\n";

        static bool insert_held = false;
        static DWORD attached_thread = 0;

        MSG msg{};
        while (m_running)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                if (msg.message == WM_QUIT)
                    m_running = false;
            }
            if (!m_running) break;

            if (GetAsyncKeyState(VK_END) & 0x8000)
            {
                m_running = false;
                break;
            }

            const bool insert_cur = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
            if (insert_cur && !insert_held)
            {
                const bool was_open = globals::menu_open;
                globals::menu_open = !globals::menu_open;
                set_click_through(m_hwnd, !globals::menu_open);

                if (globals::menu_open)
                {
                    HWND gw = game::g_game.get_hwnd();
                    const DWORD fg_thread = gw
                        ? GetWindowThreadProcessId(gw, nullptr) : 0;
                    const DWORD my_thread = GetCurrentThreadId();
                    if (fg_thread && fg_thread != my_thread)
                    {
                        AttachThreadInput(fg_thread, my_thread, TRUE);
                        attached_thread = fg_thread;
                    }
                    SetForegroundWindow(m_hwnd);
                    SetFocus(m_hwnd);
                }
                else if (was_open)
                {
                    const DWORD my_thread = GetCurrentThreadId();
                    if (attached_thread && attached_thread != my_thread)
                        AttachThreadInput(attached_thread, my_thread, FALSE);
                    attached_thread = 0;

                    HWND gw = game::g_game.get_hwnd();
                    if (gw) { SetForegroundWindow(gw); SetFocus(gw); }
                }
            }
            insert_held = insert_cur;

            game::g_reader.update();
            begin_frame();
            menu::draw();

			// All features are ran here just to demonstrate how to do simple single threaded cheat. You can move them to separate threads if you want, but make sure to handle synchronization properly.
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            esp::draw(dl, game::g_reader.snapshot());

            const auto snap = game::g_reader.snapshot();
            aimbot::update(snap);
            rcs::update(snap);
            triggerbot::update(snap);
            botwalker::update(snap);

            dl->AddText(ImVec2(10.f, 10.f),
                IM_COL32(0, 255, 0, 255),
                "simplicity v.1.0.0");

            end_frame();
        }

        shutdown();
    }

    void c_overlay::shutdown()
    {
        cleanup_imgui();
        cleanup_device();
        cleanup_window();
        std::cout << "[overlay] shutdown\n";
    }

    void c_overlay::cleanup_imgui()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void c_overlay::cleanup_device()
    {
        destroy_render_target();
        if (m_swapchain) { m_swapchain->Release();  m_swapchain = nullptr; }
        if (m_context) { m_context->Release();    m_context = nullptr; }
        if (m_device) { m_device->Release();     m_device = nullptr; }
    }

    void c_overlay::cleanup_window()
    {
        if (m_hwnd)
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
    }

    LRESULT WINAPI c_overlay::wnd_proc(HWND hWnd, UINT msg,
        WPARAM wParam, LPARAM lParam)
    {
        if (globals::menu_open &&
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_GETICON:
            return 0;

        case WM_SIZE:
            if (g_overlay.m_device && wParam != SIZE_MINIMIZED)
            {
                g_overlay.destroy_render_target();
                g_overlay.m_swapchain->ResizeBuffers(
                    0,
                    LOWORD(lParam), HIWORD(lParam),
                    DXGI_FORMAT_UNKNOWN, 0);
                g_overlay.create_render_target();
                g_overlay.m_width = LOWORD(lParam);
                g_overlay.m_height = HIWORD(lParam);
            }
            return 0;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}
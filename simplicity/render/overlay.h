#pragma once
#include <d3d11.h>
#include <Windows.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace render
{
    class c_overlay
    {
    public:
        c_overlay() = default;
        ~c_overlay() { shutdown(); }

        c_overlay(const c_overlay&) = delete;
        c_overlay& operator=(const c_overlay&) = delete;

        void run();

        void shutdown();

        bool is_running() const { return m_running; }

        int width() const { return m_width; }
        int height() const { return m_height; }

        ID3D11Device* device()  const { return m_device; }
        ID3D11DeviceContext* context() const { return m_context; }

    private:
        HWND m_hwnd = nullptr;
        WNDCLASSEXW m_wc = {};
        int m_width = 0;
        int m_height = 0;
        bool m_running = false;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;
        IDXGISwapChain* m_swapchain = nullptr;
        ID3D11RenderTargetView* m_render_target = nullptr;

        bool create_window();
        bool create_device();
        bool create_render_target();
        void destroy_render_target();
        void init_imgui();
        void begin_frame();
        void end_frame();
        void cleanup_window();
        void cleanup_device();
        void cleanup_imgui();

        static LRESULT WINAPI wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    };

    extern c_overlay g_overlay;

}
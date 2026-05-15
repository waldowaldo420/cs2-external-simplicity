#pragma once
#include <string>
#include <atomic>
#include <thread>
#include "../memory/memory.h"
#include "../offsets/offsets.hpp"
#include "../offsets/client_dll.hpp"

namespace game
{
    class c_game
    {
    public:
        c_game() = default;
        ~c_game() { shutdown(); }

        HWND get_hwnd() const { return m_hwnd; }

        c_game(const c_game&) = delete;
        c_game& operator=(const c_game&) = delete;

        bool initialize();
        void shutdown();
        bool is_running() const { return m_running.load(); }

        mem::process& get_process() { return m_process; }

        uintptr_t client_base()  const { return m_client_base; }
        uintptr_t engine_base()  const { return m_engine_base; }
        uintptr_t vphysics_base()  const { return m_vphysics_base; }
        size_t client_size() const { return m_client_size; }
        size_t engine_size() const { return m_engine_size; }
        size_t vphysics_size() const { return m_vphysics_size; }

    private:
        mem::process m_process{};
        std::atomic<bool> m_running{ false };

        uintptr_t m_client_base = 0;
        uintptr_t m_engine_base = 0;
        uintptr_t m_vphysics_base = 0;
        size_t m_client_size = 0;
        size_t m_engine_size = 0;
		size_t m_vphysics_size = 0;

        HWND m_hwnd = nullptr;

        DWORD find_process(const wchar_t* name) const;

        bool verify_attachment() const;
    };

    inline c_game g_game;
}
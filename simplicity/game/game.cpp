#include "game.h"
#include <iostream>
#include <TlHelp32.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

namespace game
{
    DWORD c_game::find_process(const wchar_t* name) const
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        DWORD pid = 0;

        if (Process32FirstW(snap, &entry))
        {
            do
            {
                std::wstring proc_name = entry.szExeFile;
                std::wstring target = name;

                for (auto& c : proc_name) c = towlower(c);
                for (auto& c : target)    c = towlower(c);

                if (proc_name == target)
                {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return pid;
    }

    bool c_game::verify_attachment() const
    {
        const uint32_t tick = m_process.read<uint32_t>(0x7FFE0320);

        if (tick == 0)
        {
            std::cout << "[game] verification failed — tick=0\n";
            return false;
        }

        std::cout << "[game] verified! TickCountMultiplier=0x"
            << std::hex << tick << std::dec << "\n";
        return true;
    }

    bool c_game::initialize()
    {
        std::cout << "[game] waiting for cs2.exe...\n";

        DWORD pid = 0;

        while (!pid)
        {
            pid = find_process(L"cs2.exe");
            if (!pid)
            {
                std::cout << "[game] cs2.exe not found, retrying...\n";
                Sleep(1000);
            }
        }

        std::cout << "[game] found cs2.exe PID=" << pid << "\n";

        if (!m_process.attach(pid))
        {
            std::cout << "[game] failed to attach!\n";
            return false;
        }

        if (!verify_attachment())
        {
            std::cout << "[game] attachment verification failed!\n";
            m_process.detach();
            return false;
        }

        m_client_base = m_process.get_module_base(L"client.dll");
        m_engine_base = m_process.get_module_base(L"engine2.dll");
        m_vphysics_base = m_process.get_module_base(L"vphysics2.dll");

        auto get_size = [&](uintptr_t base) -> size_t
            {
                if (!base) return 0;
                const uint32_t pe_offset = m_process.read<uint32_t>(base + 0x3C);
                return m_process.read<uint32_t>(base + pe_offset + 0x50);
            };

        m_client_size = get_size(m_client_base);
        m_engine_size = get_size(m_engine_base);
        m_vphysics_size = get_size(m_vphysics_base);

        if (!m_client_base)
        {
            std::cout << "[game] WARNING: client.dll not found!\n";
            std::cout << "[game] Is CS2 fully loaded?\n";
        }

        m_running.store(true);
        std::cout << "[game] ready!\n";

        for (int i = 0; i < 10 && !m_hwnd; i++)
        {
            m_hwnd = FindWindowW(nullptr, L"Counter-Strike 2");
            if (!m_hwnd) Sleep(200);
        }

        if (m_hwnd)
            std::cout << "[game] cs2 hwnd: 0x" << std::hex
            << (uintptr_t)m_hwnd << std::dec << "\n";
        else
            std::cout << "[game] WARNING: cs2 window not found!\n";

        return true;
    }

    void c_game::shutdown()
    {
        timeEndPeriod(1);
        m_running.store(false);
        m_process.detach();
        std::cout << "[game] shutdown\n";
    }
}
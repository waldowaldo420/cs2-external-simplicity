#include "reader.h"
#include "game.h"
#include "../offsets/offsets.hpp"
#include "../offsets/client_dll.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

using namespace cs2_dumper::offsets::client_dll;
using namespace cs2_dumper::schemas::client_dll;

namespace game
{
    c_reader g_reader;

    void c_reader::update()
    {
        static bool timer_set = false;
        if (!timer_set) { timeBeginPeriod(1); timer_set = true; }

        if (!g_game.is_running()) return;
        read_view_matrix();
        read_local_player();
        read_players();
        read_map_name();
        update_bvh();
        update_visibility();
        Sleep(1);
    }

    void c_reader::read_view_matrix()
    {
        auto& mem = g_game.get_process();
        const uintptr_t cb = g_game.client_base();

        const uintptr_t vm_addr = cb + dwViewMatrix;
        mem.read_raw(vm_addr, &m_snapshot.view_matrix, sizeof(math::Matrix4x4));
    }

    void c_reader::read_local_player()
    {
        auto& mem = g_game.get_process();
        const uintptr_t cb = g_game.client_base();

        m_snapshot.local_alive = false;
        m_snapshot.local_health = 0;
        m_snapshot.local_team = 0;
        m_snapshot.local_pos = {};
        m_snapshot.local_aim_punch = {};

        const uintptr_t controller = mem.read<uintptr_t>(cb + dwLocalPlayerController);
        if (!controller || controller < 0x10000) return;

        const int team = mem.read<int>(controller + C_BaseEntity::m_iTeamNum);

        const uint32_t pawn_handle = mem.read<uint32_t>(controller + CCSPlayerController::m_hPlayerPawn);
        if (!pawn_handle || pawn_handle == 0xFFFFFFFF) return;

        const uintptr_t entity_list = mem.read<uintptr_t>(cb + dwEntityList);
        if (!entity_list) return;

        const uintptr_t list_entry = mem.read<uintptr_t>(entity_list + 0x8 * ((pawn_handle & 0x7FFF) >> 9) + 0x10);
        if (!list_entry) return;

        const uintptr_t pawn = mem.read<uintptr_t>(list_entry + 0x70 * (pawn_handle & 0x1FF));
        if (!pawn || pawn < 0x10000) return;

        const int health = mem.read<int>(pawn + C_BaseEntity::m_iHealth);
        if (health <= 0 || health > 100) return;

        m_snapshot.local_sensitivity = 1.f;
        const uintptr_t sens_ptr = mem.read<uintptr_t>(cb + dwSensitivity);
        if (sens_ptr)
        {
            const float s = mem.read<float>(sens_ptr + dwSensitivity_sensitivity);
            if (std::isfinite(s) && s > 0.f)
                m_snapshot.local_sensitivity = s;
        }

        m_snapshot.local_shots_fired = mem.read<int>(pawn + C_CSPlayerPawn::m_iShotsFired);

        const uintptr_t punch_services = mem.read<uintptr_t>(pawn + C_CSPlayerPawn::m_pAimPunchServices);
        if (punch_services && punch_services > 0x10000)
        {
            const math::Vector3 punch = mem.read<math::Vector3>(punch_services + 0x50);
                m_snapshot.local_aim_punch = punch;
        }

        m_snapshot.local_pawn = pawn;
        m_snapshot.local_health = health;
        m_snapshot.local_team = team;
        m_snapshot.local_alive = true;
        m_snapshot.local_pos = mem.read<math::Vector3>(pawn + C_BasePlayerPawn::m_vOldOrigin);
        m_snapshot.local_view_angles = mem.read<math::Vector3>(cb + dwViewAngles);
    }

    void c_reader::read_players()
    {
        auto& mem = g_game.get_process();
        const uintptr_t cb = g_game.client_base();

        m_snapshot.player_count = 0;

        const uintptr_t entity_list = mem.read<uintptr_t>(cb + dwEntityList);
        if (!entity_list) return;

        for (int i = 1; i < 64; i++)
        {
            const uintptr_t list_entry = mem.read<uintptr_t>(entity_list + 0x8 * (i >> 9) + 0x10);
            if (!list_entry) continue;

            const uintptr_t controller = mem.read<uintptr_t>(list_entry + 0x70 * (i & 0x1FF));
            if (!controller || controller < 0x10000) continue;

            const uint32_t pawn_handle = mem.read<uint32_t>(controller + CCSPlayerController::m_hPlayerPawn);
            if (!pawn_handle || pawn_handle == 0xFFFFFFFF) continue;

            const uintptr_t pawn_entry = mem.read<uintptr_t>(entity_list + 0x8 * ((pawn_handle & 0x7FFF) >> 9) + 0x10);
            if (!pawn_entry) continue;

            const uintptr_t pawn = mem.read<uintptr_t>(pawn_entry + 0x70 * (pawn_handle & 0x1FF));
            if (!pawn || pawn < 0x10000) continue;

            if (pawn == m_snapshot.local_pawn) continue;

            const int health = mem.read<int>(pawn + C_BaseEntity::m_iHealth);
            if (health <= 0 || health > 100) continue;

            auto& p = m_snapshot.players[m_snapshot.player_count++];
            p.pawn = pawn;
            p.health = health;
            p.team = mem.read<int>(controller + C_BaseEntity::m_iTeamNum);
            p.position = mem.read<math::Vector3>(pawn + C_BasePlayerPawn::m_vOldOrigin);
            p.alive = true;

            p.name[0] = '\0';
            const uintptr_t name_ptr = mem.read<uintptr_t>(controller + CCSPlayerController::m_sSanitizedPlayerName);
            if (name_ptr)
                mem.read_raw(name_ptr, p.name, sizeof(p.name) - 1);

            read_bones(p, pawn);

            if (m_snapshot.player_count >= MAX_PLAYERS) break;
        }
    }

    void c_reader::read_map_name()
    {
        auto& mem = g_game.get_process();
        const uintptr_t cb = g_game.client_base();

        m_snapshot.map_name[0] = '\0';

        const uintptr_t global_vars = mem.read<uintptr_t>(cb + dwGlobalVars);
        if (!global_vars) return;

        const uintptr_t map_name_ptr = mem.read<uintptr_t>(global_vars + 0x180);
        if (!map_name_ptr) return;

        mem.read_raw(map_name_ptr, m_snapshot.map_name, sizeof(m_snapshot.map_name) - 1);
    }

    void c_reader::read_bones(game::PlayerEntry& p, uintptr_t pawn)
    {
        auto& mem = g_game.get_process();

        for (auto& b : p.bones) b = {};

        const uintptr_t scene_node = mem.read<uintptr_t>(
            pawn + C_BaseEntity::m_pGameSceneNode);
        if (!scene_node || scene_node < 0x10000) return;

        const uintptr_t bone_array = mem.read<uintptr_t>(scene_node + 0x1D0);
        if (!bone_array || bone_array < 0x10000) return;

        constexpr int kMaxBoneIdx = 27;
        constexpr int kBoneStride = 32;
        constexpr int kBulkBytes = (kMaxBoneIdx + 1) * kBoneStride;

        uint8_t bone_buf[kBulkBytes]{};
        const bool bulk_ok = mem.read_raw(bone_array, bone_buf, kBulkBytes);

        const math::Vector3& origin = p.position;

        for (int i = 0; i < BONE_COUNT; i++)
        {
            const int game_idx = BONE_INDICES[i];

            float x, y, z;

            if (bulk_ok && game_idx <= kMaxBoneIdx)
            {
                const float* bp = reinterpret_cast<const float*>(bone_buf + game_idx * kBoneStride);
                x = bp[0]; y = bp[1]; z = bp[2];
            }
            else
            {
                struct { float x, y, z; } raw{};
                if (!mem.read_raw(bone_array + game_idx * kBoneStride,
                    &raw, sizeof(raw))) continue;
                x = raw.x; y = raw.y; z = raw.z;
            }

            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
            if (x == 0.f && y == 0.f && z == 0.f) continue;

            const float dx = x - origin.x;
            const float dy = y - origin.y;
            const float dz = z - origin.z;
            if (dx * dx + dy * dy + dz * dz > 500.f * 500.f) continue;

            p.bones[i].pos = { x, y, z };
            p.bones[i].valid = true;
        }
    }

    void c_reader::update_bvh()
    {
        if (m_snapshot.map_name[0] == '\0') return;
        if (strncmp(m_snapshot.map_name, m_last_map, sizeof(m_last_map)) == 0) return;

        strncpy_s(m_last_map, m_snapshot.map_name, sizeof(m_last_map) - 1);
        m_snapshot.bvh_ready = false;

        int attempt = 0;
        while (true)
        {
            if (strncmp(m_snapshot.map_name, m_last_map, sizeof(m_last_map)) != 0)
            {
                return;
            }

            attempt++;
            m_bvh.clear();
            m_bvh.parse();

            if (m_bvh.valid())
            {
                m_snapshot.bvh_ready = true;
                return;
            }

            Sleep(5000);
        }
    }

    void c_reader::update_visibility()
    {
        if (!m_snapshot.bvh_ready) return;
        if (!m_snapshot.local_alive) return;

        const math::Vector3 eye = {
            m_snapshot.local_pos.x,
            m_snapshot.local_pos.y,
            m_snapshot.local_pos.z + 64.f
        };

        for (int i = 0; i < m_snapshot.player_count; i++)
        {
            auto& p = m_snapshot.players[i];
            p.visible = false;

            math::Vector3 target{};
            if (p.bones[game::bone_slot::head].valid)
                target = p.bones[game::bone_slot::head].pos;
            else
            {
                target = p.position;
                target.z += 64.f;
            }

            const auto result = m_bvh.trace_ray(eye, target);
            p.visible = !result.hit || result.fraction > 0.97f;
        }
    }
}
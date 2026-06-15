#pragma once
#include <array>
#include <atomic>
#include "../memory/memory.h"
#include "../utils/math.h"
#include "bvh.h"

namespace game
{
    constexpr int MAX_PLAYERS = 64;
    constexpr int BONE_COUNT = 28;

    struct BonePos
    {
        math::Vector3 pos{};
        bool valid = false;
    };

    struct PlayerEntry
    {
        uintptr_t pawn = 0;
        BonePos bones[BONE_COUNT]{};
        math::Vector3 position = {};
        char name[128]{};
        int health = 0;
        int team = 0;
        bool visible = false;
        bool alive = false;
        
    };

    struct GameSnapshot
    {
        uintptr_t local_pawn = 0;
        bool local_alive = false;
        bool bvh_ready = false;
        int local_health = 0;
        int local_team = 0;
        int local_shots_fired = 0;
        int player_count = 0;
        float local_sensitivity = 1.0f;
        char map_name[256]{};
        math::Vector3 local_pos = {};
        math::Vector3 local_eye_pos = {};
        math::Vector3 local_aim_punch = {};
        math::Vector3 local_view_angles = {};
        math::Matrix4x4  view_matrix = {};
        PlayerEntry players[MAX_PLAYERS]{};  
    };

    namespace bone_slot
    {
        constexpr int pelvis = 0;
        constexpr int spine_0 = 1;
        constexpr int spine_1 = 2;
        constexpr int spine_2 = 3;
        constexpr int neck = 4;
        constexpr int head = 5;
        constexpr int clavicle_l = 6;
        constexpr int shoulder_l = 7;
        constexpr int elbow_l = 8;
        constexpr int hand_l = 9;
        constexpr int clavicle_r = 10;
        constexpr int shoulder_r = 11;
        constexpr int elbow_r = 12;
        constexpr int hand_r = 13;
        constexpr int hip_l = 14;
        constexpr int knee_l = 15;
        constexpr int foot_l = 16;
        constexpr int hip_r = 17;
        constexpr int knee_r = 18;
        constexpr int foot_r = 19;
        constexpr int chest = 20;
        constexpr int gun = 21;
        constexpr int eye_l = 22;
        constexpr int eye_r = 23;
    }

    static constexpr int BONE_INDICES[BONE_COUNT] = {
         1,  2,  3,  4,  6,  7,  8,  9,
        10, 11, 12, 13, 14, 15, 17, 18,
        19, 20, 21, 22, 23, 24, 25, 26,
        74, 77, 81, 86
    };

    class c_reader
    {
    public:
        c_reader() = default;
        ~c_reader() = default;

        void update();
        const GameSnapshot& snapshot() const { return m_snapshot; }

    private:
        GameSnapshot m_snapshot{};
        bvh  m_bvh{};
        char m_last_map[256]{};

        void read_local_player();
        void read_players();
        void read_map_name();
        void read_view_matrix();
        void read_bones(game::PlayerEntry& p, uintptr_t pawn);
        void update_bvh();
        void update_visibility();
    };

    extern c_reader g_reader;
}
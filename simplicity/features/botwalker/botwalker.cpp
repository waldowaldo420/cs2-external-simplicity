#include "botwalker.h"
#include "../../utils/globals.h"
#include "../../render/overlay.h"
#include "../../utils/math.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <limits>

namespace botwalker
{
    MapData g_recordings;
    bool g_is_recording = false;
    int g_edit_rec = 0;
    int g_active_rec = 0;
    int g_active_frame = 0;

    static bool s_was_alive = false;
    static bool s_w_down = false;
    static bool s_a_down = false;
    static bool s_s_down = false;
    static bool s_d_down = false;
    static bool s_space_down = false;
    static bool s_lmb_down = false;
    static bool s_engaging = false;
    static float s_aim_x = 0.f;
    static float s_aim_y = 0.f;
    static DWORD s_last_sample_ms = 0;
    static DWORD s_frame_stuck_ms = 0;
    static bool s_burst_firing = false;
    static DWORD s_burst_timer_ms = 0;
    static int s_rec_cycle_idx = 0;
    static int s_start_phase = 3;
    static DWORD s_start_timer_ms = 0;
    static math::Vector3 s_last_alive_pos = {};
    static int s_weapon_step = -1;
    static DWORD s_weapon_next_ms = 0;

    static void set_key(bool& state, WORD vk, bool down)
    {
        if (state == down) return;
        state = down;
        INPUT inp{};
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = vk;
        inp.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &inp, sizeof(INPUT));
    }

    static void set_lmb(bool down)
    {
        if (s_lmb_down == down) return;
        s_lmb_down = down;
        INPUT inp{};
        inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, &inp, sizeof(INPUT));
    }

    static void send_mouse(int dx, int dy)
    {
        if (dx == 0 && dy == 0) return;
        INPUT inp{};
        inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_MOVE;
        inp.mi.dx = dx;
        inp.mi.dy = dy;
        SendInput(1, &inp, sizeof(INPUT));
    }

    static void force_key(WORD vk, bool down)
    {
        INPUT inp{};
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = vk;
        inp.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &inp, sizeof(INPUT));
    }

    static void press_key_once(WORD vk)
    {
        INPUT in[2]{};
        in[0].type = in[1].type = INPUT_KEYBOARD;
        in[0].ki.wVk = in[1].ki.wVk = vk;
        in[0].ki.dwFlags = 0;
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
    }

    static void release_all()
    {
        force_key(0x57, false);
        s_w_down = false;
        set_key(s_a_down, 0x41, false);
        set_key(s_s_down, 0x53, false);
        set_key(s_d_down, 0x44, false);
        set_key(s_space_down, VK_SPACE, false);
        set_lmb(false);
        s_aim_x = s_aim_y = 0.f;
        s_engaging = false;
    }

    int recording_count(const std::string& map, int team)
    {
        auto m = g_recordings.find(map);
        if (m == g_recordings.end()) return 0;
        auto t = m->second.find(team);
        if (t == m->second.end()) return 0;
        return (int)t->second.size();
    }

    int frame_count(const std::string& map, int team, int idx)
    {
        auto m = g_recordings.find(map);
        if (m == g_recordings.end()) return 0;
        auto t = m->second.find(team);
        if (t == m->second.end()) return 0;
        if (idx < 0 || idx >= (int)t->second.size()) return 0;
        return (int)t->second[idx].size();
    }

    void start_recording(const game::GameSnapshot& snap)
    {
        if (!snap.map_name[0] || !snap.local_alive) return;
        auto& recs      = g_recordings[snap.map_name][snap.local_team];
        recs.emplace_back();
        g_edit_rec      = (int)recs.size() - 1;
        g_is_recording  = true;
        s_last_sample_ms = GetTickCount();
    }

    void stop_recording()
    {
        g_is_recording = false;
    }

    void delete_recording(const std::string& map, int team, int idx)
    {
        auto mit = g_recordings.find(map);
        if (mit == g_recordings.end()) return;
        auto tit = mit->second.find(team);
        if (tit == mit->second.end()) return;
        auto& recs = tit->second;
        if (idx < 0 || idx >= (int)recs.size()) return;
        recs.erase(recs.begin() + idx);
        if (g_edit_rec   >= (int)recs.size() && g_edit_rec   > 0) g_edit_rec--;
        if (g_active_rec >= (int)recs.size()) g_active_rec = 0;
    }

    void clear_all(const std::string& map, int team)
    {
        g_recordings[map][team].clear();
        g_edit_rec = 0;
        g_active_rec = 0;
        g_active_frame = 0;
    }

    static void tick_recording(const game::GameSnapshot& snap)
    {
        if (!snap.local_alive || !snap.map_name[0]) return;

        const DWORD now = GetTickCount();
        if (now - s_last_sample_ms < RECORD_INTERVAL_MS) return;
        s_last_sample_ms = now;

        auto& recs = g_recordings[snap.map_name][snap.local_team];
        if (g_edit_rec < 0 || g_edit_rec >= (int)recs.size()) return;

        const bool kw = (GetAsyncKeyState(0x57) & 0x8000) != 0;
        const bool ka = (GetAsyncKeyState(0x41) & 0x8000) != 0;
        const bool ks = (GetAsyncKeyState(0x53) & 0x8000) != 0;
        const bool kd = (GetAsyncKeyState(0x44) & 0x8000) != 0;
        const bool ksp = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        auto& rec = recs[g_edit_rec];

        if (!rec.empty())
        {
            const auto& last = rec.back();
            const float dx = snap.local_pos.x - last.x;
            const float dy = snap.local_pos.y - last.y;
            const float dz = snap.local_pos.z - last.z;
            const bool moved = (dx*dx + dy*dy + dz*dz) > 1.f;
            const bool keys_same = (kw == last.w && ka == last.a &&
                                    ks == last.s && kd == last.d && ksp == last.space);
            if (!moved && keys_same) return;
        }

        RecordedFrame f{};
        f.x = snap.local_pos.x;
        f.y = snap.local_pos.y;
        f.z = snap.local_pos.z;
        f.w = kw;
        f.a = ka;
        f.s = ks;
        f.d = kd;
        f.space = ksp;
        rec.push_back(f);
    }

    static void on_round_start(const game::GameSnapshot& snap)
    {
        static bool seeded = false;
        if (!seeded) { std::srand((unsigned)std::time(nullptr)); seeded = true; }

        auto mit = g_recordings.find(snap.map_name);
        if (mit == g_recordings.end()) return;
        auto tit = mit->second.find(snap.local_team);
        if (tit == mit->second.end()) return;

        auto& recs = tit->second;

        std::vector<int> valid;
        for (int i = 0; i < (int)recs.size(); i++)
            if ((int)recs[i].size() >= 2) valid.push_back(i);
        if (valid.empty()) return;

        g_active_rec = valid[s_rec_cycle_idx % (int)valid.size()];
        s_rec_cycle_idx++;

        g_active_frame = 0;
        s_frame_stuck_ms = GetTickCount();
    }

    static void steer_towards(const math::Vector3& target,
                               const game::GameSnapshot& snap,
                               int sw, int sh)
    {
        const float cx = float(sw) * 0.5f;

        math::Vector2 scr{};
        if (math::world_to_screen(target, snap.view_matrix, sw, sh, scr))
        {
            const float err_x = scr.x - cx;
            const float gain  = globals::botwalker::steer_gain;
            const int   mx    = (int)std::clamp(err_x * gain, -25.f, 25.f);
            send_mouse(mx, 0);
        }
        else
        {
            const float* m   = &snap.view_matrix.m[0][0];
            const float  dot = m[0]*target.x + m[1]*target.y + m[2]*target.z + m[3];
            send_mouse(dot >= 0.f ? 15 : -15, 0);
        }
    }

    static void tick_playback(const game::GameSnapshot& snap)
    {
        if (!snap.map_name[0] || !snap.local_alive) { release_all(); return; }

        auto mit = g_recordings.find(snap.map_name);
        if (mit == g_recordings.end()) { release_all(); return; }
        auto tit = mit->second.find(snap.local_team);
        if (tit == mit->second.end() || tit->second.empty()) { release_all(); return; }

        auto& recs = tit->second;
        if (g_active_rec < 0 || g_active_rec >= (int)recs.size()) g_active_rec = 0;

        auto& rec = recs[g_active_rec];
        if ((int)rec.size() < 2) { release_all(); return; }

        if (g_active_frame < 0 || g_active_frame >= (int)rec.size())
            g_active_frame = 0;

        const int sw = render::g_overlay.width();
        const int sh = render::g_overlay.height();

        const auto& cur = rec[g_active_frame];
        const float dx = cur.x - snap.local_pos.x;
        const float dy = cur.y - snap.local_pos.y;
        const float r = globals::botwalker::reach_distance;
        const bool in_reach = (dx*dx + dy*dy < r*r);
        const bool stuck = (GetTickCount() - s_frame_stuck_ms > 2000);

        if (in_reach || stuck)
        {
            if (g_active_frame < (int)rec.size() - 1)
            {
                g_active_frame++;
            }
            else
            {
                g_active_frame = 0;
            }
            s_frame_stuck_ms = GetTickCount();
        }

        const auto& target = rec[g_active_frame];

        steer_towards({target.x, target.y, target.z}, snap, sw, sh);

        force_key(0x57, true);

        set_key(s_a_down, 0x41, target.a);
        set_key(s_s_down, 0x53, target.s);
        set_key(s_d_down, 0x44, target.d);
        set_key(s_space_down, VK_SPACE, target.space);
    }

    static const game::PlayerEntry* find_engage_target(
        const game::GameSnapshot& snap, int sw, int sh)
    {
        const float cx  = float(sw) * 0.5f;
        const float cy  = float(sh) * 0.5f;
        const float fov_px = (globals::botwalker::engage_fov / 90.f) * cx;
        const float fov_sq = fov_px * fov_px;

        const game::PlayerEntry* best = nullptr;
        float best_dq = fov_sq;

        for (int i = 0; i < snap.player_count; i++)
        {
            const auto& p = snap.players[i];
            if (!p.alive || p.health <= 0 || !p.visible) continue;
            if (p.team == snap.local_team) continue;

            constexpr int slot = game::bone_slot::spine_2;
            if (!p.bones[slot].valid) continue;

            math::Vector2 scr{};
            if (!math::world_to_screen(p.bones[slot].pos,
                    snap.view_matrix, sw, sh, scr)) continue;

            const float ddx = scr.x - cx;
            const float ddy = scr.y - cy;
            const float dq  = ddx*ddx + ddy*ddy;
            if (dq < best_dq) { best_dq = dq; best = &p; }
        }
        return best;
    }

    static void engage_target(const game::PlayerEntry* tgt,
                               const game::GameSnapshot& snap,
                               int sw, int sh)
    {
        const float cx = float(sw) * 0.5f;
        const float cy = float(sh) * 0.5f;

        constexpr int slot = game::bone_slot::spine_2;
        math::Vector2 scr{};
        if (!math::world_to_screen(tgt->bones[slot].pos,
                snap.view_matrix, sw, sh, scr)) return;

        const float dx = scr.x - cx;
        const float dy = scr.y - cy;

        const float t = globals::botwalker::engage_smooth;
        s_aim_x += dx * t;
        s_aim_y += dy * t;

        const int mx = std::clamp((int)std::round(s_aim_x), -25, 25);
        const int my = std::clamp((int)std::round(s_aim_y), -25, 25);
        s_aim_x -= float(mx);
        s_aim_y -= float(my);
        send_mouse(mx, my);

        const bool on_target = (dx*dx + dy*dy < 20.f * 20.f);
        const DWORD now_ms  = GetTickCount();
        const int phase_ms  = s_burst_firing
                                    ? globals::botwalker::burst_on_ms
                                    : globals::botwalker::burst_off_ms;
        if ((int)(now_ms - s_burst_timer_ms) >= phase_ms)
        {
            s_burst_firing = !s_burst_firing;
            s_burst_timer_ms = now_ms;
        }
        set_lmb(on_target && s_burst_firing);
    }

    void update(const game::GameSnapshot& snap)
    {
        if (globals::botwalker::toggle_key != 0)
        {
            static bool s_key_was_down = false;
            const bool  down = (GetAsyncKeyState(globals::botwalker::toggle_key) & 0x8000) != 0;
            if (down && !s_key_was_down)
                globals::botwalker::enabled = !globals::botwalker::enabled;
            s_key_was_down = down;
        }

        if (g_is_recording && snap.map_name[0])
        {
            tick_recording(snap);
            return;
        }

        if (!globals::botwalker::enabled || !snap.map_name[0])
        {
            if (s_was_alive) release_all();
            s_was_alive = false;
            s_last_alive_pos = {};
            return;
        }

        if (!snap.local_alive)
        {
            if (s_was_alive) release_all();
            s_was_alive = false;
            s_start_phase = 3;
            s_last_alive_pos = {};
            return;
        }

        if (!s_last_alive_pos.is_zero())
        {
            const float dx = snap.local_pos.x - s_last_alive_pos.x;
            const float dy = snap.local_pos.y - s_last_alive_pos.y;
            if (dx*dx + dy*dy > 300.f * 300.f)
                s_was_alive = false;
        }
        s_last_alive_pos = snap.local_pos;

        if (!s_was_alive)
        {
            s_was_alive = true;
            on_round_start(snap);
            s_start_phase = 0;
            s_start_timer_ms = GetTickCount();
        }

        if (s_start_phase < 3)
        {
            const DWORD elapsed = GetTickCount() - s_start_timer_ms;
            if (s_start_phase == 0 && elapsed >= 500)
            {
                press_key_once(0x32);
                s_start_phase = 1;
                s_start_timer_ms = GetTickCount();
            }
            else if (s_start_phase == 1 && elapsed >= 2000)
            {
                press_key_once(0x31);
                s_start_phase = 2;
                s_start_timer_ms = GetTickCount();
            }
            else if (s_start_phase == 2 && elapsed >= 1500)
            {
                s_start_phase = 3;
                s_frame_stuck_ms = GetTickCount();
                s_weapon_step = -1;
                s_weapon_next_ms = GetTickCount() + 8000 + std::rand() % 7000;
            }
            force_key(0x57, false);
            set_key(s_a_down, 0x41, false);
            set_key(s_s_down, 0x53, false);
            set_key(s_d_down, 0x44, false);
            set_key(s_space_down, VK_SPACE, false);
            return;
        }

        const int sw = render::g_overlay.width();
        const int sh = render::g_overlay.height();

        if (globals::botwalker::engage_enemies)
        {
            const game::PlayerEntry* tgt = find_engage_target(snap, sw, sh);
            if (tgt)
            {
                if (!s_engaging) { s_aim_x = 0.f; s_aim_y = 0.f; }
                s_engaging = true;

                force_key(0x57, false);
                set_key(s_a_down, 0x41, false);
                set_key(s_s_down, 0x53, false);
                set_key(s_d_down, 0x44, false);

                engage_target(tgt, snap, sw, sh);
                return;
            }
        }

        if (s_engaging)
        {
            s_engaging = false;
            set_lmb(false);
            s_aim_x = 0.f;
            s_aim_y = 0.f;
            s_burst_firing = false;
            s_burst_timer_ms = GetTickCount();
        }

        {
            const DWORD nw = GetTickCount();
            if (s_weapon_step == -1 && nw >= s_weapon_next_ms)
            {
                press_key_once(0x33);
                s_weapon_step = 0;
                s_weapon_next_ms = nw + 400;
            }
            else if (s_weapon_step == 0 && nw >= s_weapon_next_ms)
            {
                press_key_once(0x32);
                s_weapon_step = 1;
                s_weapon_next_ms = nw + 400;
            }
            else if (s_weapon_step == 1 && nw >= s_weapon_next_ms)
            {
                press_key_once(0x31);
                s_weapon_step = -1;
                s_weapon_next_ms = nw + 8000 + std::rand() % 12000;
            }
        }

        tick_playback(snap);
    }

    static std::string exe_dir()
    {
        char buf[MAX_PATH]{};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        char* sl = strrchr(buf, '\\');
        if (sl) *(sl + 1) = '\0';
        return buf;
    }

    void save_recordings()
    {
        std::ofstream f(exe_dir() + "recordings.json");
        if (!f) return;

        f << std::fixed;
        f.precision(2);
        f << "{\n";

        bool first_map = true;
        for (const auto& [mn, teams] : g_recordings)
        {
            if (mn.empty()) continue;
            bool has = false;
            for (const auto& [tn, recs] : teams)
                for (const auto& r : recs)
                    if (!r.empty()) { has = true; break; }
            if (!has) continue;

            if (!first_map) f << ",\n";
            first_map = false;
            f << "  \"" << mn << "\": {\n";

            bool first_team = true;
            for (const auto& [tn, recs] : teams)
            {
                bool team_has = false;
                for (const auto& r : recs) if (!r.empty()) { team_has = true; break; }
                if (!team_has) continue;

                if (!first_team) f << ",\n";
                first_team = false;
                f << "    \"" << tn << "\": [\n";

                bool first_rec = true;
                for (const auto& rec : recs)
                {
                    if (rec.empty()) continue;
                    if (!first_rec) f << ",\n";
                    first_rec = false;
                    f << "      [\n";
                    for (int i = 0; i < (int)rec.size(); i++)
                    {
                        const auto& fr = rec[i];
                        f << "        ["
                          << fr.x << "," << fr.y << "," << fr.z << ","
                          << (fr.w ? 1 : 0) << ","
                          << (fr.a ? 1 : 0) << ","
                          << (fr.s ? 1 : 0) << ","
                          << (fr.d ? 1 : 0) << ","
                          << (fr.space ? 1 : 0) << "]";
                        if (i + 1 < (int)rec.size()) f << ",";
                        f << "\n";
                    }
                    f << "      ]";
                }
                f << "\n    ]";
            }
            f << "\n  }";
        }
        f << "\n}\n";
    }

    void load_recordings()
    {
        std::ifstream f(exe_dir() + "recordings.json");
        if (!f) return;

        std::string s((std::istreambuf_iterator<char>(f)), {});
        g_recordings.clear();
        g_edit_rec  = 0;
        g_active_rec = 0;
        g_active_frame = 0;

        size_t pos = 0;
        const size_t len = s.size();

        auto skip = [&]() {
            while (pos < len && (s[pos]==' ' || s[pos]=='\t' ||
                                 s[pos]=='\n' || s[pos]=='\r')) pos++;
        };
        auto eat = [&](char c) -> bool {
            skip();
            if (pos < len && s[pos] == c) { pos++; return true; }
            return false;
        };
        auto read_str = [&]() -> std::string {
            skip();
            if (pos >= len || s[pos] != '"') return {};
            pos++;
            std::string r;
            while (pos < len && s[pos] != '"') r += s[pos++];
            if (pos < len) pos++;
            return r;
        };
        auto read_flt = [&]() -> float {
            skip();
            char* e = nullptr;
            float v = std::strtof(s.c_str() + pos, &e);
            if (e && e != s.c_str() + pos) pos = size_t(e - s.c_str());
            return v;
        };
        auto skip_comma = [&]() {
            skip();
            if (pos < len && s[pos] == ',') pos++;
        };

        eat('{');
        while (pos < len)
        {
            skip();
            if (pos >= len || s[pos] == '}') break;

            std::string mn = read_str();
            if (mn.empty()) break;
            if (!eat(':') || !eat('{')) break;

            while (pos < len)
            {
                skip();
                if (pos >= len || s[pos] == '}') break;

                std::string ts = read_str();
                if (ts.empty()) break;
                int team = 0;
                try { team = std::stoi(ts); } catch (...) { break; }

                if (!eat(':') || !eat('[')) break;

                while (pos < len)
                {
                    skip();
                    if (pos >= len || s[pos] == ']') break;
                    if (!eat('[')) break;

                    Recording rec;
                    while (pos < len)
                    {
                        skip();
                        if (pos >= len || s[pos] == ']') break;
                        if (!eat('[')) break;

                        RecordedFrame fr{};
                        fr.x = read_flt(); eat(',');
                        fr.y = read_flt(); eat(',');
                        fr.z = read_flt(); eat(',');
                        fr.w = (int)read_flt() != 0; eat(',');
                        fr.a = (int)read_flt() != 0; eat(',');
                        fr.s = (int)read_flt() != 0; eat(',');
                        fr.d = (int)read_flt() != 0; eat(',');
                        fr.space = (int)read_flt() != 0;
                        eat(']');
                        rec.push_back(fr);
                        skip_comma();
                    }
                    eat(']');
                    g_recordings[mn][team].push_back(std::move(rec));
                    skip_comma();
                }
                eat(']');
                skip_comma();
            }
            eat('}');
            skip_comma();
        }
    }
}
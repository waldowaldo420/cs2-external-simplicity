#pragma once
#include "../../game/reader.h"
#include <map>
#include <string>
#include <vector>

namespace botwalker
{
    constexpr uint32_t RECORD_INTERVAL_MS = 15;

    struct RecordedFrame
    {
        float x, y, z;
        bool  w, a, s, d, space;
    };

    using Recording = std::vector<RecordedFrame>;
    using RecordingList = std::vector<Recording>;
    using TeamMap = std::map<int, RecordingList>;
    using MapData = std::map<std::string, TeamMap>;

    extern MapData g_recordings;
    extern bool g_is_recording;
    extern int g_edit_rec;
    extern int g_active_rec;
    extern int g_active_frame;

    void update(const game::GameSnapshot& snap);
    void start_recording(const game::GameSnapshot& snap);
    void stop_recording();
    void delete_recording(const std::string& map, int team, int idx);
    void clear_all(const std::string& map, int team);
    void save_recordings();
    void load_recordings();
    int  recording_count(const std::string& map, int team);
    int  frame_count(const std::string& map, int team, int idx);
}
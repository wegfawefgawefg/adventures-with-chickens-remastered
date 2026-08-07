#pragma once

#include "inputs.hpp"
#include "level.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

constexpr int player_slot_count = 12;

struct State;

struct PlayerSave {
    int active{};
    std::string name;
    int score{};
    int level{};
    int chickens{};

    bool operator==(const PlayerSave&) const = default;
};

struct SaveData {
    std::array<PlayerSave, player_slot_count> players;
    bool use_joystick{};
    bool always_run{};
    int difficulty{2};
    bool sound{true};
    bool music{true};

    std::vector<std::string> original_lines;
    std::string newline{"\r\n"};
    bool final_newline{};
};

std::filesystem::path native_save_path();
std::filesystem::path remaster_settings_path();
std::filesystem::path find_existing_save_path();
bool load_remaster_settings(const std::filesystem::path& path, int& master_volume,
                            int& music_volume, int& effect_volume, float& camera_zoom,
                            int& window_width, int& window_height, bool& fullscreen, bool& vsync,
                            int& presentation_rate, bool& motion_smoothing,
                            ControllerLayout& controller_layout,
                            std::array<int, level_count>& best_level_frames, std::string& error);
bool write_remaster_settings_atomic(const std::filesystem::path& path, int master_volume,
                                    int music_volume, int effect_volume, float camera_zoom,
                                    int window_width, int window_height, bool fullscreen,
                                    bool vsync, int presentation_rate, bool motion_smoothing,
                                    ControllerLayout controller_layout,
                                    const std::array<int, level_count>& best_level_frames,
                                    std::string& error);
bool load_save(const std::filesystem::path& path, SaveData& save, std::string& error);
bool write_save_atomic(const std::filesystem::path& path, const SaveData& save,
                       const std::filesystem::path& backup_source, std::string& error);
bool save_round_trip_smoke(const std::filesystem::path& source, std::string& error);
void save_remaster_settings(const State& state);

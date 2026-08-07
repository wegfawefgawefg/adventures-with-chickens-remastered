#include "save.hpp"

#include "state.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <system_error>

namespace {

bool parse_integer(const std::string& text, int& value) {
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

bool parse_float(const std::string& text, float& value) {
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

std::string decode_player_name(std::string name) {
    for (char& character : name) {
        if (character == '+') {
            character = ' ';
        }
    }
    return name;
}

std::string encode_player_name(std::string name) {
    for (char& character : name) {
        if (character == ' ') {
            character = '+';
        }
    }
    return name;
}

std::vector<std::string> split_commas(const std::string& text) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = text.find(',', start);
        if (comma == std::string::npos) {
            fields.push_back(text.substr(start));
            return fields;
        }
        fields.push_back(text.substr(start, comma - start));
        start = comma + 1;
    }
}

bool parse_player_line(const std::string& line, int& slot, PlayerSave& player) {
    if (!line.starts_with("Player")) {
        return false;
    }

    // parse player slot
    const std::size_t colon = line.find(':', 6);
    if (colon == std::string::npos || !parse_integer(line.substr(6, colon - 6), slot) || slot < 1 ||
        slot > player_slot_count) {
        return false;
    }

    // parse player fields
    std::size_t value_start = colon + 1;
    while (value_start < line.size() && line[value_start] == ' ') {
        ++value_start;
    }
    const std::vector<std::string> fields = split_commas(line.substr(value_start));
    if (fields.size() != 5) {
        return false;
    }

    // store player
    PlayerSave parsed;
    if (!parse_integer(fields[0], parsed.active) || !parse_integer(fields[2], parsed.score) ||
        !parse_integer(fields[3], parsed.level) || !parse_integer(fields[4], parsed.chickens)) {
        return false;
    }
    parsed.name = decode_player_name(fields[1]);
    player = parsed;
    return true;
}

bool parse_yes_no(const std::string& line, const char* prefix, bool& value) {
    if (!line.starts_with(prefix)) {
        return false;
    }
    const std::string text = line.substr(std::char_traits<char>::length(prefix));
    if (text == "yes") {
        value = true;
        return true;
    }
    if (text == "no") {
        value = false;
        return true;
    }
    return false;
}

void split_save_lines(const std::string& text, SaveData& save) {
    // detect newlines
    const std::size_t first_newline = text.find_first_of("\r\n");
    if (first_newline != std::string::npos) {
        if (text[first_newline] == '\r' && first_newline + 1 < text.size() &&
            text[first_newline + 1] == '\n') {
            save.newline = "\r\n";
        } else {
            save.newline.assign(1, text[first_newline]);
        }
    }

    // store source lines
    save.final_newline = !text.empty() && (text.back() == '\r' || text.back() == '\n');
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find_first_of("\r\n", start);
        if (end == std::string::npos) {
            save.original_lines.push_back(text.substr(start));
            return;
        }
        save.original_lines.push_back(text.substr(start, end - start));
        start = end + 1;
        if (text[end] == '\r' && start < text.size() && text[start] == '\n') {
            ++start;
        }
    }
}

std::string player_line(int slot, const PlayerSave& player) {
    return "Player" + std::to_string(slot) + ": " + std::to_string(player.active) + "," +
           encode_player_name(player.name) + "," + std::to_string(player.score) + "," +
           std::to_string(player.level) + "," + std::to_string(player.chickens);
}

std::vector<std::string> updated_lines(const SaveData& save) {
    // update known lines
    std::vector<std::string> lines = save.original_lines;
    std::array<bool, player_slot_count> wrote_player{};
    bool wrote_joystick = false;
    bool wrote_always_run = false;
    bool wrote_difficulty = false;
    bool wrote_sound = false;
    bool wrote_music = false;

    for (std::string& line : lines) {
        int slot = 0;
        PlayerSave ignored;
        if (parse_player_line(line, slot, ignored)) {
            line = player_line(slot, save.players[static_cast<std::size_t>(slot - 1)]);
            wrote_player[static_cast<std::size_t>(slot - 1)] = true;
        } else if (line.starts_with("UseJoystick:")) {
            line = std::string{"UseJoystick: "} + (save.use_joystick ? "yes" : "no");
            wrote_joystick = true;
        } else if (line.starts_with("AlwaysRun:")) {
            line = std::string{"AlwaysRun: "} + (save.always_run ? "yes" : "no");
            wrote_always_run = true;
        } else if (line.starts_with("Difficulty:")) {
            line = "Difficulty: " + std::to_string(save.difficulty);
            wrote_difficulty = true;
        } else if (line.starts_with("Sound:")) {
            line = std::string{"Sound: "} + (save.sound ? "yes" : "no");
            wrote_sound = true;
        } else if (line.starts_with("Music:")) {
            line = std::string{"Music: "} + (save.music ? "yes" : "no");
            wrote_music = true;
        }
    }

    // append missing fields
    for (int slot = 1; slot <= player_slot_count; ++slot) {
        if (!wrote_player[static_cast<std::size_t>(slot - 1)]) {
            lines.push_back(player_line(slot, save.players[static_cast<std::size_t>(slot - 1)]));
        }
    }
    if (!wrote_joystick) {
        lines.push_back(std::string{"UseJoystick: "} + (save.use_joystick ? "yes" : "no"));
    }
    if (!wrote_always_run) {
        lines.push_back(std::string{"AlwaysRun: "} + (save.always_run ? "yes" : "no"));
    }
    if (!wrote_difficulty) {
        lines.push_back("Difficulty: " + std::to_string(save.difficulty));
    }
    if (!wrote_sound) {
        lines.push_back(std::string{"Sound: "} + (save.sound ? "yes" : "no"));
    }
    if (!wrote_music) {
        lines.push_back(std::string{"Music: "} + (save.music ? "yes" : "no"));
    }
    return lines;
}

std::filesystem::path user_data_root() {
    if (const char* xdg_data = std::getenv("XDG_DATA_HOME");
        xdg_data != nullptr && xdg_data[0] != '\0') {
        return xdg_data;
    }
    if (const char* user_home = std::getenv("HOME"); user_home != nullptr && user_home[0] != '\0') {
        return std::filesystem::path{user_home} / ".local/share";
    }
    return ".";
}

} // namespace

std::filesystem::path native_save_path() {
    return user_data_root() / "adventures-with-chickens-remastered/Chickens.ini";
}

std::filesystem::path remaster_settings_path() {
    return user_data_root() / "adventures-with-chickens-remastered/Remaster.ini";
}

std::filesystem::path find_existing_save_path() {
    // check native save
    const std::filesystem::path native = native_save_path();
    if (std::filesystem::exists(native)) {
        return native;
    }

    // check Wine save
    const std::filesystem::path legacy =
        user_data_root() /
        "wineprefixes/adventures-with-chickens/drive_c/Games/Adventures with Chickens/Chickens.ini";
    return std::filesystem::exists(legacy) ? legacy : native;
}

bool load_remaster_settings(const std::filesystem::path& path, int& master_volume,
                            int& music_volume, int& effect_volume, float& camera_zoom,
                            int& window_width, int& window_height, bool& fullscreen, bool& vsync,
                            int& presentation_rate, bool& motion_smoothing,
                            ControllerLayout& controller_layout,
                            std::array<int, level_count>& best_level_frames, std::string& error) {
    std::ifstream file{path};
    if (!file) {
        error = "could not open " + path.string();
        return false;
    }

    // parse settings
    std::string line;
    while (std::getline(file, line)) {
        int parsed = 0;
        if (line.starts_with("MasterVolume: ") && parse_integer(line.substr(14), parsed)) {
            master_volume = std::clamp(parsed, 0, 100);
        } else if (line.starts_with("MusicVolume: ") && parse_integer(line.substr(13), parsed)) {
            music_volume = std::clamp(parsed, 0, 100);
        } else if (line.starts_with("EffectVolume: ") && parse_integer(line.substr(14), parsed)) {
            effect_volume = std::clamp(parsed, 0, 100);
        } else if (line.starts_with("CameraZoom: ")) {
            float parsed_zoom = 1.0F;
            if (parse_float(line.substr(12), parsed_zoom)) {
                camera_zoom = std::clamp(parsed_zoom, 0.5F, 2.0F);
            }
        } else if (line.starts_with("WindowWidth: ") && parse_integer(line.substr(13), parsed)) {
            window_width = std::clamp(parsed, 320, 16384);
        } else if (line.starts_with("WindowHeight: ") && parse_integer(line.substr(14), parsed)) {
            window_height = std::clamp(parsed, 240, 16384);
        } else if (line.starts_with("Fullscreen: ") && parse_integer(line.substr(12), parsed)) {
            fullscreen = parsed != 0;
        } else if (line.starts_with("VSync: ") && parse_integer(line.substr(7), parsed)) {
            vsync = parsed != 0;
        } else if (line.starts_with("PresentationRate: ") &&
                   parse_integer(line.substr(18), parsed)) {
            if (parsed == 60 || parsed == 120 || parsed == 144) {
                presentation_rate = parsed;
            }
        } else if (line.starts_with("MotionSmoothing: ") &&
                   parse_integer(line.substr(17), parsed)) {
            motion_smoothing = parsed != 0;
        } else if (line.starts_with("ControllerLayout: ") &&
                   parse_integer(line.substr(18), parsed)) {
            controller_layout = static_cast<ControllerLayout>(std::clamp(parsed, 0, 3));
        } else if (line.starts_with("BestTime")) {
            const std::size_t colon = line.find(": ", 8);
            int level = 0;
            if (colon != std::string::npos && parse_integer(line.substr(8, colon - 8), level) &&
                parse_integer(line.substr(colon + 2), parsed) && level >= 1 &&
                level <= level_count && parsed > 0) {
                best_level_frames[static_cast<std::size_t>(level - 1)] = parsed;
            }
        }
    }
    if (!file.eof()) {
        error = "could not read " + path.string();
        return false;
    }
    return true;
}

bool write_remaster_settings_atomic(const std::filesystem::path& path, int master_volume,
                                    int music_volume, int effect_volume, float camera_zoom,
                                    int window_width, int window_height, bool fullscreen,
                                    bool vsync, int presentation_rate, bool motion_smoothing,
                                    ControllerLayout controller_layout,
                                    const std::array<int, level_count>& best_level_frames,
                                    std::string& error) {
    // create settings dir
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "could not create settings directory: " + filesystem_error.message();
        return false;
    }

    // write settings temp
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        error = "could not open temporary settings " + temporary.string();
        return false;
    }
    file << "MasterVolume: " << std::clamp(master_volume, 0, 100) << '\n';
    file << "MusicVolume: " << std::clamp(music_volume, 0, 100) << '\n';
    file << "EffectVolume: " << std::clamp(effect_volume, 0, 100) << '\n';
    file << "CameraZoom: " << std::clamp(camera_zoom, 0.5F, 2.0F) << '\n';
    file << "WindowWidth: " << std::clamp(window_width, 320, 16384) << '\n';
    file << "WindowHeight: " << std::clamp(window_height, 240, 16384) << '\n';
    file << "Fullscreen: " << (fullscreen ? 1 : 0) << '\n';
    file << "VSync: " << (vsync ? 1 : 0) << '\n';
    file << "PresentationRate: "
         << (presentation_rate == 60 || presentation_rate == 120 ? presentation_rate : 144) << '\n';
    file << "MotionSmoothing: " << (motion_smoothing ? 1 : 0) << '\n';
    file << "ControllerLayout: " << std::clamp(static_cast<int>(controller_layout), 0, 3) << '\n';
    for (std::size_t index = 0; index < best_level_frames.size(); ++index) {
        if (best_level_frames[index] > 0) {
            file << "BestTime" << index + 1 << ": " << best_level_frames[index] << '\n';
        }
    }
    file.close();
    if (!file) {
        error = "could not write temporary settings " + temporary.string();
        return false;
    }

    // replace settings
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        error = "could not replace settings: " + filesystem_error.message();
        std::filesystem::remove(temporary);
        return false;
    }
    return true;
}

bool load_save(const std::filesystem::path& path, SaveData& save, std::string& error) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        error = "could not open " + path.string();
        return false;
    }

    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    if (!file.good() && !file.eof()) {
        error = "could not read " + path.string();
        return false;
    }

    // parse save
    save = {};
    split_save_lines(text, save);
    for (const std::string& line : save.original_lines) {
        int slot = 0;
        PlayerSave player;
        if (parse_player_line(line, slot, player)) {
            save.players[static_cast<std::size_t>(slot - 1)] = player;
            continue;
        }
        if (parse_yes_no(line, "UseJoystick: ", save.use_joystick) ||
            parse_yes_no(line, "AlwaysRun: ", save.always_run) ||
            parse_yes_no(line, "Sound: ", save.sound) ||
            parse_yes_no(line, "Music: ", save.music)) {
            continue;
        }
        if (line.starts_with("Difficulty: ")) {
            (void)parse_integer(line.substr(12), save.difficulty);
        }
    }
    return true;
}

bool write_save_atomic(const std::filesystem::path& path, const SaveData& save,
                       const std::filesystem::path& backup_source, std::string& error) {
    // create save dir
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "could not create save directory: " + filesystem_error.message();
        return false;
    }

    // back up Wine save
    const std::filesystem::path backup = path.string() + ".original.bak";
    if (!backup_source.empty() && std::filesystem::exists(backup_source) &&
        !std::filesystem::exists(backup)) {
        std::filesystem::copy_file(backup_source, backup, filesystem_error);
        if (filesystem_error) {
            error = "could not back up original save: " + filesystem_error.message();
            return false;
        }
    }

    // write save temp
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream file{temporary, std::ios::binary | std::ios::trunc};
    if (!file) {
        error = "could not open temporary save " + temporary.string();
        return false;
    }

    const std::vector<std::string> lines = updated_lines(save);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        file << lines[index];
        if (index + 1 < lines.size() || save.final_newline) {
            file << save.newline;
        }
    }
    file.close();
    if (!file) {
        error = "could not write temporary save " + temporary.string();
        return false;
    }

    // replace save
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        error = "could not replace save: " + filesystem_error.message();
        std::filesystem::remove(temporary);
        return false;
    }
    return true;
}

bool save_round_trip_smoke(const std::filesystem::path& source, std::string& error) {
    // write smoke save
    SaveData before;
    if (!load_save(source, before, error)) {
        return false;
    }

    const auto unique_value = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path destination =
        std::filesystem::temp_directory_path() /
        ("awc-save-smoke-" + std::to_string(unique_value) + ".ini");
    if (!write_save_atomic(destination, before, source, error)) {
        return false;
    }

    // verify backup
    const std::filesystem::path backup = destination.string() + ".original.bak";
    if (!std::filesystem::exists(backup) ||
        std::filesystem::file_size(backup) != std::filesystem::file_size(source)) {
        error = "original-save backup was not preserved";
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        std::filesystem::remove(backup, ignored);
        return false;
    }

    // clean smoke files
    SaveData after;
    const bool loaded = load_save(destination, after, error);
    std::error_code ignored;
    std::filesystem::remove(destination, ignored);
    std::filesystem::remove(backup, ignored);
    if (!loaded) {
        return false;
    }

    if (before.players != after.players || before.use_joystick != after.use_joystick ||
        before.always_run != after.always_run || before.difficulty != after.difficulty ||
        before.sound != after.sound || before.music != after.music) {
        error = "save values changed during round trip";
        return false;
    }
    return true;
}

void save_remaster_settings(const State& state) {
    std::string error;
    if (!write_remaster_settings_atomic(
            remaster_settings_path(), state.master_volume, state.music_volume, state.effect_volume,
            state.camera.zoom, state.window_width, state.window_height, state.fullscreen,
            state.vsync, state.presentation_rate, state.motion_smoothing, state.controller_layout,
            state.best_level_frames, error)) {
        std::fprintf(stderr, "could not save remaster settings: %s\n", error.c_str());
    }
}

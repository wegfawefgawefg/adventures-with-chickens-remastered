#pragma once

#include "controls.hpp"
#include "inputs.hpp"
#include "level.hpp"
#include "save.hpp"
#include "ui.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

constexpr int original_layout_width = 640;
constexpr int original_layout_height = 480;
constexpr int original_star_count = 480;
constexpr int default_window_width = 1920;
constexpr int default_window_height = 1080;

// timing constants
constexpr float step_rate = 60.0F;
constexpr float step_seconds = 1.0F / step_rate;
constexpr float motion_smoothing_response = 0.15F;
constexpr std::uint64_t original_update_period = 2;
constexpr int boosted_move_period = 2;
constexpr int normal_move_period = 4;
constexpr std::uint64_t capture_step_frames = 3 * original_update_period;
constexpr std::uint64_t capture_lock_frames = 6 * capture_step_frames;
constexpr std::uint64_t capture_animation_frames = 9 * capture_step_frames;
constexpr std::uint64_t death_step_frames = 4 * original_update_period;
constexpr std::uint64_t death_animation_frames = 9 * death_step_frames;
constexpr std::uint64_t exit_step_frames = 7 * original_update_period;
constexpr int result_explosion_frames = 215;
constexpr int result_reveal_interval = 10;
constexpr int result_chicken_frame = result_explosion_frames + result_reveal_interval;
constexpr int result_alien_frame = result_chicken_frame + result_reveal_interval;
constexpr int result_score_frame = result_alien_frame + result_reveal_interval;
constexpr int result_counter_frames = 54;
constexpr int result_rank_frame = result_score_frame + result_counter_frames;
constexpr int result_time_frame = result_rank_frame + result_reveal_interval;
constexpr int result_record_frame = result_time_frame + result_reveal_interval;
constexpr int result_cheer_frame = result_record_frame + result_reveal_interval;
constexpr int result_complete_frame = result_cheer_frame + result_reveal_interval;

constexpr int result_count_tally_frames(int target) {
    return target < 6 ? 18 : (target > 18 ? result_counter_frames : target * 3);
}

constexpr int revealed_result_groups(int animation_frame) {
    if (animation_frame < result_explosion_frames) {
        return 0;
    }
    if (animation_frame < result_chicken_frame) {
        return 1;
    }
    if (animation_frame < result_alien_frame) {
        return 2;
    }
    if (animation_frame < result_score_frame) {
        return 3;
    }
    if (animation_frame < result_rank_frame) {
        return 4;
    }
    if (animation_frame < result_time_frame) {
        return 5;
    }
    if (animation_frame < result_record_frame) {
        return 6;
    }
    if (animation_frame < result_cheer_frame) {
        return 7;
    }
    return 8;
}

constexpr int result_tally_value(int target, int animation_frame, int start_frame,
                                 int duration = result_counter_frames) {
    if (target <= 0 || animation_frame <= start_frame) {
        return 0;
    }
    if (animation_frame >= start_frame + duration) {
        return target;
    }
    const std::int64_t elapsed = animation_frame - start_frame;
    return static_cast<int>((static_cast<std::int64_t>(target) * elapsed) / duration);
}

enum class Mode {
    startup,
    title,
    player_select,
    instructions,
    options,
    order_info,
    credits,
    playing,
    results,
    congratulations,
    exiting,
};

enum class TitleChoice {
    start_game,
    instructions,
    options,
    credits,
    exit_game,
};

enum class StartupMode {
    rocksolid,
    egames,
};

enum class PlayerSelectMode {
    choose_player,
    confirm_delete,
    controller_keyboard,
};

enum class PlayingMode {
    level_start,
    active,
    paused,
    confirm_exit,
    exploding,
    level_complete,
};

enum class InstructionsPage {
    original,
    keyboard,
    controller,
};

enum class PauseMode {
    menu,
    how_to_play,
    controls,
};

enum class PauseChoice {
    resume,
    instructions,
    controls,
    quit_to_main,
};

enum class ResultsMode {
    level_incomplete,
    level_complete,
};

enum class CongratulationsMode {
    waiting,
    active,
};

enum class OptionsChoice {
    master_volume,
    difficulty,
    sound,
    music,
    use_joystick,
    always_run,
};

enum class AlienMode {
    slow_chase,
    fast_retreat,
    winded,
    random_walk,
    attack,
    pattern,
    startled,
    recovery,
    destroyed,
};

struct Camera {
    float x{};
    float y{};
    float zoom{1.0F};
};

struct AlienState {
    int x{};
    int y{};
    int frame{};
    int movement_clock{};
    AlienMode mode{AlienMode::slow_chase};
    int successful_moves{};
    int blocked_moves{};
    int update_delay{};
    int pattern{};
    int pattern_step{};
    int attack_direction{};
    bool active{true};
};

struct ChickenState {
    int x{};
    int y{};
    int frame{};
    int movement_clock{};
    bool active{true};
};

struct StarState {
    IVec2 position;
    int frame{};
};

struct State {
    InputState input;
    ControlState controls;
    UiState ui;
    Mode mode{Mode::startup};
    StartupMode startup_mode{StartupMode::rocksolid};
    TitleChoice title_choice{TitleChoice::start_game};
    PlayerSelectMode player_select_mode{PlayerSelectMode::choose_player};
    PlayingMode playing_mode{PlayingMode::level_start};
    ResultsMode results_mode{ResultsMode::level_incomplete};
    int result_animation_frame{};
    bool result_accelerated{};
    CongratulationsMode congratulations_mode{CongratulationsMode::waiting};
    OptionsChoice options_choice{OptionsChoice::master_volume};
    InstructionsPage instructions_page{InstructionsPage::original};
    PauseMode pause_mode{PauseMode::menu};
    PauseChoice pause_choice{PauseChoice::resume};
    InstructionsPage pause_page{InstructionsPage::original};
    bool confirm_exit_to_title{};
    bool confirm_exit_from_pause{};
    SaveData save;
    std::filesystem::path loaded_save_path;
    bool save_enabled{true};
    std::vector<Level> levels;
    bool levels_loaded{};
    int selected_player{};
    int current_level{1};
    bool player_select_dirty{};
    int controller_keyboard_cursor{};
    bool controller_keyboard_uppercase{true};
    float player_x{};
    float player_y{};
    float previous_player_x{};
    float previous_player_y{};
    float presented_player_x{};
    float presented_player_y{};
    float presented_player_target_x{};
    float presented_player_target_y{};
    int presentation_frames_remaining{};
    IVec2 diagonal_presentation_direction{};
    bool diagonal_presentation_half{};
    IVec2 buffered_move{};
    IVec2 preferred_move{};
    bool diagonal_horizontal_next{};
    int player_direction{};
    int capture_effect_direction{};
    std::uint64_t capture_effect_started_frame{};
    int fuel{300};
    int display_fuel{};
    int score{};
    int level_start_score{};
    int level_elapsed_frames{};
    int completed_level_frames{};
    std::array<int, level_count> best_level_frames{};
    bool new_best_time{};
    int verse_index{};
    int caught_chickens{};
    int total_chickens{};
    bool chicken_hint_active{};
    std::uint64_t chicken_hint_started_frame{};
    int aliens_shorted{};
    int total_aliens{};
    bool exit_open{};
    bool radar_visible{};
    int radar_pulse{};
    std::uint64_t exit_opened_frame{};
    int bomb_seconds{-1};
    int bomb_frame_clock{};
    int block_push_cooldown{};
    int tube_direction{};
    bool tube_crossing{};
    int movement_clock{};
    std::uint64_t rev_last_frame{};
    int rev_count{};
    std::array<std::int8_t, level_cell_count> active_tiles{};
    std::vector<int> warp_frames;
    std::vector<ChickenState> chickens;
    std::vector<AlienState> aliens;
    std::array<StarState, original_star_count> stars;
    IVec2 star_scroll;
    Camera camera{};
    Camera previous_camera{};
    Camera presented_camera{};
    std::uint64_t frame{};
    std::uint64_t mode_frame{};
    int master_volume{100};
    int music_volume{80};
    int effect_volume{100};
    int window_width{default_window_width};
    int window_height{default_window_height};
    bool fullscreen{};
    bool vsync{true};
    int presentation_rate{144};
    bool motion_smoothing{true};
    ControllerLayout controller_layout{ControllerLayout::automatic};
    bool exit_requested{};
};

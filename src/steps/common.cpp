#include "../audio.hpp"
#include "../inputs.hpp"
#include "detail.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace step::detail {

int original_warp_phase(std::size_t index) {
    // calc original warp phase
    std::uint32_t random_state = 1;
    for (std::size_t call = 0; call <= index; ++call) {
        random_state = random_state * UINT32_C(214013) + UINT32_C(2531011);
    }
    return static_cast<int>((random_state >> 16U) & UINT32_C(0x7fff)) % 20;
}

void play_sound(const State& game, Audio* audio, Sound sound) {
    if (audio != nullptr && game.save.sound) {
        audio->play(sound);
    }
}

void change_mode(State& game, Mode mode) {
    game.mode = mode;
    game.mode_frame = 0;
}

void persist_save(State& game, const char* context) {
    if (!game.save_enabled) {
        return;
    }
    std::string error;
    const std::filesystem::path destination = native_save_path();
    if (!write_save_atomic(destination, game.save, game.loaded_save_path, error)) {
        std::fprintf(stderr, "could not save %s: %s\n", context, error.c_str());
        return;
    }
    game.loaded_save_path = destination;
}

void start_level(State& game, Audio* audio) {
    if (!game.levels_loaded || game.current_level < 1 ||
        game.current_level > static_cast<int>(game.levels.size())) {
        return;
    }

    // init terrain and ship
    const Level& level = game.levels[static_cast<std::size_t>(game.current_level - 1)];
    game.active_tiles = level.tiles;
    game.player_x = static_cast<float>(level.player_start.x * 16 + 8);
    game.player_y = static_cast<float>(level.player_start.y * 16 + 8);
    game.previous_player_x = game.player_x;
    game.previous_player_y = game.player_y;
    game.presented_player_x = game.player_x;
    game.presented_player_y = game.player_y;
    game.presented_player_target_x = game.player_x;
    game.presented_player_target_y = game.player_y;
    game.presentation_frames_remaining = 0;
    game.diagonal_presentation_direction = {};
    game.diagonal_presentation_half = false;
    game.buffered_move = {};
    game.preferred_move = {};
    game.diagonal_horizontal_next = false;
    game.player_direction = 0;
    game.capture_effect_direction = 0;
    game.capture_effect_started_frame = 0;
    game.fuel = 300;
    game.display_fuel = 0;

    // reset attempt stats
    const PlayerSave& player = game.save.players[static_cast<std::size_t>(game.selected_player)];
    game.score = player.score;
    game.level_start_score = game.score;
    game.level_elapsed_frames = 0;
    game.completed_level_frames = 0;
    game.new_best_time = false;
    game.caught_chickens = 0;
    game.chicken_hint_active = false;
    game.chicken_hint_started_frame = 0;

    // init actors
    game.chickens.clear();
    game.chickens.reserve(level.chickens.size());
    for (const IVec2& chicken : level.chickens) {
        game.chickens.push_back({.x = chicken.x, .y = chicken.y});
    }
    game.total_chickens = static_cast<int>(game.chickens.size());
    game.aliens.clear();
    game.aliens.reserve(level.aliens.size());
    for (const IVec2& alien : level.aliens) {
        game.aliens.push_back({.x = alien.x, .y = alien.y});
    }
    game.aliens_shorted = 0;
    game.total_aliens = static_cast<int>(game.aliens.size());

    // init level objects
    while (game.warp_frames.size() < level.warps.size()) {
        game.warp_frames.push_back(original_warp_phase(game.warp_frames.size()));
    }
    game.exit_open = false;
    game.exit_opened_frame = 0;
    game.bomb_seconds = -1;
    game.bomb_frame_clock = 0;
    game.block_push_cooldown = 0;
    game.tube_direction = 0;
    game.tube_crossing = false;
    game.movement_clock = 0;

    // start level intro
    game.camera.x = game.player_x;
    game.camera.y = game.player_y;
    game.previous_camera = game.camera;
    game.presented_camera = game.camera;
    game.verse_index = std::rand() % 30;
    game.playing_mode = PlayingMode::level_start;
    play_sound(game, audio, Sound::harp);
    change_mode(game, Mode::playing);
}

bool passable_tile(int tile, int move_x, int move_y) {
    // check arrow direction
    if (tile == 4) {
        return move_y <= 0;
    }
    if (tile == 5) {
        return move_x >= 0;
    }
    if (tile == 6) {
        return move_y >= 0;
    }
    if (tile == 7) {
        return move_x <= 0;
    }

    // allow open cells
    if (tile == 0 || tile == 2 || tile == 8 || tile == 10 || tile == 11 || tile == 12 ||
        (tile >= 14 && tile <= 16)) {
        return true;
    }

    // check tube entrance
    if (tile == 26) {
        return move_y > 0;
    }
    if (tile == 27) {
        return move_x < 0;
    }
    if (tile == 28) {
        return move_y < 0;
    }
    if (tile == 29) {
        return move_x > 0;
    }
    return false;
}

int tile_at(const State& game, int x, int y) {
    if (x < 0 || x >= level_width || y < 0 || y >= level_height) {
        return 1;
    }
    return game.active_tiles[static_cast<std::size_t>(y * level_width + x)];
}

void set_tile(State& game, int x, int y, int tile) {
    if (x < 0 || x >= level_width || y < 0 || y >= level_height) {
        return;
    }
    game.active_tiles[static_cast<std::size_t>(y * level_width + x)] =
        static_cast<std::int8_t>(tile);
}

int difficulty(const State& game) {
    return std::clamp(game.save.difficulty, 0, 4);
}

} // namespace step::detail

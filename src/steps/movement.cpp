#include "../audio.hpp"
#include "../inputs.hpp"
#include "detail.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace step::detail {

constexpr int tile_size = 16;

void consume_movement_fuel(State& game) {
    // charge movement fuel
    ++game.movement_clock;
    if (game.movement_clock < 5) {
        return;
    }
    game.movement_clock = 0;
    game.fuel = std::max(0, game.fuel - (difficulty(game) + 1));
}

void save_completed_level(State& game) {
    // save level progress
    PlayerSave& player = game.save.players[static_cast<std::size_t>(game.selected_player)];
    player.active = 1;
    player.score = game.score;
    player.chickens += game.caught_chickens;
    player.level = game.current_level >= level_count ? 1 : game.current_level + 1;

    persist_save(game, "completed level");
}

void open_exit(State& game, Audio* audio) {
    if (game.exit_open) {
        return;
    }
    if (game.caught_chickens < game.total_chickens) {
        game.chicken_hint_active = true;
        game.chicken_hint_started_frame = game.frame;
        play_sound(game, audio, Sound::error);
        return;
    }

    // open exit
    game.chicken_hint_active = false;
    for (std::int8_t& tile : game.active_tiles) {
        if (tile == 13) {
            tile = 0;
        }
    }
    const Level& level = game.levels[static_cast<std::size_t>(game.current_level - 1)];
    set_tile(game, level.exit.x, level.exit.y, 10);
    game.exit_open = true;
    game.exit_opened_frame = game.frame;
    game.bomb_seconds = 45;
    game.bomb_frame_clock = 0;
    play_sound(game, audio, Sound::open_door);
}

bool try_activate_blocked_tile(State& game, int move_x, int move_y, Audio* audio) {
    // check pressed tile
    const int player_tile_x = static_cast<int>(game.player_x) / tile_size;
    const int player_tile_y = static_cast<int>(game.player_y) / tile_size;
    const int tile = tile_at(game, player_tile_x + move_x, player_tile_y + move_y);
    if (tile != 3 && (tile != 18 || move_y <= 0)) {
        return false;
    }

    // face pressed tile
    if (move_y < 0) {
        game.player_direction = 1;
    } else if (move_x > 0) {
        game.player_direction = 2;
    } else if (move_y > 0) {
        game.player_direction = 3;
    } else if (move_x < 0) {
        game.player_direction = 4;
    }

    // activate switch or TNT
    if (tile == 3) {
        for (std::int8_t& active_tile : game.active_tiles) {
            if (active_tile >= 4 && active_tile <= 7) {
                active_tile = static_cast<std::int8_t>(active_tile == 7 ? 4 : active_tile + 1);
            }
        }
        play_sound(game, audio, std::rand() % 2 == 0 ? Sound::clink : Sound::clunk);
    } else {
        open_exit(game, audio);
    }
    return true;
}

void reach_exit(State& game, Audio* audio) {
    // store run time and local record
    game.completed_level_frames = game.level_elapsed_frames;
    int& best = game.best_level_frames[static_cast<std::size_t>(game.current_level - 1)];
    game.new_best_time = best == 0 || game.completed_level_frames < best;
    if (game.new_best_time) {
        best = game.completed_level_frames;
        if (game.save_enabled) {
            save_remaster_settings(game);
        }
    }

    // save completion
    game.results_mode = ResultsMode::level_complete;
    game.result_animation_frame = 0;
    game.result_accelerated = false;
    game.playing_mode = PlayingMode::level_complete;
    save_completed_level(game);
    play_sound(game, audio, Sound::explode);
    change_mode(game, Mode::results);
}

bool try_move_player(State& game, int move_x, int move_y, Audio* audio) {
    // calc target cell
    const int player_tile_x = static_cast<int>(game.player_x) / tile_size;
    const int player_tile_y = static_cast<int>(game.player_y) / tile_size;
    const int target_x = player_tile_x + move_x;
    const int target_y = player_tile_y + move_y;
    int tile = tile_at(game, target_x, target_y);

    if (move_y < 0) {
        game.player_direction = 1;
    } else if (move_x > 0) {
        game.player_direction = 2;
    } else if (move_y > 0) {
        game.player_direction = 3;
    } else if (move_x < 0) {
        game.player_direction = 4;
    }

    // push asteroid
    if (tile == 17) {
        const int beyond_x = target_x + move_x;
        const int beyond_y = target_y + move_y;
        if (game.block_push_cooldown > 0 || tile_at(game, beyond_x, beyond_y) != 0) {
            return false;
        }
        set_tile(game, target_x, target_y, 0);
        set_tile(game, beyond_x, beyond_y, 17);
        game.block_push_cooldown = normal_move_period - 1;
        play_sound(game, audio, Sound::move);
        tile = 0;
    }

    // handle blocked interaction
    if (!passable_tile(tile, move_x, move_y)) {
        return false;
    }

    // apply movement
    game.player_x = static_cast<float>(target_x * tile_size + tile_size / 2);
    game.player_y = static_cast<float>(target_y * tile_size + tile_size / 2);
    game.camera.x = game.player_x;
    game.camera.y = game.player_y;

    consume_movement_fuel(game);

    // apply tile effect
    if (tile == 2) {
        game.fuel = 0;
        return true;
    }
    if (tile == 8) {
        const Level& level = game.levels[static_cast<std::size_t>(game.current_level - 1)];
        const auto warp =
            std::ranges::find(level.warps, IVec2{target_x, target_y}, &Warp::position);
        if (warp != level.warps.end()) {
            game.player_x = static_cast<float>(warp->destination.x * tile_size + tile_size / 2);
            game.player_y = static_cast<float>(warp->destination.y * tile_size + tile_size / 2);
            game.camera.x = game.player_x;
            game.camera.y = game.player_y;
            play_sound(game, audio, Sound::warp);
        }
    }
    if (tile == 10 && game.exit_open) {
        reach_exit(game, audio);
    } else if (tile == 12) {
        game.fuel = std::min(300, game.fuel + 50);
        set_tile(game, target_x, target_y, 0);
        play_sound(game, audio, Sound::yeah);
    } else if (tile >= 14 && tile <= 16) {
        constexpr std::array<int, 3> score_offsets{1, 8, 18};
        game.score += (difficulty(game) + score_offsets[static_cast<std::size_t>(tile - 14)]) * 50;
        set_tile(game, target_x, target_y, 0);
        play_sound(game, audio, Sound::pickup);
    } else if (tile >= 26 && tile <= 29) {
        game.tube_direction = tile == 26 ? 3 : (tile == 27 ? 4 : (tile == 28 ? 1 : 2));
        game.tube_crossing = false;
    }
    return true;
}

IVec2 tube_delta(int direction) {
    switch (direction) {
    case 1:
        return {0, -1};
    case 2:
        return {1, 0};
    case 3:
        return {0, 1};
    case 4:
        return {-1, 0};
    default:
        return {};
    }
}

void step_tube(State& game, Audio* audio) {
    // calc tube step
    const IVec2 delta = tube_delta(game.tube_direction);
    const IVec2 player{
        static_cast<int>(game.player_x) / tile_size,
        static_cast<int>(game.player_y) / tile_size,
    };
    const IVec2 next = player + delta;
    const int tile = tile_at(game, next.x, next.y);

    // exit tube
    if ((tile < 20 || tile > 34) && !game.tube_crossing) {
        game.tube_direction = 0;
        game.tube_crossing = false;
        play_sound(game, audio, Sound::whistle);
        (void)try_move_player(game, delta.x, delta.y, audio);
        return;
    }

    // handle tube turn
    int next_direction = game.tube_direction;
    if (!game.tube_crossing && tile >= 22 && tile <= 25) {
        const int corner = tile - 22;
        if (game.tube_direction % 4 == corner) {
            next_direction = tile - 21;
        } else if ((game.tube_direction + 1) % 4 == corner) {
            next_direction = (tile - 21) % 4 + 1;
        }
        if (next_direction != game.tube_direction) {
            play_sound(game, audio, Sound::bounce);
        }
    }
    if (tile >= 30 && tile <= 33) {
        game.tube_crossing = !game.tube_crossing;
        if (!game.tube_crossing) {
            next_direction = (tile - 28) % 4 + 1;
        }
    }

    // apply tube step
    game.player_x = static_cast<float>(next.x * tile_size + tile_size / 2);
    game.player_y = static_cast<float>(next.y * tile_size + tile_size / 2);
    game.camera.x = game.player_x;
    game.camera.y = game.player_y;
    game.player_direction = game.tube_direction;
    game.tube_direction = next_direction;
    consume_movement_fuel(game);
}

void step_warps(State& game) {
    // step warp anims
    const Level& level = game.levels[static_cast<std::size_t>(game.current_level - 1)];
    while (game.warp_frames.size() < level.warps.size()) {
        game.warp_frames.push_back(original_warp_phase(game.warp_frames.size()));
    }
    for (std::size_t index = 0; index < level.warps.size(); ++index) {
        game.warp_frames[index] = (game.warp_frames[index] + 1) % 20;
    }
}

} // namespace step::detail

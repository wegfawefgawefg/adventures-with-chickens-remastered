#include "../audio.hpp"
#include "../inputs.hpp"
#include "detail.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace step::detail {

constexpr int tile_size = 16;

void capture_chicken(State& game, Audio* audio) {
    // find adjacent chicken
    const int player_tile_x = static_cast<int>(game.player_x) / tile_size;
    const int player_tile_y = static_cast<int>(game.player_y) / tile_size;
    constexpr std::array<IVec2, 4> directions{{
        {0, -1},
        {1, 0},
        {0, 1},
        {-1, 0},
    }};
    for (std::size_t direction_index = 0; direction_index < directions.size(); ++direction_index) {
        const IVec2 direction = directions[direction_index];
        const IVec2 chicken = IVec2{player_tile_x, player_tile_y} + direction;
        if (tile_at(game, chicken.x, chicken.y) != -2) {
            continue;
        }

        // remove chicken
        set_tile(game, chicken.x, chicken.y, 0);
        const auto state = std::find_if(
            game.chickens.begin(), game.chickens.end(), [chicken](const ChickenState& candidate) {
                return candidate.active && candidate.x == chicken.x && candidate.y == chicken.y;
            });
        if (state != game.chickens.end()) {
            state->active = false;
        }

        // award capture
        ++game.caught_chickens;
        game.score += (difficulty(game) + 5) * 50;
        game.capture_effect_direction = static_cast<int>(direction_index) + 1;
        game.capture_effect_started_frame = game.frame;
        play_sound(game, audio, std::rand() % 2 == 0 ? Sound::chicken : Sound::chicken2);
        return;
    }
}

void step_chickens(State& game) {
    const int player_x = static_cast<int>(game.player_x) / tile_size;
    const int player_y = static_cast<int>(game.player_y) / tile_size;
    constexpr std::array<IVec2, 4> directions{{
        {0, 1},
        {1, 0},
        {0, -1},
        {-1, 0},
    }};

    // step chickens
    for (ChickenState& chicken : game.chickens) {
        if (!chicken.active || ++chicken.movement_clock < 7) {
            continue;
        }
        chicken.movement_clock = 0;
        const int direction = std::rand() % 4;
        const IVec2 delta = directions[static_cast<std::size_t>(direction)];
        const int target_x = chicken.x + delta.x;
        const int target_y = chicken.y + delta.y;
        if (tile_at(game, target_x, target_y) == 0 && target_x != player_x &&
            target_y != player_y) {
            set_tile(game, chicken.x, chicken.y, 0);
            chicken.x = target_x;
            chicken.y = target_y;
            set_tile(game, chicken.x, chicken.y, -2);
        }

        // set chicken anim
        chicken.frame = direction * 2 + 1;
        if (std::rand() % 2 != 0) {
            chicken.frame -= 2;
        }
        chicken.frame = std::clamp(chicken.frame, 0, 7);
    }
}

void short_alien(State& game) {
    // short adjacent alien
    const int player_tile_x = static_cast<int>(game.player_x) / tile_size;
    const int player_tile_y = static_cast<int>(game.player_y) / tile_size;
    constexpr std::array<IVec2, 4> directions{{
        {0, -1},
        {1, 0},
        {0, 1},
        {-1, 0},
    }};
    for (const IVec2& direction : directions) {
        const int alien_x = player_tile_x + direction.x;
        const int alien_y = player_tile_y + direction.y;
        if (tile_at(game, alien_x, alien_y) != -3) {
            continue;
        }
        set_tile(game, alien_x, alien_y, 0);
        return;
    }
}

void alien_hit_player(State& game, int push_x, int push_y, Audio* audio) {
    if (game.playing_mode != PlayingMode::active) {
        return;
    }
    (void)try_move_player(game, push_x, push_y, audio);
    game.fuel = std::max(0, game.fuel - 50);
    if (audio == nullptr || !audio->is_playing(Sound::alien_hit)) {
        play_sound(game, audio, Sound::alien_hit);
    }
}

constexpr std::array<IVec2, 4> alien_directions{{
    {0, -1},
    {1, 0},
    {0, 1},
    {-1, 0},
}};

constexpr std::array<std::array<int, 7>, 4> alien_patterns{{
    {{1, 2, 2, 3, 3, 3, 4}},
    {{2, 3, 3, 4, 4, 4, 1}},
    {{3, 4, 4, 1, 1, 1, 2}},
    {{4, 1, 1, 2, 2, 2, 3}},
}};

bool move_alien(State& game, AlienState& alien, int move_x, int move_y, int player_x, int player_y,
                bool avoid_player_lines = false) {
    const int target_x = alien.x + move_x;
    const int target_y = alien.y + move_y;

    // check wander bounds
    const bool blocked_by_player = avoid_player_lines
                                       ? target_x == player_x || target_y == player_y
                                       : target_x == player_x && target_y == player_y;
    if (blocked_by_player || tile_at(game, target_x, target_y) != 0) {
        return false;
    }

    // move alien marker
    set_tile(game, alien.x, alien.y, 0);
    alien.x = target_x;
    alien.y = target_y;
    set_tile(game, alien.x, alien.y, -3);
    return true;
}

void animate_alien(AlienState& alien) {
    if (alien.frame < 0 || alien.frame > 5) {
        alien.frame = 0;
    } else {
        alien.frame = (alien.frame + 1) % 6;
    }
}

void finish_alien_destruction(State& game, AlienState& alien) {
    set_tile(game, alien.x, alien.y, 0);
    alien.active = false;
    alien.x = -1;
    alien.y = -1;
    game.player_direction = 0;
    ++game.aliens_shorted;
}

void choose_alien_attack(AlienState& alien, int player_x, int player_y) {
    if (alien.mode == AlienMode::fast_retreat || alien.mode == AlienMode::destroyed) {
        return;
    }

    // start alien attack
    const int separation_x = player_x - alien.x;
    const int separation_y = player_y - alien.y;
    if (std::abs(separation_x) + std::abs(separation_y) != 1) {
        return;
    }

    alien.attack_direction =
        separation_y < 0 ? 1 : (separation_x > 0 ? 2 : (separation_y > 0 ? 3 : 4));
    alien.mode = AlienMode::attack;
}

void step_aliens(State& game, Audio* audio, bool horn_held) {
    const int player_x = static_cast<int>(game.player_x) / tile_size;
    const int player_y = static_cast<int>(game.player_y) / tile_size;
    const bool horn_active = horn_held || (audio != nullptr && audio->is_playing(Sound::horn));

    // step aliens
    for (AlienState& alien : game.aliens) {
        if (!alien.active) {
            continue;
        }

        // apply horn and shorting
        if (horn_active) {
            alien.mode = AlienMode::startled;
        }
        if (tile_at(game, alien.x, alien.y) != -3) {
            if (alien.mode == AlienMode::startled) {
                alien.mode = AlienMode::destroyed;
                game.player_direction = 15;
                if (audio == nullptr || !audio->is_playing(Sound::broken)) {
                    play_sound(game, audio, Sound::broken);
                }
            } else if (alien.mode != AlienMode::destroyed) {
                set_tile(game, alien.x, alien.y, -3);
            }
        }

        // apply mode delay
        if (++alien.movement_clock < alien.update_delay) {
            continue;
        }
        alien.movement_clock = 0;

        switch (alien.mode) {
        case AlienMode::slow_chase:
        case AlienMode::fast_retreat: {
            // calc chase move
            animate_alien(alien);
            const bool retreating = alien.mode == AlienMode::fast_retreat;
            int move_x = player_x == alien.x
                             ? 0
                             : (player_x > alien.x ? (retreating ? -1 : 1) : (retreating ? 1 : -1));
            int move_y = player_y == alien.y
                             ? 0
                             : (player_y > alien.y ? (retreating ? -1 : 1) : (retreating ? 1 : -1));
            if (tile_at(game, alien.x + move_x, alien.y) != 0) {
                move_x = 0;
            }
            if (tile_at(game, alien.x, alien.y + move_y) != 0) {
                move_y = 0;
            }
            if (move_alien(game, alien, move_x, move_y, player_x, player_y)) {
                ++alien.successful_moves;
            } else {
                ++alien.blocked_moves;
            }
            alien.update_delay = retreating ? 2 : 5;
            break;
        }
        case AlienMode::winded:
            animate_alien(alien);
            alien.successful_moves = 0;
            ++alien.blocked_moves;
            alien.update_delay = 7;
            break;
        case AlienMode::destroyed: {
            // step shorting anim
            game.player_direction = 15;
            if (alien.frame < 8) {
                alien.frame = 8;
            } else {
                ++alien.frame;
            }
            alien.update_delay = 7;
            if (alien.frame >= 14) {
                finish_alien_destruction(game, alien);
            }
            continue;
        }
        case AlienMode::random_walk: {
            animate_alien(alien);
            const IVec2 direction = alien_directions[static_cast<std::size_t>(std::rand() % 4)];
            (void)move_alien(game, alien, direction.x, direction.y, player_x, player_y, true);
            alien.update_delay = 7;
            if (std::rand() % 7 == 0) {
                alien.mode = AlienMode::slow_chase;
            }
            break;
        }
        case AlienMode::attack: {
            const IVec2 direction =
                alien_directions[static_cast<std::size_t>(alien.attack_direction - 1)];
            alien_hit_player(game, direction.x, direction.y, audio);
            alien.frame = 6;
            alien.attack_direction = 0;
            alien.mode = AlienMode::fast_retreat;
            alien.update_delay = 2;
            break;
        }
        case AlienMode::pattern: {
            // step alien pattern
            if (alien.pattern_step == 0) {
                alien.pattern = std::rand() % 4;
            }
            animate_alien(alien);
            int direction_number = 0;
            if (alien.pattern_step < 7) {
                direction_number = alien_patterns[static_cast<std::size_t>(alien.pattern)]
                                                 [static_cast<std::size_t>(alien.pattern_step)];
            } else if (alien.pattern < 3) {
                direction_number = alien_patterns[static_cast<std::size_t>(alien.pattern + 1)][0];
            }
            if (direction_number != 0) {
                const IVec2 direction =
                    alien_directions[static_cast<std::size_t>(direction_number - 1)];
                (void)move_alien(game, alien, direction.x, direction.y, player_x, player_y, true);
            }
            ++alien.pattern_step;
            alien.update_delay = 7;
            if (alien.pattern_step > 7) {
                alien.pattern_step = 0;
                alien.pattern = 0;
                alien.mode = AlienMode::slow_chase;
            }
            break;
        }
        case AlienMode::startled:
            alien.frame = 7;
            alien.update_delay = 15;
            if (std::rand() % 3 == 0) {
                alien.mode = AlienMode::slow_chase;
            }
            break;
        case AlienMode::recovery:
            // handle blocked alien
            alien.frame = 0;
            alien.successful_moves = 0;
            alien.blocked_moves = 0;
            alien.update_delay = 7;
            switch (std::rand() % 3) {
            case 0:
                alien.mode = AlienMode::random_walk;
                break;
            case 1:
                alien.mode = AlienMode::pattern;
                break;
            default:
                alien.mode = AlienMode::slow_chase;
                break;
            }
            break;
        }

        // update alien mode
        if (alien.successful_moves > 45) {
            alien.mode = AlienMode::winded;
            continue;
        }
        if (alien.blocked_moves > 20) {
            alien.mode = AlienMode::recovery;
            continue;
        }
        choose_alien_attack(alien, player_x, player_y);
    }
}

} // namespace step::detail

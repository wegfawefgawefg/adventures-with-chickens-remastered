#include "../audio.hpp"
#include "../inputs.hpp"
#include "detail.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace step::detail {

constexpr int tile_size = 16;

IVec2 held_horizontal(const InputState& input, IVec2 preferred) {
    if (preferred.x < 0 && input.left_held) {
        return {-1, 0};
    }
    if (preferred.x > 0 && input.right_held) {
        return {1, 0};
    }
    if (input.left_held != input.right_held) {
        return input.left_held ? IVec2{-1, 0} : IVec2{1, 0};
    }
    return {};
}

IVec2 held_vertical(const InputState& input, IVec2 preferred) {
    if (preferred.y < 0 && input.up_held) {
        return {0, -1};
    }
    if (preferred.y > 0 && input.down_held) {
        return {0, 1};
    }
    if (input.up_held != input.down_held) {
        return input.up_held ? IVec2{0, -1} : IVec2{0, 1};
    }
    return {};
}

bool try_compound_move(State& game, IVec2 first, IVec2 second, Audio* audio) {
    // try one cardinal half
    IVec2 moved{};
    if (first != IVec2{} && try_move_player(game, first.x, first.y, audio)) {
        moved = first;
    } else if (second != IVec2{} && try_move_player(game, second.x, second.y, audio)) {
        moved = second;
    }

    // alternate from successful axis
    if (moved != IVec2{}) {
        game.diagonal_horizontal_next = moved.y != 0;
        return true;
    }
    return false;
}

void step_playing(State& game, const InputState& input, Audio* audio) {
    // smooth fuel display
    if (game.fuel == 0) {
        game.display_fuel = 0;
    } else if (game.display_fuel < game.fuel) {
        game.display_fuel = std::min(game.display_fuel + 3, game.fuel);
    } else if (game.display_fuel > game.fuel) {
        game.display_fuel = std::max(game.display_fuel - 3, game.fuel);
    }

    switch (game.playing_mode) {
    case PlayingMode::level_start:
        if (input.confirm_pressed) {
            game.playing_mode = PlayingMode::active;
        } else if (input.back_pressed) {
            game.player_select_mode = PlayerSelectMode::choose_player;
            game.player_select_dirty = false;
            change_mode(game, Mode::player_select);
        }
        break;
    case PlayingMode::active: {
        // count active play time
        ++game.level_elapsed_frames;

        // calc world tick
        const bool world_tick = game.mode_frame % original_update_period == 0U;
        if (world_tick && input.diagnostic_low_fuel_held) {
            game.fuel = 5;
        }
        if (world_tick && input.diagnostic_full_fuel_held) {
            game.fuel = 300;
        }
        if (world_tick && input.diagnostic_score_held) {
            game.score += 3000;
        }
        if (world_tick && input.diagnostic_skip_held) {
            reach_exit(game, audio);
            break;
        }

        // step radar
        if (input.radar_pressed) {
            game.radar_visible = !game.radar_visible;
            play_sound(game, audio, Sound::clink);
        }
        if (game.radar_visible) {
            game.radar_pulse = (game.radar_pulse + 1) % 9;
        }

        // step capture anim
        game.block_push_cooldown = std::max(0, game.block_push_cooldown - 1);
        const std::uint64_t capture_elapsed = game.frame - game.capture_effect_started_frame;
        if (game.capture_effect_direction != 0 && capture_elapsed >= capture_animation_frames) {
            game.capture_effect_direction = 0;
        }
        const bool capture_locked =
            game.capture_effect_direction != 0 && capture_elapsed < capture_lock_frames;
        const int player_tile_x = static_cast<int>(game.player_x) / tile_size;
        const int player_tile_y = static_cast<int>(game.player_y) / tile_size;

        // latch interaction or buffer turn
        IVec2 pressed_move{};
        if (input.left_pressed) {
            pressed_move = {-1, 0};
        } else if (input.right_pressed) {
            pressed_move = {1, 0};
        } else if (input.up_pressed) {
            pressed_move = {0, -1};
        } else if (input.down_pressed) {
            pressed_move = {0, 1};
        }
        if (pressed_move != IVec2{}) {
            game.preferred_move = pressed_move;
            game.diagonal_horizontal_next = pressed_move.x != 0;
            if (try_activate_blocked_tile(game, pressed_move.x, pressed_move.y, audio)) {
                game.buffered_move = {};
            } else {
                game.buffered_move = pressed_move;
            }
        }

        // apply held actions
        if (!capture_locked && world_tick && tile_at(game, player_tile_x, player_tile_y) == 11 &&
            game.fuel < 300) {
            ++game.fuel;
            if (audio == nullptr || !audio->is_playing(Sound::drink)) {
                play_sound(game, audio, Sound::drink);
            }
        }
        if (!capture_locked && (input.confirm_pressed || (world_tick && input.action_held))) {
            capture_chicken(game, audio);
        }
        if (!capture_locked && (input.attack_pressed || (world_tick && input.attack_held))) {
            short_alien(game);
        }

        // loop held horn
        const bool restart_held_horn =
            world_tick && input.horn_held && (audio == nullptr || !audio->is_playing(Sound::horn));
        if (!capture_locked && (input.horn_pressed || restart_held_horn)) {
            play_sound(game, audio, Sound::horn);
        }

        // step world objects
        if (world_tick) {
            step_warps(game);
            step_chickens(game);
            step_aliens(game, audio, input.horn_pressed || input.horn_held);
            if (!capture_locked && input.run_held) {
                if (game.frame - game.rev_last_frame > 15U && game.rev_count < 2) {
                    play_sound(game, audio, Sound::rev);
                    ++game.rev_count;
                    game.rev_last_frame = game.frame;
                }
                if (game.frame - game.rev_last_frame > 150U) {
                    game.rev_count = 0;
                }
            }
        }

        // step bomb timer
        if (game.bomb_seconds >= 0 && ++game.bomb_frame_clock >= static_cast<int>(step_rate)) {
            game.bomb_frame_clock = 0;
            if (game.bomb_seconds < 44) {
                play_sound(game, audio, Sound::clunk);
            }
            --game.bomb_seconds;
            if (game.bomb_seconds <= 0) {
                game.bomb_seconds = -1;
                game.fuel = 0;
            }
        }

        // step ship movement
        const int standing_tile = tile_at(game, static_cast<int>(game.player_x) / tile_size,
                                          static_cast<int>(game.player_y) / tile_size);
        const int movement_period =
            game.tube_direction != 0 || game.save.always_run || input.run_held ? boosted_move_period
                                                                               : normal_move_period;
        if (!capture_locked && game.player_direction < 5 &&
            game.mode_frame % static_cast<std::uint64_t>(movement_period) == 0U) {
            bool moved_on_arrow = false;
            if (game.tube_direction != 0) {
                step_tube(game, audio);
                game.buffered_move = {};
            } else {
                // resolve held axes
                const IVec2 horizontal = held_horizontal(input, game.preferred_move);
                const IVec2 vertical = held_vertical(input, game.preferred_move);
                const bool diagonal = horizontal != IVec2{} && vertical != IVec2{};

                // spend one cardinal move tick
                if (game.buffered_move != IVec2{}) {
                    const IVec2 buffered = game.buffered_move;
                    game.buffered_move = {};
                    const IVec2 other =
                        !diagonal ? IVec2{} : (buffered.x != 0 ? vertical : horizontal);
                    moved_on_arrow = try_compound_move(game, buffered, other, audio);
                } else if (diagonal) {
                    const IVec2 first = game.diagonal_horizontal_next ? horizontal : vertical;
                    const IVec2 second = game.diagonal_horizontal_next ? vertical : horizontal;
                    moved_on_arrow = try_compound_move(game, first, second, audio);
                } else if (horizontal != IVec2{} || vertical != IVec2{}) {
                    const IVec2 held_move = horizontal != IVec2{} ? horizontal : vertical;
                    moved_on_arrow = try_move_player(game, held_move.x, held_move.y, audio);
                } else if (standing_tile >= 4 && standing_tile <= 7) {
                    constexpr std::array<IVec2, 4> arrow_directions{{
                        {0, -1},
                        {1, 0},
                        {0, 1},
                        {-1, 0},
                    }};
                    const IVec2 direction =
                        arrow_directions[static_cast<std::size_t>(standing_tile - 4)];
                    moved_on_arrow = try_move_player(game, direction.x, direction.y, audio);
                } else {
                    game.player_direction = 0;
                }
            }
            if (moved_on_arrow && standing_tile >= 4 && standing_tile <= 7 &&
                (audio == nullptr || !audio->is_playing(Sound::dribble))) {
                play_sound(game, audio, Sound::dribble);
            }
        }

        // check level completion
        if (game.mode != Mode::playing || game.playing_mode != PlayingMode::active) {
            break;
        }
        if (game.fuel == 0) {
            game.playing_mode = PlayingMode::exploding;
            game.mode_frame = 0;
            play_sound(game, audio, Sound::explode);
            break;
        }
        if (world_tick && game.fuel < 50 && (game.frame / original_update_period) % 64U == 0U) {
            play_sound(game, audio, Sound::error);
        }
        if (input.pause_pressed) {
            game.pause_mode = PauseMode::menu;
            game.pause_choice = PauseChoice::resume;
            game.playing_mode = PlayingMode::paused;
        } else if (input.back_pressed && input.last_device == InputDevice::keyboard) {
            game.confirm_exit_to_title = false;
            game.confirm_exit_from_pause = false;
            game.playing_mode = PlayingMode::confirm_exit;
        }
        break;
    }
    case PlayingMode::paused: {
        // close pause
        if (input.pause_pressed) {
            game.playing_mode = PlayingMode::active;
            break;
        }

        // close how-to page
        if (game.pause_mode == PauseMode::how_to_play) {
            if (input.back_pressed) {
                game.pause_mode = PauseMode::menu;
                play_sound(game, audio, Sound::pickup);
            }
            break;
        }

        // step control pages
        if (game.pause_mode == PauseMode::controls) {
            int page = static_cast<int>(game.pause_page);
            const int old_page = page;
            if (input.left_pressed && page > static_cast<int>(InstructionsPage::keyboard)) {
                --page;
            }
            if (input.right_pressed && page < static_cast<int>(InstructionsPage::controller)) {
                ++page;
            }
            if (page != old_page) {
                game.pause_page = static_cast<InstructionsPage>(page);
                play_sound(game, audio, Sound::clink);
            }
            if (input.back_pressed) {
                game.pause_mode = PauseMode::menu;
                play_sound(game, audio, Sound::pickup);
            }
            break;
        }

        // move pause cursor
        constexpr int pause_choice_count = 4;
        int choice = static_cast<int>(game.pause_choice);
        if (input.up_pressed && choice > 0) {
            --choice;
            play_sound(game, audio, Sound::clink);
        }
        if (input.down_pressed && choice + 1 < pause_choice_count) {
            ++choice;
            play_sound(game, audio, Sound::clink);
        }
        game.pause_choice = static_cast<PauseChoice>(choice);

        // use pause item
        if (input.back_pressed) {
            game.playing_mode = PlayingMode::active;
            play_sound(game, audio, Sound::yeah);
        } else if (input.confirm_pressed) {
            switch (game.pause_choice) {
            case PauseChoice::resume:
                game.playing_mode = PlayingMode::active;
                play_sound(game, audio, Sound::yeah);
                break;
            case PauseChoice::instructions:
                game.pause_page = InstructionsPage::original;
                game.pause_mode = PauseMode::how_to_play;
                play_sound(game, audio, Sound::warp);
                break;
            case PauseChoice::controls:
                game.pause_page = game.input.last_device == InputDevice::controller
                                      ? InstructionsPage::controller
                                      : InstructionsPage::keyboard;
                game.pause_mode = PauseMode::controls;
                play_sound(game, audio, Sound::warp);
                break;
            case PauseChoice::quit_to_main:
                game.confirm_exit_to_title = true;
                game.confirm_exit_from_pause = true;
                game.playing_mode = PlayingMode::confirm_exit;
                play_sound(game, audio, Sound::clink);
                break;
            }
        }
        break;
    }
    case PlayingMode::confirm_exit:
        if (input.confirm_pressed) {
            play_sound(game, audio, Sound::pickup);
            if (game.confirm_exit_to_title) {
                change_mode(game, Mode::title);
            } else {
                game.player_select_mode = PlayerSelectMode::choose_player;
                game.player_select_dirty = false;
                change_mode(game, Mode::player_select);
            }
        } else if (input.back_pressed) {
            game.playing_mode =
                game.confirm_exit_from_pause ? PlayingMode::paused : PlayingMode::active;
            play_sound(game, audio, Sound::yeah);
        }
        break;
    case PlayingMode::exploding:
        // step death anim
        if (game.mode_frame % original_update_period == 0U) {
            step_warps(game);
            step_chickens(game);
            step_aliens(game, audio, input.horn_pressed || input.horn_held);
        }
        if (game.mode_frame >= death_animation_frames) {
            game.results_mode = ResultsMode::level_incomplete;
            game.result_animation_frame = 0;
            game.result_accelerated = false;
            play_sound(game, audio, Sound::explode);
            change_mode(game, Mode::results);
        }
        break;
    case PlayingMode::level_complete:
        break;
    }
}

void step_results(State& game, const InputState& input, Audio* audio) {
    // retry incomplete level
    if (game.results_mode == ResultsMode::level_incomplete) {
        if (input.confirm_pressed || input.back_pressed) {
            game.score = game.level_start_score;
            play_sound(game, audio, Sound::pickup);
            start_level(game, audio);
        }
        return;
    }

    // continue completed level
    if (game.result_animation_frame >= result_complete_frame) {
        if (!input.confirm_pressed) {
            return;
        }
        play_sound(game, audio, Sound::pickup);
        if (game.current_level >= level_count) {
            game.congratulations_mode = CongratulationsMode::waiting;
            change_mode(game, Mode::congratulations);
            return;
        }
        ++game.current_level;
        start_level(game, audio);
        return;
    }

    // latch result speedup
    if (input.confirm_pressed) {
        game.result_accelerated = true;
    }

    // advance result timeline
    const int old_frame = game.result_animation_frame;
    const int rate = game.result_accelerated ? 6 : 1;
    game.result_animation_frame = std::min(result_complete_frame, old_frame + rate);

    // calc changed counters
    const int chicken_duration = result_count_tally_frames(game.caught_chickens);
    const int alien_duration = result_count_tally_frames(game.aliens_shorted);
    const bool tally_changed =
        result_tally_value(game.caught_chickens, old_frame, result_chicken_frame,
                           chicken_duration) !=
            result_tally_value(game.caught_chickens, game.result_animation_frame,
                               result_chicken_frame, chicken_duration) ||
        result_tally_value(game.aliens_shorted, old_frame, result_alien_frame, alien_duration) !=
            result_tally_value(game.aliens_shorted, game.result_animation_frame, result_alien_frame,
                               alien_duration) ||
        result_tally_value(game.score, old_frame, result_score_frame) !=
            result_tally_value(game.score, game.result_animation_frame, result_score_frame);
    const bool record_revealed = old_frame < result_record_frame &&
                                 game.result_animation_frame >= result_record_frame &&
                                 game.new_best_time;
    const bool group_revealed =
        revealed_result_groups(old_frame) != revealed_result_groups(game.result_animation_frame);
    const std::uint64_t clink_period = game.result_accelerated ? 2U : 4U;
    if (record_revealed) {
        play_sound(game, audio, Sound::yeah);
    } else if (group_revealed || (tally_changed && game.mode_frame % clink_period == 0U)) {
        play_sound(game, audio, Sound::clink);
    }
}

} // namespace step::detail

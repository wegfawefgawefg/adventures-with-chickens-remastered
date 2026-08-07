#include "step.hpp"

#include "audio.hpp"
#include "inputs.hpp"
#include "steps/detail.hpp"

#include <algorithm>
#include <cmath>

namespace {

IVec2 diagonal_direction(const InputState& input) {
    if (input.left_held == input.right_held || input.up_held == input.down_held) {
        return {};
    }
    return {
        input.right_held ? 1 : -1,
        input.down_held ? 1 : -1,
    };
}

int transition_frames(float response, int movement_period) {
    const float smoothing_amount = (1.0F - response) / 0.85F;
    return std::clamp(static_cast<int>(std::lround(
                          std::lerp(1.0F, static_cast<float>(movement_period), smoothing_amount))),
                      1, movement_period);
}

bool next_diagonal_step_clear(const State& game, IVec2 diagonal) {
    const IVec2 next = game.diagonal_horizontal_next ? IVec2{diagonal.x, 0} : IVec2{0, diagonal.y};
    const int player_tile_x = static_cast<int>(game.player_x) / 16;
    const int player_tile_y = static_cast<int>(game.player_y) / 16;
    const int tile = step::detail::tile_at(game, player_tile_x + next.x, player_tile_y + next.y);
    return step::detail::passable_tile(tile, next.x, next.y);
}

void step_presentation(State& game, float old_player_x, float old_player_y,
                       const Camera& old_camera, bool was_in_tube, const InputState& input) {
    // set presentation mode
    constexpr float teleport_distance = 16.0F;
    const bool smoothing = game.mode == Mode::playing && game.motion_smoothing;
    const bool moved = game.player_x != old_player_x || game.player_y != old_player_y;
    const bool teleported = std::abs(game.player_x - old_player_x) > teleport_distance ||
                            std::abs(game.player_y - old_player_y) > teleport_distance ||
                            std::abs(game.camera.x - old_camera.x) > teleport_distance ||
                            std::abs(game.camera.y - old_camera.y) > teleport_distance;
    if (!smoothing || teleported) {
        game.presentation_frames_remaining = 0;
        game.presented_player_x = game.player_x;
        game.presented_player_y = game.player_y;
        game.presented_player_target_x = game.player_x;
        game.presented_player_target_y = game.player_y;
        game.diagonal_presentation_direction = {};
        game.diagonal_presentation_half = false;
        game.presented_camera = game.camera;
        return;
    }

    // reset changed diagonal
    const int movement_period =
        was_in_tube || game.tube_direction != 0 || game.save.always_run || input.run_held ? 2 : 4;
    const IVec2 diagonal =
        !was_in_tube && game.tube_direction == 0 ? diagonal_direction(input) : IVec2{};
    const float old_target_x = game.presented_player_target_x;
    const float old_target_y = game.presented_player_target_y;
    if (diagonal != game.diagonal_presentation_direction) {
        game.presented_player_target_x = game.player_x;
        game.presented_player_target_y = game.player_y;
        game.diagonal_presentation_direction = diagonal;
        game.diagonal_presentation_half = false;
    }

    // set shared ship and camera target
    if (moved) {
        if (diagonal != IVec2{} && next_diagonal_step_clear(game, diagonal)) {
            if (game.diagonal_presentation_half) {
                game.presented_player_target_x = game.player_x;
                game.presented_player_target_y = game.player_y;
            } else {
                constexpr float diagonal_half_step = 8.0F;
                game.presented_player_target_x =
                    old_player_x + static_cast<float>(diagonal.x) * diagonal_half_step;
                game.presented_player_target_y =
                    old_player_y + static_cast<float>(diagonal.y) * diagonal_half_step;
            }
            game.diagonal_presentation_half = !game.diagonal_presentation_half;
        } else {
            game.presented_player_target_x = game.player_x;
            game.presented_player_target_y = game.player_y;
            game.diagonal_presentation_half = false;
        }
    } else if (diagonal == IVec2{}) {
        game.presented_player_target_x = game.player_x;
        game.presented_player_target_y = game.player_y;
        game.diagonal_presentation_half = false;
    }

    // set finite presentation transition
    if (game.presented_player_target_x != old_target_x ||
        game.presented_player_target_y != old_target_y) {
        game.presentation_frames_remaining =
            transition_frames(motion_smoothing_response, movement_period);
    }

    // finish ship at next guide point
    if (game.presentation_frames_remaining > 0) {
        const float remaining = static_cast<float>(game.presentation_frames_remaining);
        game.presented_player_x +=
            (game.presented_player_target_x - game.presented_player_x) / remaining;
        game.presented_player_y +=
            (game.presented_player_target_y - game.presented_player_y) / remaining;
        --game.presentation_frames_remaining;
    } else {
        game.presented_player_x = game.presented_player_target_x;
        game.presented_player_y = game.presented_player_target_y;
    }

    // keep camera centered on ship
    game.presented_camera.x = game.presented_player_x;
    game.presented_camera.y = game.presented_player_y;
    game.presented_camera.zoom = game.camera.zoom;
}

} // namespace

namespace step {

void step(State& game, const InputState& input, Audio* audio) {
    // keep last presentation positions
    game.previous_player_x = game.presented_player_x;
    game.previous_player_y = game.presented_player_y;
    game.previous_camera = game.presented_camera;
    const float old_player_x = game.player_x;
    const float old_player_y = game.player_y;
    const Camera old_camera = game.camera;
    const bool was_in_tube = game.tube_direction != 0;

    // scroll frontend stars
    if (game.frame % original_update_period == 0U &&
        (game.mode == Mode::title || game.mode == Mode::player_select ||
         game.mode == Mode::instructions || game.mode == Mode::options ||
         game.mode == Mode::order_info || game.mode == Mode::credits ||
         game.mode == Mode::exiting)) {
        game.star_scroll.x = (game.star_scroll.x + 1) % 638;
        game.star_scroll.y = (game.star_scroll.y + 1) % 478;
    }

    // step active mode
    ++game.frame;
    ++game.mode_frame;
    const Mode previous_mode = game.mode;
    const StartupMode previous_startup_mode = game.startup_mode;

    switch (game.mode) {
    case Mode::startup:
        detail::step_startup(game, input);
        break;
    case Mode::title:
        detail::step_title(game, input, audio);
        break;
    case Mode::player_select:
        detail::step_player_select(game, input, audio);
        break;
    case Mode::instructions:
    case Mode::order_info:
    case Mode::credits:
        detail::step_frontend_page(game, input, audio);
        break;
    case Mode::options:
        detail::step_options(game, input, audio);
        break;
    case Mode::playing:
        detail::step_playing(game, input, audio);
        break;
    case Mode::results:
        detail::step_results(game, input, audio);
        break;
    case Mode::congratulations:
        detail::step_congratulations(game, input, audio);
        break;
    case Mode::exiting:
        detail::step_exiting(game);
        break;
    }
    step_presentation(game, old_player_x, old_player_y, old_camera, was_in_tube, input);

    // play eGames sting
    if (previous_mode == Mode::startup && game.mode == Mode::startup &&
        previous_startup_mode == StartupMode::rocksolid &&
        game.startup_mode == StartupMode::egames) {
        detail::play_sound(game, audio, Sound::xgames);
    }
}

} // namespace step

#include "cli.hpp"

#include "inputs.hpp"
#include "save.hpp"
#include "state.hpp"
#include "step.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <ranges>
#include <string>

namespace {

bool has_flag(int argc, char** argv, const char* flag) {
    for (int index = 1; index < argc; ++index) {
        if (argv[index] != nullptr && std::strcmp(argv[index], flag) == 0) {
            return true;
        }
    }
    return false;
}

bool has_argument(int argc, char** argv, const char* name) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] != nullptr && std::strcmp(argv[index], name) == 0) {
            return true;
        }
    }
    return false;
}

int integer_argument(int argc, char** argv, const char* name, int fallback) {
    int result = fallback;
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] != nullptr && argv[index + 1] != nullptr &&
            std::strcmp(argv[index], name) == 0) {
            const int value = std::atoi(argv[index + 1]);
            if (value > 0) {
                result = value;
            }
        }
    }
    return result;
}

std::string string_argument(int argc, char** argv, const char* name) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] != nullptr && argv[index + 1] != nullptr &&
            std::strcmp(argv[index], name) == 0) {
            return argv[index + 1];
        }
    }
    return {};
}

void begin_level_for_cli(State& game, int level) {
    // init CLI level
    game.save_enabled = false;
    game.selected_player = player_slot_count - 1;
    game.save.players.back() = {
        .active = 1,
        .name = "CLI",
        .score = 0,
        .level = std::clamp(level, 1, level_count),
        .chickens = 0,
    };
    game.mode = Mode::player_select;
    game.player_select_mode = PlayerSelectMode::choose_player;
    game.player_select_dirty = false;
    step::step(game, {.confirm_pressed = true});
}

} // namespace

std::optional<int> dispatch_cli(State& game, int argc, char** argv, CliOptions& options) {
    // run save smoke
    if (has_flag(argc, argv, "--save-smoke")) {
        std::string error;
        if (!save_round_trip_smoke(game.loaded_save_path, error)) {
            std::fprintf(stderr, "save smoke failed: %s\n", error.c_str());
            return 1;
        }
        const PlayerSave& player = game.save.players[0];
        std::printf("save: %s score=%d level=%d chickens=%d\n", player.name.c_str(), player.score,
                    player.level, player.chickens);
        return 0;
    }

    // run mode smoke
    if (has_flag(argc, argv, "--smoke")) {
        game.input.confirm_pressed = true;
        step::step(game, game.input);
        consume_game_input(game.input);
        game.input.confirm_pressed = true;
        step::step(game, game.input);
        consume_game_input(game.input);
        game.input.down_pressed = true;
        step::step(game, game.input);
        consume_game_input(game.input);
        game.input.down_pressed = true;
        game.input.confirm_pressed = true;
        step::step(game, game.input);
        return game.mode == Mode::options && game.levels_loaded &&
                       game.levels.size() == static_cast<std::size_t>(level_count)
                   ? 0
                   : 1;
    }

    // check player select controls
    if (has_flag(argc, argv, "--player-select-smoke")) {
        game.selected_player = player_slot_count - 1;
        game.save.players.back() = {
            .active = 1,
            .name = "SELECT",
            .score = 0,
            .level = 1,
            .chickens = 0,
        };
        game.mode = Mode::player_select;
        game.player_select_mode = PlayerSelectMode::choose_player;
        step::step(game, {.last_device = InputDevice::controller,
                          .confirm_pressed = true,
                          .attack_pressed = true});
        const bool south_chose_player = game.mode == Mode::playing &&
                                        game.player_select_mode == PlayerSelectMode::choose_player;

        game.mode = Mode::player_select;
        game.player_select_mode = PlayerSelectMode::choose_player;
        step::step(game, {.last_device = InputDevice::controller, .remove_pressed = true});
        const bool select_opened_delete =
            game.player_select_mode == PlayerSelectMode::confirm_delete;
        step::step(game, {.last_device = InputDevice::controller, .back_pressed = true});
        const bool east_cancelled_delete =
            game.player_select_mode == PlayerSelectMode::choose_player &&
            game.save.players.back().active != 0;

        game.player_select_mode = PlayerSelectMode::controller_keyboard;
        game.controller_keyboard_cursor = 0;
        step::step(game, {.last_device = InputDevice::controller, .pause_pressed = true});
        const bool start_selected_ok = game.controller_keyboard_cursor == 29;

        std::printf("player select: choose=%s delete=%s cancel=%s start-ok=%s\n",
                    south_chose_player ? "pass" : "fail", select_opened_delete ? "pass" : "fail",
                    east_cancelled_delete ? "pass" : "fail", start_selected_ok ? "pass" : "fail");
        return south_chose_player && select_opened_delete && east_cancelled_delete &&
                       start_selected_ok
                   ? 0
                   : 1;
    }

    // run level smoke
    if (has_flag(argc, argv, "--level-smoke")) {
        if (!game.levels_loaded || game.levels.size() != static_cast<std::size_t>(level_count)) {
            return 1;
        }
        std::size_t chickens = 0;
        std::size_t aliens = 0;
        std::size_t warps = 0;
        std::size_t tube_crossings = 0;
        std::size_t tube_underlay_cells = 0;
        for (const Level& level : game.levels) {
            chickens += level.chickens.size();
            aliens += level.aliens.size();
            warps += level.warps.size();
            tube_crossings +=
                static_cast<std::size_t>(std::ranges::count(level.tiles, std::int8_t{34}));
            tube_underlay_cells += static_cast<std::size_t>(std::ranges::count_if(
                level.tube_underlay, [](std::int8_t tile) { return tile != 0; }));
        }
        const Level& level_15 = game.levels[14];
        const auto exit_return = std::ranges::find(level_15.warps, IVec2{79, 51}, &Warp::position);
        const auto maze_link = std::ranges::find(level_15.warps, IVec2{146, 96}, &Warp::position);
        const bool level_15_route_repaired =
            exit_return != level_15.warps.end() && maze_link != level_15.warps.end() &&
            exit_return->destination == IVec2{150, 25} && maze_link->destination == IVec2{9, 96};
        std::printf("levels: %zu cells=%d chickens=%zu aliens=%zu warps=%zu crossings=%zu "
                    "underlay=%zu level15-route=%s\n",
                    game.levels.size(), level_cell_count * level_count, chickens, aliens, warps,
                    tube_crossings, tube_underlay_cells,
                    level_15_route_repaired ? "repaired" : "invalid");
        return tube_crossings == 81 && tube_underlay_cells == 3886 && level_15_route_repaired ? 0
                                                                                              : 1;
    }

    // check movement
    if (has_flag(argc, argv, "--movement-smoke")) {
        begin_level_for_cli(game, 1);
        step::step(game, {.confirm_pressed = true});
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] = 0;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 0;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 3)] = 1;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        const bool moved_through_open_tile = game.player_x == 40.0F;
        game.mode_frame = 7;
        step::step(game, {.right_held = true});
        const bool stopped_at_solid_tile = game.player_x == 40.0F;
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 7;
        game.mode_frame = 11;
        step::step(game, {.right_held = true});
        const bool arrow_blocked_reverse = game.player_x == 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 5;
        game.mode_frame = 15;
        step::step(game, {.right_held = true});
        const bool arrow_allowed_forward = game.player_x == 40.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 3)] = 0;
        game.mode_frame = 19;
        step::step(game, {});
        const bool arrow_forced_forward = game.player_x == 56.0F;
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] = 6;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 0;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 1)] = 0;
        game.mode_frame = 23;
        step::step(game, {.right_held = true});
        const bool arrow_input_overrode_force = game.player_x == 40.0F && game.player_y == 24.0F;

        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.frame = 40;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 3;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 1)] = 4;
        game.mode_frame = 22;
        step::step(game, {.right_pressed = true, .right_held = true});
        const bool arrow_switch_pressed =
            game.player_x == 24.0F &&
            game.active_tiles[static_cast<std::size_t>(2 * level_width + 1)] == 5;
        game.mode_frame = 23;
        step::step(game, {.right_held = true});
        const bool arrow_switch_held =
            game.active_tiles[static_cast<std::size_t>(2 * level_width + 1)] == 5;
        game.mode_frame = 24;
        step::step(game, {.right_pressed = true, .right_held = true});
        const bool arrow_switch_repressed =
            game.active_tiles[static_cast<std::size_t>(2 * level_width + 1)] == 6;

        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.aliens.clear();
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 29;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 3)] = 20;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 4)] = 24;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 4)] = 20;
        game.active_tiles[static_cast<std::size_t>(3 * level_width + 4)] = 0;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        for (int mode_frame : {5, 7, 9, 11}) {
            game.mode_frame = static_cast<std::uint64_t>(mode_frame);
            step::step(game, {});
        }
        const bool followed_tube =
            game.player_x == 72.0F && game.player_y == 56.0F && game.tube_direction == 0;

        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.tube_direction = 0;
        game.tube_crossing = false;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 29;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 3)] = 31;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 4)] = 1;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 5)] = 33;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 6)] = 0;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        for (int mode_frame : {5, 7, 9, 11}) {
            game.mode_frame = static_cast<std::uint64_t>(mode_frame);
            step::step(game, {});
        }
        const bool crossed_tube_underpass = game.player_x == 104.0F && game.player_y == 24.0F &&
                                            game.tube_direction == 0 && !game.tube_crossing;

        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 12;
        game.fuel = 1;
        game.movement_clock = 4;
        game.save.difficulty = 0;
        game.mode_frame = 15;
        step::step(game, {.right_held = true});
        const bool final_fuel_pickup_saved =
            game.fuel == 50 && game.playing_mode == PlayingMode::active;

        // check finite smoothing and buffered turn
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.camera = {.x = 24.0F, .y = 24.0F, .zoom = 1.0F};
        game.presented_player_x = 24.0F;
        game.presented_player_y = 24.0F;
        game.presented_camera = game.camera;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 0;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 2)] = 0;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        for (int frame = 0; frame < 3; ++frame) {
            step::step(game, {});
        }
        const bool smoothing_finished =
            game.presented_player_x == game.player_x && game.presentation_frames_remaining == 0;
        game.mode_frame = 4;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        game.mode_frame = 7;
        step::step(game, {.down_held = true, .right_held = true});
        const bool buffered_turn = game.player_x == 40.0F && game.player_y == 40.0F;
        game.active_tiles[static_cast<std::size_t>(3 * level_width + 2)] = 0;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 3)] = 0;
        game.mode_frame = 11;
        step::step(game, {.down_held = true, .right_held = true});
        const bool diagonal_alternated = game.player_x == 56.0F && game.player_y == 40.0F;

        // check open diagonal speed
        game.player_x = 168.0F;
        game.player_y = 168.0F;
        game.camera = {.x = 168.0F, .y = 168.0F, .zoom = 1.0F};
        game.presented_camera = game.camera;
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
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 10)] = 0;
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 11)] = 0;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 10)] = 0;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 11)] = 0;
        game.mode_frame = 3;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        const bool diagonal_first_half = game.player_x == 168.0F && game.player_y == 184.0F;
        const bool camera_first_half = game.presented_player_target_x == 176.0F &&
                                       game.presented_player_target_y == 176.0F &&
                                       game.presented_camera.x == game.presented_player_x &&
                                       game.presented_camera.y == game.presented_player_y;
        for (int frame = 0; frame < 3; ++frame) {
            step::step(game, {.down_held = true, .right_held = true});
        }
        const bool camera_first_half_settled =
            game.presented_camera.x == 176.0F && game.presented_camera.y == 176.0F;
        step::step(game, {.down_held = true, .right_held = true});
        const bool diagonal_second_half = game.player_x == 184.0F && game.player_y == 184.0F;
        const bool camera_second_half = game.presented_player_target_x == 184.0F &&
                                        game.presented_player_target_y == 184.0F &&
                                        game.presented_camera.x == game.presented_player_x &&
                                        game.presented_camera.y == game.presented_player_y;

        // settle a released diagonal half
        game.player_x = 328.0F;
        game.player_y = 328.0F;
        game.camera = {.x = 328.0F, .y = 328.0F, .zoom = 1.0F};
        game.presented_camera = game.camera;
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
        game.active_tiles[static_cast<std::size_t>(20 * level_width + 20)] = 0;
        game.active_tiles[static_cast<std::size_t>(20 * level_width + 21)] = 0;
        game.active_tiles[static_cast<std::size_t>(21 * level_width + 20)] = 0;
        game.active_tiles[static_cast<std::size_t>(21 * level_width + 21)] = 0;
        game.mode_frame = 3;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        for (int frame = 0; frame < 3; ++frame) {
            step::step(game, {.down_held = true, .right_held = true});
        }
        step::step(game, {});
        const bool diagonal_release_started_settle =
            game.player_x == 328.0F && game.player_y == 344.0F &&
            game.presented_player_target_x == 328.0F && game.presented_player_target_y == 344.0F &&
            game.presented_player_x != game.presented_player_target_x &&
            game.presented_player_y != game.presented_player_target_y &&
            game.presented_camera.x == game.presented_player_x &&
            game.presented_camera.y == game.presented_player_y;
        for (int frame = 0; frame < 3; ++frame) {
            step::step(game, {});
        }
        const bool diagonal_release_finished_settle =
            game.presented_player_x == game.player_x && game.presented_player_y == game.player_y &&
            game.presented_camera.x == game.presented_player_x &&
            game.presented_camera.y == game.presented_player_y;

        // route around one blocked half
        game.player_x = 168.0F;
        game.player_y = 168.0F;
        game.buffered_move = {};
        game.preferred_move = {};
        game.diagonal_horizontal_next = false;
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 11)] = 0;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 10)] = 1;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 11)] = 0;
        game.mode_frame = 3;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        const bool staircase_first_half = game.player_x == 184.0F && game.player_y == 168.0F;
        game.mode_frame = 7;
        step::step(game, {.down_held = true, .right_held = true});
        const bool staircase_second_half = game.player_x == 184.0F && game.player_y == 184.0F;

        // reset diagonal after wall slide
        game.player_x = 168.0F;
        game.player_y = 168.0F;
        game.camera = {.x = 168.0F, .y = 168.0F, .zoom = 1.0F};
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
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 11)] = 0;
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 12)] = 0;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 10)] = 1;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 11)] = 1;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 12)] = 0;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 13)] = 0;
        game.mode_frame = 3;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        const bool wall_slide_stayed_cardinal =
            game.player_x == 184.0F && game.player_y == 168.0F &&
            game.presented_player_target_x == 184.0F && game.presented_player_target_y == 168.0F &&
            !game.diagonal_presentation_half;
        game.mode_frame = 7;
        step::step(game, {.down_held = true, .right_held = true});
        const bool wall_exit_started_diagonal =
            game.player_x == 200.0F && game.player_y == 168.0F &&
            game.presented_player_target_x == 192.0F && game.presented_player_target_y == 176.0F &&
            game.diagonal_presentation_half;
        game.mode_frame = 11;
        step::step(game, {.down_held = true, .right_held = true});
        const bool wall_exit_finished_diagonal =
            game.player_x == 200.0F && game.player_y == 184.0F &&
            game.presented_player_target_x == 200.0F && game.presented_player_target_y == 184.0F &&
            !game.diagonal_presentation_half;

        // reject blocked diagonal squeeze
        game.player_x = 168.0F;
        game.player_y = 168.0F;
        game.buffered_move = {};
        game.preferred_move = {};
        game.diagonal_horizontal_next = false;
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 11)] = 1;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 10)] = 1;
        game.active_tiles[static_cast<std::size_t>(11 * level_width + 11)] = 0;
        game.mode_frame = 3;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        game.mode_frame = 7;
        step::step(game, {.down_held = true, .right_held = true});
        const bool diagonal_squeeze_blocked = game.player_x == 168.0F && game.player_y == 168.0F;

        // keep sprint camera centered
        game.player_x = 248.0F;
        game.player_y = 248.0F;
        game.camera = {.x = 248.0F, .y = 248.0F, .zoom = 1.0F};
        game.presented_camera = game.camera;
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
        game.active_tiles[static_cast<std::size_t>(15 * level_width + 15)] = 0;
        game.active_tiles[static_cast<std::size_t>(15 * level_width + 16)] = 0;
        game.active_tiles[static_cast<std::size_t>(16 * level_width + 15)] = 0;
        game.active_tiles[static_cast<std::size_t>(16 * level_width + 16)] = 0;
        game.active_tiles[static_cast<std::size_t>(16 * level_width + 17)] = 0;
        const bool old_always_run = game.save.always_run;
        game.save.always_run = true;
        game.mode_frame = 1;
        step::step(game, {.down_pressed = true, .down_held = true, .right_held = true});
        step::step(game, {.down_held = true, .right_held = true});
        step::step(game, {.down_held = true, .right_held = true});
        const bool sprint_diagonal = game.player_x == 264.0F && game.player_y == 264.0F &&
                                     game.presented_player_target_x == 264.0F &&
                                     game.presented_player_target_y == 264.0F &&
                                     game.presented_camera.x == game.presented_player_x &&
                                     game.presented_camera.y == game.presented_player_y;
        game.save.always_run = old_always_run;

        const bool diagonal_motion =
            diagonal_first_half && diagonal_second_half && camera_first_half &&
            camera_first_half_settled && camera_second_half && diagonal_release_started_settle &&
            diagonal_release_finished_settle && staircase_first_half && staircase_second_half &&
            wall_slide_stayed_cardinal && wall_exit_started_diagonal &&
            wall_exit_finished_diagonal && diagonal_squeeze_blocked && sprint_diagonal;

        std::printf("movement: open=%s solid=%s arrows=%s tube=%s final-fuel=%s smooth-turn=%s "
                    "diagonal=%s\n",
                    moved_through_open_tile ? "pass" : "fail",
                    stopped_at_solid_tile ? "pass" : "fail",
                    arrow_blocked_reverse && arrow_allowed_forward && arrow_forced_forward &&
                            arrow_input_overrode_force && arrow_switch_pressed &&
                            arrow_switch_held && arrow_switch_repressed
                        ? "pass"
                        : "fail",
                    followed_tube && crossed_tube_underpass ? "pass" : "fail",
                    final_fuel_pickup_saved ? "pass" : "fail",
                    smoothing_finished && buffered_turn && diagonal_alternated ? "pass" : "fail",
                    diagonal_motion ? "pass" : "fail");
        return moved_through_open_tile && stopped_at_solid_tile && arrow_blocked_reverse &&
                       arrow_allowed_forward && arrow_forced_forward &&
                       arrow_input_overrode_force && arrow_switch_pressed && arrow_switch_held &&
                       arrow_switch_repressed && followed_tube && crossed_tube_underpass &&
                       final_fuel_pickup_saved && smoothing_finished && buffered_turn &&
                       diagonal_alternated && diagonal_motion
                   ? 0
                   : 1;
    }

    // check interactions
    if (has_flag(argc, argv, "--interaction-smoke")) {
        begin_level_for_cli(game, 1);
        step::step(game, {.confirm_pressed = true});
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(0 * level_width + 1)] = -2;
        const int score_before = game.score;
        step::step(game, {.confirm_pressed = true});
        const bool caught = game.caught_chickens == 1 &&
                            game.active_tiles[static_cast<std::size_t>(1)] == 0 &&
                            game.score > score_before;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 0;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        const bool capture_locked = game.player_x == 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] = 11;
        game.fuel = 200;
        game.mode_frame = 1;
        step::step(game, {});
        const bool capture_stopped_refill = game.fuel == 200;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] = 0;
        game.frame = game.capture_effect_started_frame + capture_lock_frames - 1U;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        const bool capture_released = game.player_x == 40.0F;
        game.player_x = 24.0F;
        game.fuel = 200;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] = 11;
        game.mode_frame = 1;
        step::step(game, {});
        const bool used_fuel_source =
            game.fuel == 201 &&
            game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] == 11;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 1)] = 0;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 11;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        const bool fuel_source_entered = game.player_x == 40.0F;
        game.player_x = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 12;
        game.mode_frame = 7;
        step::step(game, {.right_held = true});
        const bool picked_up_fuel =
            game.player_x == 40.0F && game.fuel == 251 &&
            game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] == 0;
        Level& level = game.levels[0];
        level.warps.push_back({.position = {2, 1}, .destination = {7, 9}});
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 8;
        game.mode_frame = 7;
        step::step(game, {.right_held = true});
        const bool warped = game.player_x == 120.0F && game.player_y == 152.0F;
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] = 17;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 3)] = 0;
        game.active_tiles[static_cast<std::size_t>(1 * level_width + 4)] = 0;
        game.mode_frame = 11;
        step::step(game, {.right_held = true});
        const bool pushed = game.player_x == 40.0F &&
                            game.active_tiles[static_cast<std::size_t>(1 * level_width + 2)] == 0 &&
                            game.active_tiles[static_cast<std::size_t>(1 * level_width + 3)] == 17;
        for (int frame = 0; frame < 2; ++frame) {
            step::step(game, {.run_held = true, .right_held = true});
        }
        const bool boost_push_waited = game.player_x == 40.0F;
        for (int frame = 0; frame < 2; ++frame) {
            step::step(game, {.run_held = true, .right_held = true});
        }
        const bool boost_push_capped =
            game.player_x == 56.0F &&
            game.active_tiles[static_cast<std::size_t>(1 * level_width + 4)] == 17;
        game.aliens.clear();
        game.chickens = {{.x = 10, .y = 10}};
        for (int y = 9; y <= 11; ++y) {
            for (int x = 9; x <= 11; ++x) {
                game.active_tiles[static_cast<std::size_t>(y * level_width + x)] = 0;
            }
        }
        game.active_tiles[static_cast<std::size_t>(10 * level_width + 10)] = -2;
        for (int frame = 0; frame < 14; ++frame) {
            step::step(game, {});
        }
        const ChickenState& wandering_chicken = game.chickens[0];
        const bool wandered = wandering_chicken.x != 10 || wandering_chicken.y != 10;
        std::printf(
            "interaction: chicken=%s capture=%s source=%s pickup=%s warp=%s block=%s wander=%s "
            "score=%d\n",
            caught ? "pass" : "fail",
            capture_locked && capture_stopped_refill && capture_released ? "pass" : "fail",
            used_fuel_source && fuel_source_entered ? "pass" : "fail",
            picked_up_fuel ? "pass" : "fail", warped ? "pass" : "fail",
            pushed && boost_push_waited && boost_push_capped ? "pass" : "fail",
            wandered ? "pass" : "fail", game.score);
        return caught && capture_locked && capture_stopped_refill && capture_released &&
                       used_fuel_source && fuel_source_entered && picked_up_fuel && warped &&
                       pushed && boost_push_waited && boost_push_capped && wandered
                   ? 0
                   : 1;
    }

    // check level flow
    if (has_flag(argc, argv, "--results-smoke")) {
        begin_level_for_cli(game, 2);
        const bool timer_waited_for_play = game.level_elapsed_frames == 0;
        step::step(game, {.confirm_pressed = true});
        const bool timer_still_waited = game.level_elapsed_frames == 0;
        game.fuel = 100;
        game.score = 0;
        step::step(game, {.diagnostic_low_fuel_held = true});
        const bool timer_started = game.level_elapsed_frames == 1;
        const bool set_low_fuel = game.fuel == 5;
        step::step(game, {.diagnostic_full_fuel_held = true});
        step::step(game, {.diagnostic_full_fuel_held = true});
        const bool refilled_fuel = game.fuel == 300;
        for (int frame = 0; frame < 4; ++frame) {
            step::step(game, {.diagnostic_score_held = true});
        }
        const bool added_score = game.score == 6000;
        step::step(game, {.diagnostic_skip_held = true});
        step::step(game, {.diagnostic_skip_held = true});
        const PlayerSave& diagnostic_player = game.save.players.back();
        const bool skipped_level = game.mode == Mode::results &&
                                   game.results_mode == ResultsMode::level_complete &&
                                   diagnostic_player.score == 6000 && diagnostic_player.level == 3;
        const int completed_frames = game.completed_level_frames;
        const bool stored_record = completed_frames > 0 && game.new_best_time &&
                                   game.best_level_frames[1] == completed_frames;
        step::step(game, {.confirm_pressed = true});
        const bool accelerated_results =
            game.mode == Mode::results && game.current_level == 2 && game.result_accelerated;
        step::step(game, {.back_pressed = true});
        const bool ignored_completed_back = game.mode == Mode::results && game.current_level == 2;
        while (game.result_animation_frame < result_complete_frame) {
            step::step(game, {});
        }
        const bool finished_result_animation =
            result_tally_value(
                game.caught_chickens, game.result_animation_frame, result_chicken_frame,
                result_count_tally_frames(game.caught_chickens)) == game.caught_chickens &&
            result_tally_value(game.aliens_shorted, game.result_animation_frame, result_alien_frame,
                               result_count_tally_frames(game.aliens_shorted)) ==
                game.aliens_shorted &&
            result_tally_value(game.score, game.result_animation_frame, result_score_frame) ==
                game.score;
        const bool timer_stopped = game.level_elapsed_frames == completed_frames;
        game.mode = Mode::playing;
        game.playing_mode = PlayingMode::exploding;
        game.mode_frame = death_animation_frames - 2U;
        game.chickens.clear();
        game.aliens.clear();
        step::step(game, {});
        const bool held_death = game.mode == Mode::playing;
        step::step(game, {});
        const bool finished_death =
            game.mode == Mode::results && game.results_mode == ResultsMode::level_incomplete;
        game.mode = Mode::results;
        game.results_mode = ResultsMode::level_incomplete;
        game.current_level = 2;
        step::step(game, {.back_pressed = true});
        const bool retried = game.mode == Mode::playing && game.current_level == 2;
        game.mode = Mode::results;
        game.results_mode = ResultsMode::level_complete;
        game.result_animation_frame = result_complete_frame;
        game.result_accelerated = false;
        game.current_level = level_count;
        step::step(game, {.confirm_pressed = true});
        const bool reached_ending = game.mode == Mode::congratulations &&
                                    game.congratulations_mode == CongratulationsMode::waiting;
        for (int frame = 0; frame < 239; ++frame) {
            step::step(game, {});
        }
        const bool held_ending = game.congratulations_mode == CongratulationsMode::waiting;
        step::step(game, {});
        const bool revealed_ending = game.congratulations_mode == CongratulationsMode::active;
        step::step(game, {.confirm_pressed = true});
        const bool ignored_enter = game.mode == Mode::congratulations;
        step::step(game, {.back_pressed = true});
        const bool exited_ending = game.mode == Mode::title;
        std::printf(
            "results: diagnostics=%s tally=%s death=%s retry=%s ending=%s delay=%s "
            "controls=%s timer=%s record=%s\n",
            set_low_fuel && refilled_fuel && added_score && skipped_level ? "pass" : "fail",
            accelerated_results && ignored_completed_back && finished_result_animation ? "pass"
                                                                                       : "fail",
            held_death && finished_death ? "pass" : "fail", retried ? "pass" : "fail",
            reached_ending ? "pass" : "fail", held_ending && revealed_ending ? "pass" : "fail",
            ignored_enter && exited_ending ? "pass" : "fail",
            timer_waited_for_play && timer_still_waited && timer_started && timer_stopped ? "pass"
                                                                                          : "fail",
            stored_record ? "pass" : "fail");
        return set_low_fuel && refilled_fuel && added_score && skipped_level && held_death &&
                       accelerated_results && ignored_completed_back && finished_result_animation &&
                       finished_death && retried && reached_ending && held_ending &&
                       revealed_ending && ignored_enter && exited_ending && timer_waited_for_play &&
                       timer_still_waited && timer_started && timer_stopped && stored_record
                   ? 0
                   : 1;
    }

    // check aliens
    if (has_flag(argc, argv, "--alien-smoke")) {
        begin_level_for_cli(game, 1);
        step::step(game, {.confirm_pressed = true});
        game.player_x = 24.0F;
        game.player_y = 24.0F;
        game.aliens.clear();
        game.aliens.push_back({.x = 1, .y = 0});
        game.total_aliens = 1;
        game.active_tiles[static_cast<std::size_t>(1)] = -3;
        game.mode_frame = 1;
        step::step(game, {.attack_pressed = true});
        const bool required_horn = game.aliens[0].mode != AlienMode::destroyed &&
                                   game.active_tiles[static_cast<std::size_t>(1)] == -3;
        game.mode_frame = 1;
        step::step(game, {.attack_pressed = true, .horn_held = true});
        step::step(game, {});
        const bool started_breaking =
            game.aliens[0].active && game.aliens[0].mode == AlienMode::destroyed &&
            game.active_tiles[static_cast<std::size_t>(1)] == 0 && game.player_direction == 15;
        game.mode_frame = 3;
        step::step(game, {.right_held = true});
        const bool short_locked_player = game.player_x == 24.0F;
        for (int frame = 0; frame < 100; ++frame) {
            step::step(game, {});
        }
        const bool finished_breaking = !game.aliens[0].active && game.aliens_shorted == 1;
        game.chickens.clear();
        game.aliens = {{.x = 1,
                        .y = 1,
                        .movement_clock = 1,
                        .mode = AlienMode::attack,
                        .update_delay = 2,
                        .attack_direction = 2}};
        game.player_x = 40.0F;
        game.player_y = 24.0F;
        game.fuel = 300;
        game.movement_clock = 0;
        for (int x = 0; x <= 3; ++x) {
            game.active_tiles[static_cast<std::size_t>(level_width + x)] = 0;
        }
        game.active_tiles[static_cast<std::size_t>(level_width + 1)] = -3;
        game.mode_frame = 1;
        step::step(game, {});
        const bool shoved_player = game.player_x == 56.0F && game.player_y == 24.0F &&
                                   game.fuel == 250 &&
                                   game.aliens[0].mode == AlienMode::fast_retreat;
        for (int frame = 0; frame < 4; ++frame) {
            step::step(game, {});
        }
        const bool retreated = game.aliens[0].x == 0 && game.aliens[0].y == 1;
        game.aliens = {{.x = 1,
                        .y = 1,
                        .movement_clock = 3,
                        .mode = AlienMode::slow_chase,
                        .update_delay = 5}};
        game.active_tiles[static_cast<std::size_t>(level_width + 1)] = -3;
        game.mode_frame = 1;
        step::step(game, {.horn_held = true});
        const bool horn_preserved_clock = game.aliens[0].mode == AlienMode::startled &&
                                          game.aliens[0].movement_clock == 4 &&
                                          game.aliens[0].update_delay == 5;
        game.aliens = {{.x = 1,
                        .y = 1,
                        .movement_clock = 6,
                        .mode = AlienMode::pattern,
                        .update_delay = 7,
                        .pattern = 0,
                        .pattern_step = 1}};
        game.active_tiles[static_cast<std::size_t>(level_width + 1)] = -3;
        game.active_tiles[static_cast<std::size_t>(level_width + 2)] = 0;
        game.player_x = 40.0F;
        game.player_y = 88.0F;
        game.mode_frame = 1;
        step::step(game, {});
        const bool pattern_avoided_column = game.aliens[0].x == 1 && game.aliens[0].y == 1;
        game.aliens = {{.x = 1,
                        .y = 1,
                        .movement_clock = 4,
                        .mode = AlienMode::slow_chase,
                        .update_delay = 5}};
        game.active_tiles[static_cast<std::size_t>(level_width + 1)] = -3;
        game.active_tiles[static_cast<std::size_t>(level_width + 2)] = 1;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 1)] = 0;
        game.active_tiles[static_cast<std::size_t>(2 * level_width + 2)] = 0;
        game.player_x = 56.0F;
        game.player_y = 56.0F;
        game.mode_frame = 1;
        step::step(game, {});
        const bool respected_corner = game.aliens[0].x == 1 && game.aliens[0].y == 2;
        game.aliens = {{.x = 1,
                        .y = 1,
                        .movement_clock = 4,
                        .mode = AlienMode::slow_chase,
                        .successful_moves = 45,
                        .update_delay = 5}};
        for (int y = 1; y <= 3; ++y) {
            for (int x = 1; x <= 3; ++x) {
                game.active_tiles[static_cast<std::size_t>(y * level_width + x)] = 0;
            }
        }
        game.active_tiles[static_cast<std::size_t>(level_width + 1)] = -3;
        game.player_x = 72.0F;
        game.player_y = 72.0F;
        const int shorted_before_winded = game.aliens_shorted;
        game.mode_frame = 1;
        step::step(game, {});
        const bool became_winded = game.aliens[0].mode == AlienMode::winded;
        game.aliens[0].movement_clock = 6;
        game.mode_frame = 1;
        step::step(game, {});
        const bool remained_alive =
            game.aliens[0].active && game.aliens[0].mode == AlienMode::winded &&
            game.aliens[0].frame <= 5 && game.aliens_shorted == shorted_before_winded;
        std::printf(
            "alien: gate=%s short=%s animation=%s shove=%s retreat=%s horn=%s lines=%s corner=%s "
            "winded=%s\n",
            required_horn ? "pass" : "fail",
            started_breaking && short_locked_player ? "pass" : "fail",
            finished_breaking ? "pass" : "fail", shoved_player ? "pass" : "fail",
            retreated ? "pass" : "fail", horn_preserved_clock ? "pass" : "fail",
            pattern_avoided_column ? "pass" : "fail", respected_corner ? "pass" : "fail",
            became_winded && remained_alive ? "pass" : "fail");
        return required_horn && started_breaking && short_locked_player && finished_breaking &&
                       shoved_player && retreated && horn_preserved_clock &&
                       pattern_avoided_column && respected_corner && became_winded && remained_alive
                   ? 0
                   : 1;
    }

    // check bomb exit
    if (has_flag(argc, argv, "--exit-smoke")) {
        begin_level_for_cli(game, 1);
        step::step(game, {.confirm_pressed = true});
        int exit_switch_x = -1;
        int exit_switch_y = -1;
        for (int y = 0; y < level_height && exit_switch_x < 0; ++y) {
            for (int x = 0; x < level_width; ++x) {
                if (game.active_tiles[static_cast<std::size_t>(y * level_width + x)] == 18) {
                    exit_switch_x = x;
                    exit_switch_y = y;
                    break;
                }
            }
        }
        game.player_x = static_cast<float>(exit_switch_x * 16 + 8);
        game.player_y = static_cast<float>((exit_switch_y - 1) * 16 + 8);
        game.caught_chickens = 0;
        game.mode_frame = 2;
        step::step(game, {.down_pressed = true, .down_held = true});
        const std::uint64_t hint_frame = game.chicken_hint_started_frame;
        const bool missing_chickens_rejected = !game.exit_open && game.chicken_hint_active;
        game.mode_frame = 3;
        step::step(game, {.down_held = true});
        const bool tnt_hold_latched =
            !game.exit_open && game.chicken_hint_started_frame == hint_frame;
        game.caught_chickens = game.total_chickens;
        game.mode_frame = 4;
        step::step(game, {.down_pressed = true, .down_held = true});
        const Level& level = game.levels[0];
        const bool opened =
            game.exit_open && game.bomb_seconds == 45 &&
            game.active_tiles[static_cast<std::size_t>(level.exit.y * level_width +
                                                       level.exit.x)] == 10 &&
            std::ranges::none_of(game.active_tiles, [](std::int8_t tile) { return tile == 13; });
        game.aliens.clear();
        for (int frame = 0; frame < static_cast<int>(step_rate) * 45; ++frame) {
            step::step(game, {});
        }
        const bool timer_failed =
            game.playing_mode == PlayingMode::exploding && game.bomb_seconds == -1;
        std::printf("exit: latch=%s chickens=%s open=%s timer=%s\n",
                    tnt_hold_latched ? "pass" : "fail", missing_chickens_rejected ? "pass" : "fail",
                    opened ? "pass" : "fail", timer_failed ? "pass" : "fail");
        return tnt_hold_latched && missing_chickens_rejected && opened && timer_failed ? 0 : 1;
    }

    // parse runtime flags
    options.render_smoke = has_flag(argc, argv, "--render-smoke");
    options.report_performance = has_flag(argc, argv, "--report-performance");
    options.capture_path = string_argument(argc, argv, "--capture-frame");
    const int play_level = integer_argument(argc, argv, "--play-level", 0);
    if (play_level >= 1 && play_level <= level_count) {
        begin_level_for_cli(game, play_level);
    }
    if (has_flag(argc, argv, "--radar")) {
        options.controls_preview = true;
        game.playing_mode = PlayingMode::active;
        game.radar_visible = true;
    }
    if (has_flag(argc, argv, "--controls-controller")) {
        options.controls_preview = true;
        game.mode = Mode::instructions;
        game.instructions_page = InstructionsPage::controller;
        game.input.last_device = InputDevice::controller;
        game.mode_frame = 0;
    } else if (has_flag(argc, argv, "--controls-keyboard")) {
        options.controls_preview = true;
        game.mode = Mode::instructions;
        game.instructions_page = InstructionsPage::keyboard;
        game.input.last_device = InputDevice::keyboard;
        game.mode_frame = 0;
    } else if (has_flag(argc, argv, "--instructions")) {
        options.controls_preview = true;
        game.mode = Mode::instructions;
        game.instructions_page = InstructionsPage::original;
        game.input.last_device = InputDevice::keyboard;
        game.mode_frame = 0;
    }
    options.explicit_window_size =
        has_argument(argc, argv, "--width") || has_argument(argc, argv, "--height");
    game.window_width = integer_argument(argc, argv, "--width", game.window_width);
    game.window_height = integer_argument(argc, argv, "--height", game.window_height);
    if (options.explicit_window_size) {
        game.fullscreen = false;
    }

    return std::nullopt;
}

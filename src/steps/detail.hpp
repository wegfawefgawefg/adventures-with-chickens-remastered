#pragma once

#include "../state.hpp"

struct InputState;
class Audio;
enum class Sound;

namespace step::detail {

int original_warp_phase(std::size_t index);
void play_sound(const State& game, Audio* audio, Sound sound);
void change_mode(State& game, Mode mode);
void persist_save(State& game, const char* context);
void start_level(State& game, Audio* audio);
bool passable_tile(int tile, int move_x, int move_y);
int tile_at(const State& game, int x, int y);
void set_tile(State& game, int x, int y, int tile);
int difficulty(const State& game);

void capture_chicken(State& game, Audio* audio);
void step_chickens(State& game);
void short_alien(State& game);
void step_aliens(State& game, Audio* audio, bool horn_held);

void consume_movement_fuel(State& game);
void reach_exit(State& game, Audio* audio);
bool try_activate_blocked_tile(State& game, int move_x, int move_y, Audio* audio);
bool try_move_player(State& game, int move_x, int move_y, Audio* audio);
void step_tube(State& game, Audio* audio);
void step_warps(State& game);

void step_startup(State& game, const InputState& input);
void step_title(State& game, const InputState& input, Audio* audio);
void step_frontend_page(State& game, const InputState& input, Audio* audio);
void step_options(State& game, const InputState& input, Audio* audio);
void step_player_select(State& game, const InputState& input, Audio* audio);
void step_playing(State& game, const InputState& input, Audio* audio);
void step_results(State& game, const InputState& input, Audio* audio);
void step_congratulations(State& game, const InputState& input, Audio* audio);
void step_exiting(State& game);

} // namespace step::detail

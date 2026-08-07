#pragma once

struct Assets;
struct SDL_Renderer;
struct State;
enum class InputDevice;

void draw_controls_screen(SDL_Renderer* renderer, const Assets& assets, const State& game,
                          InputDevice device, int output_width, int output_height);

#pragma once

#include "state.hpp"

struct SDL_Renderer;
struct Assets;

void draw(SDL_Renderer* renderer, const Assets& assets, const State& game,
          float presentation_alpha = 1.0F);

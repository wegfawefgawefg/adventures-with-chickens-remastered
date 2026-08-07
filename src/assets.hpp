#pragma once

#include <array>

struct SDL_Renderer;
struct SDL_Texture;

struct Assets {
    SDL_Texture* rocksolid{};
    SDL_Texture* xgames{};
    SDL_Texture* stars{};
    SDL_Texture* dark{};
    SDL_Texture* window_back{};
    SDL_Texture* window_corners{};
    SDL_Texture* window_slide{};
    SDL_Texture* main_screen{};
    SDL_Texture* selections{};
    SDL_Texture* selector{};
    SDL_Texture* character{};
    SDL_Texture* order{};
    SDL_Texture* options{};
    SDL_Texture* instructions{};
    SDL_Texture* credits{};
    SDL_Texture* tiles{};
    SDL_Texture* arrows{};
    SDL_Texture* crosses{};
    SDL_Texture* tubes{};
    SDL_Texture* warp_tile{};
    SDL_Texture* exit{};
    SDL_Texture* ship{};
    SDL_Texture* chicken{};
    SDL_Texture* chickens{};
    SDL_Texture* aliens{};
    SDL_Texture* fuel{};
    SDL_Texture* radar{};
    SDL_Texture* font{};
    SDL_Texture* number_font{};
    SDL_Texture* separator{};
    SDL_Texture* paused{};
    SDL_Texture* explosion{};
    SDL_Texture* congratulations{};
    std::array<SDL_Texture*, 4> controller_buttons{};
};

bool load_assets(SDL_Renderer* renderer, Assets& assets);
void destroy_assets(Assets& assets);

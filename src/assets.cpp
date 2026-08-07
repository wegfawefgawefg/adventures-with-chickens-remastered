#include "assets.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <ranges>
#include <string>
#include <vector>

#ifndef AWC_ASSET_ROOT
#define AWC_ASSET_ROOT "assets"
#endif

namespace {

bool clear_paused_prompt(SDL_Surface* surface) {
    // remove green prompt packed below logo
    SDL_Palette* palette = SDL_GetSurfacePalette(surface);
    if (palette == nullptr || !SDL_LockSurface(surface)) {
        return false;
    }
    for (int y = 0; y < surface->h; ++y) {
        auto* row = static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch;
        for (int x = 0; x < surface->w; ++x) {
            const SDL_Color color = palette->colors[row[x]];
            if (color.r == 0 && color.g > 0 && color.b == 0) {
                row[x] = 0;
            }
        }
    }
    SDL_UnlockSurface(surface);
    return true;
}

SDL_Texture* load_texture(SDL_Renderer* renderer, const char* filename,
                          bool remove_paused_prompt = false) {
    // load BMP
    const std::string path = std::string{AWC_ASSET_ROOT} + "/graphics/" + filename;
    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
    if (surface == nullptr) {
        std::fprintf(stderr, "could not load %s: %s\n", path.c_str(), SDL_GetError());
        return nullptr;
    }

    if (remove_paused_prompt && !clear_paused_prompt(surface)) {
        std::fprintf(stderr, "could not clean %s: %s\n", path.c_str(), SDL_GetError());
        SDL_DestroySurface(surface);
        return nullptr;
    }

    // set palette color key
    if (!SDL_SetSurfaceColorKey(surface, true, 0)) {
        std::fprintf(stderr, "could not set color key for %s: %s\n", path.c_str(), SDL_GetError());
        SDL_DestroySurface(surface);
        return nullptr;
    }

    // upload texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        std::fprintf(stderr, "could not create texture for %s: %s\n", path.c_str(), SDL_GetError());
        return nullptr;
    }

    (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

SDL_Texture* load_recolored_texture(SDL_Renderer* renderer, const char* filename,
                                    SDL_Color target) {
    // load indexed source
    const std::string path = std::string{AWC_ASSET_ROOT} + "/graphics/" + filename;
    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
    SDL_Palette* palette = surface != nullptr ? SDL_GetSurfacePalette(surface) : nullptr;
    if (surface == nullptr || palette == nullptr) {
        std::fprintf(stderr, "could not load recolored %s: %s\n", path.c_str(), SDL_GetError());
        SDL_DestroySurface(surface);
        return nullptr;
    }

    // map source brightness into target color
    std::vector<SDL_Color> colors(palette->colors, palette->colors + palette->ncolors);
    for (SDL_Color& color : colors) {
        const int brightness = std::max({color.r, color.g, color.b});
        color.r = static_cast<std::uint8_t>(static_cast<int>(target.r) * brightness / 252);
        color.g = static_cast<std::uint8_t>(static_cast<int>(target.g) * brightness / 252);
        color.b = static_cast<std::uint8_t>(static_cast<int>(target.b) * brightness / 252);
    }
    if (!SDL_SetPaletteColors(palette, colors.data(), 0, palette->ncolors) ||
        !SDL_SetSurfaceColorKey(surface, true, 0)) {
        std::fprintf(stderr, "could not recolor %s: %s\n", path.c_str(), SDL_GetError());
        SDL_DestroySurface(surface);
        return nullptr;
    }

    // upload recolored texture
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture != nullptr) {
        (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }
    return texture;
}

} // namespace

bool load_assets(SDL_Renderer* renderer, Assets& assets) {
    // load textures
    assets.rocksolid = load_texture(renderer, "RockSolid.bmp");
    assets.xgames = load_texture(renderer, "XGames.bmp");
    assets.stars = load_texture(renderer, "Stars.bmp");
    assets.dark = load_texture(renderer, "Dark.bmp");
    assets.window_back = load_texture(renderer, "WindowBack.bmp");
    assets.window_corners = load_texture(renderer, "WindowCorners.bmp");
    assets.window_slide = load_texture(renderer, "WindowSlide.bmp");
    assets.main_screen = load_texture(renderer, "MainScreen.bmp");
    assets.selections = load_texture(renderer, "Selections.bmp");
    assets.selector = load_texture(renderer, "Selector.bmp");
    assets.character = load_texture(renderer, "Character.bmp");
    assets.order = load_texture(renderer, "Order.bmp");
    assets.options = load_texture(renderer, "Options.bmp");
    assets.instructions = load_texture(renderer, "Instructions.bmp");
    assets.credits = load_texture(renderer, "Credits.bmp");
    assets.tiles = load_texture(renderer, "Tiles.bmp");
    assets.arrows = load_texture(renderer, "Arrows.bmp");
    assets.crosses = load_texture(renderer, "Crosses.bmp");
    assets.tubes = load_texture(renderer, "Tubes.bmp");
    assets.warp_tile = load_texture(renderer, "WarpTile.bmp");
    assets.exit = load_texture(renderer, "Exit.bmp");
    assets.ship = load_texture(renderer, "Ship.bmp");
    assets.chicken = load_texture(renderer, "Chicken.bmp");
    assets.chickens = load_texture(renderer, "Chickens.bmp");
    assets.aliens = load_texture(renderer, "Aliens.bmp");
    assets.fuel = load_texture(renderer, "Fuel.bmp");
    assets.radar = load_texture(renderer, "Radar.bmp");
    assets.font = load_texture(renderer, "Font.bmp");
    assets.number_font = load_texture(renderer, "NumberFont.bmp");
    assets.separator = load_texture(renderer, "Separator.bmp");
    assets.paused = load_texture(renderer, "Paused.bmp", true);
    assets.explosion = load_texture(renderer, "Explosion.bmp");
    assets.congratulations = load_texture(renderer, "Congratulations.bmp");
    constexpr std::array button_colors{
        SDL_Color{48, 112, 220, 255},
        SDL_Color{54, 170, 82, 255},
        SDL_Color{200, 62, 62, 255},
        SDL_Color{220, 180, 40, 255},
    };
    for (std::size_t index = 0; index < assets.controller_buttons.size(); ++index) {
        assets.controller_buttons[index] =
            load_recolored_texture(renderer, "Tiles.bmp", button_colors[index]);
    }

    // validate textures
    if (assets.rocksolid != nullptr && assets.xgames != nullptr && assets.stars != nullptr &&
        assets.dark != nullptr && assets.window_back != nullptr &&
        assets.window_corners != nullptr && assets.window_slide != nullptr &&
        assets.main_screen != nullptr && assets.selections != nullptr &&
        assets.selector != nullptr && assets.character != nullptr && assets.order != nullptr &&
        assets.options != nullptr && assets.instructions != nullptr && assets.credits != nullptr &&
        assets.tiles != nullptr && assets.arrows != nullptr && assets.crosses != nullptr &&
        assets.tubes != nullptr && assets.warp_tile != nullptr && assets.exit != nullptr &&
        assets.ship != nullptr && assets.chicken != nullptr && assets.chickens != nullptr &&
        assets.aliens != nullptr && assets.fuel != nullptr && assets.radar != nullptr &&
        assets.font != nullptr && assets.number_font != nullptr && assets.separator != nullptr &&
        assets.paused != nullptr && assets.explosion != nullptr &&
        assets.congratulations != nullptr &&
        std::ranges::none_of(assets.controller_buttons,
                             [](SDL_Texture* texture) { return texture == nullptr; })) {
        return true;
    }

    destroy_assets(assets);
    return false;
}

void destroy_assets(Assets& assets) {
    // free textures
    for (SDL_Texture* texture : assets.controller_buttons) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyTexture(assets.congratulations);
    SDL_DestroyTexture(assets.explosion);
    SDL_DestroyTexture(assets.paused);
    SDL_DestroyTexture(assets.separator);
    SDL_DestroyTexture(assets.number_font);
    SDL_DestroyTexture(assets.font);
    SDL_DestroyTexture(assets.radar);
    SDL_DestroyTexture(assets.fuel);
    SDL_DestroyTexture(assets.aliens);
    SDL_DestroyTexture(assets.chickens);
    SDL_DestroyTexture(assets.chicken);
    SDL_DestroyTexture(assets.ship);
    SDL_DestroyTexture(assets.exit);
    SDL_DestroyTexture(assets.warp_tile);
    SDL_DestroyTexture(assets.tubes);
    SDL_DestroyTexture(assets.crosses);
    SDL_DestroyTexture(assets.arrows);
    SDL_DestroyTexture(assets.tiles);
    SDL_DestroyTexture(assets.credits);
    SDL_DestroyTexture(assets.instructions);
    SDL_DestroyTexture(assets.options);
    SDL_DestroyTexture(assets.order);
    SDL_DestroyTexture(assets.character);
    SDL_DestroyTexture(assets.selector);
    SDL_DestroyTexture(assets.selections);
    SDL_DestroyTexture(assets.main_screen);
    SDL_DestroyTexture(assets.window_slide);
    SDL_DestroyTexture(assets.window_corners);
    SDL_DestroyTexture(assets.window_back);
    SDL_DestroyTexture(assets.dark);
    SDL_DestroyTexture(assets.stars);
    SDL_DestroyTexture(assets.xgames);
    SDL_DestroyTexture(assets.rocksolid);
    assets = {};
}

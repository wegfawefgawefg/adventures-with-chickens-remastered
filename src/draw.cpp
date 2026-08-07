#include "draw.hpp"

#include "assets.hpp"
#include "controls.hpp"
#include "draw_controls.hpp"
#include "verses.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int tile_size = 16;
constexpr int star_field_width = 637;
constexpr int star_field_height = 477;

void fill(SDL_Renderer* renderer, SDL_FRect rectangle, std::uint8_t red, std::uint8_t green,
          std::uint8_t blue) {
    (void)SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    (void)SDL_RenderFillRect(renderer, &rectangle);
}

void shade_screen(SDL_Renderer* renderer, int output_width, int output_height, std::uint8_t alpha) {
    // draw black shade
    (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    (void)SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
    const SDL_FRect screen{
        .x = 0.0F,
        .y = 0.0F,
        .w = static_cast<float>(output_width),
        .h = static_cast<float>(output_height),
    };
    (void)SDL_RenderFillRect(renderer, &screen);
    (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

SDL_FRect layout_rect(SDL_FRect rectangle, int output_width, int output_height) {
    // calc frontend layout
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float content_width = static_cast<float>(original_layout_width) * scale;
    const float offset_x = (static_cast<float>(output_width) - content_width) * 0.5F;
    return {
        .x = offset_x + rectangle.x * scale,
        .y = rectangle.y * scale,
        .w = rectangle.w * scale,
        .h = rectangle.h * scale,
    };
}

void fill_layout(SDL_Renderer* renderer, SDL_FRect rectangle, int output_width, int output_height,
                 std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    fill(renderer, layout_rect(rectangle, output_width, output_height), red, green, blue);
}

void tile_screen(SDL_Renderer* renderer, SDL_Texture* texture, int output_width,
                 int output_height) {
    // tile texture
    constexpr float texture_size = 64.0F;
    for (float y = 0.0F; y < static_cast<float>(output_height); y += texture_size) {
        for (float x = 0.0F; x < static_cast<float>(output_width); x += texture_size) {
            const SDL_FRect destination{
                .x = x,
                .y = y,
                .w = std::min(texture_size, static_cast<float>(output_width) - x),
                .h = std::min(texture_size, static_cast<float>(output_height) - y),
            };
            const SDL_FRect source{
                .x = 0.0F,
                .y = 0.0F,
                .w = destination.w,
                .h = destination.h,
            };
            (void)SDL_RenderTexture(renderer, texture, &source, &destination);
        }
    }
}

void draw_window_panel(SDL_Renderer* renderer, const Assets& assets, SDL_FRect panel, float scale) {
    // draw window fill
    constexpr float background_size = 128.0F;
    for (float y = 0.0F; y < panel.h; y += background_size) {
        for (float x = 0.0F; x < panel.w; x += background_size) {
            const SDL_FRect destination{
                .x = panel.x + x,
                .y = panel.y + y,
                .w = std::min(background_size, panel.w - x),
                .h = std::min(background_size, panel.h - y),
            };
            const SDL_FRect source{
                .x = 0.0F,
                .y = 0.0F,
                .w = destination.w,
                .h = destination.h,
            };
            (void)SDL_RenderTexture(renderer, assets.window_back, &source, &destination);
        }
    }

    // draw window corners
    const float corner_width = 8.0F * scale;
    const float corner_height = 4.0F * scale;
    for (int corner = 0; corner < 4; ++corner) {
        const SDL_FRect source{
            .x = static_cast<float>(corner * 8),
            .y = 0.0F,
            .w = 8.0F,
            .h = 4.0F,
        };
        const bool right = corner % 2 != 0;
        const bool bottom = corner >= 2;
        const SDL_FRect destination{
            .x = right ? panel.x + panel.w - corner_width : panel.x,
            .y = bottom ? panel.y + panel.h - corner_height : panel.y,
            .w = corner_width,
            .h = corner_height,
        };
        (void)SDL_RenderTexture(renderer, assets.window_corners, &source, &destination);
    }
}

void draw_starter_chicken(SDL_Renderer* renderer, std::uint64_t frame, int output_width,
                          int output_height) {
    const float bob = ((frame / 24U) % 2U) == 0U ? 0.0F : 2.0F;

    fill_layout(renderer, {288, 199 + bob, 72, 56}, output_width, output_height, 232, 235, 219);
    fill_layout(renderer, {344, 183 + bob, 40, 40}, output_width, output_height, 246, 239, 210);
    fill_layout(renderer, {376, 199 + bob, 16, 8}, output_width, output_height, 242, 159, 45);
    fill_layout(renderer, {366, 191 + bob, 6, 6}, output_width, output_height, 17, 20, 25);
    fill_layout(renderer, {280, 207 + bob, 24, 32}, output_width, output_height, 205, 213, 202);
    fill_layout(renderer, {304, 215 + bob, 24, 16}, output_width, output_height, 194, 201, 190);
    fill_layout(renderer, {304, 255 + bob, 6, 20}, output_width, output_height, 224, 154, 47);
    fill_layout(renderer, {342, 255 + bob, 6, 20}, output_width, output_height, 224, 154, 47);
    fill_layout(renderer, {296, 273 + bob, 18, 5}, output_width, output_height, 224, 154, 47);
    fill_layout(renderer, {338, 273 + bob, 18, 5}, output_width, output_height, 224, 154, 47);
}

float wrap_star_position(float position, float field_size) {
    position = std::fmod(position, field_size);
    return position < 0 ? position + field_size : position;
}

void draw_stars(SDL_Renderer* renderer, SDL_Texture* texture, const State& game, float offset_x,
                float offset_y, int output_width, int output_height) {
    // set original scale and centered layout
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float layout_left =
        (static_cast<float>(output_width) - static_cast<float>(original_layout_width) * scale) *
        0.5F;
    const float repeat_width = static_cast<float>(star_field_width) * scale;

    // draw original field and repeat it into widescreen margins
    for (const StarState& star : game.stars) {
        const float x =
            wrap_star_position(static_cast<float>(star.position.x) - offset_x, star_field_width);
        const float y =
            wrap_star_position(static_cast<float>(star.position.y) - offset_y, star_field_height);
        const SDL_FRect source{
            .x = static_cast<float>(star.frame * 3),
            .y = 0.0F,
            .w = 3.0F,
            .h = 3.0F,
        };
        const float base_x = layout_left + x * scale;
        for (float screen_x = base_x; screen_x > -3.0F * scale; screen_x -= repeat_width) {
            const SDL_FRect destination{
                .x = screen_x,
                .y = y * scale,
                .w = std::max(1.0F, 3.0F * scale),
                .h = std::max(1.0F, 3.0F * scale),
            };
            (void)SDL_RenderTexture(renderer, texture, &source, &destination);
        }
        for (float screen_x = base_x + repeat_width; screen_x < static_cast<float>(output_width);
             screen_x += repeat_width) {
            const SDL_FRect destination{
                .x = screen_x,
                .y = y * scale,
                .w = std::max(1.0F, 3.0F * scale),
                .h = std::max(1.0F, 3.0F * scale),
            };
            (void)SDL_RenderTexture(renderer, texture, &source, &destination);
        }
    }
}

void draw_starter_screen(SDL_Renderer* renderer, const Assets& assets, const State& game,
                         int output_width, int output_height) {
    draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
               static_cast<float>(game.star_scroll.y), output_width, output_height);
    draw_starter_chicken(renderer, game.frame, output_width, output_height);
    fill_layout(renderer, {218, 318, 204, 2}, output_width, output_height, 54, 126, 158);
    fill_layout(renderer, {250, 328, 140, 1}, output_width, output_height, 29, 68, 87);
}

void draw_frontend_screen(SDL_Renderer* renderer, SDL_Texture* texture, int output_width,
                          int output_height) {
    // draw frontend background
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float width = static_cast<float>(original_layout_width) * scale;
    const SDL_FRect destination{
        .x = (static_cast<float>(output_width) - width) * 0.5F,
        .y = 0.0F,
        .w = width,
        .h = static_cast<float>(output_height),
    };
    (void)SDL_RenderTexture(renderer, texture, nullptr, &destination);
}

void draw_frontend_with_stars(SDL_Renderer* renderer, const Assets& assets, const State& game,
                              SDL_Texture* texture, int output_width, int output_height) {
    draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
               static_cast<float>(game.star_scroll.y), output_width, output_height);
    draw_frontend_screen(renderer, texture, output_width, output_height);
}

void draw_instructions_screen(SDL_Renderer* renderer, SDL_Texture* texture, int output_width,
                              int output_height) {
    // keep original help art above obsolete controls
    constexpr float content_height = 370.0F;
    constexpr SDL_FRect obsolete_space_prompt{298.0F, 165.0F, 155.0F, 18.0F};
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float width = static_cast<float>(original_layout_width) * scale;
    const float left = (static_cast<float>(output_width) - width) * 0.5F;
    const auto draw_slice = [&](SDL_FRect source) {
        const SDL_FRect destination{
            .x = left + source.x * scale,
            .y = source.y * scale,
            .w = source.w * scale,
            .h = source.h * scale,
        };
        (void)SDL_RenderTexture(renderer, texture, &source, &destination);
    };
    draw_slice({0.0F, 0.0F, static_cast<float>(original_layout_width), obsolete_space_prompt.y});
    draw_slice({0.0F, obsolete_space_prompt.y, obsolete_space_prompt.x, obsolete_space_prompt.h});
    const SDL_FRect remaining_line{
        obsolete_space_prompt.x + obsolete_space_prompt.w,
        obsolete_space_prompt.y,
        static_cast<float>(original_layout_width) - obsolete_space_prompt.x -
            obsolete_space_prompt.w,
        obsolete_space_prompt.h,
    };
    const SDL_FRect shifted_line{
        left + (obsolete_space_prompt.x + 6.0F) * scale,
        remaining_line.y * scale,
        remaining_line.w * scale,
        remaining_line.h * scale,
    };
    (void)SDL_RenderTexture(renderer, texture, &remaining_line, &shifted_line);
    draw_slice({0.0F, obsolete_space_prompt.y + obsolete_space_prompt.h,
                static_cast<float>(original_layout_width),
                content_height - obsolete_space_prompt.y - obsolete_space_prompt.h});
}

void draw_selector(SDL_Renderer* renderer, SDL_Texture* texture, std::uint64_t frame, float x,
                   float y, float scale, float layout_left) {
    // draw menu cursor
    constexpr int selector_columns = 10;
    constexpr int selector_frames = 20;
    constexpr float selector_size = 40.0F;
    const int selector_frame = static_cast<int>((frame / original_update_period) % selector_frames);
    const SDL_FRect source{
        .x = static_cast<float>(selector_frame % selector_columns) * selector_size,
        .y = static_cast<float>(selector_frame / selector_columns) * selector_size,
        .w = selector_size,
        .h = selector_size,
    };
    const SDL_FRect destination{
        .x = layout_left + x * scale,
        .y = y * scale,
        .w = selector_size * scale,
        .h = selector_size * scale,
    };
    (void)SDL_RenderTexture(renderer, texture, &source, &destination);
}

void draw_title_menu(SDL_Renderer* renderer, const Assets& assets, const State& game,
                     int output_width, int output_height) {
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float layout_left =
        (static_cast<float>(output_width) - static_cast<float>(original_layout_width) * scale) *
        0.5F;

    // draw title menu
    constexpr std::array<int, 5> source_rows{0, 1, 2, 4, 5};
    constexpr std::array<float, 5> destination_rows{150.0F, 200.0F, 250.0F, 300.0F, 355.0F};
    for (std::size_t row = 0; row < source_rows.size(); ++row) {
        const SDL_FRect source{
            .x = 0.0F,
            .y = static_cast<float>(source_rows[row] * 50),
            .w = 345.0F,
            .h = 50.0F,
        };
        const SDL_FRect destination{
            .x = layout_left + 190.0F * scale,
            .y = destination_rows[row] * scale,
            .w = 345.0F * scale,
            .h = 50.0F * scale,
        };
        (void)SDL_RenderTexture(renderer, assets.selections, &source, &destination);
    }

    draw_selector(renderer, assets.selector, game.mode_frame, 140.0F,
                  150.0F + static_cast<float>(static_cast<int>(game.title_choice)) * 50.0F, scale,
                  layout_left);
}

SDL_FRect world_destination(float world_x, float world_y, float width, float height,
                            const Camera& camera, float scale, int output_width,
                            int output_height) {
    // world to screen
    return {
        .x = (world_x - camera.x) * scale + static_cast<float>(output_width) * 0.5F,
        .y = (world_y - camera.y) * scale + static_cast<float>(output_height) * 0.5F,
        .w = width * scale,
        .h = height * scale,
    };
}

void draw_world_cell(SDL_Renderer* renderer, SDL_Texture* texture, int source_x, int source_y,
                     float world_x, float world_y, const Camera& camera, float scale,
                     int output_width, int output_height) {
    const SDL_FRect source{
        .x = static_cast<float>(source_x),
        .y = static_cast<float>(source_y),
        .w = 16.0F,
        .h = 16.0F,
    };
    const SDL_FRect destination = world_destination(world_x, world_y, 16.0F, 16.0F, camera, scale,
                                                    output_width, output_height);
    (void)SDL_RenderTexture(renderer, texture, &source, &destination);
}

int cosmetic_flicker(std::uint64_t frame, int tile_x, int tile_y) {
    // calc tile variant
    std::uint64_t value = frame / original_update_period;
    value ^= static_cast<std::uint64_t>(tile_x + 1) * UINT64_C(0x9e3779b185ebca87);
    value ^= static_cast<std::uint64_t>(tile_y + 1) * UINT64_C(0xc2b2ae3d27d4eb4f);
    value ^= value >> 29U;
    value *= UINT64_C(0x165667b19e3779f9);
    return static_cast<int>((value ^ (value >> 32U)) & 1U);
}

void draw_terrain_cell(SDL_Renderer* renderer, const Assets& assets, int tile, int tile_x,
                       int tile_y, const State& game, const Camera& camera, float scale,
                       int output_width, int output_height) {
    if (tile == 0 || tile < 0 || tile == 8 || tile == 9) {
        return;
    }

    // draw animated tile
    const float world_x = static_cast<float>(tile_x * 16);
    const float world_y = static_cast<float>(tile_y * 16);
    if (tile >= 4 && tile <= 7) {
        const int frame = static_cast<int>((game.frame / 4U) % 8U);
        draw_world_cell(renderer, assets.arrows, frame * 16, (tile - 4) * 16, world_x, world_y,
                        camera, scale, output_width, output_height);
        return;
    }
    if (tile >= 14 && tile <= 16) {
        const int frame = static_cast<int>((game.frame / 8U) % 7U);
        draw_world_cell(renderer, assets.crosses, frame * 16, (tile - 14) * 16, world_x, world_y,
                        camera, scale, output_width, output_height);
        return;
    }
    if (tile == 3) {
        const int frame = 4 + static_cast<int>((game.frame / 8U) % 2U);
        draw_world_cell(renderer, assets.tiles, frame * 16, 0, world_x, world_y, camera, scale,
                        output_width, output_height);
        return;
    }
    if (tile >= 20 && tile <= 34) {
        const int relative = tile - 20;
        const int row = relative / 7;
        const int column = relative % 7;
        const int player_x = static_cast<int>(game.player_x) / tile_size;
        const int player_y = static_cast<int>(game.player_y) / tile_size;
        const int occupied_offset = tile_x == player_x && tile_y == player_y ? 16 : 0;
        draw_world_cell(renderer, assets.tubes, column * 32 + occupied_offset, row * 16, world_x,
                        world_y, camera, scale, output_width, output_height);
        return;
    }

    // draw static tile
    int frame = 0;
    switch (tile) {
    case 2:
        frame = 2 + cosmetic_flicker(game.frame, tile_x, tile_y);
        break;
    case 11:
        frame = 13 + static_cast<int>((game.frame / 32U) % 4U);
        break;
    case 12:
        frame = 6;
        break;
    case 13:
        frame = 10 + static_cast<int>((game.frame / 40U) % 3U);
        break;
    case 17:
        frame = 1;
        break;
    case 18:
        frame = 7;
        break;
    case 19:
        frame = 8 + cosmetic_flicker(game.frame, tile_x, tile_y);
        break;
    default:
        break;
    }
    draw_world_cell(renderer, assets.tiles, frame * 16, 0, world_x, world_y, camera, scale,
                    output_width, output_height);
}

void draw_actor(SDL_Renderer* renderer, SDL_Texture* texture, int frame, const IVec2& position,
                const Camera& camera, float scale, int output_width, int output_height) {
    draw_world_cell(renderer, texture, frame * 16, 0, static_cast<float>(position.x * 16),
                    static_cast<float>(position.y * 16), camera, scale, output_width,
                    output_height);
}

void draw_number(SDL_Renderer* renderer, SDL_Texture* texture, int value, float right, float top,
                 float digit_size) {
    // draw number
    const std::string digits = std::to_string(std::max(0, value));
    float x = right - static_cast<float>(digits.size()) * digit_size;
    for (const char character : digits) {
        const int digit = character - '0';
        const SDL_FRect source{
            .x = static_cast<float>(digit % 5) * 40.0F,
            .y = static_cast<float>(digit / 5) * 40.0F,
            .w = 40.0F,
            .h = 40.0F,
        };
        const SDL_FRect destination{
            .x = x,
            .y = top,
            .w = digit_size,
            .h = digit_size,
        };
        (void)SDL_RenderTexture(renderer, texture, &source, &destination);
        x += digit_size;
    }
}

float text_width(const std::string& text, float scale) {
    return static_cast<float>(text.size()) * 11.0F * scale;
}

void draw_text(SDL_Renderer* renderer, SDL_Texture* texture, const std::string& text, float x,
               float y, float scale) {
    // draw text
    for (const char character : text) {
        const int code = static_cast<unsigned char>(character);
        if (code < 32 || code >= 32 + 96) {
            x += 11.0F * scale;
            continue;
        }
        const int index = code - 32;
        const SDL_FRect source{
            .x = static_cast<float>(index % 32) * 11.0F + 0.01F,
            .y = static_cast<float>(index / 32) * 16.0F + 0.01F,
            .w = 10.98F,
            .h = 15.98F,
        };
        const SDL_FRect destination{
            .x = x,
            .y = y,
            .w = 11.0F * scale,
            .h = 16.0F * scale,
        };
        (void)SDL_RenderTexture(renderer, texture, &source, &destination);
        x += 11.0F * scale;
    }
}

std::string format_level_time(int frames) {
    // format fixed-step time
    const int safe_frames = std::max(0, frames);
    const int total_seconds = safe_frames / static_cast<int>(step_rate);
    const int minutes = total_seconds / 60;
    const int seconds = total_seconds % 60;
    const int hundredths =
        (safe_frames % static_cast<int>(step_rate)) * 100 / static_cast<int>(step_rate);
    const auto padded = [](int value) {
        return std::string{value < 10 ? "0" : ""} + std::to_string(value);
    };
    return std::to_string(minutes) + ":" + padded(seconds) + "." + padded(hundredths);
}

float number_text_width(const std::string& text, float digit_size) {
    // calc mixed timer width
    float width = 0.0F;
    for (const char character : text) {
        width += character >= '0' && character <= '9' ? digit_size : digit_size * 0.6875F;
    }
    return width;
}

void draw_number_text(SDL_Renderer* renderer, SDL_Texture* numbers, SDL_Texture* font,
                      const std::string& text, float left, float top, float digit_size) {
    // draw digits with font separators
    float x = left;
    for (const char character : text) {
        if (character >= '0' && character <= '9') {
            const int digit = character - '0';
            const SDL_FRect source{
                .x = static_cast<float>(digit % 5) * 40.0F,
                .y = static_cast<float>(digit / 5) * 40.0F,
                .w = 40.0F,
                .h = 40.0F,
            };
            const SDL_FRect destination{
                .x = x,
                .y = top,
                .w = digit_size,
                .h = digit_size,
            };
            (void)SDL_RenderTexture(renderer, numbers, &source, &destination);
            x += digit_size;
            continue;
        }
        const float separator_scale = digit_size / 16.0F;
        draw_text(renderer, font, std::string(1, character), x, top, separator_scale);
        x += digit_size * 0.6875F;
    }
}

std::string primary_action_label(const State& game, ControlAction action) {
    // get first active-device bind
    std::string label = control_binding_label(game.controls, game.input.last_device, action,
                                              game.controller_layout);
    const std::size_t separator = label.find(" / ");
    if (separator != std::string::npos) {
        label.resize(separator);
    }
    if (!label.empty()) {
        return label;
    }

    // use defaults before controls init
    if (game.input.last_device == InputDevice::keyboard) {
        switch (action) {
        case ControlAction::confirm:
            return "ENTER";
        case ControlAction::back:
            return "ESC";
        case ControlAction::left:
            return "LEFT";
        case ControlAction::right:
            return "RIGHT";
        case ControlAction::remove_player:
            return "DEL";
        default:
            return "";
        }
    }
    const ControllerLayout layout =
        effective_controller_layout(game.controls, game.controller_layout);
    switch (action) {
    case ControlAction::confirm:
        return controller_label_for_layout("A", layout);
    case ControlAction::back:
        return controller_label_for_layout("B", layout);
    case ControlAction::left:
        return "LEFT";
    case ControlAction::right:
        return "RIGHT";
    case ControlAction::remove_player:
        return controller_label_for_layout("SELECT", layout);
    default:
        return "";
    }
}

void draw_instruction_navigation(SDL_Renderer* renderer, const Assets& assets, const State& game,
                                 InstructionsPage page, InstructionsPage first_page,
                                 InstructionsPage last_page, int output_width, int output_height) {
    // draw page arrows in widescreen space
    const float scale = std::clamp(static_cast<float>(output_height) / 720.0F, 0.8F, 1.6F);
    const int page_index = static_cast<int>(page);
    const float bob = ((game.frame / 18U) % 2U) == 0U ? 0.0F : 8.0F * scale;
    if (page == InstructionsPage::original && page_index < static_cast<int>(last_page)) {
        const std::string arrow = ">";
        const float x = static_cast<float>(output_width) - 78.0F * scale + bob;
        draw_text(renderer, assets.font, arrow, x, static_cast<float>(output_height) * 0.48F,
                  2.2F * scale);
        if (page == InstructionsPage::original) {
            draw_text(renderer, assets.font, "CONTROLS",
                      static_cast<float>(output_width) - 126.0F * scale,
                      static_cast<float>(output_height) * 0.55F, 0.75F * scale);
        }
    }
    if (page == InstructionsPage::original && page_index > static_cast<int>(first_page)) {
        const std::string arrow = "<";
        draw_text(renderer, assets.font, arrow, 54.0F * scale - bob,
                  static_cast<float>(output_height) * 0.48F, 2.2F * scale);
    }

    // show page keys on original art
    if (page == InstructionsPage::original) {
        const std::string hint = primary_action_label(game, ControlAction::back) + " back     <  " +
                                 primary_action_label(game, ControlAction::left) + " / " +
                                 primary_action_label(game, ControlAction::right) + " pages  >";
        const float hint_scale = 1.0F * scale;
        const float footer_bottom = static_cast<float>(output_height) - 34.0F * scale;
        draw_text(renderer, assets.font, hint,
                  static_cast<float>(output_width) * 0.5F - text_width(hint, hint_scale) * 0.5F,
                  footer_bottom - 38.0F * scale, hint_scale);
    }
}

std::vector<std::string> wrap_text(std::string_view text, std::size_t maximum_characters) {
    // wrap scripture
    std::vector<std::string> lines;
    std::string line;
    std::size_t position = 0;
    while (position < text.size()) {
        while (position < text.size() && text[position] == ' ') {
            ++position;
        }
        const std::size_t word_end = text.find(' ', position);
        const std::size_t length =
            (word_end == std::string_view::npos ? text.size() : word_end) - position;
        const std::string_view word = text.substr(position, length);
        if (!line.empty() && line.size() + word.size() + 1 > maximum_characters) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty()) {
            line.push_back(' ');
        }
        line.append(word);
        position += length;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

void draw_level_intro(SDL_Renderer* renderer, const Assets& assets, const State& game,
                      int output_width, int output_height) {
    // calc intro panel
    tile_screen(renderer, assets.dark, output_width, output_height);
    const float ui_scale = std::clamp(static_cast<float>(output_height) / 720.0F, 0.85F, 1.6F);
    const float panel_width = std::min(static_cast<float>(output_width) * 0.82F, 920.0F * ui_scale);
    const float text_scale = 1.35F * ui_scale;
    const std::size_t maximum_characters =
        static_cast<std::size_t>((panel_width - 96.0F * ui_scale) / (11.0F * text_scale));
    const std::vector<std::string> lines =
        wrap_text(level_verses[static_cast<std::size_t>(game.verse_index)], maximum_characters);
    const float line_height = 25.0F * ui_scale;
    const float panel_height = (170.0F + static_cast<float>(lines.size()) * 25.0F) * ui_scale;
    const SDL_FRect panel{
        .x = (static_cast<float>(output_width) - panel_width) * 0.5F,
        .y = (static_cast<float>(output_height) - panel_height) * 0.5F,
        .w = panel_width,
        .h = panel_height,
    };
    draw_window_panel(renderer, assets, panel, ui_scale);

    // draw level intro
    const std::string heading = "LEVEL " + std::to_string(game.current_level);
    const float heading_scale = 2.0F * ui_scale;
    draw_text(renderer, assets.font, heading,
              panel.x + panel.w * 0.5F - text_width(heading, heading_scale) * 0.5F,
              panel.y + 28.0F * ui_scale, heading_scale);

    float y = panel.y + 88.0F * ui_scale;
    for (const std::string& line : lines) {
        draw_text(renderer, assets.font, line,
                  panel.x + panel.w * 0.5F - text_width(line, text_scale) * 0.5F, y, text_scale);
        y += line_height;
    }
    const std::string prompt = primary_action_label(game, ControlAction::back) + " back     " +
                               primary_action_label(game, ControlAction::confirm) + " begin";
    draw_text(renderer, assets.font, prompt,
              panel.x + panel.w * 0.5F - text_width(prompt, text_scale) * 0.5F,
              panel.y + panel.h - 45.0F * ui_scale, text_scale);
}

const char* rank_for_score(int score) {
    if (score <= 10'000)
        return "Goober";
    if (score <= 30'000)
        return "Lettuce Head";
    if (score <= 50'000)
        return "Chicken";
    if (score <= 70'000)
        return "Cadet";
    if (score <= 90'000)
        return "Walnut";
    if (score <= 110'000)
        return "Flight Man";
    if (score <= 130'000)
        return "Potato";
    if (score <= 150'000)
        return "Captain";
    if (score <= 170'000)
        return "SuperStud";
    if (score <= 190'000)
        return "General";
    if (score <= 210'000)
        return "Super Chicken";
    return "Child of Light";
}

void draw_layout_text(SDL_Renderer* renderer, SDL_Texture* texture, const std::string& text,
                      float x, float y, float scale, float layout_left) {
    draw_text(renderer, texture, text, layout_left + x * scale, y * scale, scale);
}

void draw_layout_centered_text(SDL_Renderer* renderer, SDL_Texture* texture,
                               const std::string& text, float y, float scale, float layout_left) {
    // center text in original layout
    draw_text(renderer, texture, text,
              layout_left + 320.0F * scale - text_width(text, scale) * 0.5F, y * scale, scale);
}

void draw_player_select(SDL_Renderer* renderer, const Assets& assets, const State& game,
                        int output_width, int output_height) {
    draw_frontend_with_stars(renderer, assets, game, assets.character, output_width, output_height);
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float layout_left =
        (static_cast<float>(output_width) - static_cast<float>(original_layout_width) * scale) *
        0.5F;

    // draw player slots
    draw_selector(renderer, assets.selector, game.mode_frame, 82.0F,
                  130.0F + static_cast<float>(game.selected_player) * 22.0F, scale, layout_left);
    for (int slot = 0; slot < player_slot_count; ++slot) {
        const PlayerSave& player = game.save.players[static_cast<std::size_t>(slot)];
        const float y = 142.0F + static_cast<float>(slot) * 22.0F;
        draw_layout_text(renderer, assets.font, player.active != 0 ? player.name : "", 125.0F, y,
                         scale, layout_left);
    }

    // draw player details
    const PlayerSave& player = game.save.players[static_cast<std::size_t>(game.selected_player)];
    if (player.active != 0) {
        draw_layout_text(renderer, assets.font, rank_for_score(player.score), 350.0F, 143.0F, scale,
                         layout_left);
        draw_layout_text(renderer, assets.font, std::to_string(player.score), 350.0F, 220.0F, scale,
                         layout_left);
        draw_layout_text(renderer, assets.font, std::to_string(std::max(1, player.level)), 350.0F,
                         301.0F, scale, layout_left);
        draw_layout_text(renderer, assets.font, std::to_string(player.chickens), 350.0F, 381.0F,
                         scale, layout_left);
        if ((game.mode_frame / 2U) % 6U >= 3U) {
            draw_layout_text(
                renderer, assets.font, "|", 125.0F + static_cast<float>(player.name.size()) * 11.0F,
                141.0F + static_cast<float>(game.selected_player) * 22.0F, scale, layout_left);
        }
    }

    // draw player footer
    switch (game.player_select_mode) {
    case PlayerSelectMode::choose_player:
        if (player.active != 0) {
            const std::string prompt =
                primary_action_label(game, ControlAction::back) + " back  " +
                primary_action_label(game, ControlAction::remove_player) + " delete  " +
                primary_action_label(game, ControlAction::confirm) + " choose";
            draw_layout_centered_text(renderer, assets.font, prompt, 455.0F, scale, layout_left);
        } else {
            const std::string prompt =
                game.input.last_device == InputDevice::controller
                    ? primary_action_label(game, ControlAction::back) + " back  " +
                          primary_action_label(game, ControlAction::confirm) + " enter name"
                    : primary_action_label(game, ControlAction::back) +
                          " back  Type a name to create new player";
            draw_layout_centered_text(renderer, assets.font, prompt, 455.0F, scale, layout_left);
        }
        break;
    case PlayerSelectMode::confirm_delete: {
        const SDL_FRect panel = layout_rect({92, 406, 456, 70}, output_width, output_height);
        draw_window_panel(renderer, assets, panel, scale);
        draw_layout_text(renderer, assets.font, "Delete " + player.name + "?", 116.0F, 416.0F,
                         scale, layout_left);
        const std::string prompt = primary_action_label(game, ControlAction::back) + " cancel  " +
                                   primary_action_label(game, ControlAction::confirm) + " delete";
        draw_layout_centered_text(renderer, assets.font, prompt, 446.0F, scale, layout_left);
        break;
    }
    case PlayerSelectMode::controller_keyboard: {
        // draw controller keyboard
        const SDL_FRect panel = layout_rect({48, 202, 544, 258}, output_width, output_height);
        draw_window_panel(renderer, assets, panel, scale);
        draw_layout_text(renderer, assets.font, "NAME: " + player.name, 72.0F, 218.0F, scale,
                         layout_left);
        for (int index = 0; index < 30; ++index) {
            std::string label;
            constexpr std::string_view letters = "QWERTYUIOPASDFGHJKLZXCVBNM";
            const int letter = index <= 18 ? index : (index >= 20 && index <= 26 ? index - 1 : -1);
            if (letter >= 0) {
                char character = letters[static_cast<std::size_t>(letter)];
                if (!game.controller_keyboard_uppercase) {
                    character = static_cast<char>(character - 'A' + 'a');
                }
                label.push_back(character);
            } else {
                label = index == 19 ? "<" : (index == 27 ? "SPC" : (index == 28 ? "Aa" : "OK"));
            }
            if (index == game.controller_keyboard_cursor) {
                label = "[" + label + "]";
            }
            const float x = 64.0F + static_cast<float>(index % 10) * 52.0F;
            const float y = 278.0F + static_cast<float>(index / 10) * 52.0F;
            draw_layout_text(renderer, assets.font, label, x, y, scale, layout_left);
        }
        const std::string prompt = primary_action_label(game, ControlAction::back) + " back  " +
                                   primary_action_label(game, ControlAction::confirm) + " select";
        draw_layout_centered_text(renderer, assets.font, prompt, 435.0F, scale, layout_left);
        break;
    }
    }
}

const char* yes_no(bool value) {
    return value ? "yes" : "no";
}

void draw_options(SDL_Renderer* renderer, const Assets& assets, const State& game, int output_width,
                  int output_height) {
    draw_frontend_screen(renderer, assets.options, output_width, output_height);
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float layout_left =
        (static_cast<float>(output_width) - static_cast<float>(original_layout_width) * scale) *
        0.5F;

    // set option labels
    constexpr std::array<const char*, 6> labels{{
        "Master Volume:",
        "Difficulty:",
        "Sound:",
        "Music:",
        "Use Joystick:",
        "Always Run:",
    }};
    constexpr std::array<const char*, 5> difficulty_names{{
        "Simple",
        "Nonchalant",
        "Typical",
        "Arduous",
        "Grueling",
    }};
    const std::array<std::string, 6> values{{
        std::to_string(game.master_volume) + "%",
        difficulty_names[static_cast<std::size_t>(std::clamp(game.save.difficulty, 0, 4))],
        yes_no(game.save.sound),
        yes_no(game.save.music),
        yes_no(game.save.use_joystick),
        yes_no(game.save.always_run),
    }};

    // draw option rows
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const float y = 143.0F + static_cast<float>(index) * 48.0F;
        const SDL_FRect value_panel =
            layout_rect({354.0F, y - 8.0F, 220.0F, 30.0F}, output_width, output_height);
        draw_window_panel(renderer, assets, value_panel, scale);
        for (int side = 0; side < 2; ++side) {
            const SDL_FRect source{
                .x = static_cast<float>(side * 4),
                .y = 0.0F,
                .w = 4.0F,
                .h = 24.0F,
            };
            const SDL_FRect destination{
                .x = side == 0 ? value_panel.x : value_panel.x + value_panel.w - 4.0F * scale,
                .y = value_panel.y + 3.0F * scale,
                .w = 4.0F * scale,
                .h = 24.0F * scale,
            };
            (void)SDL_RenderTexture(renderer, assets.window_slide, &source, &destination);
        }
        if (index == static_cast<std::size_t>(game.options_choice)) {
            draw_layout_text(renderer, assets.font, ">", 151.0F, y, scale, layout_left);
        }
        draw_layout_text(renderer, assets.font, labels[index], 178.0F, y, scale, layout_left);
        draw_layout_text(renderer, assets.font, values[index], 370.0F, y, scale, layout_left);
    }
    const std::string prompt = primary_action_label(game, ControlAction::back) + " back     " +
                               primary_action_label(game, ControlAction::left) + " / " +
                               primary_action_label(game, ControlAction::right) + " change";
    draw_layout_centered_text(renderer, assets.font, prompt, 446.0F, scale, layout_left);
}

void set_radar_cross(std::array<std::uint8_t, level_cell_count>& pixels, int x, int y,
                     std::uint8_t color) {
    constexpr std::array<IVec2, 4> offsets{{
        {.x = 0, .y = -1},
        {.x = -1, .y = 0},
        {.x = 0, .y = 1},
        {.x = 1, .y = 0},
    }};
    for (const IVec2& offset : offsets) {
        const int next_x = x + offset.x;
        const int next_y = y + offset.y;
        if (next_x >= 0 && next_x < level_width && next_y >= 0 && next_y < level_height) {
            pixels[static_cast<std::size_t>(next_y * level_width + next_x)] = color;
        }
    }
}

void draw_hud(SDL_Renderer* renderer, const Assets& assets, const State& game, int output_width,
              int output_height) {
    const float ui_scale = static_cast<float>(output_height) / 720.0F;
    const float margin = 18.0F * ui_scale;
    const float label_scale = 1.15F * ui_scale;
    const float digit_size = 30.0F * ui_scale;
    const float label_gap = 8.0F * ui_scale;

    // calc missing-chicken hint
    constexpr std::uint64_t chicken_hint_frames = 42;
    const std::uint64_t chicken_hint_elapsed = game.frame - game.chicken_hint_started_frame;
    const bool chicken_hint =
        game.chicken_hint_active && chicken_hint_elapsed < chicken_hint_frames;
    const float chicken_hint_progress = chicken_hint ? static_cast<float>(chicken_hint_elapsed) /
                                                           static_cast<float>(chicken_hint_frames)
                                                     : 1.0F;
    const float chicken_pulse = chicken_hint ? std::sin(chicken_hint_progress * 3.14159265F) : 0.0F;
    const float chicken_scale = 1.0F + chicken_pulse * 0.14F;
    const float chicken_wiggle = chicken_hint
                                     ? std::sin(static_cast<float>(chicken_hint_elapsed) * 1.7F) *
                                           (1.0F - chicken_hint_progress) * 5.0F * ui_scale
                                     : 0.0F;

    // draw corner counters at score size
    const int chicken_frame = static_cast<int>((game.frame / 8U) % 8U);
    const float counter_gap = 6.0F * ui_scale;
    const float counter_icon_size = digit_size;
    const float slash_width = text_width("/", label_scale);
    const auto number_width = [&](int value) {
        return static_cast<float>(std::to_string(std::max(0, value)).size()) * digit_size;
    };
    const auto counter_width = [&](int current, int total) {
        return counter_icon_size + label_gap + number_width(current) + counter_gap + slash_width +
               counter_gap + number_width(total);
    };
    const auto draw_counter = [&](bool right_aligned, SDL_Texture* icon, int icon_frame,
                                  int current, int total, float counter_scale, float wiggle) {
        const float width = counter_width(current, total);
        const float left =
            right_aligned ? static_cast<float>(output_width) - margin - width : margin;
        const float top = margin;
        const float center_x = left + width * 0.5F + wiggle;
        const float center_y = top + digit_size * 0.5F;
        const auto x = [&](float position) {
            return center_x + (position - (left + width * 0.5F)) * counter_scale;
        };
        const auto y = [&](float position) {
            return center_y + (position - (top + digit_size * 0.5F)) * counter_scale;
        };
        const SDL_FRect source{
            .x = static_cast<float>(icon_frame * 16),
            .y = 0.0F,
            .w = 16.0F,
            .h = 16.0F,
        };
        float cursor = left;
        const SDL_FRect destination{
            .x = x(cursor),
            .y = y(top),
            .w = counter_icon_size * counter_scale,
            .h = counter_icon_size * counter_scale,
        };
        (void)SDL_RenderTexture(renderer, icon, &source, &destination);

        cursor += counter_icon_size + label_gap;
        draw_number(renderer, assets.number_font, current, x(cursor + number_width(current)),
                    y(top), digit_size * counter_scale);
        cursor += number_width(current) + counter_gap;
        draw_text(renderer, assets.font, "/", x(cursor),
                  y(top + (digit_size - 16.0F * label_scale) * 0.5F), label_scale * counter_scale);
        cursor += slash_width + counter_gap;
        draw_number(renderer, assets.number_font, total, x(cursor + number_width(total)), y(top),
                    digit_size * counter_scale);
    };
    const float chicken_width = counter_width(game.caught_chickens, game.total_chickens);
    const float alien_width = counter_width(game.aliens_shorted, game.total_aliens);
    draw_counter(false, assets.chicken, chicken_frame, game.caught_chickens, game.total_chickens,
                 chicken_scale, chicken_wiggle);
    draw_counter(true, assets.aliens, 0, game.aliens_shorted, game.total_aliens, 1.0F, 0.0F);

    // fit score and time between counters
    const float middle_left = margin + chicken_width + 24.0F * ui_scale;
    const float middle_right =
        static_cast<float>(output_width) - margin - alien_width - 24.0F * ui_scale;
    const float middle_width = std::max(0.0F, middle_right - middle_left);
    const float score_center = middle_left + middle_width * 0.28F;
    const float time_center = middle_left + middle_width * 0.72F;
    const auto draw_centered_value = [&](const std::string& label, int value, float center,
                                         float top) {
        const float label_width = text_width(label, label_scale);
        const float value_width =
            static_cast<float>(std::to_string(std::max(0, value)).size()) * digit_size;
        const float left = center - (label_width + label_gap + value_width) * 0.5F;
        draw_text(renderer, assets.font, label, left,
                  top + (digit_size - 16.0F * label_scale) * 0.5F, label_scale);
        draw_number(renderer, assets.number_font, value,
                    left + label_width + label_gap + value_width, top, digit_size);
    };
    draw_centered_value("SCORE", game.score, score_center, margin);

    // draw live run time
    const std::string time = format_level_time(game.level_elapsed_frames);
    const float time_label_width = text_width("TIME", label_scale);
    const float time_width = number_text_width(time, digit_size);
    const float time_left = time_center - (time_label_width + label_gap + time_width) * 0.5F;
    draw_text(renderer, assets.font, "TIME", time_left,
              margin + (digit_size - 16.0F * label_scale) * 0.5F, label_scale);
    draw_number_text(renderer, assets.number_font, assets.font, time,
                     time_left + time_label_width + label_gap, margin, digit_size);

    // draw TNT timer below run time
    if (game.bomb_seconds >= 0) {
        draw_centered_value("TNT", game.bomb_seconds, time_center,
                            margin + digit_size + 8.0F * ui_scale);
    }

    // draw level in lower left
    const std::string level_label = "LEVEL";
    const float level_digit_size = 34.0F * ui_scale;
    const float level_top = static_cast<float>(output_height) - margin - level_digit_size;
    const float level_label_width = text_width(level_label, label_scale);
    const float level_number_width =
        static_cast<float>(std::to_string(std::max(0, game.current_level)).size()) *
        level_digit_size;
    draw_text(renderer, assets.font, level_label, margin,
              level_top + (level_digit_size - 16.0F * label_scale) * 0.5F, label_scale);
    draw_number(renderer, assets.number_font, game.current_level,
                margin + level_label_width + label_gap + level_number_width, level_top,
                level_digit_size);

    // draw fuel gauge below alien counter
    const int fuel = std::clamp(game.display_fuel, 0, 300);
    const float fuel_scale = ui_scale;
    const SDL_FRect fuel_gauge{
        .x = static_cast<float>(output_width) - margin - 20.0F * fuel_scale,
        .y = margin + digit_size + 12.0F * ui_scale,
        .w = 20.0F * fuel_scale,
        .h = 300.0F * fuel_scale,
    };
    if (fuel > 0) {
        const int fuel_column = (fuel - 1) / 100 + 1;
        const SDL_FRect fuel_source{
            .x = static_cast<float>(fuel_column * 20),
            .y = static_cast<float>(300 - fuel),
            .w = 20.0F,
            .h = static_cast<float>(fuel),
        };
        const SDL_FRect fuel_destination{
            .x = fuel_gauge.x,
            .y = fuel_gauge.y + static_cast<float>(300 - fuel) * fuel_scale,
            .w = fuel_gauge.w,
            .h = static_cast<float>(fuel) * fuel_scale,
        };
        (void)SDL_RenderTexture(renderer, assets.fuel, &fuel_source, &fuel_destination);
    }
    const SDL_FRect empty_fuel_source{.x = 0.0F, .y = 0.0F, .w = 20.0F, .h = 300.0F};
    (void)SDL_RenderTexture(renderer, assets.fuel, &empty_fuel_source, &fuel_gauge);

    // draw radar
    if (game.radar_visible) {
        // fit largest integer scale between HUD edges
        constexpr float radar_width = 242.0F;
        constexpr float radar_height = 134.0F;
        const float horizontal_margin = 64.0F * ui_scale;
        const float vertical_margin = 72.0F * ui_scale;
        const int horizontal_scale = static_cast<int>(std::floor(
            (static_cast<float>(output_width) - horizontal_margin * 2.0F) / radar_width));
        const int vertical_scale = static_cast<int>(std::floor(
            (static_cast<float>(output_height) - vertical_margin * 2.0F) / radar_height));
        const float radar_scale =
            static_cast<float>(std::max(1, std::min(horizontal_scale, vertical_scale)));
        const float scaled_width = radar_width * radar_scale;
        const float scaled_height = radar_height * radar_scale;
        const SDL_FRect radar{
            .x = std::floor((static_cast<float>(output_width) - scaled_width) * 0.5F),
            .y = std::floor((static_cast<float>(output_height) - scaled_height) * 0.5F),
            .w = scaled_width,
            .h = scaled_height,
        };
        (void)SDL_RenderTexture(renderer, assets.radar, nullptr, &radar);

        const SDL_FRect radar_interior{
            .x = radar.x + radar_scale,
            .y = radar.y + radar_scale,
            .w = static_cast<float>(level_width) * radar_scale,
            .h = static_cast<float>(level_height) * radar_scale,
        };
        fill(renderer, radar_interior, 16, 32, 16);

        constexpr std::array<std::array<std::uint8_t, 3>, 9> radar_palette{{
            {152, 152, 152},
            {0, 96, 0},
            {0, 252, 0},
            {96, 96, 0},
            {252, 252, 0},
            {96, 0, 96},
            {252, 0, 252},
            {104, 80, 52},
            {252, 192, 128},
        }};
        constexpr std::array<std::uint8_t, 9> radar_codes{
            0x09, 0x25, 0x2f, 0x45, 0x4f, 0x55, 0x5f, 0xa5, 0xaf,
        };

        // group radar cells
        std::array<std::uint8_t, level_cell_count> radar_pixels{};
        radar_pixels.fill(0xd0);
        const int player_x = static_cast<int>(game.player_x) / tile_size;
        const int player_y = static_cast<int>(game.player_y) / tile_size;
        const bool pulse_visible = game.radar_pulse < 4;
        for (int y = 0; y < level_height; ++y) {
            for (int x = 0; x < level_width; ++x) {
                const std::size_t cell = static_cast<std::size_t>(y * level_width + x);
                const int tile = game.active_tiles[cell];
                if (tile == 1 || tile == 17) {
                    radar_pixels[cell] = 0x09;
                }
                if (tile == 9 || tile == 10) {
                    radar_pixels[cell] = 0x4f;
                    if (pulse_visible) {
                        set_radar_cross(radar_pixels, x, y, 0x45);
                    }
                }
                if (tile == -3) {
                    radar_pixels[cell] = 0x2f;
                    if (pulse_visible) {
                        set_radar_cross(radar_pixels, x, y, 0x25);
                    }
                }
                if (tile == -2) {
                    radar_pixels[cell] = 0x5f;
                    if (pulse_visible) {
                        set_radar_cross(radar_pixels, x, y, 0x55);
                    }
                }
                if (x == player_x && y == player_y) {
                    radar_pixels[cell] = 0xaf;
                    if (pulse_visible) {
                        set_radar_cross(radar_pixels, x, y, 0xa5);
                    }
                }
            }
        }

        // draw radar groups
        std::array<std::vector<SDL_FRect>, radar_codes.size()> radar_rectangles;
        for (int y = 0; y < level_height; ++y) {
            for (int x = 0; x < level_width; ++x) {
                const std::uint8_t code =
                    radar_pixels[static_cast<std::size_t>(y * level_width + x)];
                const auto found = std::find(radar_codes.begin(), radar_codes.end(), code);
                if (found == radar_codes.end()) {
                    continue;
                }
                const std::size_t palette_index =
                    static_cast<std::size_t>(std::distance(radar_codes.begin(), found));
                radar_rectangles[palette_index].push_back({
                    .x = radar_interior.x + static_cast<float>(x) * radar_scale,
                    .y = radar_interior.y + static_cast<float>(y) * radar_scale,
                    .w = radar_scale,
                    .h = radar_scale,
                });
            }
        }
        for (std::size_t index = 0; index < radar_palette.size(); ++index) {
            const auto& color = radar_palette[index];
            (void)SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], 255);
            const std::vector<SDL_FRect>& rectangles = radar_rectangles[index];
            (void)SDL_RenderFillRects(renderer, rectangles.data(),
                                      static_cast<int>(rectangles.size()));
        }
    }
}

float presentation_position(float previous, float current, float alpha) {
    return std::lerp(previous, current, alpha);
}

void draw_gameplay(SDL_Renderer* renderer, const Assets& assets, const State& game,
                   int output_width, int output_height, float presentation_alpha) {
    if (!game.levels_loaded || game.current_level < 1 ||
        game.current_level > static_cast<int>(game.levels.size())) {
        return;
    }

    // calc presentation positions
    const float alpha = std::clamp(presentation_alpha, 0.0F, 1.0F);
    const Camera camera{
        .x = presentation_position(game.previous_camera.x, game.presented_camera.x, alpha),
        .y = presentation_position(game.previous_camera.y, game.presented_camera.y, alpha),
        .zoom = game.camera.zoom,
    };
    const float player_x =
        presentation_position(game.previous_player_x, game.presented_player_x, alpha);
    const float player_y =
        presentation_position(game.previous_player_y, game.presented_player_y, alpha);

    // calc visible tiles
    constexpr float original_star_parallax = 1.0F / 16.0F;
    draw_stars(renderer, assets.stars, game, camera.x * original_star_parallax,
               camera.y * original_star_parallax, output_width, output_height);
    const Level& level = game.levels[static_cast<std::size_t>(game.current_level - 1)];
    const float scale = static_cast<float>(output_height) /
                        static_cast<float>(original_layout_height) * camera.zoom;
    const float half_width = static_cast<float>(output_width) / (2.0F * scale);
    const float half_height = static_cast<float>(output_height) / (2.0F * scale);
    const int first_x = std::clamp(
        static_cast<int>(std::floor((camera.x - half_width) / 16.0F)) - 1, 0, level_width - 1);
    const int last_x = std::clamp(static_cast<int>(std::ceil((camera.x + half_width) / 16.0F)) + 1,
                                  0, level_width - 1);
    const int first_y = std::clamp(
        static_cast<int>(std::floor((camera.y - half_height) / 16.0F)) - 1, 0, level_height - 1);
    const int last_y = std::clamp(static_cast<int>(std::ceil((camera.y + half_height) / 16.0F)) + 1,
                                  0, level_height - 1);

    // draw tube underlays
    for (int tile_y = first_y; tile_y <= last_y; ++tile_y) {
        for (int tile_x = first_x; tile_x <= last_x; ++tile_x) {
            const std::size_t index = static_cast<std::size_t>(tile_y * level_width + tile_x);
            const int tube_underlay = level.tube_underlay[index];
            if (tube_underlay != 0) {
                const int player_tile_x = static_cast<int>(game.player_x) / tile_size;
                const int player_tile_y = static_cast<int>(game.player_y) / tile_size;
                const int occupied_offset =
                    tile_x == player_tile_x && tile_y == player_tile_y ? tile_size : 0;
                draw_world_cell(renderer, assets.tubes, tube_underlay * 32 + occupied_offset, 32,
                                static_cast<float>(tile_x * tile_size),
                                static_cast<float>(tile_y * tile_size), camera, scale, output_width,
                                output_height);
            }
            draw_terrain_cell(renderer, assets, game.active_tiles[index], tile_x, tile_y, game,
                              camera, scale, output_width, output_height);
        }
    }

    // draw world objects
    for (const ChickenState& chicken : game.chickens) {
        if (!chicken.active) {
            continue;
        }
        draw_actor(renderer, assets.chicken, chicken.frame, {chicken.x, chicken.y}, camera, scale,
                   output_width, output_height);
    }
    for (const AlienState& alien : game.aliens) {
        if (!alien.active) {
            continue;
        }
        draw_actor(renderer, assets.aliens, alien.frame, {alien.x, alien.y}, camera, scale,
                   output_width, output_height);
    }
    for (std::size_t index = 0; index < level.warps.size(); ++index) {
        const Warp& warp = level.warps[index];
        const int warp_frame = index < game.warp_frames.size() ? game.warp_frames[index] / 2 : 0;
        draw_actor(renderer, assets.warp_tile, warp_frame, warp.position, camera, scale,
                   output_width, output_height);
    }
    const int exit_frame =
        game.exit_open ? std::min(6, 1 + static_cast<int>((game.frame - game.exit_opened_frame) /
                                                          exit_step_frames))
                       : 0;
    draw_actor(renderer, assets.exit, exit_frame, level.exit, camera, scale, output_width,
               output_height);

    // draw ship
    const SDL_FRect ship_destination = world_destination(
        player_x - 8.0F, player_y - 8.0F, 16.0F, 16.0F, camera, scale, output_width, output_height);
    const std::uint64_t capture_elapsed = game.frame - game.capture_effect_started_frame;
    const bool drawing_capture = game.capture_effect_direction != 0 &&
                                 capture_elapsed < capture_animation_frames &&
                                 game.playing_mode != PlayingMode::exploding;
    if (drawing_capture) {
        const int capture_frame = static_cast<int>(capture_elapsed / capture_step_frames);
        const int capture_row = game.capture_effect_direction - 1;
        const auto draw_capture_cell = [&](int frame, float offset_x, float offset_y) {
            const SDL_FRect source{
                .x = static_cast<float>(frame * 16),
                .y = static_cast<float>(capture_row * 16),
                .w = 16.0F,
                .h = 16.0F,
            };
            SDL_FRect destination = ship_destination;
            destination.x += offset_x * scale;
            destination.y += offset_y * scale;
            (void)SDL_RenderTexture(renderer, assets.chickens, &source, &destination);
        };
        if (capture_frame < 6) {
            draw_capture_cell(capture_frame * 2 + 1, 0.0F, 0.0F);
            constexpr std::array<IVec2, 4> offsets{{
                {0, -1},
                {1, 0},
                {0, 1},
                {-1, 0},
            }};
            const IVec2 offset = offsets[static_cast<std::size_t>(capture_row)];
            draw_capture_cell(capture_frame * 2, static_cast<float>(offset.x * 16),
                              static_cast<float>(offset.y * 16));
        } else {
            draw_capture_cell(capture_frame + 6, 0.0F, 0.0F);
        }
    } else if (game.tube_direction == 0 || game.playing_mode == PlayingMode::exploding) {
        const int ship_frame =
            game.playing_mode == PlayingMode::exploding
                ? std::min(13, 5 + static_cast<int>(game.mode_frame / death_step_frames))
                : game.player_direction;
        const SDL_FRect ship_source{
            .x = static_cast<float>(ship_frame * 16),
            .y = 0.0F,
            .w = 16.0F,
            .h = 16.0F,
        };
        (void)SDL_RenderTexture(renderer, assets.ship, &ship_source, &ship_destination);
    }

    // draw HUD and overlays
    draw_hud(renderer, assets, game, output_width, output_height);

    if (game.playing_mode == PlayingMode::level_start) {
        draw_level_intro(renderer, assets, game, output_width, output_height);
    }
    if (game.playing_mode == PlayingMode::paused ||
        game.playing_mode == PlayingMode::confirm_exit) {
        tile_screen(renderer, assets.dark, output_width, output_height);
        const float ui_scale = std::clamp(static_cast<float>(output_height) / 720.0F, 0.85F, 1.6F);
        if (game.playing_mode == PlayingMode::confirm_exit) {
            const SDL_FRect panel{
                .x = static_cast<float>(output_width) * 0.5F - 300.0F * ui_scale,
                .y = static_cast<float>(output_height) * 0.5F - 80.0F * ui_scale,
                .w = 600.0F * ui_scale,
                .h = 160.0F * ui_scale,
            };
            draw_window_panel(renderer, assets, panel, ui_scale);
            const std::string question =
                game.confirm_exit_to_title ? "Quit to main menu?" : "You wish to exit this level?";
            const std::string prompt =
                primary_action_label(game, ControlAction::back) + " cancel     " +
                primary_action_label(game, ControlAction::confirm) + " accept";
            const float text_scale = 1.25F * ui_scale;
            draw_text(renderer, assets.font, question,
                      panel.x + panel.w * 0.5F - text_width(question, text_scale) * 0.5F,
                      panel.y + 30.0F * ui_scale, text_scale);
            draw_text(renderer, assets.font, prompt,
                      panel.x + panel.w * 0.5F - text_width(prompt, text_scale) * 0.5F,
                      panel.y + 100.0F * ui_scale, text_scale);
            return;
        }

        // draw instruction pages inside pause
        if (game.pause_mode == PauseMode::how_to_play) {
            shade_screen(renderer, output_width, output_height, 128);
            draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
                       static_cast<float>(game.star_scroll.y), output_width, output_height);
            draw_instructions_screen(renderer, assets.instructions, output_width, output_height);
            const std::string hint = primary_action_label(game, ControlAction::back) + " back";
            const float hint_scale = 0.72F * ui_scale;
            draw_text(renderer, assets.font, hint,
                      static_cast<float>(output_width) * 0.5F - text_width(hint, hint_scale) * 0.5F,
                      static_cast<float>(output_height) - 25.0F * ui_scale, hint_scale);
            return;
        }
        if (game.pause_mode == PauseMode::controls) {
            draw_controls_screen(renderer, assets, game,
                                 game.pause_page == InstructionsPage::controller
                                     ? InputDevice::controller
                                     : InputDevice::keyboard,
                                 output_width, output_height);
            draw_instruction_navigation(renderer, assets, game, game.pause_page,
                                        InstructionsPage::keyboard, InstructionsPage::controller,
                                        output_width, output_height);
            return;
        }

        // draw pause menu
        const SDL_FRect panel{
            .x = static_cast<float>(output_width) * 0.5F - 270.0F * ui_scale,
            .y = static_cast<float>(output_height) * 0.5F - 220.0F * ui_scale,
            .w = 540.0F * ui_scale,
            .h = 440.0F * ui_scale,
        };
        draw_window_panel(renderer, assets, panel, ui_scale);
        const float paused_scale = 2.0F * ui_scale;
        const SDL_FRect paused_source{
            .x = 0.0F,
            .y = 0.0F,
            .w = 127.0F,
            .h = 36.0F,
        };
        const SDL_FRect paused{
            .x = static_cast<float>(output_width) * 0.5F - 127.0F * paused_scale * 0.5F,
            .y = panel.y + 38.0F * ui_scale,
            .w = 127.0F * paused_scale,
            .h = 36.0F * paused_scale,
        };
        (void)SDL_RenderTexture(renderer, assets.paused, &paused_source, &paused);
        constexpr std::array<const char*, 4> pause_items{
            "RESUME",
            "HOW TO PLAY",
            "CONTROLS",
            "QUIT TO MAIN MENU",
        };
        const float menu_scale = 1.35F * ui_scale;
        for (std::size_t index = 0; index < pause_items.size(); ++index) {
            std::string label =
                static_cast<int>(index) == static_cast<int>(game.pause_choice) ? "> " : "  ";
            label += pause_items[index];
            draw_text(renderer, assets.font, label, panel.x + 105.0F * ui_scale,
                      panel.y + (155.0F + static_cast<float>(index) * 58.0F) * ui_scale,
                      menu_scale);
        }
        const std::string hint = primary_action_label(game, ControlAction::back) + " resume     " +
                                 primary_action_label(game, ControlAction::confirm) + " choose";
        const float hint_scale = 0.9F * ui_scale;
        draw_text(renderer, assets.font, hint,
                  panel.x + panel.w * 0.5F - text_width(hint, hint_scale) * 0.5F,
                  panel.y + panel.h - 42.0F * ui_scale, hint_scale);
    }
}

void draw_results(SDL_Renderer* renderer, const Assets& assets, const State& game, int output_width,
                  int output_height) {
    // draw stars behind transparent explosion
    draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
               static_cast<float>(game.star_scroll.y), output_width, output_height);
    draw_frontend_screen(renderer, assets.explosion, output_width, output_height);
    const float scale =
        static_cast<float>(output_height) / static_cast<float>(original_layout_height);
    const float layout_left =
        (static_cast<float>(output_width) - static_cast<float>(original_layout_width) * scale) *
        0.5F;
    const auto draw_separator = [&](float y) {
        constexpr float separator_width = 400.0F;
        const SDL_FRect destination{
            .x = layout_left + 120.0F * scale,
            .y = y * scale,
            .w = separator_width * scale,
            .h = 4.0F * scale,
        };
        (void)SDL_RenderTexture(renderer, assets.separator, nullptr, &destination);
    };
    const auto draw_centered = [&](const std::string& text, float y, float text_scale) {
        const float center = static_cast<float>(output_width) * 0.5F;
        draw_text(renderer, assets.font, text, center - text_width(text, text_scale) * 0.5F,
                  y * scale, text_scale);
    };
    const auto draw_icon_row = [&](SDL_Texture* texture, SDL_FRect source, const std::string& text,
                                   float y) {
        const float text_scale = 1.08F * scale;
        const float width = text_width(text, text_scale);
        const float text_x = static_cast<float>(output_width) * 0.5F - width * 0.5F;
        constexpr float icon_size = 22.0F;
        constexpr float icon_gap = 10.0F;
        const SDL_FRect left_icon{
            .x = text_x - (icon_size + icon_gap) * scale,
            .y = (y - 2.0F) * scale,
            .w = icon_size * scale,
            .h = icon_size * scale,
        };
        SDL_FRect right_icon = left_icon;
        right_icon.x = text_x + width + icon_gap * scale;
        (void)SDL_RenderTexture(renderer, texture, &source, &left_icon);
        (void)SDL_RenderTexture(renderer, texture, &source, &right_icon);
        draw_text(renderer, assets.font, text, text_x, y * scale, text_scale);
    };

    // reveal result rows
    const bool complete = game.results_mode == ResultsMode::level_complete;
    const int animation_frame = complete ? game.result_animation_frame : result_complete_frame;
    const int revealed = revealed_result_groups(animation_frame);
    if (!complete || revealed >= 1) {
        draw_separator(86.0F);
        draw_separator(126.0F);
        draw_centered(complete ? "LEVEL COMPLETE" : "LEVEL INCOMPLETE", 98.0F, 1.45F * scale);
    }
    if (!complete) {
        draw_centered(primary_action_label(game, ControlAction::confirm) + " retry", 420.0F,
                      0.9F * scale);
        return;
    }

    const int chicken_count =
        result_tally_value(game.caught_chickens, animation_frame, result_chicken_frame,
                           result_count_tally_frames(game.caught_chickens));
    const int alien_count =
        result_tally_value(game.aliens_shorted, animation_frame, result_alien_frame,
                           result_count_tally_frames(game.aliens_shorted));
    const int score = result_tally_value(game.score, animation_frame, result_score_frame);
    if (revealed >= 2) {
        const int frame = static_cast<int>((game.frame / 8U) % 8U);
        draw_icon_row(assets.chicken,
                      {.x = static_cast<float>(frame * 16), .y = 0, .w = 16, .h = 16},
                      "Chickens Caught: " + std::to_string(chicken_count), 164.0F);
    }
    if (revealed >= 3) {
        draw_icon_row(assets.aliens, {.x = 0, .y = 0, .w = 16, .h = 16},
                      "Aliens Shorted: " + std::to_string(alien_count) + "/" +
                          std::to_string(game.total_aliens),
                      208.0F);
    }
    if (revealed >= 4) {
        const int frame = static_cast<int>((game.frame / 8U) % 7U);
        draw_icon_row(assets.crosses,
                      {.x = static_cast<float>(frame * 16), .y = 0, .w = 16, .h = 16},
                      "Total Score: " + std::to_string(score), 252.0F);
    }
    if (revealed >= 5) {
        draw_centered(std::string{"Current Rank: "} + rank_for_score(game.score), 290.0F,
                      1.08F * scale);
    }
    if (revealed >= 6) {
        const int best = game.best_level_frames[static_cast<std::size_t>(game.current_level - 1)];
        draw_centered("Time: " + format_level_time(game.completed_level_frames) +
                          "   Best: " + format_level_time(best),
                      326.0F, 1.08F * scale);
    }
    if (revealed >= 7 && game.new_best_time) {
        draw_centered("NEW RECORD!", 358.0F, 1.24F * scale);
    }
    if (revealed >= 8) {
        draw_separator(390.0F);
        const PlayerSave& player =
            game.save.players[static_cast<std::size_t>(game.selected_player)];
        draw_centered("Go " + player.name + "!", 400.0F, 1.20F * scale);
    }
    if (animation_frame >= result_complete_frame) {
        draw_centered(primary_action_label(game, ControlAction::confirm) + " continue", 446.0F,
                      0.9F * scale);
    } else if (revealed >= 1 && !game.result_accelerated) {
        draw_centered(primary_action_label(game, ControlAction::confirm) + " speed up", 446.0F,
                      0.9F * scale);
    }
}

} // namespace

void draw(SDL_Renderer* renderer, const Assets& assets, const State& game,
          float presentation_alpha) {
    // draw active mode
    int output_width = default_window_width;
    int output_height = default_window_height;
    (void)SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);

    (void)SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    (void)SDL_RenderClear(renderer);

    switch (game.mode) {
    case Mode::startup:
        draw_frontend_screen(renderer,
                             game.startup_mode == StartupMode::rocksolid ? assets.rocksolid
                                                                         : assets.xgames,
                             output_width, output_height);
        break;
    case Mode::title:
        draw_starter_screen(renderer, assets, game, output_width, output_height);
        draw_frontend_screen(renderer, assets.main_screen, output_width, output_height);
        draw_title_menu(renderer, assets, game, output_width, output_height);
        break;
    case Mode::exiting: {
        draw_starter_screen(renderer, assets, game, output_width, output_height);
        draw_frontend_screen(renderer, assets.main_screen, output_width, output_height);
        draw_title_menu(renderer, assets, game, output_width, output_height);
        const Uint8 alpha =
            static_cast<Uint8>(std::clamp<std::uint64_t>(game.mode_frame * 255U / 60U, 0U, 255U));
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        (void)SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
        const SDL_FRect screen{
            .x = 0.0F,
            .y = 0.0F,
            .w = static_cast<float>(output_width),
            .h = static_cast<float>(output_height),
        };
        (void)SDL_RenderFillRect(renderer, &screen);
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        break;
    }
    case Mode::player_select:
        draw_player_select(renderer, assets, game, output_width, output_height);
        break;
    case Mode::instructions:
        if (game.instructions_page == InstructionsPage::original) {
            draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
                       static_cast<float>(game.star_scroll.y), output_width, output_height);
            draw_instructions_screen(renderer, assets.instructions, output_width, output_height);
        } else {
            draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
                       static_cast<float>(game.star_scroll.y), output_width, output_height);
            draw_controls_screen(renderer, assets, game,
                                 game.instructions_page == InstructionsPage::controller
                                     ? InputDevice::controller
                                     : InputDevice::keyboard,
                                 output_width, output_height);
        }
        draw_instruction_navigation(renderer, assets, game, game.instructions_page,
                                    InstructionsPage::original, InstructionsPage::controller,
                                    output_width, output_height);
        break;
    case Mode::options:
        draw_stars(renderer, assets.stars, game, static_cast<float>(game.star_scroll.x),
                   static_cast<float>(game.star_scroll.y), output_width, output_height);
        draw_options(renderer, assets, game, output_width, output_height);
        break;
    case Mode::order_info:
        draw_frontend_with_stars(renderer, assets, game, assets.order, output_width, output_height);
        break;
    case Mode::credits:
        draw_frontend_with_stars(renderer, assets, game, assets.credits, output_width,
                                 output_height);
        break;
    case Mode::playing:
        switch (game.playing_mode) {
        case PlayingMode::level_start:
        case PlayingMode::active:
        case PlayingMode::paused:
        case PlayingMode::confirm_exit:
        case PlayingMode::exploding:
        case PlayingMode::level_complete:
            draw_gameplay(renderer, assets, game, output_width, output_height, presentation_alpha);
            break;
        }
        break;
    case Mode::results:
        draw_results(renderer, assets, game, output_width, output_height);
        break;
    case Mode::congratulations:
        if (game.congratulations_mode == CongratulationsMode::active) {
            draw_frontend_screen(renderer, assets.congratulations, output_width, output_height);
        }
        break;
    }
}

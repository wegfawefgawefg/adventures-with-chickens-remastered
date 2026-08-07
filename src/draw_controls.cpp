#include "draw_controls.hpp"

#include "assets.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace {

struct ControlRow {
    const char* action;
    const char* keyboard;
    const char* controller;
};

constexpr std::array control_rows{
    ControlRow{"MOVE SHIP", "ARROWS / WASD", "LEFT STICK / D-PAD"},
    ControlRow{"CATCH CHICKEN", "SPACE", "A"},
    ControlRow{"SHORT ALIEN", "CTRL / SPACE", "A"},
    ControlRow{"HONK HORN", "Q / F / H", "X"},
    ControlRow{"MOVE FAST", "SHIFT", "B / LB / RB / LT / RT"},
    ControlRow{"TOGGLE RADAR", "TAB", "BACK"},
    ControlRow{"PAUSE", "P", "START"},
};

void fill(SDL_Renderer* renderer, SDL_FRect rectangle, Uint8 red, Uint8 green, Uint8 blue) {
    (void)SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    (void)SDL_RenderFillRect(renderer, &rectangle);
}

void tile_texture(SDL_Renderer* renderer, SDL_Texture* texture, SDL_FRect area, float scale) {
    // tile original texture
    float texture_width = 0.0F;
    float texture_height = 0.0F;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height)) {
        return;
    }
    const float tile_width = texture_width * scale;
    const float tile_height = texture_height * scale;
    for (float y = area.y; y < area.y + area.h; y += tile_height) {
        for (float x = area.x; x < area.x + area.w; x += tile_width) {
            const SDL_FRect destination{
                .x = x,
                .y = y,
                .w = std::min(tile_width, area.x + area.w - x),
                .h = std::min(tile_height, area.y + area.h - y),
            };
            const SDL_FRect source{
                .x = 0.0F,
                .y = 0.0F,
                .w = destination.w / scale,
                .h = destination.h / scale,
            };
            (void)SDL_RenderTexture(renderer, texture, &source, &destination);
        }
    }
}

void draw_window_panel(SDL_Renderer* renderer, const Assets& assets, SDL_FRect panel, float scale) {
    // tile original brown panel
    tile_texture(renderer, assets.window_back, panel, scale);

    // cap panel corners
    const float corner_width = 8.0F * scale;
    const float corner_height = 4.0F * scale;
    for (int corner = 0; corner < 4; ++corner) {
        const bool right = corner % 2 != 0;
        const bool bottom = corner >= 2;
        const SDL_FRect source{
            .x = static_cast<float>(corner * 8),
            .y = 0.0F,
            .w = 8.0F,
            .h = 4.0F,
        };
        const SDL_FRect destination{
            .x = right ? panel.x + panel.w - corner_width : panel.x,
            .y = bottom ? panel.y + panel.h - corner_height : panel.y,
            .w = corner_width,
            .h = corner_height,
        };
        (void)SDL_RenderTexture(renderer, assets.window_corners, &source, &destination);
    }
}

void draw_text(SDL_Renderer* renderer, SDL_Texture* font, std::string_view text, float x, float y,
               float scale) {
    // draw original font
    for (const char character : text) {
        const int code = static_cast<unsigned char>(character);
        if (code < 32 || code >= 128) {
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
        (void)SDL_RenderTexture(renderer, font, &source, &destination);
        x += 11.0F * scale;
    }
}

void draw_keyboard_key(SDL_Renderer* renderer, const Assets& assets, std::string_view label,
                       SDL_FRect key, bool active, float scale) {
    // draw active panel or dim stipple key
    if (active) {
        draw_window_panel(renderer, assets, key, scale);
    } else {
        fill(renderer, key, 0, 0, 0);
        tile_texture(renderer, assets.dark, key, scale);
    }
    if (!active) {
        return;
    }

    // center readable active label
    const float text_scale = (label.size() > 4 ? 0.7F : 0.86F) * scale;
    const float label_width = static_cast<float>(label.size()) * 11.0F * text_scale;
    draw_text(renderer, assets.font, label, key.x + (key.w - label_width) * 0.5F,
              key.y + (key.h - 16.0F * text_scale) * 0.5F, text_scale);
}

void draw_keyboard(SDL_Renderer* renderer, const Assets& assets, SDL_FRect area, float scale,
                   std::uint64_t frame) {
    // lay out recognizable keyboard rows over space
    constexpr std::array<std::array<const char*, 12>, 4> rows{{
        {{"ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-"}},
        {{"TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "ENTER"}},
        {{"CAPS", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"}},
        {{"SHIFT", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "CTRL"}},
    }};
    const SDL_FRect keyboard{
        .x = area.x + 10.0F * scale,
        .y = area.y + 102.0F * scale,
        .w = area.w - 20.0F * scale,
        .h = 300.0F * scale,
    };
    const float gap = 5.0F * scale;
    const float key_width = (keyboard.w - gap * 11.0F) / 12.0F;
    const float key_height = 44.0F * scale;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (std::size_t column = 0; column < rows[row].size(); ++column) {
            const char* label = rows[row][column];
            const std::string_view name{label};
            const bool active = name == "ESC" || name == "TAB" || name == "W" || name == "A" ||
                                name == "S" || name == "D" || name == "H" || name == "P" ||
                                name == "Q" || name == "F" || name == "ENTER" || name == "SHIFT" ||
                                name == "Z" || name == "CTRL";
            const SDL_FRect key{
                .x = keyboard.x + static_cast<float>(column) * (key_width + gap),
                .y = keyboard.y + static_cast<float>(row) * (key_height + 7.0F * scale),
                .w = key_width,
                .h = key_height,
            };
            draw_keyboard_key(renderer, assets, label, key, active, scale);
        }
    }

    // draw space bar
    const SDL_FRect space{
        .x = keyboard.x + keyboard.w * 0.24F,
        .y = keyboard.y + 215.0F * scale,
        .w = keyboard.w * 0.42F,
        .h = 48.0F * scale,
    };
    draw_keyboard_key(renderer, assets, "SPACE", space, true, scale);

    // build arrow-key cluster from original arrows
    const SDL_FPoint arrows{keyboard.x + keyboard.w * 0.83F, keyboard.y + 239.0F * scale};
    const int arrow_frame = static_cast<int>((frame / 4U) % 8U);
    constexpr std::array<IVec2, 4> offsets{{{0, -1}, {1, 0}, {0, 0}, {-1, 0}}};
    for (int direction = 0; direction < 4; ++direction) {
        const IVec2 offset = offsets[static_cast<std::size_t>(direction)];
        const SDL_FRect source{static_cast<float>(arrow_frame * 16),
                               static_cast<float>(direction * 16), 16.0F, 16.0F};
        const SDL_FRect destination{
            .x = arrows.x + static_cast<float>(offset.x) * 32.0F * scale - 17.0F * scale,
            .y = arrows.y + static_cast<float>(offset.y) * 32.0F * scale - 17.0F * scale,
            .w = 34.0F * scale,
            .h = 34.0F * scale,
        };
        (void)SDL_RenderTexture(renderer, assets.arrows, &source, &destination);
    }
}

void draw_controller(SDL_Renderer* renderer, const Assets& assets, SDL_FRect area, float scale,
                     ControllerLayout layout, std::uint64_t frame) {
    // build textured controller shell
    const SDL_FPoint center{area.x + area.w * 0.52F, area.y + area.h * 0.51F};
    const float body_width = std::min(area.w - 38.0F * scale, 560.0F * scale);
    const SDL_FRect body{
        .x = center.x - body_width * 0.5F,
        .y = center.y - 116.0F * scale,
        .w = body_width,
        .h = 270.0F * scale,
    };
    // leave stars between separate textured grips
    const SDL_FRect grip_gap{
        .x = center.x - 78.0F * scale,
        .y = center.y + 74.0F * scale,
        .w = 156.0F * scale,
        .h = 90.0F * scale,
    };
    const SDL_FRect upper_body{body.x, body.y, body.w, grip_gap.y - body.y};
    const SDL_FRect left_grip{body.x, grip_gap.y, grip_gap.x - body.x,
                              body.y + body.h - grip_gap.y};
    const SDL_FRect right_grip{grip_gap.x + grip_gap.w, grip_gap.y,
                               body.x + body.w - grip_gap.x - grip_gap.w,
                               body.y + body.h - grip_gap.y};
    draw_window_panel(renderer, assets, upper_body, scale);
    draw_window_panel(renderer, assets, left_grip, scale);
    draw_window_panel(renderer, assets, right_grip, scale);

    // add ship crest
    const SDL_FRect ship_source{0.0F, 0.0F, 16.0F, 16.0F};
    const SDL_FRect ship_destination{
        .x = center.x - 24.0F * scale,
        .y = center.y + 18.0F * scale,
        .w = 48.0F * scale,
        .h = 48.0F * scale,
    };
    (void)SDL_RenderTexture(renderer, assets.ship, &ship_source, &ship_destination);

    const auto centered_text = [&](std::string_view text, SDL_FRect panel, float text_scale) {
        const float width = static_cast<float>(text.size()) * 11.0F * text_scale;
        draw_text(renderer, assets.font, text, panel.x + (panel.w - width) * 0.5F,
                  panel.y + (panel.h - 16.0F * text_scale) * 0.5F, text_scale);
    };

    // draw separate bumpers and triggers
    const SDL_FRect left_trigger{body.x + 54.0F * scale, body.y - 58.0F * scale, 72.0F * scale,
                                 30.0F * scale};
    const SDL_FRect left_bumper{body.x + 18.0F * scale, body.y - 31.0F * scale, 142.0F * scale,
                                36.0F * scale};
    const SDL_FRect right_trigger{body.x + body.w - 126.0F * scale, body.y - 58.0F * scale,
                                  72.0F * scale, 30.0F * scale};
    const SDL_FRect right_bumper{body.x + body.w - 160.0F * scale, body.y - 31.0F * scale,
                                 142.0F * scale, 36.0F * scale};
    const char* left_trigger_label = layout == ControllerLayout::playstation
                                         ? "L2"
                                         : (layout == ControllerLayout::nintendo ? "ZL" : "LT");
    const char* left_bumper_label = layout == ControllerLayout::playstation
                                        ? "L1"
                                        : (layout == ControllerLayout::nintendo ? "L" : "LB");
    const char* right_trigger_label = layout == ControllerLayout::playstation
                                          ? "R2"
                                          : (layout == ControllerLayout::nintendo ? "ZR" : "RT");
    const char* right_bumper_label = layout == ControllerLayout::playstation
                                         ? "R1"
                                         : (layout == ControllerLayout::nintendo ? "R" : "RB");
    draw_window_panel(renderer, assets, left_trigger, scale);
    draw_window_panel(renderer, assets, right_trigger, scale);
    draw_window_panel(renderer, assets, left_bumper, scale);
    draw_window_panel(renderer, assets, right_bumper, scale);
    centered_text(left_trigger_label, left_trigger, scale * 0.78F);
    centered_text(left_bumper_label, left_bumper, scale * 0.86F);
    centered_text(right_trigger_label, right_trigger, scale * 0.78F);
    centered_text(right_bumper_label, right_bumper, scale * 0.86F);

    // add left stick above d-pad
    const SDL_FPoint left_stick{body.x + body.w * 0.18F, center.y - 43.0F * scale};
    const SDL_FRect left_stick_source{4.0F * 16.0F, 0.0F, 16.0F, 16.0F};
    const SDL_FRect left_stick_destination{
        .x = left_stick.x - 29.0F * scale,
        .y = left_stick.y - 29.0F * scale,
        .w = 58.0F * scale,
        .h = 58.0F * scale,
    };
    (void)SDL_RenderTexture(renderer, assets.tiles, &left_stick_source, &left_stick_destination);
    const float stick_label_scale = 0.86F * scale;
    draw_text(renderer, assets.font, "LS", left_stick.x - 11.0F * stick_label_scale,
              left_stick.y - 8.0F * stick_label_scale, stick_label_scale);

    // place d-pad below stick
    const SDL_FPoint dpad{body.x + body.w * 0.32F, center.y + 45.0F * scale};
    const int arrow_frame = static_cast<int>((frame / 4U) % 8U);
    constexpr std::array<IVec2, 4> arrow_offsets{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
    for (int direction = 0; direction < 4; ++direction) {
        const IVec2 offset = arrow_offsets[static_cast<std::size_t>(direction)];
        const SDL_FRect source{
            .x = static_cast<float>(arrow_frame * 16),
            .y = static_cast<float>(direction * 16),
            .w = 16.0F,
            .h = 16.0F,
        };
        const SDL_FRect destination{
            .x = dpad.x + static_cast<float>(offset.x) * 38.0F * scale - 19.0F * scale,
            .y = dpad.y + static_cast<float>(offset.y) * 38.0F * scale - 19.0F * scale,
            .w = 38.0F * scale,
            .h = 38.0F * scale,
        };
        (void)SDL_RenderTexture(renderer, assets.arrows, &source, &destination);
    }
    const SDL_FRect rotator_source{
        .x = static_cast<float>((4 + static_cast<int>((frame / 8U) % 2U)) * 16),
        .y = 0.0F,
        .w = 16.0F,
        .h = 16.0F,
    };
    const SDL_FRect rotator_destination{dpad.x - 19.0F * scale, dpad.y - 19.0F * scale,
                                        38.0F * scale, 38.0F * scale};
    (void)SDL_RenderTexture(renderer, assets.tiles, &rotator_source, &rotator_destination);

    // build static face buttons from recolored rotators
    const SDL_FPoint face{body.x + body.w * 0.75F, center.y + 24.0F * scale};
    const auto button = [&](std::string_view label, std::size_t color, float x, float y) {
        const SDL_FRect source{4.0F * 16.0F, 0.0F, 16.0F, 16.0F};
        const SDL_FRect destination{x - 23.0F * scale, y - 23.0F * scale, 46.0F * scale,
                                    46.0F * scale};
        (void)SDL_RenderTexture(renderer, assets.controller_buttons[color], &source, &destination);
        const float text_scale = 1.08F * scale;
        const float width = static_cast<float>(label.size()) * 11.0F * text_scale;
        draw_text(renderer, assets.font, label, x - width * 0.5F, y - 8.0F * text_scale,
                  text_scale);
    };
    const std::array<std::string_view, 4> face_labels =
        layout == ControllerLayout::playstation
            ? std::array<std::string_view, 4>{"SQ", "CR", "CI", "TR"}
            : (layout == ControllerLayout::nintendo
                   ? std::array<std::string_view, 4>{"Y", "B", "A", "X"}
                   : std::array<std::string_view, 4>{"X", "A", "B", "Y"});
    const std::array<std::size_t, 4> face_colors = layout == ControllerLayout::nintendo
                                                       ? std::array<std::size_t, 4>{3, 2, 1, 0}
                                                       : std::array<std::size_t, 4>{0, 1, 2, 3};
    button(face_labels[0], face_colors[0], face.x - 40.0F * scale, face.y);
    button(face_labels[1], face_colors[1], face.x, face.y + 40.0F * scale);
    button(face_labels[2], face_colors[2], face.x + 40.0F * scale, face.y);
    button(face_labels[3], face_colors[3], face.x, face.y - 40.0F * scale);

    // draw center buttons
    const char* back_label = layout == ControllerLayout::playstation
                                 ? "SELECT"
                                 : (layout == ControllerLayout::nintendo ? "MINUS" : "SELECT");
    const char* start_label = layout == ControllerLayout::playstation
                                  ? "OPTIONS"
                                  : (layout == ControllerLayout::nintendo ? "PLUS" : "START");
    const SDL_FRect back_button{center.x - 78.0F * scale, center.y - 68.0F * scale, 62.0F * scale,
                                26.0F * scale};
    const SDL_FRect start_button{center.x + 16.0F * scale, center.y - 68.0F * scale, 62.0F * scale,
                                 26.0F * scale};
    draw_window_panel(renderer, assets, back_button, scale);
    draw_window_panel(renderer, assets, start_button, scale);
    centered_text(back_label, back_button, scale * 0.74F);
    centered_text(start_label, start_button, scale * 0.74F);
}

void draw_control_icon(SDL_Renderer* renderer, const Assets& assets, std::size_t row,
                       SDL_FRect area, std::uint64_t frame) {
    // choose original action art
    SDL_Texture* texture = assets.ship;
    SDL_FRect source{0.0F, 0.0F, 16.0F, 16.0F};
    switch (row) {
    case 0:
        texture = assets.ship;
        break;
    case 1:
        texture = assets.chicken;
        source.x = static_cast<float>((frame / 8U) % 8U) * 16.0F;
        break;
    case 2:
        texture = assets.aliens;
        break;
    case 3:
        texture = assets.ship;
        source.x = 4.0F * 16.0F;
        break;
    case 4:
        texture = assets.arrows;
        source.x = static_cast<float>((frame / 4U) % 8U) * 16.0F;
        source.y = 16.0F;
        break;
    case 5:
        texture = assets.warp_tile;
        source.x = static_cast<float>((frame / 2U) % 10U) * 16.0F;
        break;
    case 6:
        texture = assets.paused;
        source = {0.0F, 0.0F, 127.0F, 35.0F};
        area.x -= 3.0F;
        area.y += 9.0F;
        area.w = 50.0F;
        area.h = 14.0F;
        break;
    default:
        break;
    }
    (void)SDL_RenderTexture(renderer, texture, &source, &area);
}

} // namespace

void draw_controls_screen(SDL_Renderer* renderer, const Assets& assets, const State& game,
                          InputDevice device, int output_width, int output_height) {
    // calc responsive controls layout
    const float scale = std::clamp(static_cast<float>(output_height) / 720.0F, 0.8F, 1.6F);
    const float margin = 34.0F * scale;
    const SDL_FRect panel{
        .x = margin,
        .y = margin,
        .w = static_cast<float>(output_width) - margin * 2.0F,
        .h = static_cast<float>(output_height) - margin * 2.0F,
    };
    const bool controller = device == InputDevice::controller;

    // draw heading with original chicken
    const ControllerLayout layout =
        effective_controller_layout(game.controls, game.controller_layout);
    const std::string heading = controller
                                    ? std::string{"CONTROLS // "} + controller_layout_name(layout)
                                    : "CONTROLS // KEYBOARD";
    draw_text(renderer, assets.font, heading, panel.x + 42.0F * scale, panel.y + 30.0F * scale,
              1.75F * scale);
    const int chicken_frame = static_cast<int>((game.frame / 8U) % 8U);
    const SDL_FRect chicken_source{static_cast<float>(chicken_frame * 16), 0, 16, 16};
    const SDL_FRect chicken_destination{
        panel.x + panel.w - 78.0F * scale,
        panel.y + 20.0F * scale,
        48.0F * scale,
        48.0F * scale,
    };
    (void)SDL_RenderTexture(renderer, assets.chicken, &chicken_source, &chicken_destination);

    // calc action and device columns
    const float divider_x = panel.x + panel.w * 0.43F;
    const SDL_FRect device_area{
        .x = divider_x,
        .y = panel.y + 58.0F * scale,
        .w = panel.x + panel.w - divider_x - 22.0F * scale,
        .h = panel.h - 112.0F * scale,
    };
    if (controller) {
        draw_controller(renderer, assets, device_area, scale, layout, game.frame);
    } else {
        draw_keyboard(renderer, assets, device_area, scale, game.frame);
    }

    // place action rows
    const float first_row_y = panel.y + 126.0F * scale;
    const float row_gap = 62.0F * scale;
    for (std::size_t index = 0; index < control_rows.size(); ++index) {
        const ControlRow& row = control_rows[index];
        const float y = first_row_y + static_cast<float>(index) * row_gap;
        const SDL_FRect row_panel{
            .x = panel.x + 32.0F * scale,
            .y = y - 8.0F * scale,
            .w = divider_x - panel.x - 54.0F * scale,
            .h = 56.0F * scale,
        };
        draw_window_panel(renderer, assets, row_panel, scale);
        const std::string current_label =
            controller ? controller_label_for_layout(game.controls.controller_labels[index], layout)
                       : game.controls.keyboard_labels[index];
        const char* fallback_label = controller ? row.controller : row.keyboard;
        const std::string displayed_label =
            !current_label.empty()
                ? current_label
                : (controller ? controller_label_for_layout(fallback_label, layout)
                              : std::string{fallback_label});
        const SDL_FRect icon_area{
            .x = row_panel.x + 12.0F * scale,
            .y = row_panel.y + 10.0F * scale,
            .w = 36.0F * scale,
            .h = 36.0F * scale,
        };
        draw_control_icon(renderer, assets, index, icon_area, game.frame);
        draw_text(renderer, assets.font, row.action, row_panel.x + 64.0F * scale,
                  row_panel.y + 3.0F * scale, 1.25F * scale);
        draw_text(renderer, assets.font, displayed_label, row_panel.x + 64.0F * scale,
                  row_panel.y + 32.0F * scale, 1.05F * scale);
    }

    // show bind editor hint
    const std::string editor_hint = "F1 edit binds";
    const float editor_scale = 0.9F * scale;
    const float editor_width = static_cast<float>(editor_hint.size()) * 11.0F * editor_scale;
    draw_text(renderer, assets.font, editor_hint, panel.x + panel.w * 0.5F - editor_width * 0.5F,
              panel.y + panel.h - 62.0F * scale, editor_scale);

    // draw page hints
    const auto first_bind = [&](ControlAction action, const char* fallback) {
        std::string label =
            control_binding_label(game.controls, device, action, game.controller_layout);
        const std::size_t separator = label.find(" / ");
        if (separator != std::string::npos) {
            label.resize(separator);
        }
        return label.empty() ? std::string{fallback} : label;
    };
    const std::string hint = first_bind(ControlAction::back, controller ? "B" : "ESC") +
                             " back     <  " + first_bind(ControlAction::left, "LEFT") + " / " +
                             first_bind(ControlAction::right, "RIGHT") + " pages  >";
    const float hint_scale = 1.0F * scale;
    const float hint_width = static_cast<float>(hint.size()) * 11.0F * hint_scale;
    draw_text(renderer, assets.font, hint, panel.x + panel.w * 0.5F - hint_width * 0.5F,
              panel.y + panel.h - 38.0F * scale, hint_scale);
}

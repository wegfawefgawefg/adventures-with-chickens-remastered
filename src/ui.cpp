#include "ui.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>
#include <imgui.h>

namespace {

struct Grid {
    float margin;
    float top;
    float gap;
    float left_width;
    float middle_x;
    float middle_width;
    float right_x;
    float right_width;
    float height;
};

const char* mode_name(Mode mode) {
    switch (mode) {
    case Mode::startup:
        return "startup";
    case Mode::title:
        return "title";
    case Mode::player_select:
        return "player select";
    case Mode::instructions:
        return "instructions";
    case Mode::options:
        return "options";
    case Mode::order_info:
        return "order info";
    case Mode::credits:
        return "credits";
    case Mode::playing:
        return "playing";
    case Mode::results:
        return "results";
    case Mode::congratulations:
        return "congratulations";
    case Mode::exiting:
        return "exiting";
    }
    return "unknown";
}

Grid grid() {
    // calc tool grid
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    constexpr float margin = 12.0F;
    constexpr float top = 36.0F;
    constexpr float gap = 8.0F;
    const float left_width = std::clamp(display.x * 0.22F, 260.0F, 320.0F);
    const float middle_width = std::clamp(display.x * 0.30F, 320.0F, 440.0F);
    const float middle_x = margin + left_width + gap;
    const float right_x = middle_x + middle_width + gap;
    return {
        .margin = margin,
        .top = top,
        .gap = gap,
        .left_width = left_width,
        .middle_x = middle_x,
        .middle_width = middle_width,
        .right_x = right_x,
        .right_width = std::max(display.x - right_x - margin, 1.0F),
        .height = std::max(display.y - top - margin, 1.0F),
    };
}

void place(float x, float y, float width, float height, bool arrange) {
    if (!arrange) {
        return;
    }
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
}

void draw_launcher(UiState& ui, const Grid& layout) {
    // draw tool launcher
    place(layout.margin, layout.top, layout.left_width, 180.0F, ui.arrange_requested);
    const char* title = ui.workspace == ToolWorkspace::settings ? "Chicken Settings###AwcTools"
                                                                : "Chicken Tester###AwcTools";
    if (ImGui::Begin(title, &ui.launcher_visible)) {
        ImGui::TextDisabled("F1 settings  |  F2 tester");
        ImGui::TextDisabled("F11 fullscreen");
        if (ImGui::Button("Settings")) {
            ui.workspace = ToolWorkspace::settings;
            ui.arrange_requested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Tester")) {
            ui.workspace = ToolWorkspace::tester;
            ui.arrange_requested = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Arrange")) {
            ui.arrange_requested = true;
        }
        ImGui::SeparatorText("Windows");
        ImGui::Checkbox("Display", &ui.display_visible);
        ImGui::SameLine();
        ImGui::Checkbox("Enhancements", &ui.enhancements_visible);
        ImGui::Checkbox("Audio", &ui.audio_visible);
        ImGui::SameLine();
        ImGui::Checkbox("Controls", &ui.controls_visible);
    }
    ImGui::End();
}

void draw_display(UiState& ui, const Grid& layout, SDL_Window* window, State& game) {
    if (!ui.display_visible) {
        return;
    }

    // draw display settings
    place(layout.margin, layout.top + 188.0F, layout.left_width,
          std::min(300.0F, std::max(layout.height - 188.0F, 1.0F)), ui.arrange_requested);
    if (ImGui::Begin("Display", &ui.display_visible)) {
#ifndef __EMSCRIPTEN__
        if (ImGui::Button("1280 x 720")) {
            game.fullscreen = false;
            (void)SDL_SetWindowFullscreen(window, false);
            (void)SDL_SetWindowSize(window, 1280, 720);
            game.window_width = 1280;
            game.window_height = 720;
            save_remaster_settings(game);
        }
        if (ImGui::Button("1920 x 1080")) {
            game.fullscreen = false;
            (void)SDL_SetWindowFullscreen(window, false);
            (void)SDL_SetWindowSize(window, 1920, 1080);
            game.window_width = 1920;
            game.window_height = 1080;
            save_remaster_settings(game);
        }
#else
        ImGui::Text("Canvas: Auto");
#endif
        if (ImGui::Checkbox("Fullscreen", &game.fullscreen)) {
            (void)SDL_SetWindowFullscreen(window, game.fullscreen);
            save_remaster_settings(game);
        }
        if (ImGui::Checkbox("Vertical sync", &game.vsync)) {
            save_remaster_settings(game);
        }

#ifndef __EMSCRIPTEN__
        // set presentation rate
        constexpr std::array<int, 3> presentation_rates{60, 120, 144};
        int presentation_choice =
            game.presentation_rate == 60 ? 0 : (game.presentation_rate == 120 ? 1 : 2);
        constexpr std::array<const char*, 3> presentation_labels{"60 Hz", "120 Hz", "144 Hz"};
        ImGui::SetNextItemWidth(150.0F);
        if (ImGui::Combo("Presentation cap", &presentation_choice, presentation_labels.data(),
                         static_cast<int>(presentation_labels.size()))) {
            game.presentation_rate =
                presentation_rates[static_cast<std::size_t>(presentation_choice)];
            save_remaster_settings(game);
        }
#else
        ImGui::Text("Presentation: browser refresh");
#endif
        ImGui::SetNextItemWidth(150.0F);
        (void)ImGui::SliderFloat("Camera zoom", &game.camera.zoom, 0.5F, 2.0F, "%.2fx");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            save_remaster_settings(game);
        }
        int width = 0;
        int height = 0;
        (void)SDL_GetWindowSizeInPixels(window, &width, &height);
        ImGui::Text("Output: %d x %d", width, height);
        ImGui::TextDisabled("Native widescreen camera;");
        ImGui::TextDisabled("no 640x480 framebuffer.");
    }
    ImGui::End();
}

void draw_enhancements(UiState& ui, const Grid& layout, State& game) {
    if (!ui.enhancements_visible) {
        return;
    }

    // draw modern presentation options
    place(layout.margin, layout.top + 496.0F, layout.left_width,
          std::max(layout.height - 496.0F, 1.0F), ui.arrange_requested);
    if (ImGui::Begin("Enhancements", &ui.enhancements_visible)) {
        if (ImGui::Checkbox("Motion smoothing", &game.motion_smoothing)) {
            save_remaster_settings(game);
        }
        ImGui::TextDisabled("Game rules and timing stay unchanged.");
    }
    ImGui::End();
}

void draw_audio(UiState& ui, const Grid& layout, Audio& audio, State& game) {
    if (!ui.audio_visible) {
        return;
    }

    // draw audio settings
    place(layout.middle_x, layout.top, layout.middle_width, layout.height, ui.arrange_requested);
    if (ImGui::Begin("Audio", &ui.audio_visible)) {
        if (ImGui::SliderInt("Master", &game.master_volume, 0, 100, "%d%%")) {
            audio.set_master_volume(static_cast<float>(game.master_volume) / 100.0F);
        }
        bool finished_edit = ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::SliderInt("Music", &game.music_volume, 0, 100, "%d%%")) {
            audio.set_music_volume(static_cast<float>(game.music_volume) / 100.0F);
        }
        finished_edit = finished_edit || ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::SliderInt("Effects", &game.effect_volume, 0, 100, "%d%%")) {
            audio.set_effect_volume(static_cast<float>(game.effect_volume) / 100.0F);
        }
        finished_edit = finished_edit || ImGui::IsItemDeactivatedAfterEdit();
        if (finished_edit) {
            save_remaster_settings(game);
        }
        ImGui::Separator();
        ImGui::Text("Music: game-dispatched original MIDI");
#ifdef __EMSCRIPTEN__
        ImGui::Text("Synth: browser renders of original MIDI");
        ImGui::Text("Effects: browser playback of original WAV");
#else
        ImGui::Text("Synth: in-process FluidSynth");
        ImGui::Text("Effects: in-process original WAV");
#endif
    }
    ImGui::End();
}

bool draw_action_bindings(GubsyRuntime& runtime, ControlAction action) {
    const BindsProfile* profile = gubsy_find_binds_profile(runtime, control_profile_id());
    if (profile == nullptr) {
        ImGui::TextDisabled("Profile unavailable");
        return false;
    }

    // draw action binds
    ImGui::PushID(static_cast<int>(action));
    ImGui::TextUnformatted(control_action_name(action));
    ImGui::Indent();
    const auto& bindings = profile->button_binds();
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        if (bindings[index].action != static_cast<int>(action)) {
            continue;
        }
        ImGui::PushID(static_cast<int>(index));
        ImGui::TextUnformatted(
            binds_input_label(BindsActionType::Button, bindings[index].device_button).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            BindsProfile updated = *profile;
            (void)remove_bind_at(updated, BindsActionType::Button, static_cast<int>(index));
            const bool replaced = gubsy_replace_binds_profile(runtime, updated);
            ImGui::PopID();
            ImGui::Unindent();
            ImGui::PopID();
            return replaced;
        }
        ImGui::PopID();
    }

    // add action bind
    if (ImGui::BeginCombo("Add binding", "Choose input...")) {
        for (const InputChoice& choice : binds_input_choices(BindsActionType::Button)) {
            if (choice.label != nullptr && ImGui::Selectable(choice.label)) {
                BindsProfile updated = *profile;
                bind_button(updated, choice.code, static_cast<int>(action));
                const bool replaced = gubsy_replace_binds_profile(runtime, updated);
                ImGui::EndCombo();
                ImGui::Unindent();
                ImGui::PopID();
                return replaced;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Unindent();
    ImGui::Separator();
    ImGui::PopID();
    return false;
}

void draw_controllers(GubsyRuntime& runtime, State& game) {
    // set displayed controller family
    constexpr std::array<const char*, 4> layout_labels{
        "Auto",
        "Xbox",
        "PlayStation",
        "Nintendo",
    };
    int layout = static_cast<int>(game.controller_layout);
    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::Combo("Button labels", &layout, layout_labels.data(),
                     static_cast<int>(layout_labels.size()))) {
        game.controller_layout = static_cast<ControllerLayout>(layout);
        save_remaster_settings(game);
    }
    const ControllerLayout detected = game.controls.detected_layout;
    const ControllerLayout effective =
        effective_controller_layout(game.controls, game.controller_layout);
    ImGui::Text("Detected: %s", controller_layout_name(detected));
    ImGui::Text("Showing: %s", controller_layout_name(effective));

    // list connected controllers
    if (ImGui::Button("Refresh and assign connected controllers")) {
        gubsy_refresh_gamepads(runtime);
        assign_unclaimed_gamepads(runtime);
        detect_controller_layout(game.controls);
    }
    const std::vector<GubsyGamepad> gamepads = gubsy_get_gamepads(runtime);
    if (gamepads.empty()) {
        ImGui::TextDisabled("No controller connected.");
        return;
    }
    for (const GubsyGamepad& gamepad : gamepads) {
        ImGui::BulletText("%s  [device %d]", gamepad.name.c_str(), gamepad.device_id);
    }
}

void draw_controls(UiState& ui, const Grid& layout, GubsyRuntime& runtime, State& game) {
    if (!ui.controls_visible) {
        return;
    }

    // draw control settings
    place(layout.right_x, layout.top, layout.right_width, layout.height, ui.arrange_requested);
    if (ImGui::Begin("Controls", &ui.controls_visible)) {
        ImGui::TextDisabled("Persistent Gubsy profile; changes save immediately.");
        if (ImGui::Button("Reset all bindings to defaults")) {
            (void)reset_controls(runtime);
        }
        if (ImGui::BeginTabBar("ControlPages")) {
            if (ImGui::BeginTabItem("Bindings")) {
                for (const ControlAction action : control_actions) {
                    if (draw_action_bindings(runtime, action)) {
                        break;
                    }
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Controllers")) {
                draw_controllers(runtime, game);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void draw_tester(UiState& ui, const Grid& layout, State& game) {
    // draw state inspector
    if (ui.state_visible) {
        place(layout.middle_x, layout.top, layout.middle_width, 250.0F, ui.arrange_requested);
        if (ImGui::Begin("Game State", &ui.state_visible)) {
            ImGui::Text("Mode: %s", mode_name(game.mode));
            ImGui::Text("Frame: %llu", static_cast<unsigned long long>(game.frame));
            ImGui::Text("Level: %d / %d", game.current_level, level_count);
            ImGui::Text("Score: %d", game.score);
            ImGui::Text("Chickens: %d / %d", game.caught_chickens, game.total_chickens);
            ImGui::Text("Aliens: %d / %d", game.aliens_shorted, game.total_aliens);
            ImGui::Text("Fuel: %d / 300", game.fuel);
            ImGui::Text("Player: %.1f, %.1f", static_cast<double>(game.player_x),
                        static_cast<double>(game.player_y));
            ImGui::Text("Camera: %.1f, %.1f  zoom %.2f", static_cast<double>(game.camera.x),
                        static_cast<double>(game.camera.y), static_cast<double>(game.camera.zoom));
        }
        ImGui::End();
    }

    // draw mode shortcuts
    if (ui.setup_visible) {
        place(layout.middle_x, layout.top + 258.0F, layout.middle_width,
              std::max(layout.height - 258.0F, 1.0F), ui.arrange_requested);
        if (ImGui::Begin("Setup", &ui.setup_visible)) {
            ImGui::SliderInt("Level", &game.current_level, 1, level_count);
            if (ImGui::Button("Title")) {
                game.mode = Mode::title;
                game.mode_frame = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Player select")) {
                game.mode = Mode::player_select;
                game.player_select_mode = PlayerSelectMode::choose_player;
                game.player_select_dirty = false;
                game.mode_frame = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Options")) {
                game.mode = Mode::options;
                game.mode_frame = 0;
            }
            if (ImGui::Button("Incomplete result")) {
                game.mode = Mode::results;
                game.results_mode = ResultsMode::level_incomplete;
                game.result_animation_frame = 0;
                game.result_accelerated = false;
                game.mode_frame = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Complete result")) {
                game.mode = Mode::results;
                game.results_mode = ResultsMode::level_complete;
                game.result_animation_frame = 0;
                game.result_accelerated = false;
                game.mode_frame = 0;
            }
            if (ImGui::Button("Congratulations")) {
                game.mode = Mode::congratulations;
                game.congratulations_mode = CongratulationsMode::waiting;
                game.mode_frame = 0;
            }
        }
        ImGui::End();
    }

    // draw object inspector
    if (ui.objects_visible) {
        place(layout.right_x, layout.top, layout.right_width, layout.height, ui.arrange_requested);
        if (ImGui::Begin("Level Objects", &ui.objects_visible)) {
            if (game.levels_loaded && game.current_level >= 1 &&
                game.current_level <= static_cast<int>(game.levels.size())) {
                const Level& level = game.levels[static_cast<std::size_t>(game.current_level - 1)];
                ImGui::Text("Terrain cells: %d", level_cell_count);
                ImGui::Text("Chickens: %zu", level.chickens.size());
                ImGui::Text("Aliens: %zu", level.aliens.size());
                ImGui::Text("Warps: %zu", level.warps.size());
                ImGui::Text("Start: %d, %d", level.player_start.x, level.player_start.y);
                ImGui::Text("Exit: %d, %d", level.exit.x, level.exit.y);
                if (game.mode == Mode::playing && ImGui::Button("Rescue all chickens")) {
                    for (ChickenState& chicken : game.chickens) {
                        if (!chicken.active) {
                            continue;
                        }
                        const std::size_t index =
                            static_cast<std::size_t>(chicken.y * level_width + chicken.x);
                        game.active_tiles[index] = 0;
                        chicken.active = false;
                        ++game.caught_chickens;
                        game.score += (std::clamp(game.save.difficulty, 0, 4) + 5) * 50;
                    }
                }
                if (game.mode == Mode::playing && ImGui::Button("Go to exit opener")) {
                    const auto opener =
                        std::ranges::find(game.active_tiles, static_cast<std::int8_t>(18));
                    if (opener != game.active_tiles.end()) {
                        const int index =
                            static_cast<int>(std::distance(game.active_tiles.begin(), opener));
                        game.player_x = static_cast<float>((index % level_width) * 16 + 8);
                        game.player_y = static_cast<float>((index / level_width - 1) * 16 + 8);
                        game.camera.x = game.player_x;
                        game.camera.y = game.player_y;
                    }
                }
                if (game.mode == Mode::playing && ImGui::Button("Go to remote exit")) {
                    game.player_x = static_cast<float>((level.exit.x - 1) * 16 + 8);
                    game.player_y = static_cast<float>(level.exit.y * 16 + 8);
                    game.camera.x = game.player_x;
                    game.camera.y = game.player_y;
                }
            } else {
                ImGui::TextDisabled("No resident level selected.");
            }
        }
        ImGui::End();
    }
}

} // namespace

void toggle_settings_workspace(UiState& ui) {
    ui.workspace =
        ui.workspace == ToolWorkspace::settings ? ToolWorkspace::hidden : ToolWorkspace::settings;
    ui.arrange_requested = ui.workspace != ToolWorkspace::hidden;
}

void toggle_tester_workspace(UiState& ui) {
    ui.workspace =
        ui.workspace == ToolWorkspace::tester ? ToolWorkspace::hidden : ToolWorkspace::tester;
    ui.arrange_requested = ui.workspace != ToolWorkspace::hidden;
}

bool ui_owns_game_input(const UiState& ui) {
    return ui.workspace != ToolWorkspace::hidden;
}

void draw_ui(UiState& ui, GubsyRuntime& runtime, SDL_Window* window, Audio& audio, State& game) {
    if (ui.workspace == ToolWorkspace::hidden) {
        return;
    }

    // draw active workspace
    const Grid layout = grid();
    draw_launcher(ui, layout);
    draw_display(ui, layout, window, game);
    if (ui.workspace == ToolWorkspace::settings) {
        draw_enhancements(ui, layout, game);
        draw_audio(ui, layout, audio, game);
        draw_controls(ui, layout, runtime, game);
    } else {
        draw_tester(ui, layout, game);
    }

    // update workspace state
    ui.arrange_requested = false;
    if (!ui.launcher_visible) {
        ui.workspace = ToolWorkspace::hidden;
        ui.launcher_visible = true;
    }
}

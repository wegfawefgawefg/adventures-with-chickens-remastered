#include "assets.hpp"
#include "audio.hpp"
#include "cli.hpp"
#include "controls.hpp"
#include "draw.hpp"
#include "gubsy.hpp"
#include "inputs.hpp"
#include "save.hpp"
#include "setup.hpp"
#include "src/imgui_layer.hpp"
#include "state.hpp"
#include "step.hpp"
#include "ui.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <gubsy/runtime.hpp>
#include <optional>
#include <string>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

constexpr char window_title[] = "Adventures with Chickens Remastered";
constexpr float maximum_frame_seconds = 0.25F;

Clock::duration draw_interval_for(int presentation_rate) {
    return std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<float>(1.0F / static_cast<float>(presentation_rate)));
}

} // namespace

int main(int argc, char** argv) {
    // init state
    State state = setup::create();
    CliOptions options;
    if (const std::optional<int> result = dispatch_cli(state, argc, argv, options)) {
        return *result;
    }

    // init window
    Window window{window_title, state.window_width, state.window_height};
    if (window.handle() == nullptr) {
        std::fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        return 1;
    }
    (void)SDL_StartTextInput(window.handle());

    SDL_Renderer* renderer = SDL_CreateRenderer(window.handle(), nullptr);
    if (renderer == nullptr) {
        std::fprintf(stderr, "could not create renderer: %s\n", SDL_GetError());
        return 1;
    }
    (void)SDL_SetRenderVSync(renderer, state.vsync ? 1 : 0);

    // init dev tools
    GubsyRuntime runtime;
    std::string imgui_ini_path;
    if (!setup_gubsy(runtime, window.handle(), renderer, state, imgui_ini_path)) {
        SDL_DestroyRenderer(renderer);
        return 1;
    }
    detect_controller_layout(state.controls);

    // load assets and audio
    Assets assets;
    if (!load_assets(renderer, assets)) {
        destroy_assets(assets);
        shutdown_gubsy(runtime);
        SDL_DestroyRenderer(renderer);
        return 1;
    }
    Audio audio;
    if (!audio.open()) {
        std::fprintf(stderr, "audio unavailable: %s\n", SDL_GetError());
    }
    audio.set_master_volume(static_cast<float>(state.master_volume) / 100.0F);
    audio.set_music_volume(static_cast<float>(state.music_volume) / 100.0F);
    audio.set_effect_volume(static_cast<float>(state.effect_volume) / 100.0F);
    sync_music(audio, state);
    if (state.save.sound && state.mode == Mode::startup) {
        audio.play(Sound::intro);
    }

    // init clocks
    int rendered_frames = 0;
    auto previous_pump_time = Clock::now();
    const auto performance_start_time = previous_pump_time;
    auto next_draw_time = previous_pump_time + draw_interval_for(state.presentation_rate);
    float accumulated_step_time = 0.0F;
    Clock::duration total_draw_time{};
    Clock::duration maximum_draw_time{};

    while (!state.input.close_requested && !state.exit_requested) {
        // calc frame time
        const auto current_time = Clock::now();
        const float pump_seconds =
            std::chrono::duration<float>(current_time - previous_pump_time).count();
        previous_pump_time = current_time;
        accumulated_step_time += std::min(pump_seconds, maximum_frame_seconds);

        // pump input
        pump_inputs(state.input, &runtime);
        if (state.input.toggle_settings_pressed) {
            toggle_settings_workspace(state.ui);
        }
        if (state.input.toggle_tester_pressed) {
            toggle_tester_workspace(state.ui);
        }
        if (state.input.toggle_fullscreen_pressed) {
            state.fullscreen = !state.fullscreen;
            (void)SDL_SetWindowFullscreen(window.handle(), state.fullscreen);
            save_remaster_settings(state);
        }

        // merge mapped input
        const float clamped_pump_seconds = std::min(pump_seconds, maximum_frame_seconds);
        gubsy_update_device_state(runtime);
        gubsy_update_runtime(runtime, clamped_pump_seconds);
        if (state.input.gamepad_changed) {
            assign_unclaimed_gamepads(runtime);
            detect_controller_layout(state.controls);
        }
        sample_controls(runtime, state.controls, state.input);
        if (options.controls_preview) {
            suppress_game_input(state.input);
        }
        if (ui_owns_game_input(state.ui)) {
            suppress_game_input(state.input);
        }

        // run fixed steps
        while (accumulated_step_time >= step_seconds) {
            step::step(state, state.input, &audio);
            sync_music(audio, state);
            consume_game_input(state.input);

            accumulated_step_time -= step_seconds;
        }

        // draw frame
        if (current_time >= next_draw_time) {
            const auto draw_start_time =
                options.report_performance ? Clock::now() : Clock::time_point{};
            imgui_new_frame();
            const float presentation_alpha =
                std::clamp(accumulated_step_time / step_seconds, 0.0F, 1.0F);
            draw(renderer, assets, state, presentation_alpha);
            draw_ui(state.ui, runtime, window.handle(), audio, state);
            (void)SDL_SetRenderVSync(renderer, state.vsync ? 1 : 0);
            imgui_render_layer();

            // capture direct CLI previews without desktop input
            if (!options.capture_path.empty()) {
                SDL_Surface* capture = SDL_RenderReadPixels(renderer, nullptr);
                if (capture == nullptr || !SDL_SaveBMP(capture, options.capture_path.c_str())) {
                    std::fprintf(stderr, "could not capture frame: %s\n", SDL_GetError());
                }
                SDL_DestroySurface(capture);
                state.exit_requested = true;
            }
            SDL_RenderPresent(renderer);
            ++rendered_frames;
            if (options.report_performance) {
                const Clock::duration draw_time = Clock::now() - draw_start_time;
                total_draw_time += draw_time;
                maximum_draw_time = std::max(maximum_draw_time, draw_time);
            }
            if (options.render_smoke && rendered_frames >= 3) {
                break;
            }

            const Clock::duration draw_interval = draw_interval_for(state.presentation_rate);
            next_draw_time += draw_interval;
            if (next_draw_time < current_time) {
                next_draw_time = current_time + draw_interval;
            }
            continue;
        }

        // sleep
        std::this_thread::sleep_until(next_draw_time);
    }

    // print perf stats
    if (options.report_performance) {
        const double elapsed_seconds =
            std::chrono::duration<double>(Clock::now() - performance_start_time).count();
        const double average_draw_milliseconds =
            rendered_frames > 0
                ? std::chrono::duration<double, std::milli>(total_draw_time).count() /
                      static_cast<double>(rendered_frames)
                : 0.0;
        const double maximum_draw_milliseconds =
            std::chrono::duration<double, std::milli>(maximum_draw_time).count();
        std::printf("performance: frames=%d elapsed=%.3fs rate=%.1fHz draw-average=%.3fms "
                    "draw-maximum=%.3fms\n",
                    rendered_frames, elapsed_seconds,
                    static_cast<double>(rendered_frames) / elapsed_seconds,
                    average_draw_milliseconds, maximum_draw_milliseconds);
    }

    // save window size
    if (!options.explicit_window_size) {
        if (!state.fullscreen) {
            (void)SDL_GetWindowSize(window.handle(), &state.window_width, &state.window_height);
        }
        save_remaster_settings(state);
    }

    // free renderer resources
    destroy_assets(assets);
    shutdown_gubsy(runtime);
    SDL_DestroyRenderer(renderer);
    return 0;
}

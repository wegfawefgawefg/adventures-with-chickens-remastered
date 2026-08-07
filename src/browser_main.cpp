#include "assets.hpp"
#include "audio.hpp"
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

#include <SDL3/SDL.h>
#include <algorithm>
#include <emscripten/emscripten.h>
#include <gubsy/runtime.hpp>
#include <memory>
#include <string>

namespace {

constexpr char window_title[] = "Adventures with Chickens Remastered";
constexpr float maximum_frame_seconds = 0.25F;

State state;
Assets assets;
GubsyRuntime runtime;
std::unique_ptr<Audio> audio;
SDL_Window* window{};
SDL_Renderer* renderer{};
std::string imgui_ini_path;
std::string last_error;
float accumulated_step_time{};
bool started{};

// clang-format off
EM_JS(int, resume_web_audio, (), {
    const state = globalThis.__awcAudio;
    if (!state)
        return 0;
    state.unlocked = true;
    if (state.musicElement)
        void state.musicElement.play().catch(() => {});
    return 1;
});
// clang-format on

void shutdown_runtime() {
    if (!started) {
        return;
    }

    // save and free runtime resources
    save_remaster_settings(state);
    destroy_assets(assets);
    shutdown_gubsy(runtime);
    audio.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    renderer = nullptr;
    window = nullptr;
    state = {};
    assets = {};
    accumulated_step_time = 0.0F;
    started = false;
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int native_awc_start(int canvas_width, int canvas_height) {
    shutdown_runtime();
    last_error.clear();

    // init persistent state and browser canvas
    state = setup::create();
    state.window_width = std::max(canvas_width, 1);
    state.window_height = std::max(canvas_height, 1);
    state.fullscreen = false;
    state.vsync = false;
    (void)SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#game-canvas");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        last_error = std::string{"could not initialize SDL: "} + SDL_GetError();
        return 0;
    }
    window = SDL_CreateWindow(window_title, state.window_width, state.window_height,
                              SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        last_error = std::string{"could not create browser canvas: "} + SDL_GetError();
        SDL_Quit();
        return 0;
    }
    (void)SDL_StartTextInput(window);

    // init renderer and tools
    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        last_error = std::string{"could not create renderer: "} + SDL_GetError();
        SDL_DestroyWindow(window);
        window = nullptr;
        SDL_Quit();
        return 0;
    }
    (void)SDL_SetRenderVSync(renderer, 0);
    if (!setup_gubsy(runtime, window, renderer, state, imgui_ini_path)) {
        last_error = std::string{"could not initialize settings and controls: "} + SDL_GetError();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        renderer = nullptr;
        window = nullptr;
        SDL_Quit();
        return 0;
    }
    detect_controller_layout(state.controls);

    // load game assets and browser audio
    if (!load_assets(renderer, assets)) {
        last_error = "could not load game assets";
        shutdown_gubsy(runtime);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        renderer = nullptr;
        window = nullptr;
        SDL_Quit();
        return 0;
    }
    audio = std::make_unique<Audio>();
    (void)audio->open();

    // unlock while still inside the play-button click
    (void)resume_web_audio();
    audio->set_master_volume(static_cast<float>(state.master_volume) / 100.0F);
    audio->set_music_volume(static_cast<float>(state.music_volume) / 100.0F);
    audio->set_effect_volume(static_cast<float>(state.effect_volume) / 100.0F);
    sync_music(*audio, state);
    if (state.save.sound) {
        audio->play(Sound::intro);
    }

    started = true;
    return 1;
}

EMSCRIPTEN_KEEPALIVE int native_awc_resize(int canvas_width, int canvas_height) {
    if (!started || window == nullptr || canvas_width < 1 || canvas_height < 1) {
        return 0;
    }

    // match renderer output to displayed browser pixels
    if (canvas_width == state.window_width && canvas_height == state.window_height) {
        return 1;
    }
    if (!SDL_SetWindowSize(window, canvas_width, canvas_height)) {
        return 0;
    }
    state.window_width = canvas_width;
    state.window_height = canvas_height;
    return 1;
}

EMSCRIPTEN_KEEPALIVE int native_awc_frame(double elapsed_seconds) {
    if (!started) {
        return 0;
    }
    const float elapsed =
        std::clamp(static_cast<float>(elapsed_seconds), 0.0F, maximum_frame_seconds);
    accumulated_step_time += elapsed;

    // pump browser input
    pump_inputs(state.input, &runtime);
    if (state.input.toggle_settings_pressed) {
        toggle_settings_workspace(state.ui);
    }
    if (state.input.toggle_tester_pressed) {
        toggle_tester_workspace(state.ui);
    }
    if (state.input.toggle_fullscreen_pressed) {
        state.fullscreen = !state.fullscreen;
        (void)SDL_SetWindowFullscreen(window, state.fullscreen);
        save_remaster_settings(state);
    }

    // merge mapped input
    gubsy_update_device_state(runtime);
    gubsy_update_runtime(runtime, elapsed);
    if (state.input.gamepad_changed) {
        assign_unclaimed_gamepads(runtime);
        detect_controller_layout(state.controls);
    }
    sample_controls(runtime, state.controls, state.input);
    if (ui_owns_game_input(state.ui)) {
        suppress_game_input(state.input);
    }

    // run fixed game steps
    while (accumulated_step_time >= step_seconds) {
        step::step(state, state.input, audio.get());
        sync_music(*audio, state);
        consume_game_input(state.input);
        accumulated_step_time -= step_seconds;
    }

    if (state.input.close_requested || state.exit_requested) {
        return 0;
    }
    // draw browser frame
    imgui_new_frame();
    const float presentation_alpha = std::clamp(accumulated_step_time / step_seconds, 0.0F, 1.0F);
    draw(renderer, assets, state, presentation_alpha);
    draw_ui(state.ui, runtime, window, *audio, state);
    imgui_render_layer();
    SDL_RenderPresent(renderer);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int native_awc_audio_resume() {
    return resume_web_audio();
}

EMSCRIPTEN_KEEPALIVE void native_awc_shutdown() {
    shutdown_runtime();
}

EMSCRIPTEN_KEEPALIVE const char* native_awc_last_error() {
    return last_error.c_str();
}

} // extern "C"

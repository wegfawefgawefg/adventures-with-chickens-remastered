#include "gubsy.hpp"

#include "controls.hpp"
#include "save.hpp"
#include "src/imgui_layer.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <cstdio>
#include <gubsy/runtime.hpp>
#include <imgui.h>

namespace {

constexpr char window_title[] = "Adventures with Chickens Remastered";

} // namespace

bool setup_gubsy(GubsyRuntime& runtime, SDL_Window* window, SDL_Renderer* renderer,
                 const State& state, std::string& imgui_ini_path) {
    // init Gubsy
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = AWC_SOURCE_DIR;
    config.data_root = native_save_path().parent_path().string();
    config.engine_assets_root = AWC_GUBSY_ASSETS_DIR;
    config.window_title = window_title;
    config.window_width = state.window_width;
    config.window_height = state.window_height;
    config.render_width = state.window_width;
    config.render_height = state.window_height;
    config.utility_window = true;
    config.resizable_window = true;
    config.apply_display_settings = false;
    if (!init_gubsy_runtime(runtime, config)) {
        std::fprintf(stderr, "could not initialize Gubsy: %s\n", SDL_GetError());
        return false;
    }
    if (!gubsy_attach_sdl_renderer(runtime, window, renderer, state.window_width,
                                   state.window_height) ||
        !init_imgui_layer(window, renderer)) {
        std::fprintf(stderr, "could not initialize Gubsy/ImGui: %s\n", SDL_GetError());
        cleanup_gubsy_runtime(runtime);
        return false;
    }

    // restore display
    if (!SDL_SetWindowSize(window, state.window_width, state.window_height)) {
        std::fprintf(stderr, "could not restore requested window size: %s\n", SDL_GetError());
        shutdown_gubsy(runtime);
        return false;
    }
    (void)SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    if (state.fullscreen && !SDL_SetWindowFullscreen(window, true)) {
        std::fprintf(stderr, "could not enter fullscreen: %s\n", SDL_GetError());
        shutdown_gubsy(runtime);
        return false;
    }

    // load tool settings
    imgui_ini_path = (native_save_path().parent_path() / "imgui.ini").string();
    ImGui::GetIO().IniFilename = imgui_ini_path.c_str();
    if (!register_controls(runtime)) {
        std::fprintf(stderr, "could not initialize persistent controls\n");
        shutdown_gubsy(runtime);
        return false;
    }
    assign_unclaimed_gamepads(runtime);
    return true;
}

void shutdown_gubsy(GubsyRuntime& runtime) {
    shutdown_imgui_layer();
    cleanup_gubsy_runtime(runtime);
}

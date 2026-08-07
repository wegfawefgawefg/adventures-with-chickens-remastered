#include "setup.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {

int next_original_random(std::uint32_t& random_state) {
    random_state = random_state * UINT32_C(214013) + UINT32_C(2531011);
    return static_cast<int>((random_state >> 16U) & UINT32_C(0x7fff));
}

void init_stars(State& state) {
    // seed a private copy of original MSVC random generator
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    std::uint32_t random_state =
        static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());

    // match original position and sprite rolls
    for (StarState& star : state.stars) {
        star.position.x = next_original_random(random_state) % 637;
        star.position.y = next_original_random(random_state) % 477;
        star.frame = next_original_random(random_state) % 7;
        if (next_original_random(random_state) % 50 == 1) {
            star.frame = next_original_random(random_state) % 3 + 7;
        }
    }
}

} // namespace

namespace setup {

State create() {
    State state;

    // init stars
    init_stars(state);

    // load save
    state.loaded_save_path = find_existing_save_path();
    if (std::filesystem::exists(state.loaded_save_path)) {
        std::string error;
        if (!load_save(state.loaded_save_path, state.save, error)) {
            std::fprintf(stderr, "could not load save: %s\n", error.c_str());
        }
    }

    // load settings
    const std::filesystem::path settings_path = remaster_settings_path();
    if (std::filesystem::exists(settings_path)) {
        std::string error;
        if (!load_remaster_settings(settings_path, state.master_volume, state.music_volume,
                                    state.effect_volume, state.camera.zoom, state.window_width,
                                    state.window_height, state.fullscreen, state.vsync,
                                    state.presentation_rate, state.motion_smoothing,
                                    state.controller_layout, state.best_level_frames, error)) {
            std::fprintf(stderr, "could not load remaster settings: %s\n", error.c_str());
        }
    }

    // load levels
    std::string level_error;
    const std::filesystem::path levels_path =
        std::filesystem::path{AWC_ASSET_ROOT} / "data/Levels.dat";
    state.levels_loaded = load_levels(levels_path, state.levels, level_error);
    if (!state.levels_loaded) {
        std::fprintf(stderr, "could not load levels: %s\n", level_error.c_str());
    }

    // select player
    for (int slot = 0; slot < player_slot_count; ++slot) {
        if (state.save.players[static_cast<std::size_t>(slot)].active != 0) {
            state.selected_player = slot;
            state.current_level = std::clamp(
                state.save.players[static_cast<std::size_t>(slot)].level, 1, level_count);
            break;
        }
    }
    return state;
}

} // namespace setup

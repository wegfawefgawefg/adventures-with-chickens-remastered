#include "inputs.hpp"

#include "src/imgui_layer.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

void pump_inputs(InputState& input, GubsyRuntime* runtime) {
    // clear host edges
    input.toggle_fullscreen_pressed = false;
    input.toggle_settings_pressed = false;
    input.toggle_tester_pressed = false;
    input.gamepad_changed = false;

    // poll SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (runtime != nullptr) {
            gubsy_process_sdl_event(*runtime, event);
        }
        if (event.type == SDL_EVENT_QUIT) {
            input.close_requested = true;
            continue;
        }
        if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            input.gamepad_changed = true;
        }
        if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
            constexpr Sint16 trigger_threshold = 8000;
            if (event.gaxis.value > trigger_threshold || event.gaxis.value < -trigger_threshold) {
                input.last_device = InputDevice::controller;
            }
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                input.left_trigger_run_held = event.gaxis.value > trigger_threshold;
            } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
                input.right_trigger_run_held = event.gaxis.value > trigger_threshold;
            }
        }
        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            input.last_device = InputDevice::controller;
        }
        if (event.type == SDL_EVENT_TEXT_INPUT && !imgui_want_capture_keyboard()) {
            input.last_device = InputDevice::keyboard;
            input.text_entered += event.text.text;
            continue;
        }
        if (event.type != SDL_EVENT_KEY_DOWN) {
            continue;
        }
        input.last_device = InputDevice::keyboard;

        // filter debug chords
        const bool diagnostic_modifiers =
            (event.key.mod & SDL_KMOD_CTRL) != 0 && (event.key.mod & SDL_KMOD_SHIFT) != 0;
        // ignore key repeats
        if (event.key.repeat) {
            continue;
        }
        if (event.key.key == SDLK_F11) {
            input.toggle_fullscreen_pressed = true;
        } else if (event.key.key == SDLK_F1) {
            input.toggle_settings_pressed = true;
        } else if (event.key.key == SDLK_F2) {
            input.toggle_tester_pressed = true;
        } else if (event.key.key == SDLK_Q && (event.key.mod & SDL_KMOD_CTRL) != 0 &&
                   !diagnostic_modifiers) {
            input.close_requested = true;
        } else if (imgui_want_capture_keyboard()) {
            continue;
        } else if (event.key.key == SDLK_BACKSPACE) {
            input.backspace_pressed = true;
        }
    }

    // sample debug chords
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const SDL_Keymod modifiers = SDL_GetModState();
    const bool diagnostic_modifiers =
        (modifiers & SDL_KMOD_CTRL) != 0 && (modifiers & SDL_KMOD_SHIFT) != 0;
    const bool diagnostics_enabled = diagnostic_modifiers && !imgui_want_capture_keyboard();
    input.diagnostic_low_fuel_held = diagnostics_enabled && keys[SDL_SCANCODE_Q] != 0;
    input.diagnostic_full_fuel_held = diagnostics_enabled && keys[SDL_SCANCODE_W] != 0;
    input.diagnostic_score_held = diagnostics_enabled && keys[SDL_SCANCODE_E] != 0;
    input.diagnostic_skip_held = diagnostics_enabled && keys[SDL_SCANCODE_R] != 0;
}

void consume_game_input(InputState& input) {
    // clear game edges
    input.up_pressed = false;
    input.down_pressed = false;
    input.left_pressed = false;
    input.right_pressed = false;
    input.confirm_pressed = false;
    input.back_pressed = false;
    input.pause_pressed = false;
    input.radar_pressed = false;
    input.horn_pressed = false;
    input.attack_pressed = false;
    input.backspace_pressed = false;
    input.remove_pressed = false;
    input.text_entered.clear();
}

void suppress_game_input(InputState& input) {
    // block game input
    consume_game_input(input);
    input.up_held = false;
    input.down_held = false;
    input.left_held = false;
    input.right_held = false;
    input.action_held = false;
    input.run_held = false;
    input.left_trigger_run_held = false;
    input.right_trigger_run_held = false;
    input.attack_held = false;
    input.horn_held = false;
    input.diagnostic_low_fuel_held = false;
    input.diagnostic_full_fuel_held = false;
    input.diagnostic_score_held = false;
    input.diagnostic_skip_held = false;
}

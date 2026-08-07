#pragma once

#include <string>

class GubsyRuntime;

enum class InputDevice {
    keyboard,
    controller,
};

enum class ControllerLayout {
    automatic,
    xbox,
    playstation,
    nintendo,
};

struct InputState {
    InputDevice last_device{InputDevice::keyboard};
    bool close_requested{};
    bool toggle_fullscreen_pressed{};
    bool toggle_settings_pressed{};
    bool toggle_tester_pressed{};
    bool gamepad_changed{};
    bool up_pressed{};
    bool down_pressed{};
    bool left_pressed{};
    bool right_pressed{};
    bool confirm_pressed{};
    bool back_pressed{};
    bool pause_pressed{};
    bool radar_pressed{};
    bool horn_pressed{};
    bool attack_pressed{};
    bool backspace_pressed{};
    bool remove_pressed{};
    std::string text_entered{};
    bool action_held{};
    bool run_held{};
    bool left_trigger_run_held{};
    bool right_trigger_run_held{};
    bool attack_held{};
    bool horn_held{};
    bool diagnostic_low_fuel_held{};
    bool diagnostic_full_fuel_held{};
    bool diagnostic_score_held{};
    bool diagnostic_skip_held{};
    bool up_held{};
    bool down_held{};
    bool left_held{};
    bool right_held{};
};

void pump_inputs(InputState& input, GubsyRuntime* runtime = nullptr);
void consume_game_input(InputState& input);
void suppress_game_input(InputState& input);

#pragma once

#include "inputs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class GubsyRuntime;

enum class ControlAction : int {
    left = 1,
    right = 2,
    up = 3,
    down = 4,
    confirm = 5,
    back = 6,
    pause = 7,
    horn = 8,
    run = 10,
    attack = 11,
    radar = 12,
    remove_player = 13,
};

inline constexpr std::array control_actions{
    ControlAction::left, ControlAction::right,   ControlAction::up,
    ControlAction::down, ControlAction::confirm, ControlAction::back,
    ControlAction::run,  ControlAction::attack,  ControlAction::pause,
    ControlAction::horn, ControlAction::radar,   ControlAction::remove_player,
};

struct ControlState {
    std::array<bool, control_actions.size()> previous{};
    std::array<std::string, control_actions.size()> keyboard_action_labels{};
    std::array<std::string, control_actions.size()> controller_action_labels{};
    std::array<std::string, 7> keyboard_labels{};
    std::array<std::string, 7> controller_labels{};
    std::uint64_t labels_hash{};
    ControllerLayout detected_layout{ControllerLayout::xbox};
};

bool register_controls(GubsyRuntime& runtime);
void assign_unclaimed_gamepads(GubsyRuntime& runtime);
void detect_controller_layout(ControlState& controls);
void sample_controls(GubsyRuntime& runtime, ControlState& controls, InputState& input);
bool reset_controls(GubsyRuntime& runtime);
int control_profile_id();
const char* control_action_name(ControlAction action);
std::string control_binding_label(const ControlState& controls, InputDevice device,
                                  ControlAction action, ControllerLayout requested);
std::string controller_label_for_layout(std::string label, ControllerLayout layout);
ControllerLayout effective_controller_layout(const ControlState& controls,
                                             ControllerLayout requested);
const char* controller_layout_name(ControllerLayout layout);

#include "controls.hpp"

#include "inputs.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>
#include <iterator>
#include <string>

namespace {

constexpr int awc_profile_id = 7301;
constexpr int move_axis_id = 1;
constexpr int retired_quit_action_id = 9;
constexpr const char* awc_profile_name = "AdventuresWithChickensPrimaryV2";

void bind(BindsProfile& profile, GubsyButton button, ControlAction action) {
    bind_button(profile, button, static_cast<int>(action));
}

BindsProfile default_profile() {
    BindsProfile profile;
    profile.id = awc_profile_id;
    profile.name = awc_profile_name;

    // set keyboard defaults
    bind(profile, GubsyButton::KB_LEFT, ControlAction::left);
    bind(profile, GubsyButton::KB_A, ControlAction::left);
    bind(profile, GubsyButton::KB_RIGHT, ControlAction::right);
    bind(profile, GubsyButton::KB_D, ControlAction::right);
    bind(profile, GubsyButton::KB_UP, ControlAction::up);
    bind(profile, GubsyButton::KB_W, ControlAction::up);
    bind(profile, GubsyButton::KB_DOWN, ControlAction::down);
    bind(profile, GubsyButton::KB_S, ControlAction::down);
    bind(profile, GubsyButton::KB_ENTER, ControlAction::confirm);
    bind(profile, GubsyButton::KB_SPACE, ControlAction::confirm);
    bind(profile, GubsyButton::KB_Z, ControlAction::confirm);
    bind(profile, GubsyButton::KB_ESCAPE, ControlAction::back);
    bind(profile, GubsyButton::KB_ESCAPE, ControlAction::pause);
    bind(profile, GubsyButton::KB_X, ControlAction::back);
    bind(profile, GubsyButton::KB_LSHIFT, ControlAction::run);
    bind(profile, GubsyButton::KB_RSHIFT, ControlAction::run);
    bind(profile, GubsyButton::KB_LCTRL, ControlAction::attack);
    bind(profile, GubsyButton::KB_RCTRL, ControlAction::attack);
    bind(profile, GubsyButton::KB_SPACE, ControlAction::attack);
    bind(profile, GubsyButton::KB_P, ControlAction::pause);
    bind(profile, GubsyButton::KB_H, ControlAction::horn);
    bind(profile, GubsyButton::KB_Q, ControlAction::horn);
    bind(profile, GubsyButton::KB_F, ControlAction::horn);
    bind(profile, GubsyButton::KB_TAB, ControlAction::radar);
    bind(profile, GubsyButton::KB_DELETE, ControlAction::remove_player);

    // set gamepad defaults
    bind(profile, GubsyButton::GP_DPAD_LEFT, ControlAction::left);
    bind(profile, GubsyButton::GP_DPAD_RIGHT, ControlAction::right);
    bind(profile, GubsyButton::GP_DPAD_UP, ControlAction::up);
    bind(profile, GubsyButton::GP_DPAD_DOWN, ControlAction::down);
    bind(profile, GubsyButton::GP_A, ControlAction::confirm);
    bind(profile, GubsyButton::GP_A, ControlAction::attack);
    bind(profile, GubsyButton::GP_B, ControlAction::run);
    bind(profile, GubsyButton::GP_B, ControlAction::back);
    bind(profile, GubsyButton::GP_LEFT_SHOULDER, ControlAction::run);
    bind(profile, GubsyButton::GP_RIGHT_SHOULDER, ControlAction::run);
    bind(profile, GubsyButton::GP_X, ControlAction::horn);
    bind(profile, GubsyButton::GP_START, ControlAction::pause);
    bind(profile, GubsyButton::GP_BACK, ControlAction::radar);
    bind(profile, GubsyButton::GP_BACK, ControlAction::remove_player);
    bind_2d_analog(profile, Gubsy2DAnalog::GP_LEFT_STICK, move_axis_id);
    return profile;
}

bool gamepad_assigned(const GubsyLobbyState& lobby, int device_id) {
    for (const GubsyLobbyPlayer& player : lobby.local_players) {
        const bool found =
            std::ranges::any_of(player.devices, [device_id](GubsyLobbyDeviceAssignment device) {
                return device.type == InputSourceType::Gamepad && device.device_id == device_id;
            });
        if (found) {
            return true;
        }
    }
    return false;
}

bool action_down(GubsyRuntime& runtime, ControlAction action) {
    return gubsy_lobby_player_action_down(runtime, 0, static_cast<int>(action));
}

bool has_action(const BindsProfile& profile, ControlAction action) {
    return std::ranges::any_of(profile.button_binds(), [action](const auto& binding) {
        return binding.action == static_cast<int>(action);
    });
}

bool has_bind(const BindsProfile& profile, GubsyButton button, ControlAction action) {
    return std::ranges::any_of(profile.button_binds(), [button, action](const auto& binding) {
        return binding.device_button == static_cast<int>(button) &&
               binding.action == static_cast<int>(action);
    });
}

bool has_axis_bind(const BindsProfile& profile, Gubsy2DAnalog stick, int axis) {
    return std::ranges::any_of(profile.axis_2d_binds(), [stick, axis](const auto& binding) {
        return binding.device_stick == static_cast<int>(stick) && binding.axis_2d == axis;
    });
}

void remove_bind(BindsProfile& profile, GubsyButton button, ControlAction action) {
    (void)ginput::remove_button_bind(profile, {static_cast<int>(button), static_cast<int>(action)});
}

std::size_t action_index(ControlAction action) {
    const auto found = std::ranges::find(control_actions, action);
    return static_cast<std::size_t>(std::distance(control_actions.begin(), found));
}

std::string short_label(int code) {
    const auto button = static_cast<GubsyButton>(code);
    switch (button) {
    case GubsyButton::KB_LSHIFT:
        return "LSHIFT";
    case GubsyButton::KB_RSHIFT:
        return "RSHIFT";
    case GubsyButton::KB_LCTRL:
        return "LCTRL";
    case GubsyButton::KB_RCTRL:
        return "RCTRL";
    case GubsyButton::GP_LEFT_SHOULDER:
        return "LB";
    case GubsyButton::GP_RIGHT_SHOULDER:
        return "RB";
    case GubsyButton::GP_BACK:
        return "SELECT";
    case GubsyButton::GP_START:
        return "START";
    case GubsyButton::GP_DPAD_LEFT:
        return "LEFT";
    case GubsyButton::GP_DPAD_RIGHT:
        return "RIGHT";
    case GubsyButton::GP_DPAD_UP:
        return "UP";
    case GubsyButton::GP_DPAD_DOWN:
        return "DOWN";
    default:
        break;
    }
    std::string label = binds_input_label(BindsActionType::Button, code);
    constexpr std::array prefixes{"Keyboard ", "Gamepad "};
    for (const char* prefix : prefixes) {
        if (label.starts_with(prefix)) {
            label.erase(0, std::char_traits<char>::length(prefix));
        }
    }
    return label;
}

void append_label(std::string& labels, const std::string& label) {
    const std::string padded = " / " + labels + " / ";
    if (label.empty() || padded.find(" / " + label + " / ") != std::string::npos) {
        return;
    }
    if (!labels.empty()) {
        labels += " / ";
    }
    labels += label;
}

bool row_has_action(std::size_t row, ControlAction action) {
    switch (row) {
    case 0:
        return action == ControlAction::left || action == ControlAction::right ||
               action == ControlAction::up || action == ControlAction::down;
    case 1:
        return action == ControlAction::confirm;
    case 2:
        return action == ControlAction::attack;
    case 3:
        return action == ControlAction::horn;
    case 4:
        return action == ControlAction::run;
    case 5:
        return action == ControlAction::radar;
    case 6:
        return action == ControlAction::pause;
    default:
        return false;
    }
}

void refresh_control_labels(GubsyRuntime& runtime, ControlState& controls) {
    const BindsProfile* profile = gubsy_find_binds_profile(runtime, awc_profile_id);
    if (profile == nullptr) {
        controls.labels_hash = 0;
        controls.keyboard_action_labels = {};
        controls.controller_action_labels = {};
        controls.keyboard_labels = {};
        controls.controller_labels = {};
        return;
    }

    // skip unchanged binds
    std::uint64_t labels_hash = 1469598103934665603ULL;
    for (const auto& binding : profile->button_binds()) {
        labels_hash ^= static_cast<std::uint64_t>(binding.device_button);
        labels_hash *= 1099511628211ULL;
        labels_hash ^= static_cast<std::uint64_t>(binding.action);
        labels_hash *= 1099511628211ULL;
    }
    for (const auto& binding : profile->axis_2d_binds()) {
        labels_hash ^= static_cast<std::uint64_t>(binding.device_stick);
        labels_hash *= 1099511628211ULL;
        labels_hash ^= static_cast<std::uint64_t>(binding.axis_2d);
        labels_hash *= 1099511628211ULL;
    }
    if (labels_hash == controls.labels_hash) {
        return;
    }
    controls.labels_hash = labels_hash;

    // read current binds for controls pages
    controls.keyboard_action_labels = {};
    controls.controller_action_labels = {};
    controls.keyboard_labels = {};
    controls.controller_labels = {};
    const int gamepad_first = static_cast<int>(GubsyButton::GP_A);
    int keyboard_move_binds = 0;
    int controller_move_binds = 0;
    for (const auto& binding : profile->button_binds()) {
        const auto action = static_cast<ControlAction>(binding.action);
        const std::size_t index = action_index(action);
        std::string& action_labels = binding.device_button >= gamepad_first
                                         ? controls.controller_action_labels[index]
                                         : controls.keyboard_action_labels[index];
        append_label(action_labels, short_label(binding.device_button));
        for (std::size_t row = 0; row < controls.keyboard_labels.size(); ++row) {
            if (!row_has_action(row, action)) {
                continue;
            }
            std::string& labels = binding.device_button >= gamepad_first
                                      ? controls.controller_labels[row]
                                      : controls.keyboard_labels[row];
            append_label(labels, short_label(binding.device_button));
            if (row == 0) {
                if (binding.device_button >= gamepad_first) {
                    ++controller_move_binds;
                } else {
                    ++keyboard_move_binds;
                }
            }
        }
    }

    // collapse standard four-way groups
    const bool has_arrows = has_bind(*profile, GubsyButton::KB_LEFT, ControlAction::left) &&
                            has_bind(*profile, GubsyButton::KB_RIGHT, ControlAction::right) &&
                            has_bind(*profile, GubsyButton::KB_UP, ControlAction::up) &&
                            has_bind(*profile, GubsyButton::KB_DOWN, ControlAction::down);
    const bool has_wasd = has_bind(*profile, GubsyButton::KB_A, ControlAction::left) &&
                          has_bind(*profile, GubsyButton::KB_D, ControlAction::right) &&
                          has_bind(*profile, GubsyButton::KB_W, ControlAction::up) &&
                          has_bind(*profile, GubsyButton::KB_S, ControlAction::down);
    const bool has_dpad = has_bind(*profile, GubsyButton::GP_DPAD_LEFT, ControlAction::left) &&
                          has_bind(*profile, GubsyButton::GP_DPAD_RIGHT, ControlAction::right) &&
                          has_bind(*profile, GubsyButton::GP_DPAD_UP, ControlAction::up) &&
                          has_bind(*profile, GubsyButton::GP_DPAD_DOWN, ControlAction::down);
    const bool has_left_stick = has_axis_bind(*profile, Gubsy2DAnalog::GP_LEFT_STICK, move_axis_id);
    const int standard_keyboard_move_binds = (has_arrows ? 4 : 0) + (has_wasd ? 4 : 0);
    if (keyboard_move_binds == standard_keyboard_move_binds && standard_keyboard_move_binds > 0) {
        controls.keyboard_labels[0].clear();
        if (has_arrows) {
            append_label(controls.keyboard_labels[0], "ARROWS");
        }
        if (has_wasd) {
            append_label(controls.keyboard_labels[0], "WASD");
        }
    }
    if (has_dpad && controller_move_binds == 4) {
        controls.controller_labels[0].clear();
        if (has_left_stick) {
            append_label(controls.controller_labels[0], "LEFT STICK");
        }
        append_label(controls.controller_labels[0], "D-PAD");
    } else if (has_left_stick) {
        append_label(controls.controller_labels[0], "LEFT STICK");
    }

    // include direct trigger bindings
    append_label(controls.controller_labels[4], "LT");
    append_label(controls.controller_labels[4], "RT");
    append_label(controls.controller_action_labels[action_index(ControlAction::run)], "LT");
    append_label(controls.controller_action_labels[action_index(ControlAction::run)], "RT");
}

} // namespace

bool register_controls(GubsyRuntime& runtime) {
    // register actions
    BindsSchema schema;
    for (const ControlAction action : control_actions) {
        (void)schema.add_action(static_cast<int>(action), control_action_name(action), "Game");
    }
    (void)schema.add_axis_2d(move_axis_id, "Move ship", "Game");
    gubsy_register_binds_schema(runtime, schema);

    // load control profile
    const BindsProfile* installed = gubsy_find_binds_profile(runtime, awc_profile_id);
    if (installed == nullptr) {
        if (!gubsy_replace_binds_profile(runtime, default_profile())) {
            return false;
        }
    } else {
        BindsProfile migrated = *installed;
        bool changed = false;
        const bool has_retired_quit_bind =
            std::ranges::any_of(migrated.button_binds(), [](const auto& binding) {
                return binding.action == retired_quit_action_id;
            });
        if (has_retired_quit_bind) {
            remove_binds_for_action(migrated, BindsActionType::Button, retired_quit_action_id);
            changed = true;
        }
        if (migrated.name != awc_profile_name) {
            constexpr std::array modern_keyboard_defaults{
                std::pair{GubsyButton::KB_SPACE, ControlAction::attack},
                std::pair{GubsyButton::KB_Q, ControlAction::horn},
                std::pair{GubsyButton::KB_F, ControlAction::horn},
            };
            for (const auto& [button, action] : modern_keyboard_defaults) {
                if (!has_bind(migrated, button, action)) {
                    bind(migrated, button, action);
                }
            }
            if (!has_axis_bind(migrated, Gubsy2DAnalog::GP_LEFT_STICK, move_axis_id)) {
                bind_2d_analog(migrated, Gubsy2DAnalog::GP_LEFT_STICK, move_axis_id);
            }
            migrated.name = awc_profile_name;
            changed = true;
        }
        if (!has_action(*installed, ControlAction::attack)) {
            remove_bind(migrated, GubsyButton::KB_LCTRL, ControlAction::run);
            remove_bind(migrated, GubsyButton::KB_RCTRL, ControlAction::run);
            remove_bind(migrated, GubsyButton::GP_X, ControlAction::horn);
            bind(migrated, GubsyButton::KB_LSHIFT, ControlAction::run);
            bind(migrated, GubsyButton::KB_RSHIFT, ControlAction::run);
            bind(migrated, GubsyButton::KB_LCTRL, ControlAction::attack);
            bind(migrated, GubsyButton::KB_RCTRL, ControlAction::attack);
            bind(migrated, GubsyButton::GP_X, ControlAction::attack);
            bind(migrated, GubsyButton::GP_Y, ControlAction::horn);
            changed = true;
        }
        if (has_bind(migrated, GubsyButton::KB_BACKSPACE, ControlAction::back)) {
            remove_bind(migrated, GubsyButton::KB_BACKSPACE, ControlAction::back);
            changed = true;
        }
        if (!has_action(migrated, ControlAction::radar)) {
            bind(migrated, GubsyButton::KB_TAB, ControlAction::radar);
            bind(migrated, GubsyButton::GP_BACK, ControlAction::radar);
            changed = true;
        }
        if (!has_bind(migrated, GubsyButton::KB_ESCAPE, ControlAction::pause)) {
            bind(migrated, GubsyButton::KB_ESCAPE, ControlAction::pause);
            changed = true;
        }
        if (has_bind(migrated, GubsyButton::GP_X, ControlAction::attack)) {
            remove_bind(migrated, GubsyButton::GP_X, ControlAction::attack);
            changed = true;
        }
        if (has_bind(migrated, GubsyButton::GP_Y, ControlAction::horn)) {
            remove_bind(migrated, GubsyButton::GP_Y, ControlAction::horn);
            changed = true;
        }
        constexpr std::array modern_gamepad_defaults{
            std::pair{GubsyButton::GP_A, ControlAction::attack},
            std::pair{GubsyButton::GP_B, ControlAction::run},
            std::pair{GubsyButton::GP_B, ControlAction::back},
            std::pair{GubsyButton::GP_LEFT_SHOULDER, ControlAction::run},
            std::pair{GubsyButton::GP_RIGHT_SHOULDER, ControlAction::run},
            std::pair{GubsyButton::GP_X, ControlAction::horn},
            std::pair{GubsyButton::GP_BACK, ControlAction::remove_player},
        };
        for (const auto& [button, action] : modern_gamepad_defaults) {
            if (!has_bind(migrated, button, action)) {
                bind(migrated, button, action);
                changed = true;
            }
        }
        if (changed && !gubsy_replace_binds_profile(runtime, migrated)) {
            return false;
        }
    }
    return gubsy_set_lobby_player_binds_profile(runtime, 0, awc_profile_id);
}

void assign_unclaimed_gamepads(GubsyRuntime& runtime) {
    // assign gamepads
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr) {
        return;
    }
    for (int index = 0; index < count; ++index) {
        const int device_id = static_cast<int>(gamepads[index]);
        if (!gamepad_assigned(gubsy_get_lobby_state(runtime), device_id)) {
            gubsy_toggle_lobby_player_device(runtime, 0, {InputSourceType::Gamepad, device_id});
        }
    }
    SDL_free(gamepads);
}

void detect_controller_layout(ControlState& controls) {
    // detect first connected controller family
    controls.detected_layout = ControllerLayout::xbox;
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr || count == 0) {
        SDL_free(gamepads);
        return;
    }
    switch (SDL_GetGamepadTypeForID(gamepads[0])) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        controls.detected_layout = ControllerLayout::playstation;
        break;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        controls.detected_layout = ControllerLayout::nintendo;
        break;
    default:
        break;
    }
    SDL_free(gamepads);
}

void sample_controls(GubsyRuntime& runtime, ControlState& controls, InputState& input) {
    refresh_control_labels(runtime, controls);

    // sample mapped input
    constexpr float stick_threshold = 0.4F;
    const ginput::Vec2 move_axis = gubsy_lobby_player_axis_2d(runtime, 0, move_axis_id);
    std::array<bool, control_actions.size()> current{};
    for (const ControlAction action : control_actions) {
        const std::size_t index = action_index(action);
        current[index] = action_down(runtime, action);
        switch (action) {
        case ControlAction::left:
            current[index] = current[index] || move_axis.x < -stick_threshold;
            break;
        case ControlAction::right:
            current[index] = current[index] || move_axis.x > stick_threshold;
            break;
        case ControlAction::up:
            current[index] = current[index] || move_axis.y < -stick_threshold;
            break;
        case ControlAction::down:
            current[index] = current[index] || move_axis.y > stick_threshold;
            break;
        default:
            break;
        }
        const bool pressed = current[index] && !controls.previous[index];

        // set input state
        switch (action) {
        case ControlAction::left:
            input.left_held = current[index];
            input.left_pressed = input.left_pressed || pressed;
            break;
        case ControlAction::right:
            input.right_held = current[index];
            input.right_pressed = input.right_pressed || pressed;
            break;
        case ControlAction::up:
            input.up_held = current[index];
            input.up_pressed = input.up_pressed || pressed;
            break;
        case ControlAction::down:
            input.down_held = current[index];
            input.down_pressed = input.down_pressed || pressed;
            break;
        case ControlAction::confirm:
            input.confirm_pressed = input.confirm_pressed || pressed;
            input.action_held = current[index];
            break;
        case ControlAction::back:
            input.back_pressed = input.back_pressed || pressed;
            break;
        case ControlAction::run:
            input.run_held =
                current[index] || input.left_trigger_run_held || input.right_trigger_run_held;
            break;
        case ControlAction::attack:
            input.attack_pressed = input.attack_pressed || pressed;
            input.attack_held = current[index];
            break;
        case ControlAction::pause:
            input.pause_pressed = input.pause_pressed || pressed;
            break;
        case ControlAction::horn:
            input.horn_pressed = input.horn_pressed || pressed;
            input.horn_held = current[index];
            break;
        case ControlAction::radar:
            input.radar_pressed = input.radar_pressed || pressed;
            break;
        case ControlAction::remove_player:
            input.remove_pressed = input.remove_pressed || pressed;
            break;
        }
    }
    controls.previous = current;
}

bool reset_controls(GubsyRuntime& runtime) {
    return gubsy_replace_binds_profile(runtime, default_profile()) &&
           gubsy_set_lobby_player_binds_profile(runtime, 0, awc_profile_id);
}

int control_profile_id() {
    return awc_profile_id;
}

const char* control_action_name(ControlAction action) {
    switch (action) {
    case ControlAction::left:
        return "Move left";
    case ControlAction::right:
        return "Move right";
    case ControlAction::up:
        return "Move up";
    case ControlAction::down:
        return "Move down";
    case ControlAction::confirm:
        return "Confirm / action";
    case ControlAction::back:
        return "Back";
    case ControlAction::run:
        return "Run / fast movement";
    case ControlAction::attack:
        return "Short stunned alien";
    case ControlAction::pause:
        return "Pause";
    case ControlAction::horn:
        return "Horn";
    case ControlAction::radar:
        return "Toggle radar";
    case ControlAction::remove_player:
        return "Remove player";
    }
    return "Unknown";
}

ControllerLayout effective_controller_layout(const ControlState& controls,
                                             ControllerLayout requested) {
    return requested == ControllerLayout::automatic ? controls.detected_layout : requested;
}

const char* controller_layout_name(ControllerLayout layout) {
    switch (layout) {
    case ControllerLayout::automatic:
        return "Auto";
    case ControllerLayout::xbox:
        return "Xbox";
    case ControllerLayout::playstation:
        return "PlayStation";
    case ControllerLayout::nintendo:
        return "Nintendo";
    }
    return "Xbox";
}

std::string controller_label_for_layout(std::string label, ControllerLayout layout) {
    if (layout == ControllerLayout::automatic || layout == ControllerLayout::xbox) {
        return label;
    }

    // replace standardized controller positions with family labels
    constexpr std::string_view separator = " / ";
    std::size_t start = 0;
    while (start <= label.size()) {
        const std::size_t end = label.find(separator, start);
        std::string token = label.substr(start, end - start);
        if (layout == ControllerLayout::playstation) {
            if (token == "A") {
                token = "CROSS";
            } else if (token == "B") {
                token = "CIRCLE";
            } else if (token == "X") {
                token = "SQUARE";
            } else if (token == "Y") {
                token = "TRIANGLE";
            } else if (token == "LB") {
                token = "L1";
            } else if (token == "RB") {
                token = "R1";
            } else if (token == "LT") {
                token = "L2";
            } else if (token == "RT") {
                token = "R2";
            } else if (token == "BACK" || token == "SELECT") {
                token = "SELECT";
            } else if (token == "START") {
                token = "OPTIONS";
            }
        } else if (layout == ControllerLayout::nintendo) {
            if (token == "A") {
                token = "B";
            } else if (token == "B") {
                token = "A";
            } else if (token == "X") {
                token = "Y";
            } else if (token == "Y") {
                token = "X";
            } else if (token == "LB") {
                token = "L";
            } else if (token == "RB") {
                token = "R";
            } else if (token == "LT") {
                token = "ZL";
            } else if (token == "RT") {
                token = "ZR";
            } else if (token == "BACK" || token == "SELECT") {
                token = "MINUS";
            } else if (token == "START") {
                token = "PLUS";
            }
        }
        label.replace(start, end == std::string::npos ? label.size() - start : end - start, token);
        if (end == std::string::npos) {
            break;
        }
        start += token.size() + separator.size();
    }
    return label;
}

std::string control_binding_label(const ControlState& controls, InputDevice device,
                                  ControlAction action, ControllerLayout requested) {
    const std::size_t index = action_index(action);
    if (device == InputDevice::keyboard) {
        return controls.keyboard_action_labels[index];
    }
    return controller_label_for_layout(controls.controller_action_labels[index],
                                       effective_controller_layout(controls, requested));
}

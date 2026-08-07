#include "../audio.hpp"
#include "../inputs.hpp"
#include "detail.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace step::detail {

constexpr std::uint64_t rocksolid_splash_frames = 258;
constexpr std::uint64_t xgames_splash_frames = 191;
constexpr int title_choice_count = 5;
constexpr int options_choice_count = 6;
constexpr int keyboard_columns = 10;
constexpr int keyboard_cells = 30;
constexpr std::string_view keyboard_letters = "QWERTYUIOPASDFGHJKLZXCVBNM";

int keyboard_letter_index(int cursor) {
    if (cursor >= 0 && cursor <= 18) {
        return cursor;
    }
    if (cursor >= 20 && cursor <= 26) {
        return cursor - 1;
    }
    return -1;
}

void append_controller_character(State& game, PlayerSave& player, char character, Audio* audio) {
    const unsigned char code = static_cast<unsigned char>(character);
    if (code < 32 || code > 126 || character == ',' || player.name.size() >= 12) {
        play_sound(game, audio, Sound::error);
        return;
    }
    if (player.active == 0) {
        player = {.active = 1, .name = {}, .score = 0, .level = 1, .chickens = 0};
    }
    player.name.push_back(character);
    game.player_select_dirty = true;
    play_sound(game, audio, Sound::clunk);
}

void step_startup(State& game, const InputState& input) {
    // step publisher cards
    const std::uint64_t duration = game.startup_mode == StartupMode::rocksolid
                                       ? rocksolid_splash_frames
                                       : xgames_splash_frames;
    if (game.mode_frame < duration && !input.confirm_pressed && !input.back_pressed) {
        return;
    }

    // advance publisher card
    if (game.startup_mode == StartupMode::rocksolid) {
        game.startup_mode = StartupMode::egames;
        game.mode_frame = 0;
        return;
    }

    change_mode(game, Mode::title);
}

void step_title(State& game, const InputState& input, Audio* audio) {
    // move title cursor
    int choice = static_cast<int>(game.title_choice);
    if (input.up_pressed && choice > 0) {
        --choice;
        play_sound(game, audio, Sound::clink);
    }
    if (input.down_pressed && choice + 1 < title_choice_count) {
        ++choice;
        play_sound(game, audio, Sound::clink);
    }
    game.title_choice = static_cast<TitleChoice>(choice);

    // handle title cancel
    if (input.back_pressed && game.title_choice != TitleChoice::exit_game) {
        game.title_choice = TitleChoice::exit_game;
        play_sound(game, audio, Sound::clink);
        return;
    }
    if (!input.confirm_pressed) {
        return;
    }

    // select title item
    switch (game.title_choice) {
    case TitleChoice::start_game:
        play_sound(game, audio, Sound::warp);
        game.player_select_mode = PlayerSelectMode::choose_player;
        game.player_select_dirty = false;
        change_mode(game, Mode::player_select);
        break;
    case TitleChoice::instructions:
        play_sound(game, audio, Sound::warp);
        game.instructions_page = InstructionsPage::original;
        change_mode(game, Mode::instructions);
        break;
    case TitleChoice::options:
        play_sound(game, audio, Sound::warp);
        game.options_choice = OptionsChoice::master_volume;
        change_mode(game, Mode::options);
        break;
    case TitleChoice::credits:
        play_sound(game, audio, Sound::warp);
        change_mode(game, Mode::credits);
        play_sound(game, audio, Sound::applause);
        break;
    case TitleChoice::exit_game:
        play_sound(game, audio, Sound::attic_door);
        change_mode(game, Mode::exiting);
        break;
    }
}

void step_frontend_page(State& game, const InputState& input, Audio* audio) {
    // step static page
    if (game.mode == Mode::credits) {
        if (input.confirm_pressed || input.back_pressed || game.mode_frame >= 1800) {
            change_mode(game, Mode::title);
        }
        return;
    }
    if (game.mode == Mode::instructions) {
        int page = static_cast<int>(game.instructions_page);
        const int old_page = page;
        if (input.left_pressed && page > static_cast<int>(InstructionsPage::original)) {
            --page;
        }
        if (input.right_pressed && page < static_cast<int>(InstructionsPage::controller)) {
            ++page;
        }
        if (page != old_page) {
            game.instructions_page = static_cast<InstructionsPage>(page);
            play_sound(game, audio, Sound::clink);
            return;
        }
    }
    if (input.confirm_pressed || input.back_pressed) {
        play_sound(game, audio, Sound::pickup);
        change_mode(game, Mode::title);
    }
}

void step_options(State& game, const InputState& input, Audio* audio) {
    // move options cursor
    int choice = static_cast<int>(game.options_choice);
    if (input.up_pressed && choice > 0) {
        --choice;
        play_sound(game, audio, Sound::clink);
    }
    if (input.down_pressed && choice + 1 < options_choice_count) {
        ++choice;
        play_sound(game, audio, Sound::clink);
    }
    game.options_choice = static_cast<OptionsChoice>(choice);

    // close options
    if (input.back_pressed) {
        play_sound(game, audio, Sound::yeah);
        persist_save(game, "options");
        change_mode(game, Mode::title);
        return;
    }

    // handle master volume
    if (game.options_choice == OptionsChoice::master_volume) {
        if (input.left_pressed || input.right_pressed) {
            game.master_volume =
                std::clamp(game.master_volume + (input.left_pressed ? -10 : 10), 0, 100);
            if (audio != nullptr) {
                audio->set_master_volume(static_cast<float>(game.master_volume) / 100.0F);
            }
            save_remaster_settings(game);
            play_sound(game, audio, Sound::move);
        }
        return;
    }

    // change option value
    const bool decrease = input.left_pressed;
    const bool increase = input.right_pressed;
    if (!decrease && !increase) {
        return;
    }

    switch (game.options_choice) {
    case OptionsChoice::master_volume:
        break;
    case OptionsChoice::difficulty:
        game.save.difficulty = std::clamp(game.save.difficulty + (decrease ? -1 : 1), 0, 4);
        play_sound(game, audio, Sound::clunk);
        break;
    case OptionsChoice::sound: {
        const bool value = increase;
        if (value != game.save.sound) {
            if (game.save.sound) {
                play_sound(game, audio, Sound::clunk);
            }
            game.save.sound = value;
            if (game.save.sound) {
                play_sound(game, audio, Sound::clunk);
            }
        }
        break;
    }
    case OptionsChoice::music: {
        const bool value = increase;
        if (value != game.save.music) {
            game.save.music = value;
            play_sound(game, audio, Sound::clunk);
        }
        break;
    }
    case OptionsChoice::use_joystick: {
        const bool value = increase;
        if (value != game.save.use_joystick) {
            game.save.use_joystick = value;
            play_sound(game, audio, Sound::clunk);
        }
        break;
    }
    case OptionsChoice::always_run: {
        const bool value = increase;
        if (value != game.save.always_run) {
            game.save.always_run = value;
            play_sound(game, audio, Sound::clunk);
        }
        break;
    }
    }
}

void step_player_select(State& game, const InputState& input, Audio* audio) {
    switch (game.player_select_mode) {
    case PlayerSelectMode::choose_player: {
        // move player cursor
        const bool typing =
            input.last_device == InputDevice::keyboard && !input.text_entered.empty();
        if (!typing && input.up_pressed && game.selected_player > 0) {
            --game.selected_player;
            play_sound(game, audio, Sound::clink);
        }
        if (!typing && input.down_pressed && game.selected_player + 1 < player_slot_count) {
            ++game.selected_player;
            play_sound(game, audio, Sound::clink);
        }

        // edit player name
        PlayerSave& player = game.save.players[static_cast<std::size_t>(game.selected_player)];
        for (const char character : input.text_entered) {
            const unsigned char code = static_cast<unsigned char>(character);
            if (code < 32 || code > 126 || character == ',' || player.name.size() >= 12) {
                continue;
            }
            if (player.active == 0) {
                player = {.active = 1, .name = {}, .score = 0, .level = 1, .chickens = 0};
            }
            player.name.push_back(character);
            game.player_select_dirty = true;
            play_sound(game, audio, Sound::clunk);
        }
        if (input.backspace_pressed && player.active != 0 && !player.name.empty()) {
            player.name.pop_back();
            game.player_select_dirty = true;
            play_sound(game, audio, Sound::clunk);
        }

        // handle player action
        if (input.remove_pressed && player.active != 0) {
            game.player_select_mode = PlayerSelectMode::confirm_delete;
            play_sound(game, audio, Sound::clink);
            break;
        }
        if (!typing && input.last_device == InputDevice::controller && input.confirm_pressed &&
            player.active == 0) {
            game.controller_keyboard_cursor = 0;
            game.controller_keyboard_uppercase = true;
            game.player_select_mode = PlayerSelectMode::controller_keyboard;
            play_sound(game, audio, Sound::clink);
            break;
        }
        if (!typing && input.confirm_pressed && player.active != 0 && !player.name.empty()) {
            if (game.player_select_dirty) {
                persist_save(game, "player name");
                game.player_select_dirty = false;
            }
            game.current_level = std::clamp(player.level, 1, level_count);
            play_sound(game, audio, Sound::chicken2);
            start_level(game, audio);
        } else if (input.back_pressed) {
            if (game.player_select_dirty) {
                persist_save(game, "player name");
                game.player_select_dirty = false;
            }
            play_sound(game, audio, Sound::pickup);
            change_mode(game, Mode::title);
        }
        break;
    }
    case PlayerSelectMode::confirm_delete:
        // confirm player delete
        if (input.confirm_pressed) {
            play_sound(game, audio, Sound::alien_hit);
            game.save.players[static_cast<std::size_t>(game.selected_player)] = {};
            persist_save(game, "deleted player");
            game.player_select_dirty = false;
            game.player_select_mode = PlayerSelectMode::choose_player;
        } else if (input.back_pressed) {
            play_sound(game, audio, Sound::yeah);
            game.player_select_mode = PlayerSelectMode::choose_player;
        }
        break;
    case PlayerSelectMode::controller_keyboard: {
        // move keyboard cursor
        int cursor = game.controller_keyboard_cursor;
        const bool controller_input = input.last_device == InputDevice::controller;
        if (controller_input && input.pause_pressed) {
            cursor = keyboard_cells - 1;
        } else {
            if (controller_input && input.left_pressed && cursor % keyboard_columns != 0) {
                --cursor;
            }
            if (controller_input && input.right_pressed &&
                cursor % keyboard_columns != keyboard_columns - 1) {
                ++cursor;
            }
            if (controller_input && input.up_pressed && cursor >= keyboard_columns) {
                cursor -= keyboard_columns;
            }
            if (controller_input && input.down_pressed &&
                cursor + keyboard_columns < keyboard_cells) {
                cursor += keyboard_columns;
            }
        }
        if (cursor != game.controller_keyboard_cursor) {
            game.controller_keyboard_cursor = cursor;
            play_sound(game, audio, Sound::clink);
        }

        // apply keyboard key
        PlayerSave& player = game.save.players[static_cast<std::size_t>(game.selected_player)];
        for (const char character : input.text_entered) {
            append_controller_character(game, player, character, audio);
        }
        if (input.backspace_pressed && player.active != 0 && !player.name.empty()) {
            player.name.pop_back();
            game.player_select_dirty = true;
            play_sound(game, audio, Sound::clunk);
        }
        if (controller_input && input.confirm_pressed) {
            const int letter = keyboard_letter_index(cursor);
            if (letter >= 0) {
                char character = keyboard_letters[static_cast<std::size_t>(letter)];
                if (!game.controller_keyboard_uppercase) {
                    character = static_cast<char>(character - 'A' + 'a');
                }
                append_controller_character(game, player, character, audio);
            } else if (cursor == 19) {
                if (player.active != 0 && !player.name.empty()) {
                    player.name.pop_back();
                    game.player_select_dirty = true;
                    play_sound(game, audio, Sound::clunk);
                }
            } else if (cursor == 27) {
                append_controller_character(game, player, ' ', audio);
            } else if (cursor == 28) {
                game.controller_keyboard_uppercase = !game.controller_keyboard_uppercase;
                play_sound(game, audio, Sound::clunk);
            } else if (cursor == 29 && player.active != 0 && !player.name.empty()) {
                persist_save(game, "controller player name");
                game.player_select_dirty = false;
                game.player_select_mode = PlayerSelectMode::choose_player;
                play_sound(game, audio, Sound::yeah);
            }
        }
        if (!controller_input && input.text_entered.empty() && input.confirm_pressed &&
            player.active != 0 && !player.name.empty()) {
            persist_save(game, "keyboard player name");
            game.player_select_dirty = false;
            game.player_select_mode = PlayerSelectMode::choose_player;
            play_sound(game, audio, Sound::yeah);
        }
        if (input.back_pressed && input.text_entered.empty()) {
            if (player.active != 0 && player.name.empty()) {
                player = {};
            }
            game.player_select_mode = PlayerSelectMode::choose_player;
            play_sound(game, audio, Sound::pickup);
        }
        break;
    }
    }
}

void step_congratulations(State& game, const InputState& input, Audio* audio) {
    // step ending
    switch (game.congratulations_mode) {
    case CongratulationsMode::waiting:
        if (game.mode_frame >= 240) {
            game.congratulations_mode = CongratulationsMode::active;
            play_sound(game, audio, Sound::excellent);
        }
        break;
    case CongratulationsMode::active:
        if (input.back_pressed) {
            change_mode(game, Mode::title);
        }
        break;
    }
}

void step_exiting(State& game) {
    if (game.mode_frame >= 60) {
        game.exit_requested = true;
    }
}

} // namespace step::detail

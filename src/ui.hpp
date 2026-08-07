#pragma once

class Audio;
struct State;
class GubsyRuntime;
struct SDL_Window;

enum class ToolWorkspace {
    hidden,
    settings,
    tester,
};

struct UiState {
    ToolWorkspace workspace{ToolWorkspace::hidden};
    bool launcher_visible{true};
    bool display_visible{true};
    bool enhancements_visible{true};
    bool audio_visible{true};
    bool controls_visible{true};
    bool state_visible{true};
    bool setup_visible{true};
    bool objects_visible{true};
    bool arrange_requested{};
};

void toggle_settings_workspace(UiState& ui);
void toggle_tester_workspace(UiState& ui);
bool ui_owns_game_input(const UiState& ui);
void draw_ui(UiState& ui, GubsyRuntime& runtime, SDL_Window* window, Audio& audio, State& game);

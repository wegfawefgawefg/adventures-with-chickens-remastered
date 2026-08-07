#pragma once

#include <string>

class GubsyRuntime;
struct SDL_Renderer;
struct SDL_Window;
struct State;

bool setup_gubsy(GubsyRuntime& runtime, SDL_Window* window, SDL_Renderer* renderer,
                 const State& state, std::string& imgui_ini_path);
void shutdown_gubsy(GubsyRuntime& runtime);

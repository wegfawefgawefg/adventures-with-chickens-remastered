#include "window.hpp"

#include <SDL3/SDL.h>

Window::Window(const char* title, int width, int height) {
    // init SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        return;
    }

    // create floating window
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_UTILITY;
    mWindow = SDL_CreateWindow(title, width, height, flags);
    if (mWindow != nullptr) {
        (void)SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

Window::~Window() {
    // shutdown SDL
    SDL_DestroyWindow(mWindow);
    SDL_Quit();
}

SDL_Window* Window::handle() const {
    return mWindow;
}

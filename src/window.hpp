#pragma once

struct SDL_Window;

class Window {
  public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* handle() const;

  private:
    SDL_Window* mWindow{};
};

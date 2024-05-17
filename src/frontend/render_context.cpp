#include "render_context.hpp"

#include "SDL.h"

RenderContext::RenderContext(SDL_Window* sdl_window, UpdateGuiCallback update_gui)
  : sdl_window(sdl_window),
    update_gui(update_gui),
    frame_counter{}
{
}

SDL_Window* RenderContext::GetWindow()
{
    return sdl_window;
}

void RenderContext::UpdateFrameCounter()
{
    if (++frame_counter == 60) {
        static std::chrono::time_point time = std::chrono::steady_clock::now();
        auto microsecs_to_render_60_frames =
          std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time).count();
        [[maybe_unused]] float fps = 60.0f * 1'000'000.0f / float(microsecs_to_render_60_frames);
        // UpdateWindowTitle(fps);
        frame_counter = 0;
        time = std::chrono::steady_clock::now();
    }
}

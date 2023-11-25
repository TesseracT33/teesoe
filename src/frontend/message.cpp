#include "frontend/message.hpp"
#include "log.hpp"
#include "status.hpp"

#include "SDL.h"

#include <cstdlib>
#include <format>
#include <iostream>
#include <string>

namespace message {

static SDL_Window* sdl_window; /* Must be set via 'Init' before any messages are shown. */

void Error(std::string_view message)
{
    log_error(message);
    if (sdl_window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.data(), sdl_window);
    }
}

void Fatal(std::string_view message /*, std::source_location loc*/)
{
    log_fatal(message /*, loc*/);
    if (sdl_window) {
        /* std::string shown_message = std::format("Fatal Error at {}({}:{}), function {}: {}",
          loc.file_name(),
          loc.line(),
          loc.column(),
          loc.function_name(),
          message);*/
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal", message.data(), sdl_window);
    }
    exit(EXIT_FAILURE);
}

void Info(std::string_view message)
{
    log_info(message);
    if (sdl_window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Information", message.data(), sdl_window);
    }
}

Status Init(SDL_Window* sdl_window_arg)
{
    if (sdl_window_arg) {
        sdl_window = sdl_window_arg;
        return OkStatus();
    } else {
        return FailureStatus("nullptr SDL_Window given as argument.");
    }
}

void Warn(std::string_view message)
{
    log_warn(message);
    if (sdl_window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", message.data(), sdl_window);
    }
}

} // namespace message

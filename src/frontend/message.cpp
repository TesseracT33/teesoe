#include "message.hpp"
#include "log.hpp"
#include "status.hpp"

#include "SDL.h"

#include <format>
#include <iostream>
#include <string>

namespace message {

static SDL_Window* sdl_window; /* Must be set via 'Init' before any messages are shown. */

void error(std::string_view message)
{
    log_error(message);
    if (sdl_window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.data(), sdl_window);
    }
}

void fatal(std::string_view message /*, std::source_location loc*/)
{
    log_fatal(message /*, loc*/);
    if (sdl_window) {
        /* std::string shown_message = std::format("Fatal error at {}({}:{}), function {}: {}",
          loc.file_name(),
          loc.line(),
          loc.column(),
          loc.function_name(),
          message);*/
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal", message.data(), sdl_window);
    }
}

void info(std::string_view message)
{
    log_info(message);
    if (sdl_window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Information", message.data(), sdl_window);
    }
}

Status init(SDL_Window* sdl_window_arg)
{
    if (sdl_window_arg) {
        sdl_window = sdl_window_arg;
        return status_ok();
    } else {
        return status_failure("nullptr SDL_Window given as argument.");
    }
}

void warning(std::string_view message)
{
    log_warn(message);
    if (sdl_window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", message.data(), sdl_window);
    }
}

} // namespace message

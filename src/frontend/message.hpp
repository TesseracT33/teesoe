#pragma once

#include <source_location>
#include <string_view>

struct SDL_Window;
class Status;

namespace message {

void Error(std::string_view message);
void Fatal(std::string_view message /*,std::source_location loc = std::source_location::current()*/);
void Info(std::string_view message);
Status Init(SDL_Window* sdl_window);
void Warn(std::string_view message);

} // namespace message

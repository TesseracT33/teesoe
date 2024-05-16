#pragma once

#include "SDL.h"
#include "status.hpp"

#include <filesystem>

namespace frontend::gui {

SDL_Window* GetSdlWindow();
void GetWindowSize(int* w, int* h);
Status Init(std::filesystem::path work_path);
Status LoadGame(std::filesystem::path const& rom_path);
void OnCtrlKeyPress(SDL_Keycode keycode);
void PollEvents();
void Run(bool boot_game_immediately = false);

} // namespace frontend::gui

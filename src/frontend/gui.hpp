#pragma once

#include "status.hpp"

#include "imgui_impl_vulkan.h"
#include "SDL.h"

#include <filesystem>

namespace frontend::gui {

void FrameVulkan(VkCommandBuffer vk_command_buffer); // To be called by a core while game is running
SDL_Window* GetSdlWindow();
void GetWindowSize(int* w, int* h);
Status Init(std::filesystem::path work_path);
Status LoadGame(std::filesystem::path const& path);
void OnCtrlKeyPress(SDL_Keycode keycode);
void PollEvents();
void Run(bool boot_game_immediately = false);

} // namespace frontend::gui

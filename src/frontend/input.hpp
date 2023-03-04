#pragma once

#include "status.hpp"
#include "types.hpp"

#include "SDL.h"

#include <optional>

namespace frontend::input {
void ClearAllBindings();
void ClearControllerBindings();
void ClearKeyBindings();
// std::optional<u8> GetControllerBinding(N64::Control control);
// std::optional<SDL_Scancode> GetKeyBinding(N64::Control control);
Status Init();
Status LoadBindingsFromDisk();
void OnControllerAxisMotion(SDL_Event const& event);
void OnControllerButtonDown(SDL_Event const& event);
void OnControllerButtonUp(SDL_Event const& event);
void OnControllerDeviceAdded(SDL_Event const& event);
void OnControllerDeviceRemoved(SDL_Event const& event);
void OnKeyDown(SDL_Event const& event);
void OnKeyUp(SDL_Event const& event);
void OnMouseButtonDown(SDL_Event const& event);
void OnMouseButtonUp(SDL_Event const& event);
// void RemoveControllerBinding(N64::Control control);
// void RemoveKeyBinding(N64::Control control);
Status SaveBindingsToDisk();
// void SetBinding(SDL_GamepadAxis axis, N64::Control control);
// void SetBinding(SDL_GamepadButton button, N64::Control control);
// void SetBinding(SDL_Scancode key, N64::Control control);
void SetDefaultBindings();

} // namespace frontend::input

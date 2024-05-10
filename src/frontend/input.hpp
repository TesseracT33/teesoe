#pragma once

#include "loader.hpp"
#include "status.hpp"
#include "numtypes.hpp"

#include "SDL.h"

#include <optional>

namespace frontend::input {

void ClearAllBindings(System system);
void ClearGamepadBindings(System system);
void ClearKeyBindings(System system);
std::optional<u8> GetGamepadBinding(System system, size_t input_index);
std::optional<SDL_Scancode> GetKeyBinding(System system, size_t input_index);
Status Init();
Status LoadBindingsFromDisk();
void OnCoreLoaded(System system);
void OnGamepadAxisMotion(SDL_Event const& event);
void OnGamepadButtonChange(SDL_Event const& event, bool pressed);
void OnGamepadAdded(SDL_Event const& event);
void OnGamepadRemoved(SDL_Event const& event);
void OnKeyChange(SDL_Event const& event, bool pressed);
void RemoveGamepadBinding(System system, size_t input_index);
void RemoveKeyBinding(System system, size_t input_index);
Status SaveBindingsToDisk();
void SetBinding(System system, size_t input_index, SDL_GamepadAxis axis);
void SetBinding(System system, size_t input_index, SDL_GamepadButton button);
void SetBinding(System system, size_t input_index, SDL_Scancode key);
void SetDefaultBindings();

} // namespace frontend::input

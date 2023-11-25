#include "input.hpp"
#include "frontend/message.hpp"
#include "log.hpp"

#include <cassert>
#include <format>
#include <iostream>
#include <limits>
#include <unordered_map>

namespace frontend::input {

using GamepadBindings = std::unordered_map<u8, u8>; // key: SDL_GameControllerAxis/SDL_GameControllerButton
using KeyBindings = std::unordered_map<SDL_Scancode, u8>;

struct Bindings {
    GamepadBindings gamepad_bindings;
    KeyBindings key_bindings;
};

static std::unordered_map<System, Bindings> bindings;
static Bindings const* current_bindings;

void ClearAllBindings(System system)
{
    ClearGamepadBindings(system);
    ClearKeyBindings(system);
}

void ClearGamepadBindings(System system)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.gamepad_bindings.clear();
}

void ClearKeyBindings(System system)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.key_bindings.clear();
}

std::optional<u8> GetGamepadBinding(System system, size_t input_index)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    GamepadBindings const& gamepad_bindings = system_it->second.gamepad_bindings;
    for (auto const& [sdl_button, core_control] : gamepad_bindings) {
        if (input_index == core_control) {
            return sdl_button;
        }
    }
    return {};
}

std::optional<SDL_Scancode> GetKeyBinding(System system, size_t input_index)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    KeyBindings const& key_bindings = system_it->second.key_bindings;
    for (auto const& [sdl_button, core_control] : key_bindings) {
        if (input_index == core_control) {
            return sdl_button;
        }
    }
    return {};
}

Status Init()
{
    if (SDL_Init(SDL_INIT_GAMEPAD) != 0) {
        return FailureStatus(std::format("Failed to init gamecontroller system: {}\n", SDL_GetError()));
    }
    if (!LoadBindingsFromDisk().Ok()) {
        log_warn("Failed to load user bindings! Using defaults.");
        SetDefaultBindings();
        SaveBindingsToDisk();
    }
    return OkStatus();
}

Status LoadBindingsFromDisk()
{
    return UnimplementedStatus(); // TODO
}

void OnCoreLoaded(System system)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    current_bindings = &system_it->second;
}

void OnGamepadAxisMotion(SDL_Event const& event)
{
    // TODO: possibly store and compare against previous axis value to stop from going further for every single axis
    // change.
    // if (auto binding_it = controller_bindings.find(event.caxis.axis); binding_it != controller_bindings.end()) {
    //    using enum N64::Control;
    //    N64::Control n64_control = binding_it->second;
    //    s16 axis_value = event.caxis.value;
    //    if (n64_control == JX || n64_control == JY) {
    //        N64::OnJoystickMovement(n64_control, axis_value);
    //    } else if (n64_control == CX || n64_control == CY) {
    //        // Register a C button press for a "large enough" (absolute) axis value.
    //        static constexpr s16 axis_min_threshold = std::numeric_limits<s16>::min() * 3 / 4;
    //        static constexpr s16 axis_max_threshold = std::numeric_limits<s16>::max() * 3 / 4;
    //        if (axis_value < axis_min_threshold) {
    //            N64::OnButtonDown(n64_control == CX ? CLeft : CUp);
    //        } else if (axis_value > axis_max_threshold) {
    //            N64::OnButtonDown(n64_control == CX ? CRight : CDown);
    //        } else {
    //            N64::OnButtonUp(n64_control == CX ? CLeft : CUp);
    //            N64::OnButtonUp(n64_control == CX ? CRight : CDown);
    //        }
    //    } else {
    //        std::unreachable();
    //    }
    //}
}

void OnGamepadButtonChange(SDL_Event const& event, bool pressed)
{
    if (current_bindings != nullptr) {
        assert(CoreIsLoaded());
        auto it = current_bindings->gamepad_bindings.find(event.gbutton.button);
        if (it != current_bindings->gamepad_bindings.end()) {
            GetCore()->NotifyButtonState(0, it->second, pressed);
        }
    }
}

void OnGamepadAdded(SDL_Event const& event)
{
    // TODO
}

void OnGamepadRemoved(SDL_Event const& event)
{
    // TODO
}

void OnKeyChange(SDL_Event const& event, bool pressed)
{
    if (current_bindings != nullptr) {
        assert(CoreIsLoaded());
        auto it = current_bindings->key_bindings.find(event.key.keysym.scancode);
        if (it != current_bindings->key_bindings.end()) {
            GetCore()->NotifyButtonState(0, it->second, pressed);
        }
    }
    // SDL_Keycode keycode = event.key.keysym.sym;
    // if ((SDL_GetModState() & SDL_KMOD_CTRL) != 0 && keycode != SDLK_LCTRL
    //     && keycode != SDLK_RCTRL) { /* LCTRL/RCTRL is held */
    //     Gui::OnCtrlKeyPress(keycode);
    // }
}

void RemoveGamepadBinding(System system, size_t input_index)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    GamepadBindings& gamepad_bindings = system_it->second.gamepad_bindings;
    for (auto const& [sdl_button, core_control] : gamepad_bindings) {
        if (input_index == core_control) {
            gamepad_bindings.erase(sdl_button);
        }
    }
}

void RemoveKeyBinding(System system, size_t input_index)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    KeyBindings& key_bindings = system_it->second.key_bindings;
    for (auto const& [sdl_button, core_control] : key_bindings) {
        if (input_index == core_control) {
            key_bindings.erase(sdl_button);
        }
    }
}

Status SaveBindingsToDisk()
{
    return UnimplementedStatus(); // TODO
}

void SetBinding(System system, size_t input_index, SDL_GamepadAxis axis)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.gamepad_bindings[axis] = input_index;
}

void SetBinding(System system, size_t input_index, SDL_GamepadButton button)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.gamepad_bindings[button] = input_index;
}

void SetBinding(System system, size_t input_index, SDL_Scancode key)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.key_bindings[key] = input_index;
}

void SetDefaultBindings()
{
    // controller_bindings.clear();
    // controller_bindings[SDL_GAMEPAD_BUTTON_A] = N64::Control::A;
    // controller_bindings[SDL_GAMEPAD_BUTTON_B] = N64::Control::B;
    // controller_bindings[SDL_GAMEPAD_BUTTON_START] = N64::Control::Start;
    // controller_bindings[SDL_GAMEPAD_BUTTON_DPAD_UP] = N64::Control::DUp;
    // controller_bindings[SDL_GAMEPAD_BUTTON_DPAD_DOWN] = N64::Control::DDown;
    // controller_bindings[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = N64::Control::DLeft;
    // controller_bindings[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = N64::Control::DRight;
    // controller_bindings[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = N64::Control::ShoulderL;
    // controller_bindings[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = N64::Control::ShoulderR;
    // controller_bindings[SDL_GAMEPAD_AXIS_TRIGGER_RIGHT] = N64::Control::Z;
    // controller_bindings[SDL_GAMEPAD_AXIS_RIGHTX] = N64::Control::CX;
    // controller_bindings[SDL_GAMEPAD_AXIS_RIGHTY] = N64::Control::CY;
    // controller_bindings[SDL_GAMEPAD_AXIS_LEFTX] = N64::Control::JX;
    // controller_bindings[SDL_GAMEPAD_AXIS_LEFTY] = N64::Control::JY;

    // key_bindings.clear();
    // key_bindings[SDL_SCANCODE_A] = N64::Control::A;
    // key_bindings[SDL_SCANCODE_B] = N64::Control::B;
    // key_bindings[SDL_SCANCODE_RETURN] = N64::Control::Start;
    // key_bindings[SDL_SCANCODE_UP] = N64::Control::DUp;
    // key_bindings[SDL_SCANCODE_DOWN] = N64::Control::DDown;
    // key_bindings[SDL_SCANCODE_LEFT] = N64::Control::DLeft;
    // key_bindings[SDL_SCANCODE_RIGHT] = N64::Control::DRight;
    // key_bindings[SDL_SCANCODE_L] = N64::Control::ShoulderL;
    // key_bindings[SDL_SCANCODE_R] = N64::Control::ShoulderR;
    // key_bindings[SDL_SCANCODE_Z] = N64::Control::Z;
    // key_bindings[SDL_SCANCODE_KP_8] = N64::Control::CUp;
    // key_bindings[SDL_SCANCODE_KP_2] = N64::Control::CDown;
    // key_bindings[SDL_SCANCODE_KP_4] = N64::Control::CLeft;
    // key_bindings[SDL_SCANCODE_KP_6] = N64::Control::CRight;
}

} // namespace frontend::input

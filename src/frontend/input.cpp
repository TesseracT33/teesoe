#include "input.hpp"
#include "frontend/message.hpp"
#include "log.hpp"
#include "n64/common/control.hpp"

#include <cassert>
#include <format>
#include <unordered_map>

namespace frontend::input {

struct Bindings {
    std::unordered_map<SDL_GamepadAxis, u8> gamepad_axis_bindings;
    std::unordered_map<SDL_GamepadButton, u8> gamepad_button_bindings;
    std::unordered_map<SDL_Scancode, u8> key_bindings;
};

static std::unordered_map<System, Bindings> bindings;
static Bindings const* current_bindings;

void SetUpDefaultN64Bindings()
{
    // bindings[System::N64].gamepad_axis_bindings = {
    //     { SDL_GAMEPAD_AXIS_LEFTX, n64::Control::JX },
    //     { SDL_GAMEPAD_AXIS_LEFTY, n64::Control::JY },
    //     { SDL_GAMEPAD_AXIS_RIGHTX, n64::Control::CLeft },
    //     { SDL_GAMEPAD_AXIS_RIGHTY, n64::Control::CUp },
    //     { SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, n64::Control::Z },
    // };

    bindings[System::N64].gamepad_button_bindings = {
        { SDL_GAMEPAD_BUTTON_SOUTH, std::to_underlying(n64::Control::A) },
        { SDL_GAMEPAD_BUTTON_EAST, std::to_underlying(n64::Control::B) },
        { SDL_GAMEPAD_BUTTON_NORTH, std::to_underlying(n64::Control::Start) },
        { SDL_GAMEPAD_BUTTON_WEST, std::to_underlying(n64::Control::Z) },
        { SDL_GAMEPAD_BUTTON_DPAD_UP, std::to_underlying(n64::Control::DUp) },
        { SDL_GAMEPAD_BUTTON_DPAD_DOWN, std::to_underlying(n64::Control::DDown) },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, std::to_underlying(n64::Control::DLeft) },
        { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, std::to_underlying(n64::Control::DRight) },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, std::to_underlying(n64::Control::L) },
        { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, std::to_underlying(n64::Control::R) },
        { SDL_GAMEPAD_BUTTON_START, std::to_underlying(n64::Control::Start) },
    };

    bindings[System::N64].key_bindings = {
        { SDL_SCANCODE_A, std::to_underlying(n64::Control::A) },
        { SDL_SCANCODE_B, std::to_underlying(n64::Control::B) },
        { SDL_SCANCODE_LCTRL, std::to_underlying(n64::Control::Z) },
        { SDL_SCANCODE_RETURN, std::to_underlying(n64::Control::Start) },
        { SDL_SCANCODE_UP, std::to_underlying(n64::Control::DUp) },
        { SDL_SCANCODE_DOWN, std::to_underlying(n64::Control::DDown) },
        { SDL_SCANCODE_LEFT, std::to_underlying(n64::Control::DLeft) },
        { SDL_SCANCODE_RIGHT, std::to_underlying(n64::Control::DRight) },
    };
}

void ClearAllBindings(System system)
{
    ClearGamepadBindings(system);
    ClearKeyBindings(system);
}

void ClearGamepadBindings(System system)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.gamepad_axis_bindings.clear();
    system_it->second.gamepad_button_bindings.clear();
}

void ClearKeyBindings(System system)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.key_bindings.clear();
}

std::optional<u8> GetGamepadBinding(System system, size_t input_index)
{
    (void)system;
    (void)input_index;
    // auto system_it = bindings.find(system);
    // assert(system_it != bindings.end());
    // GamepadBindings const& gamepad_bindings = system_it->second.gamepad_bindings;
    // for (auto const& [sdl_button, core_control] : gamepad_bindings) {
    //     if (input_index == core_control) {
    //         return sdl_button;
    //     }
    // }
    return {};
}

std::optional<SDL_Scancode> GetKeyBinding(System system, size_t input_index)
{
    (void)system;
    (void)input_index;
    // auto system_it = bindings.find(system);
    // assert(system_it != bindings.end());
    // KeyBindings const& key_bindings = system_it->second.key_bindings;
    // for (auto const& [sdl_button, core_control] : key_bindings) {
    //     if (input_index == core_control) {
    //         return sdl_button;
    //     }
    // }
    return {};
}

Status Init()
{
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        return FailureStatus(std::format("Failed to init gamepad system: {}\n", SDL_GetError()));
    }
    if (!LoadBindingsFromDisk().Ok()) {
        LogWarn("Failed to load user bindings! Using defaults.");
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
    (void)event;
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
        auto it = current_bindings->gamepad_button_bindings.find(static_cast<SDL_GamepadButton>(event.gbutton.button));
        if (it != current_bindings->gamepad_button_bindings.end()) {
            GetCore()->NotifyButtonState(0, it->second, pressed);
        }
    }
}

void OnGamepadAdded(SDL_Event const& event)
{
    (void)event;
    // TODO
}

void OnGamepadRemoved(SDL_Event const& event)
{
    (void)event;
    // TODO
}

void OnKeyChange(SDL_Event const& event, bool pressed)
{
    if (current_bindings != nullptr) {
        assert(CoreIsLoaded());
        auto it = current_bindings->key_bindings.find(event.key.scancode);
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
    (void)system;
    (void)input_index;
    // auto system_it = bindings.find(system);
    // assert(system_it != bindings.end());
    // GamepadBindings& gamepad_bindings = system_it->second.gamepad_bindings;
    // for (auto const& [sdl_button, core_control] : gamepad_bindings) {
    //     if (input_index == core_control) {
    //         gamepad_bindings.erase(sdl_button);
    //     }
    // }
}

void RemoveKeyBinding(System system, size_t input_index)
{
    (void)system;
    (void)input_index;
    // auto system_it = bindings.find(system);
    // assert(system_it != bindings.end());
    // KeyBindings& key_bindings = system_it->second.key_bindings;
    // for (auto const& [sdl_button, core_control] : key_bindings) {
    //     if (input_index == core_control) {
    //         key_bindings.erase(sdl_button);
    //     }
    // }
}

Status SaveBindingsToDisk()
{
    return UnimplementedStatus(); // TODO
}

void SetBinding(System system, size_t input_index, SDL_GamepadAxis axis)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.gamepad_axis_bindings[axis] = u8(input_index);
}

void SetBinding(System system, size_t input_index, SDL_GamepadButton button)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.gamepad_button_bindings[button] = u8(input_index);
}

void SetBinding(System system, size_t input_index, SDL_Scancode key)
{
    auto system_it = bindings.find(system);
    assert(system_it != bindings.end());
    system_it->second.key_bindings[key] = u8(input_index);
}

void SetDefaultBindings()
{
    SetUpDefaultN64Bindings();
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

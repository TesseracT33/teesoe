#pragma once

#include "serializer.hpp"
#include "types.hpp"

#include <concepts>

namespace gba::keypad {

enum class Button {
    A,
    B,
    Select,
    Start,
    Right,
    Left,
    Up,
    Down,
    R,
    L
};

void Initialize();
void NotifyButtonPressed(uint index);
void NotifyButtonReleased(uint index);
template<std::integral Int> Int ReadReg(u32 addr);
void StreamState(Serializer& stream);
template<std::integral Int> void WriteReg(u32 addr, Int data);

} // namespace gba::keypad

#pragma once

#include "types.hpp"

namespace n64 {

enum class Control : u32 {
    A,
    B,
    CUp,
    CDown,
    CLeft,
    CRight,
    DUp,
    DDown,
    DLeft,
    DRight,
    JX,
    JY,
    ShoulderL,
    ShoulderR,
    Start,
    Z,
    CX,
    CY /* alternative to CUp, CDown etc for controlling C buttons using a joystick */
};

inline constexpr size_t num_controls = 18;

} // namespace n64

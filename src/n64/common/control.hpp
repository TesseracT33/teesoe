#pragma once

#include "types.hpp"

#include <array>
#include <string_view>

namespace n64 {

// TODO: allow controlling C buttons with joystick

enum class Control : u32 {
    A,
    B,
    Start,
    Z,
    L,
    R,
    DUp,
    DDown,
    DLeft,
    DRight,
    CUp,
    CDown,
    CLeft,
    CRight,
    JX,
    JY,
};

constexpr std::array<std::string_view, 18> control_names = {
    "A",
    "B",
    "START",
    "Z",
    "L",
    "R",
    "D-pad up",
    "D-pad down",
    "D-pad left",
    "D-pad right",
    "C up",
    "C down",
    "C left",
    "C right",
    "Joy X",
    "Joy Y",
};

} // namespace n64

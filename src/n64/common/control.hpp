#pragma once

#include "numtypes.hpp"

#include <array>
#include <string_view>

namespace n64 {

using namespace std::literals::string_view_literals;

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

constexpr std::array control_names = {
    "A"sv,
    "B"sv,
    "START"sv,
    "Z"sv,
    "L"sv,
    "R"sv,
    "D-pad up"sv,
    "D-pad down"sv,
    "D-pad left"sv,
    "D-pad right"sv,
    "C up"sv,
    "C down"sv,
    "C left"sv,
    "C right"sv,
    "Joy X"sv,
    "Joy Y"sv,
};

} // namespace n64

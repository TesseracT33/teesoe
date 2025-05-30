#pragma once

#include "control.hpp"
#include "numtypes.hpp"
#include "status.hpp"

#include <concepts>
#include <filesystem>

namespace n64::pif {

void OnButtonAction(Control control, bool pressed);
Status LoadIPL12(std::filesystem::path const& path);
void OnJoystickMovement(Control control, s16 value);
template<std::signed_integral Int> Int ReadMemory(u32 addr);
void RunCommands();
template<size_t access_size> void WriteMemory(u32 addr, s64 data);

} // namespace n64::pif

#pragma once

#include "status.hpp"
#include "numtypes.hpp"

#include <concepts>
#include <filesystem>

namespace gba::cart {

void Initialize();
Status LoadRom(std::filesystem::path const& path);
u8 ReadSram(u32 addr);
template<std::integral Int> Int ReadRom(u32 addr);
void WriteSram(u32 addr, u8 data);

} // namespace gba::cart

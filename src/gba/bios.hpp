#pragma once

#include "status.hpp"
#include "types.hpp"

#include <concepts>
#include <filesystem>

namespace gba::bios {

void Initialize();
Status Load(std::filesystem::path const& path);
template<std::integral Int> Int Read(u32 addr);

} // namespace gba::bios

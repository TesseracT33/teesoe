#pragma once

#include "status.hpp"
#include "numtypes.hpp"

#include <filesystem>
#include <span>

namespace n64::controller_pak {

Status LoadFromFile(std::filesystem::path const& path);
std::span<u8 const, 32> Read(u16 addr);
Status StoreToFile(std::filesystem::path const& path);
void Write(u16 addr, std::span<u8 const, 32> data);

} // namespace n64::controller_pak

#pragma once

#include "numtypes.hpp"
#include "status.hpp"

#include <concepts>
#include <filesystem>

namespace n64::cart {

size_t GetNumberOfBytesUntilRomEnd(u32 addr);
u8* GetPointerToRom(u32 addr);
u8* GetPointerToSram(u32 addr);
Status LoadRom(std::filesystem::path const& rom_path);
Status LoadSram(std::filesystem::path const& sram_path);
template<std::signed_integral Int> Int ReadDma(u32 addr);
template<std::signed_integral Int> Int ReadRom(u32 addr);
template<std::signed_integral Int> Int ReadSram(u32 addr);
template<size_t access_size> void WriteSram(u32 addr, s64 data);
template<size_t access_size> void WriteRom(u32 addr, s64 data);

} // namespace n64::cart

#pragma once

#include "numtypes.hpp"

#include <concepts>

namespace n64::rdram {

size_t GetNumberOfBytesUntilMemoryEnd(u32 addr);
u8* GetPointerToMemory(u32 addr = 0);
size_t GetSize();
void Initialize();
template<std::signed_integral Int> Int Read(u32 addr);
u32 ReadReg(u32 addr);
u64 RdpReadCommand(u32 addr);
template<size_t access_size, typename... MaskT> void Write(u32 addr, s64 data, MaskT... mask);
void WriteReg(u32 addr, u32 data);

} // namespace n64::rdram

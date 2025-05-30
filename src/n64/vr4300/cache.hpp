#pragma once

#include "mmu.hpp"
#include "numtypes.hpp"

#include <concepts>

namespace n64::vr4300 {

void InitCache();
template<std::signed_integral Int, MemOp> Int ReadCacheableArea(u32 phys_addr);
template<u32 access_size, typename... MaskT> void WriteCacheableArea(u32 phys_addr, s64 data, MaskT... mask);

} // namespace n64::vr4300

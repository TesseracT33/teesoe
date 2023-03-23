#pragma once

#include "mmu.hpp"
#include "types.hpp"

#include <concepts>

namespace n64::vr4300 {

void cache(u32 rs, u32 rt, s16 imm);
void InitCache();
template<std::signed_integral Int, MemOp> Int ReadCacheableArea(u32 phys_addr);
template<size_t access_size, typename... MaskT> void WriteCacheableArea(u32 phys_addr, s64 data, MaskT... mask);

/* TODO: making these anything but 0 makes NM64 not get to the title screen */
inline constexpr uint cache_hit_read_cycle_delay = 0;
inline constexpr uint cache_hit_write_cycle_delay = 0;
inline constexpr uint cache_miss_cycle_delay = 0; /* Magic number gathered from ares / CEN64 */

} // namespace n64::vr4300

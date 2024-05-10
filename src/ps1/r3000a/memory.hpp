#pragma once

#include "numtypes.hpp"


#include <array>
#include <concepts>

namespace ps1::r3000a {

enum class Alignment {
    Aligned,
    Unaligned
};

enum class MemOp {
    DataLoad,
    DataStore,
    InstrFetch
};

inline constexpr size_t bios_size = 512_KiB;

inline std::array<u8, bios_size> bios;

template<std::signed_integral Int, Alignment alignment = Alignment::Aligned, MemOp mem_op = MemOp::DataLoad>
Int read(u32 addr);

template<size_t size, Alignment alignment = Alignment::Aligned> void write(u32 addr, s32 data);

} // namespace ps1::r3000a

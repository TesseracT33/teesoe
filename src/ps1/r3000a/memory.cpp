#include "memory.hpp"
#include "exceptions.hpp"

namespace ps1::r3000a {

struct IoAddr {
    enum : u32 {
        MemCtrlExpansion1BaseAddr = 0x1F80'1000,
        MemCtrlExpansion2BaseAddr = 0x1F80'1000,
        MemCtrlExpansion1DelaySize = 0x1F80'1000,
        MemCtrlExpansion3DelaySize = 0x1F80'1000,
        MemCtrlBiosRomSize = 0x1F80'1000,
        MemCtrlExpansion1BaseAddr = 0x1F80'1000,
        MemCtrlExpansion1BaseAddr = 0x1F80'1000,
        MemCtrlExpansion1BaseAddr = 0x1F80'1000,
        MemCtrlExpansion1BaseAddr = 0x1F80'1000,
        MemCtrlExpansion1BaseAddr = 0x1F80'1000,
        JoyData = 0x1F80'1040,
        JoyStat = 0x1F80'1044,
        JoyMode = 0x1F80'1048,
        JoyCtrl = 0x1F80'104A,
        JoyBaud = 0x1F80'104E,
        SioData = 0x1F80'1050,
        SioStat = 0x1F80'1054,
        SioMode = 0x1F80'1058,
        SioCtrl = 0x1F80'105A,
        SioMisc = 0x1F80'105C,
        SioBaud = 0x1F80'105E,
        IStat = 0x1F80'1070,
        IMask = 0x1F80'1074,
    };
};

template<std::signed_integral Int, Alignment alignment, MemOp mem_op> Int read(u32 addr)
{
    static_assert(sizeof(Int) <= 4);
    static constexpr size_t size = sizeof(Int);
    if constexpr (alignment == Alignment::Aligned && size > 1 && mem_op == MemOp::DataLoad) {
        if (addr & (size - 1)) {
            address_error_exception(addr, mem_op);
            return {};
        }
    }
    if (addr >= 0xFFFE'0000) {}
    if (s32(addr & 0xF000'0000) >= 0x2000'0000_s32) { // 0x2000'0000 - 0x7FFF'FFFF
        bus_error_exception(mem_op);
        return {};
    }
    addr &= 0x1FFF'FFFF;

    return {};
}

template<std::signed_integral Int> Int read_io(u32 addr)
{
    u32 aligned_addr = addr & ~3;
}

template<size_t size, Alignment alignment> void write(u32 addr, s32 data)
{
    static_assert(size == 1 || size == 2 || size == 4);
    if constexpr (alignment == Alignment::Aligned && size > 1) {
        if (addr & (size - 1)) {
            return address_error_exception(addr, mem_op);
        }
    }
}

template s8 read<s8, Alignment::Aligned, MemOp::DataLoad>(u32);
template s16 read<s16, Alignment::Aligned, MemOp::DataLoad>(u32);
template s32 read<s32, Alignment::Aligned, MemOp::DataLoad>(u32);
template s32 read<s32, Alignment::Unaligned, MemOp::DataLoad>(u32);
template s32 read<s32, Alignment::Aligned, MemOp::InstrFetch>(u32);

template void write<1, Alignment::Aligned>(u32, s32);
template void write<1, Alignment::Aligned>(u32, s32);
template void write<2, Alignment::Aligned>(u32, s32);
template void write<2, Alignment::Aligned>(u32, s32);
template void write<4, Alignment::Aligned>(u32, s32);
template void write<4, Alignment::Aligned>(u32, s32);
template void write<4, Alignment::Unaligned>(u32, s32);
template void write<4, Alignment::Unaligned>(u32, s32);

} // namespace ps1::r3000a

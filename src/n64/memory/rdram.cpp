#include "rdram.hpp"
#include "vr4300/recompiler.hpp"

#include <bit>
#include <cstring>

namespace n64::rdram {

struct {
    u32 device_type, device_id, delay, mode, ref_interval, ref_row, ras_interval, min_interval, addr_select,
      device_manuf, dummy0, dummy1, dummy2, dummy3, dummy4, dummy5;
} static reg;

constexpr size_t rdram_expanded_size = 0x80'0000;

/* Note: could not use std::array here as .data() does not become properly aligned */
/* TODO: parallel-rdp required 4096 on my system. Investigate further. */
alignas(4096) static u8 rdram[rdram_expanded_size]; /* TODO: make it dynamic? */

size_t GetNumberOfBytesUntilMemoryEnd(u32 addr)
{
    /* TODO handle mirroring (for DMA) */
    return sizeof(rdram) - (addr & (sizeof(rdram) - 1));
}

u8* GetPointerToMemory(u32 addr)
{
    return rdram + (addr & (sizeof(rdram) - 1));
}

size_t GetSize()
{
    return sizeof(rdram);
}

void Initialize()
{
    std::memset(rdram, 0, sizeof(rdram));
    std::memset(&reg, 0, sizeof(reg));
    /* values taken from Peter Lemon RDRAMTest */
    reg.device_type = 0xB419'0010;
    reg.delay = 0x2B3B'1A0B;
    reg.ras_interval = 0x101C'0A04;
}

/* 0 - $7F'FFFF */
template<std::signed_integral Int> Int Read(u32 addr)
{ /* CPU precondition: addr is always aligned */
    // RDRAM is stored in LE, word-wise
    if constexpr (sizeof(Int) == 1) addr ^= 3;
    if constexpr (sizeof(Int) == 2) addr ^= 2;
    u8 const* rdram_src = rdram + (addr & (sizeof(rdram) - 1));
    Int ret;
    if constexpr (sizeof(Int) <= 4) {
        std::memcpy(&ret, rdram_src, sizeof(Int));
    } else {
        std::memcpy(&ret, rdram_src + 4, 4);
        std::memcpy(reinterpret_cast<u8*>(&ret) + 4, rdram_src, 4);
    }
    return ret;
}

/* $03F0'0000 - $03FF'FFFF */
u32 ReadReg(u32 addr)
{
    static_assert(sizeof(reg) >> 2 == 0x10);
    u32 offset = addr >> 2 & 0xF;
    u32 ret;
    std::memcpy(&ret, (u32*)(&reg) + offset, 4);
    return ret;
}

u64 RdpReadCommand(u32 addr)
{ // addr is aligned to 8 bytes
    u64 command;
    std::memcpy(&command, &rdram[addr & (sizeof(rdram) - 1)], 8);
    return command;
}

/* 0 - $7F'FFFF */
template<size_t access_size, typename... MaskT> void Write(u32 addr, s64 data, MaskT... mask)
{ /* Precondition: phys_addr is aligned to access_size if sizeof...(mask) == 0 */
    static_assert(std::has_single_bit(access_size) && access_size <= 8);
    static_assert(sizeof...(mask) <= 1);
    // RDRAM is stored in LE, word-wise
    static constexpr bool apply_mask = sizeof...(mask) == 1;
    if constexpr (apply_mask) {
        addr &= ~(access_size - 1);
    }
    if constexpr (access_size == 1) addr ^= 3;
    if constexpr (access_size == 2) addr ^= 2;
    addr &= sizeof(rdram) - 1;
    u8* rdram_dst = rdram + addr;
    auto to_write = [&] {
        if constexpr (access_size == 1) return u8(data);
        if constexpr (access_size == 2) return u16(data);
        if constexpr (access_size == 4) return u32(data);
        if constexpr (access_size == 8) return data;
    }();
    if constexpr (apply_mask) {
        u64 existing;
        if constexpr (access_size <= 4) {
            std::memcpy(&existing, rdram_dst, access_size);
        } else {
            std::memcpy(&existing, rdram_dst + 4, 4);
            std::memcpy(reinterpret_cast<u8*>(&existing) + 4, rdram_dst, 4);
        }
        to_write &= (..., mask);
        to_write |= existing & (..., ~mask);
    }
    if constexpr (access_size <= 4) {
        std::memcpy(rdram_dst, &to_write, access_size);
    } else {
        std::memcpy(rdram_dst, reinterpret_cast<u8 const*>(&to_write) + 4, 4);
        std::memcpy(rdram_dst + 4, &to_write, 4);
    }
    vr4300::Invalidate(addr);
}

/* $03F0'0000 - $03FF'FFFF */
void WriteReg(u32 addr, u32 data)
{
    (void)addr;
    (void)data;
    /* TODO */
}

template s8 Read<s8>(u32);
template s16 Read<s16>(u32);
template s32 Read<s32>(u32);
template s64 Read<s64>(u32);
template void Write<1>(u32, s64);
template void Write<2>(u32, s64);
template void Write<4>(u32, s64);
template void Write<8>(u32, s64);
template void Write<4, u32>(u32, s64, u32);
template void Write<8, u64>(u32, s64, u64);

} // namespace n64::rdram

#include "si.hpp"
#include "log.hpp"
#include "memory/pif.hpp"
#include "memory/rdram.hpp"
#include "mi.hpp"
#include "n64_build_options.hpp"
#include "scheduler.hpp"
#include "vr4300/recompiler.hpp"

#include <bit>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

namespace n64::si {

enum Register {
    DramAddr = 0,
    AddrRd64B = 1,
    AddrWr4B = 2,
    AddrWr64B = 4,
    AddrRd4B = 5,
    Status = 6
};

struct {
    u32 dram_addr;
    u32 pif_addr_rd64b;
    u32 pif_addr_wr4b;
    u32 : 32;
    u32 pif_addr_wr64b;
    u32 pif_addr_rd4b;
    struct {
        u32 dma_busy     : 1;
        u32 io_busy      : 1;
        u32 read_pending : 1;
        u32 dma_error    : 1;
        u32 pch_state    : 4;
        u32 dma_state    : 4;
        u32 interrupt    : 1;
        u32              : 19;
    } status;
    u32 : 32;
} static io;

static void InitReadDma();
static void InitWriteDma();
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

void InitReadDma()
{
    if constexpr (log_dma) {
        Log(std::format("DMA: from PIF ${:X} to RDRAM ${:X}: ${:X} bytes", io.pif_addr_rd64b, io.dram_addr, 64));
    }
    s32 pif_command = pif::ReadCommand();
    if (pif_command & 1) {
        pif::RunJoybusProtocol();
        pif::WriteCommand(pif_command & ~1);
    }
    io.status.dma_busy = 1;
    auto dram_start = io.dram_addr;
    for (auto i = pif::ram_size / 4; i > 0; --i) {
        rdram::Write<4>(io.dram_addr, pif::ReadMemory<s32>(io.pif_addr_rd64b));
        io.dram_addr += 4;
        io.pif_addr_rd64b += 4;
    }
    scheduler::AddEvent(scheduler::EventType::SiDmaFinish, 131070, OnDmaFinish);
    vr4300::InvalidateRange(dram_start, io.dram_addr);
}

void InitWriteDma()
{
    if constexpr (log_dma) {
        Log(std::format("DMA: from RDRAM ${:X} to PIF ${:X}: ${:X} bytes", io.dram_addr, io.pif_addr_wr64b, 64));
    }
    io.status.dma_busy = 1;
    for (auto i = pif::ram_size / 4; i > 0; --i) {
        pif::WriteMemory<4>(io.pif_addr_wr64b, rdram::Read<s32>(io.dram_addr));
        io.dram_addr += 4;
        io.pif_addr_wr64b += 4;
    }
    scheduler::AddEvent(scheduler::EventType::SiDmaFinish, 131070, OnDmaFinish);
}

void Initialize()
{
    io = {};
}

void OnDmaFinish()
{
    io.status.dma_busy = 0;
    io.status.io_busy = 0;
    io.status.read_pending = 0;
    io.status.pch_state = 0;
    io.status.dma_state = 0;
    io.status.interrupt = 1;
    mi::RaiseInterrupt(mi::InterruptType::SI);
}

template<std::signed_integral Int> Int ReadMemory(u32 addr)
{
    return pif::ReadMemory<Int>(addr);
}

u32 ReadReg(u32 addr)
{
    static_assert(sizeof(io) >> 2 == 8);
    u32 offset = addr >> 2 & 7;
    u32 ret;
    std::memcpy(&ret, (u32*)(&io) + offset, 4);
    if constexpr (log_io_si) {
        Log(std::format("SI: {} => ${:08X}", RegOffsetToStr(offset), ret));
    }
    return ret;
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case DramAddr: return "SI_DRAM_ADDR";
    case AddrRd64B: return "SI_ADDR_RD64B";
    case AddrWr4B: return "SI_ADDR_WR4B";
    case AddrWr64B: return "SI_ADDR_WR64B";
    case AddrRd4B: return "SI_ADDR_RD4B";
    case Status: return "SI_STATUS";
    default: return "SI_UNKNOWN";
    }
}

template<size_t access_size> void WriteMemory(u32 addr, s64 data)
{
    /*io.status.io_busy = 1;
    io.status.dma_busy = 1;*/
    pif::WriteMemory<access_size>(addr, data);
}

void WriteReg(u32 addr, u32 data)
{
    static_assert(sizeof(io) >> 2 == 8);
    u32 offset = addr >> 2 & 7;
    if constexpr (log_io_si) {
        Log(std::format("SI: {} <= ${:08X}", RegOffsetToStr(offset), data));
    }

    switch (offset) {
    case Register::DramAddr: io.dram_addr = data & 0xFF'FFF8; break;

    case Register::AddrRd64B:
        io.pif_addr_rd64b = data & ~3;
        InitReadDma();
        break;

    case Register::AddrWr4B:
        io.pif_addr_wr4b = data;
        LogWarn("Tried to start SI WR4B DMA, which is currently unimplemented.");
        break;

    case Register::AddrWr64B:
        io.pif_addr_wr64b = data & ~3;
        InitWriteDma();
        break;

    case Register::AddrRd4B:
        io.pif_addr_rd4b = data;
        LogWarn("Tried to start SI RD4B DMA, which is currently unimplemented.");
        break;

    case Register::Status:
        io.status.interrupt = 0;
        mi::ClearInterrupt(mi::InterruptType::SI);
        break;

    default: LogWarn(std::format("Unexpected write made to SI register at address ${:08X}", addr));
    }
}

template s8 ReadMemory<s8>(u32);
template s16 ReadMemory<s16>(u32);
template s32 ReadMemory<s32>(u32);
template s64 ReadMemory<s64>(u32);
template void WriteMemory<1>(u32, s64);
template void WriteMemory<2>(u32, s64);
template void WriteMemory<4>(u32, s64);
template void WriteMemory<8>(u32, s64);

} // namespace n64::si

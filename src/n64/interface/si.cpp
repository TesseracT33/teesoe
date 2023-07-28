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

enum class DmaType {
    PifToRdram,
    RdramToPif
};

enum Register {
    DramAddr = 0,
    AddrRd64B = 1,
    AddrWr4B = 2,
    AddrWr64B = 4,
    AddrRd4B = 5,
    Status = 6
};

struct {
    u32 dram_addr, pif_addr_rd64b, pif_addr_wr4b, dummy0, pif_addr_wr64b, pif_addr_rd4b;
    struct {
        u32 dma_busy     : 1;
        u32 io_busy      : 1;
        u32 read_pending : 1;
        u32 dma_error    : 1;
        u32              : 8;
        u32 interrupt    : 1;
        u32              : 19;
    } status;
    u32 dummy1;
} static si;

template<DmaType> static void InitDma();
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

template<DmaType type> void InitDma()
{
    si.status.dma_busy = 1;
    if constexpr (type == DmaType::PifToRdram) {
        if constexpr (log_dma) {
            log(std::format("DMA: from PIF ${:X} to RDRAM ${:X}: ${:X} bytes", si.pif_addr_rd64b, si.dram_addr, 64));
        }
        auto dram_start = si.dram_addr;
        for (int i = 0; i < 16; ++i) {
            rdram::Write<4>(si.dram_addr, pif::ReadMemory<s32>(si.pif_addr_rd64b));
            si.dram_addr += 4;
            si.pif_addr_rd64b += 4;
        }
        vr4300::InvalidateRange(dram_start, si.dram_addr);
    } else { /* RDRAM to PIF */
        if constexpr (log_dma) {
            log(std::format("DMA: from RDRAM ${:X} to PIF ${:X}: ${:X} bytes", si.dram_addr, si.pif_addr_wr64b, 64));
        }
        for (int i = 0; i < 16; ++i) {
            pif::WriteMemory<4>(si.pif_addr_wr64b, rdram::Read<s32>(si.dram_addr));
            si.dram_addr += 4;
            si.pif_addr_wr64b += 4;
        }
    }
    scheduler::AddEvent(scheduler::EventType::SiDmaFinish, 131070, OnDmaFinish);
}

void Initialize()
{
    si = {};
}

void OnDmaFinish()
{
    si.status.interrupt = 1;
    si.status.dma_busy = 0;
    mi::RaiseInterrupt(mi::InterruptType::SI);
}

u32 ReadReg(u32 addr)
{
    static_assert(sizeof(si) >> 2 == 8);
    u32 offset = addr >> 2 & 7;
    u32 ret;
    std::memcpy(&ret, (u32*)(&si) + offset, 4);
    if constexpr (log_io_si) {
        log(std::format("SI: {} => ${:08X}", RegOffsetToStr(offset), ret));
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
    default: return "UNKNOWN";
    }
}

void WriteReg(u32 addr, u32 data)
{
    static_assert(sizeof(si) >> 2 == 8);
    u32 offset = addr >> 2 & 7;
    if constexpr (log_io_si) {
        log(std::format("SI: {} <= ${:08X}", RegOffsetToStr(offset), data));
    }

    switch (offset) {
    case Register::DramAddr: si.dram_addr = data & 0xFF'FFF8; break;

    case Register::AddrRd64B:
        si.pif_addr_rd64b = data & ~1;
        InitDma<DmaType::PifToRdram>();
        break;

    case Register::AddrWr4B:
        si.pif_addr_wr4b = data;
        /* TODO */
        log_warn("Tried to start SI WR4B DMA, which is currently unimplemented.");
        break;

    case Register::AddrWr64B:
        si.pif_addr_wr64b = data & ~1;
        InitDma<DmaType::RdramToPif>();
        break;

    case Register::AddrRd4B:
        si.pif_addr_rd4b = data;
        /* TODO */
        log_warn("Tried to start SI RD4B DMA, which is currently unimplemented.");
        break;

    case Register::Status:
        /* Writing any value to si.STATUS clears bit 12 (SI Interrupt flag), not only here,
           but also in the RCP Interrupt Cause register and in MI. */
        si.status.interrupt = 0;
        mi::ClearInterrupt(mi::InterruptType::SI);
        break;

    default: log_warn(std::format("Unexpected write made to SI register at address ${:08X}", addr));
    }
}
} // namespace n64::si

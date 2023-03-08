#include "si.hpp"
#include "log.hpp"
#include "memory/pif.hpp"
#include "memory/rdram.hpp"
#include "mi.hpp"
#include "n64_build_options.hpp"
#include "scheduler.hpp"

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
    s32 dram_addr, pif_addr_rd64b, pif_addr_wr4b, dummy0, pif_addr_wr64b, pif_addr_rd4b, status, dummy1;
} static si;

static size_t dma_len;
static s32* pif_addr_reg_last_dma;

template<DmaType> static void InitDma();
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

void ClearStatusFlag(StatusFlag status_flag)
{
    si.status &= ~std::to_underlying(status_flag);
}

template<DmaType type> void InitDma()
{
    SetStatusFlag(StatusFlag::DmaBusy);
    u32 pif_addr;
    size_t bytes_until_pif_end;
    if constexpr (type == DmaType::PifToRdram) {
        pif_addr = si.pif_addr_rd64b;
        pif_addr_reg_last_dma = &si.pif_addr_rd64b;
    } else { /* RDRAM to PIF */
        pif_addr = si.pif_addr_wr64b;
        pif_addr_reg_last_dma = &si.pif_addr_wr64b;
    }
    bytes_until_pif_end = pif::GetNumberOfBytesUntilMemoryEnd(pif_addr);
    u8* rdram_ptr = rdram::GetPointerToMemory(si.dram_addr);
    size_t bytes_until_rdram_end = rdram::GetNumberOfBytesUntilMemoryEnd(si.dram_addr);
    static constexpr size_t max_dma_len = 64;
    dma_len = std::min(max_dma_len, std::min(bytes_until_rdram_end, bytes_until_pif_end));
    if constexpr (type == DmaType::PifToRdram) {
        u8* pif_ptr = pif::GetPointerToMemory(pif_addr);
        for (size_t i = 0; i < dma_len; i += 4) {
            s32 val;
            std::memcpy(&val, pif_ptr + i, 4);
            val = std::byteswap(val); // PIF BE, RDRAM LE
            std::memcpy(rdram_ptr + i, &val, 4);
        }
        if constexpr (log_dma) {
            log(std::format("From PIF ${:X} to RDRAM ${:X}: ${:X} bytes", pif_addr, si.dram_addr, dma_len));
        }
    } else { /* RDRAM to PIF */
        size_t num_bytes_in_rom_area = pif::GetNumberOfBytesUntilRamStart(pif_addr);
        if (num_bytes_in_rom_area < dma_len) {
            pif_addr += num_bytes_in_rom_area;
            rdram_ptr += num_bytes_in_rom_area;
            dma_len -= num_bytes_in_rom_area;
            for (size_t i = 0; i < dma_len; i += 4) {
                s32 val;
                std::memcpy(&val, rdram_ptr + i, 4);
                pif::WriteMemory<4>(pif_addr + i, val); // PIF BE, RDRAM LE, but val byteswapped in pif::WriteMemory
            }
            if constexpr (log_dma) {
                log(std::format("From RDRAM ${:X} to PIF ${:X}: ${:X} bytes",
                  si.dram_addr,
                  pif_addr,
                  dma_len - num_bytes_in_rom_area));
            }
        } else {
            log(std::format(
              "Attempted from RDRAM ${:X} to PIF ${:X}, but the target PIF memory area was entirely in the ROM region",
              si.dram_addr,
              pif_addr));
            OnDmaFinish();
            return;
        }
    }

    static constexpr auto cycles_per_byte_dma = 18;
    auto cycles_until_finish = dma_len * cycles_per_byte_dma;
    scheduler::AddEvent(scheduler::EventType::SiDmaFinish, cycles_until_finish, OnDmaFinish);
}

void Initialize()
{
    std::memset(&si, 0, sizeof(si));
}

void OnDmaFinish()
{
    SetStatusFlag(StatusFlag::Interrupt);
    ClearStatusFlag(StatusFlag::DmaBusy);
    mi::RaiseInterrupt(mi::InterruptType::SI);
    si.dram_addr = (si.dram_addr + dma_len) & 0xFF'FFFF;
    *pif_addr_reg_last_dma = (*pif_addr_reg_last_dma + dma_len) & 0x7FC;
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

void SetStatusFlag(StatusFlag status_flag)
{
    si.status |= std::to_underlying(status_flag);
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
        ClearStatusFlag(StatusFlag::Interrupt);
        mi::ClearInterrupt(mi::InterruptType::SI);
        // TODO: RCP flag
        break;

    default: log_warn(std::format("Unexpected write made to SI register at address ${:08X}", addr));
    }
}
} // namespace n64::si

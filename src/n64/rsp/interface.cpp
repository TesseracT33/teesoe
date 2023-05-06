#include "interface.hpp"
#include "interface/mi.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "n64_build_options.hpp"
#include "rsp.hpp"
#include "scheduler.hpp"
#include "util.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace n64::rsp {

enum class DmaType {
    RdToSp,
    SpToRd
};

enum Register {
    DmaSpaddr,
    DmaRamaddr,
    DmaRdlen,
    DmaWrlen,
    Status,
    DmaFull,
    DmaBusy,
    Semaphore
};

/* What was written last to either SP_DMA_RDLEN/ SP_DMA_WRLEN during an ongoing DMA */
static s32 buffered_dma_rdlen, buffered_dma_wrlen;
static DmaType in_progress_dma_type, pending_dma_type;

constexpr s32 sp_pc_addr = 0x0408'0000;

template<DmaType dma_type> static void InitDma();
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

template<DmaType dma_type> void InitDma()
{
    in_progress_dma_type = dma_type;
    sp.status.dma_busy = 1;
    sp.dma_busy |= 1;

    s32 rows, bytes_per_row, skip;
    if constexpr (dma_type == DmaType::RdToSp) {
        /* The number of bytes is RDLEN plus 1, rounded up to 8 bytes.
        Since the lower three bits of RDLEN are always 0, this effectively results
        in the number of bytes always being rounded up to the next 8-byte multiple. */
        bytes_per_row = (sp.dma_rdlen & 0xFF8) + 8;
        rows = (sp.dma_rdlen >> 12 & 0xFF) + 1;
        skip = sp.dma_rdlen >> 20 & 0xFFF;
    } else {
        bytes_per_row = (sp.dma_wrlen & 0xFF8) + 8;
        rows = (sp.dma_wrlen >> 12 & 0xFF) + 1;
        skip = sp.dma_wrlen >> 20 & 0xFFF;
    }

    s32 bytes_to_copy = rows * bytes_per_row;

    /* The speed of transfer is about 3.7 bytes per VR4300 (PClock) cycle (plus some small fixed overhead). */
    uint cpu_cycles_until_finish = bytes_to_copy / 4 + 1;

    if constexpr (log_dma) {
        std::string_view rsp_mem_bank = sp.dma_spaddr & 0x1000 ? "IMEM" : "DMEM";
        std::string output = [&] {
            if constexpr (dma_type == DmaType::RdToSp) {
                return std::format("DMA; From RDRAM ${:X} to RSP {} ${:X}; ${:X} bytes",
                  sp.dma_ramaddr & 0xFF'FFFF,
                  rsp_mem_bank,
                  sp.dma_spaddr & 0xFFF,
                  bytes_to_copy);
            } else {
                return std::format("DMA; From RSP {} ${:X} to RDRAM ${:X}; ${:X} bytes",
                  rsp_mem_bank,
                  sp.dma_spaddr & 0xFFF,
                  sp.dma_ramaddr & 0xFF'FFFF,
                  bytes_to_copy);
            }
        }();
        log(output);
    }

    scheduler::AddEvent(scheduler::EventType::SpDmaFinish, cpu_cycles_until_finish, OnDmaFinish);

    auto AlignAddresses = [] {
        sp.dma_spaddr = sp.dma_spaddr & 0x1000 | sp.dma_spaddr + 4 & 0xFFF;
        sp.dma_ramaddr += 4; // TODO: ensure no overflow
    };

    auto DstPtr = [] {
        if constexpr (dma_type == DmaType::RdToSp) {
            return rsp::GetPointerToMemory(sp.dma_spaddr);
        } else {
            return rdram::GetPointerToMemory(sp.dma_ramaddr);
        }
    };

    auto SrcPtr = [] {
        if constexpr (dma_type == DmaType::RdToSp) {
            return rdram::GetPointerToMemory(sp.dma_ramaddr);
        } else {
            return rsp::GetPointerToMemory(sp.dma_spaddr);
        }
    };

    /* The DMA engine allows to transfer multiple "rows" of data in RDRAM, separated by a "skip" value. This allows for
       instance to transfer a rectangular portion of a larger image, by specifying the size of each row of the
       selection portion, the number of rows, and a "skip" value that corresponds to the bytes between the end of a row
       and the beginning of the following one. Notice that this applies only to RDRAM: accesses in IMEM/DMEM are always
       linear. Moreover, if spaddr overflow it will wrap around within IMEM/DMEM (e.g. $FFC => 0; $1FFC => $1000)
     */
    if (skip == 0) {
        for (size_t i = 0; i < bytes_to_copy; i += 4) {
            s32 val;
            std::memcpy(&val, SrcPtr(), 4);
            val = std::byteswap(val); // rdram LE, rsp ram BE
            std::memcpy(DstPtr(), &val, 4);
            AlignAddresses();
        }
    } else {
        for (s32 i = 0; i < rows; ++i) {
            s32 bytes_to_copy_this_row = std::min(bytes_to_copy, bytes_per_row);
            for (s32 i = 0; i < bytes_to_copy_this_row; i += 4) {
                s32 val;
                std::memcpy(&val, SrcPtr(), 4);
                val = std::byteswap(val); // rdram LE, rsp ram BE
                std::memcpy(DstPtr(), &val, 4);
                AlignAddresses();
            }
            bytes_to_copy -= bytes_to_copy_this_row;
            sp.dma_ramaddr += skip; // TODO: ensure no overflow
        }
    }
}

void OnDmaFinish()
{
    if (sp.status.dma_full) {
        sp.status.dma_full = 0;
        sp.dma_full &= ~1;
        if (in_progress_dma_type == DmaType::RdToSp) {
            sp.dma_rdlen = buffered_dma_rdlen;
            sp.dma_wrlen = buffered_dma_rdlen;
        } else {
            sp.dma_rdlen = buffered_dma_wrlen;
            sp.dma_wrlen = buffered_dma_wrlen;
        }
        if (pending_dma_type == DmaType::RdToSp) {
            InitDma<DmaType::RdToSp>();
        } else {
            InitDma<DmaType::SpToRd>();
        }
    } else {
        sp.status.dma_busy = 0;
        sp.dma_busy &= ~1;
        /* After the transfer is finished, the fields RDLEN and WRLEN contains the value 0xFF8,
        COUNT is reset to 0, and SKIP is unchanged. SPADDR and RAMADDR contain the
        addresses after the last ones that were read/written during the last DMA. */
        if (in_progress_dma_type == DmaType::RdToSp) {
            sp.dma_rdlen = 0xFF8 | sp.dma_rdlen & 0xFF80'0000;
            sp.dma_wrlen = 0xFF8 | sp.dma_rdlen & 0xFF80'0000;
        } else {
            sp.dma_rdlen = 0xFF8 | sp.dma_wrlen & 0xFF80'0000;
            sp.dma_wrlen = 0xFF8 | sp.dma_wrlen & 0xFF80'0000;
        }
    }

    mi::RaiseInterrupt(mi::InterruptType::SP);
}

u32 ReadReg(u32 addr)
{
    if (addr == sp_pc_addr) {
        // TODO: return random number if !halted, else pc
        // return halted ? pc : Int(Random<s32>(0, 0xFFF));
        if constexpr (log_io_rsp) {
            log(std::format("RSP IO: SP_PC => ${:08X}", pc));
        }
        return pc;
    } else {
        static_assert(sizeof(sp) >> 2 == 8);
        u32 offset = addr >> 2 & 7;
        u32 ret = [&] {
            switch (offset) {
            case DmaSpaddr: return sp.dma_spaddr;

            case DmaRamaddr: return sp.dma_ramaddr;

            case DmaRdlen:
                /* SP_DMA_WRLEN and SP_DMA_RDLEN both always returns the same data on read, relative to
                the current transfer, irrespective on the direction of the transfer. */
                return ~7 & [&] {
                    if (sp.status.dma_busy) {
                        return in_progress_dma_type == DmaType::RdToSp ? sp.dma_rdlen : sp.dma_wrlen;
                    } else {
                        return sp.dma_rdlen;
                    }
                }();

            case DmaWrlen:
                return ~7 & [&] {
                    if (sp.status.dma_busy) {
                        return in_progress_dma_type == DmaType::RdToSp ? sp.dma_rdlen : sp.dma_wrlen;
                    } else {
                        return sp.dma_wrlen;
                    }
                }();

            case Status: return std::bit_cast<u32>(sp.status);

            case DmaFull: return sp.dma_full;

            case DmaBusy: return sp.dma_busy;

            case Semaphore: {
                auto ret = sp.semaphore;
                sp.semaphore = 1;
                return ret;
            }

            default: std::unreachable();
            }
        }();
        if constexpr (log_io_rsp) {
            log(std::format("RSP IO: {} => ${:08X}", RegOffsetToStr(offset), ret));
        }
        return ret;
    }
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case DmaSpaddr: return "SP_DMA_SP_ADDR";
    case DmaRamaddr: return "SP_DMA_RAM_ADDR";
    case DmaRdlen: return "SP_DMA_RDLEN";
    case DmaWrlen: return "SP_DMA_WRLEN";
    case Status: return "SP_STATUS";
    case DmaFull: return "SP_DMA_FULL";
    case DmaBusy: return "SP_DMA_BUSY";
    case Semaphore: return "SP_SEMAPHORE";
    default: std::unreachable();
    }
}

void WriteReg(u32 addr, u32 data)
{
    if (addr == sp_pc_addr) {
        pc = data & 0xFFC;
        jump_is_pending = in_branch_delay_slot = false;
        if constexpr (log_io_rsp) {
            log(std::format("RSP IO: SP_PC <= ${:08X}", data));
        }
    } else {
        static_assert(sizeof(sp) >> 2 == 8);
        u32 offset = addr >> 2 & 7;
        if constexpr (log_io_rsp) {
            log(std::format("RSP IO: {} <= ${:08X}", RegOffsetToStr(offset), data));
        }

        switch (offset) {
        case DmaSpaddr: sp.dma_spaddr = data & 0x03FF'FFF8; break;

        case DmaRamaddr: sp.dma_ramaddr = data &= 0x03FF'FFF8; break;

        case DmaRdlen:
            if (sp.status.dma_busy) {
                buffered_dma_rdlen = data & 0xFF8F'FFFF;
                sp.status.dma_full = 1;
                sp.dma_full |= 1;
                pending_dma_type = DmaType::RdToSp;
            } else {
                sp.dma_rdlen = data & 0xFF8F'FFFF;
                InitDma<DmaType::RdToSp>();
            }
            break;

        case DmaWrlen:
            if (sp.status.dma_busy) {
                buffered_dma_wrlen = data & 0xFF8F'FFFF;
                sp.status.dma_full = 1;
                sp.dma_full |= 1;
                pending_dma_type = DmaType::SpToRd;
            } else {
                sp.dma_wrlen = data & 0xFF8F'FFFF;
                InitDma<DmaType::SpToRd>();
            }
            break;

        case Status: {
            if ((data & 1) && !(data & 2)) {
                /* CLR_HALT: Start running RSP code from the current RSP PC (clear the HALTED flag) */
                sp.status.halted = 0;
            } else if (!(data & 1) && (data & 2)) {
                /* 	SET_HALT: Pause running RSP code (set the HALTED flag) */
                sp.status.halted = 1;
            }
            if (data & 4) {
                /* CLR_BROKE: Clear the BROKE flag, that is automatically set every time a BREAK opcode is run.
                This flag has no effect on the running/idle state of the RSP; it is just a latch
                that remembers whether a BREAK opcode was ever run. */
                sp.status.broke = 0;
            }
            if ((data & 8) && !(data & 0x10)) {
                /* 	CLR_INTR: Acknowledge a pending RSP MI interrupt. This must be done any time a RSP MI interrupt
                was generated, otherwise the interrupt line on the VR4300 will stay asserted. */
                mi::ClearInterrupt(mi::InterruptType::SP);
            } else if (!(data & 8) && (data & 0x10)) {
                /* 	SET_INTR: Manually trigger a RSP MI interrupt on the VR4300. It might be useful if the RSP wants to
                manually trigger a VR4300 interrupt at any point during its execution. */
                mi::RaiseInterrupt(mi::InterruptType::SP);
            }
            if ((data & 0x20) && !(data & 0x40)) {
                /* CLR_SSTEP: Disable single-step mode. */
                sp.status.sstep = 0;
            } else if (!(data & 0x20) && (data & 0x40)) {
                /* 	SET_SSTEP: Enable single-step mode. When this mode is activated, the RSP auto-halts itself after
                every opcode that is run. The VR4300 can then trigger a new step by unhalting it. */
                sp.status.sstep = 1;
            }
            if ((data & 0x80) && !(data & 0x100)) {
                /* CLR_INTBREAK: Disable the INTBREAK flag. When this flag is disabled, running a BREAK opcode will not
                generate any RSP MI interrupt, but it will still halt the RSP. */
                sp.status.intbreak = 0;
            } else if (!(data & 0x80) && (data & 0x100)) {
                /* 	SET_INTBREAK: Enable the INTBREAK flag. When this flag is enabled, running a BREAK opcode will
                generate a RSP MI interrupt, in addition to halting the RSP. */
                sp.status.intbreak = 1;
            }
            /* 	CLR_SIG<n>/SET_SIG<n>: Set to 0 or 1 the 8 available bitflags that can be used as communication protocol
             * between RSP and CPU. */
            u32 written_value_mask = 0x200;
            u32 status_mask = 1;
            for (int i = 0; i < 8; ++i) {
                if ((data & written_value_mask) && !(data & written_value_mask << 1)) {
                    sp.status.sig &= ~status_mask;
                } else if (!(data & written_value_mask) && (data & written_value_mask << 1)) {
                    sp.status.sig |= status_mask;
                }
                written_value_mask <<= 2;
                status_mask <<= 1;
            }
            break;
        }

        case DmaFull:
        case DmaBusy: /* read-only */ break;

        case Semaphore:
            sp.semaphore = 0; /* goes against n64brew, but is according to ares */
            break;

        default: std::unreachable();
        }
    }
}
} // namespace n64::rsp

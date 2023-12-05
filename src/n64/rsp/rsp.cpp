#include "rsp.hpp"
#include "disassembler.hpp"
#include "interface/mi.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "n64_build_options.hpp"
#include "rdp/rdp.hpp"
#include "rsp/recompiler.hpp"
#include "scheduler.hpp"
#include "util.hpp"
#include "vr4300/recompiler.hpp"

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

struct Register {
    enum {
        DmaSpaddr,
        DmaRamaddr,
        DmaRdlen,
        DmaWrlen,
        Status,
        DmaFull,
        DmaBusy,
        Semaphore
    };
};

/* What was written last to either SP_DMA_RDLEN/ SP_DMA_WRLEN during an ongoing DMA */
static s32 buffered_dma_rdlen, buffered_dma_wrlen;
static DmaType in_progress_dma_type, pending_dma_type;

constexpr s32 sp_pc_addr = 0x0408'0000;

template<DmaType dma_type> static void InitDma();
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

void AdvancePipeline(u64 cycles)
{
    cycle_counter += cycles;
}

u32 FetchInstruction(u32 addr)
{
    ASSUME(addr < 0x1000 && !(addr & 3));
    u32 instr;
    std::memcpy(&instr, &imem[addr], 4);
    instr = std::byteswap(instr);
    return instr;
}

u8* GetPointerToMemory(u32 addr)
{
    return mem.data() + (addr & 0x1FFF);
}

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

    auto bytes_to_copy = rows * bytes_per_row;
    auto cpu_cycles_until_finish = (bytes_to_copy + 8) / 8 * 3; // credit: Ares

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
        Log(output);
    }

    scheduler::AddEvent(scheduler::EventType::SpDmaFinish, cpu_cycles_until_finish, OnDmaFinish);

    auto dram_start = sp.dma_ramaddr;
    auto sp_start = sp.dma_spaddr & 0x1FFF;
    bool sp_full_cycle{};

    auto AlignAddresses = [&] {
        sp.dma_spaddr = sp.dma_spaddr & 0x1000 | sp.dma_spaddr + 4 & 0xFFF; // stay within DMEM or IMEM
        sp.dma_ramaddr += 4; // TODO: ensure no overflow
        sp_full_cycle |= sp.dma_spaddr == sp_start;
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

    if constexpr (dma_type == DmaType::RdToSp) {
        if (rsp::cpu_impl == CpuImpl::Recompiler && (sp.dma_spaddr & 0x1000)) {
            if (sp_full_cycle) {
                rsp::InvalidateRange(0, 0x1000);
            } else if (sp_start < (sp.dma_spaddr & 0x1FFF)) {
                rsp::InvalidateRange(sp_start - 0x1000, sp.dma_spaddr - 0x1000);
            } else {
                if (sp.dma_spaddr != 0x1000) {
                    rsp::InvalidateRange(0, sp.dma_spaddr - 0x1000);
                }
                rsp::InvalidateRange(sp_start - 0x1000, 0x1000);
            }
        }

    } else {
        // TODO: handle case where skip > 0
        vr4300::InvalidateRange(dram_start, sp.dma_ramaddr);
    }
}

void Link(u32 reg)
{
    gpr.set(reg, (pc + 8) & 0xFFF);
}

void NotifyIllegalInstr(std::string_view instr)
{
    LogError(std::format("Illegal RSP instruction {} encountered.\n", instr));
}

void NotifyIllegalInstrCode(u32 instr_code)
{
    LogError(std::format("Illegal RSP instruction code {:08X} encountered.\n", instr_code));
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

void PerformBranch()
{
    pc = jump_addr;
    jump_is_pending = in_branch_delay_slot = false;
    if constexpr (log_rsp_branches) {
        Log(std::format("RSP branch to 0x{:03X}; RA = 0x{:08X}; SP = 0x{:08X}", u32(pc), u32(gpr[31]), u32(gpr[29])));
    }
}

void PowerOn()
{
    jump_is_pending = false;
    pc = 0;
    mem.fill(0);
    std::memset(&sp, 0, sizeof(sp));
    sp.status.halted = true;
}

template<std::signed_integral Int> Int ReadDMEM(u32 addr)
{
    /* Addr may be misaligned and the read can go out of bounds */
    Int ret;
    for (size_t i = 0; i < sizeof(Int); ++i) {
        *((u8*)(&ret) + sizeof(Int) - i - 1) = dmem[addr + i & 0xFFF];
    }
    return ret;
}

template<std::signed_integral Int> Int ReadMemoryCpu(u32 addr)
{ /* CPU precondition; the address is always aligned */
    if (addr < 0x0404'0000) {
        Int ret;
        std::memcpy(&ret, mem.data() + (addr & 0x1FFF), sizeof(Int));
        return std::byteswap(ret);
    } else if constexpr (sizeof(Int) == 4) {
        return ReadReg(addr);
    } else {
        LogWarn(
          std::format("Attempted to read RSP memory region at address ${:08X} for sized int {}", addr, sizeof(Int)));
        return {};
    }
}

u32 ReadReg(u32 addr)
{
    if (addr == sp_pc_addr) {
        // TODO: return random number if !halted, else pc
        // return halted ? pc : Int(Random<s32>(0, 0xFFF));
        if constexpr (log_io_rsp) {
            Log(std::format("RSP IO: SP_PC => ${:08X}", pc));
        }
        return pc;
    } else {
        static_assert(sizeof(sp) >> 2 == 8);
        u32 offset = addr >> 2 & 7;
        u32 ret = [offset] {
            switch (offset) {
            case Register::DmaSpaddr: return sp.dma_spaddr;

            case Register::DmaRamaddr: return sp.dma_ramaddr;

            case Register::DmaRdlen:
                /* SP_DMA_WRLEN and SP_DMA_RDLEN both always returns the same data on read, relative to
                the current transfer, irrespective on the direction of the transfer. */
                return ~7 & [&] {
                    if (sp.status.dma_busy) {
                        return in_progress_dma_type == DmaType::RdToSp ? sp.dma_rdlen : sp.dma_wrlen;
                    } else {
                        return sp.dma_rdlen;
                    }
                }();

            case Register::DmaWrlen:
                return ~7 & [&] {
                    if (sp.status.dma_busy) {
                        return in_progress_dma_type == DmaType::RdToSp ? sp.dma_rdlen : sp.dma_wrlen;
                    } else {
                        return sp.dma_wrlen;
                    }
                }();

            case Register::Status: return std::bit_cast<u32>(sp.status);

            case Register::DmaFull: return sp.dma_full;

            case Register::DmaBusy: return sp.dma_busy;

            case Register::Semaphore: {
                auto ret = sp.semaphore;
                sp.semaphore = 1;
                return ret;
            }

            default: std::unreachable();
            }
        }();
        if constexpr (log_io_rsp) {
            Log(std::format("RSP IO: {} => ${:08X}", RegOffsetToStr(offset), ret));
        }
        return ret;
    }
}

u64 RdpReadCommand(u32 addr)
{ // The address is aligned to 8 bytes
    u32 words[2];
    std::memcpy(words, &dmem[addr & 0xFFF], 8);
    words[0] = std::byteswap(words[0]);
    words[1] = std::byteswap(words[1]);
    return std::bit_cast<u64>(words);
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case Register::DmaSpaddr: return "SP_DMA_SP_ADDR";
    case Register::DmaRamaddr: return "SP_DMA_RAM_ADDR";
    case Register::DmaRdlen: return "SP_DMA_RDLEN";
    case Register::DmaWrlen: return "SP_DMA_WRLEN";
    case Register::Status: return "SP_STATUS";
    case Register::DmaFull: return "SP_DMA_FULL";
    case Register::DmaBusy: return "SP_DMA_BUSY";
    case Register::Semaphore: return "SP_SEMAPHORE";
    default: std::unreachable();
    }
}

void SetActiveCpuImpl(CpuImpl cpu_impl)
{
    rsp::cpu_impl = cpu_impl;
    if (cpu_impl == CpuImpl::Interpreter) {
        TearDownRecompiler();
    } else {
        Status status = InitRecompiler();
        if (!status.Ok()) {
            LogError(status.Message());
        }
    }
}

void TakeBranch(u32 target_address)
{
    in_branch_delay_slot = true;
    jump_is_pending = false;
    jump_addr = target_address & 0xFFC;
}

template<std::signed_integral Int> void WriteDMEM(u32 addr, Int data)
{
    /* Addr may be misaligned and the write can go out of bounds */
    for (size_t i = 0; i < sizeof(Int); ++i) {
        dmem[(addr + i) & 0xFFF] = *((u8*)(&data) + sizeof(Int) - i - 1);
    }
}

template<size_t access_size> void WriteMemoryCpu(u32 addr, s64 data)
{
    s32 to_write = [&] {
        if constexpr (access_size == 1) return data << (8 * (3 - (addr & 3)));
        if constexpr (access_size == 2) return data << (8 * (2 - (addr & 2)));
        if constexpr (access_size == 4) return data;
        if constexpr (access_size == 8) return data >> 32;
    }();
    if (addr < 0x0404'0000) {
        addr &= 0x1FFC;
        to_write = std::byteswap(to_write);
        std::memcpy(&mem[addr], &to_write, 4);
        if (addr & 0x1000) {
            Invalidate(addr & 0xFFF);
        }
    } else {
        WriteReg(addr, to_write);
    }
}

void WriteReg(u32 addr, u32 data)
{
    if (addr == sp_pc_addr) {
        if constexpr (log_io_rsp) {
            Log(std::format("RSP IO: SP_PC <= ${:08X}", data));
        }
        jump_addr = data & 0xFFC;
        PerformBranch();
    } else {
        static_assert(sizeof(sp) >> 2 == 8);
        u32 offset = addr >> 2 & 7;
        if constexpr (log_io_rsp) {
            Log(std::format("RSP IO: {} <= ${:08X}", RegOffsetToStr(offset), data));
        }

        switch (offset) {
        case Register::DmaSpaddr: sp.dma_spaddr = data & 0x03FF'FFF8; break;

        case Register::DmaRamaddr: sp.dma_ramaddr = data & 0x03FF'FFF8; break;

        case Register::DmaRdlen:
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

        case Register::DmaWrlen:
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

        case Register::Status: {
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

        case Register::DmaFull:
        case Register::DmaBusy: /* read-only */ break;

        case Register::Semaphore:
            sp.semaphore = 0; /* goes against n64brew, but is according to ares */
            break;

        default: std::unreachable();
        }
    }
}

template s8 ReadDMEM<s8>(u32);
template s16 ReadDMEM<s16>(u32);
template s32 ReadDMEM<s32>(u32);
template s64 ReadDMEM<s64>(u32);
template s8 ReadMemoryCpu<s8>(u32);
template s16 ReadMemoryCpu<s16>(u32);
template s32 ReadMemoryCpu<s32>(u32);
template s64 ReadMemoryCpu<s64>(u32);
template void WriteDMEM<s8>(u32, s8);
template void WriteDMEM<s16>(u32, s16);
template void WriteDMEM<s32>(u32, s32);
template void WriteDMEM<s64>(u32, s64);
template void WriteMemoryCpu<1>(u32, s64);
template void WriteMemoryCpu<2>(u32, s64);
template void WriteMemoryCpu<4>(u32, s64);
template void WriteMemoryCpu<8>(u32, s64);

} // namespace n64::rsp

#include "rdp.hpp"
#include "frontend/message.hpp"
#include "interface/mi.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "n64_build_options.hpp"
#include "parallel_rdp_wrapper.hpp"
#include "rsp/rsp.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <format>
#include <string_view>

namespace n64::rdp {

enum class CommandLocation {
    DMEM,
    RDRAM
};

enum Register {
    StartReg,
    EndReg,
    CurrentReg,
    StatusReg,
    ClockReg,
    BufBusyReg,
    PipeBusyReg,
    TmemReg
};

struct {
    u32 start, end, current;
    struct {
        u32 cmd_source   : 1;
        u32 freeze       : 1;
        u32 flush        : 1;
        u32 start_gclk   : 1;
        u32 tmem_busy    : 1;
        u32 pipe_busy    : 1;
        u32 command_busy : 1;
        u32 ready        : 1;
        u32 dma_busy     : 1;
        u32 end_valid    : 1;
        u32 start_valid  : 1;
        u32              : 21;
    } status;
    u32 clock, bufbusy, pipebusy, tmem;
} static dp;

static u32 queue_word_offset;
static u32 num_queued_words;
static std::array<u32, 0x100000> cmd_buffer;
constexpr u32 cmd_buffer_word_capacity = cmd_buffer.size();

template<CommandLocation> static u64 LoadCommandDword(u32 addr);
template<CommandLocation> static void LoadExecuteCommands();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

void Initialize()
{
    std::memset(&dp, 0, sizeof(dp));
    dp.status.ready = 1;
}

template<CommandLocation cmd_loc> u64 LoadCommandDword(u32 addr)
{
    if constexpr (cmd_loc == CommandLocation::DMEM) { /* todo: byteswap? */
        return rsp::RdpReadCommandByteswapped(addr);
    } else {
        return rdram::RdpReadCommandByteswapped(addr);
    }
}

template<CommandLocation cmd_loc> void LoadExecuteCommands()
{
    if (dp.status.freeze) {
        return;
    }
    dp.status.pipe_busy = dp.status.start_gclk = true;
    u32 current = dp.current;
    if (dp.end <= current) {
        return;
    }
    u32 num_dwords = (dp.end - current) / 8;
    if (num_queued_words + 2 * num_dwords >= cmd_buffer_word_capacity) {
        return;
    }

    do {
        u64 dword = LoadCommandDword<cmd_loc>(current); /* TODO: swap words? */
        std::memcpy(&cmd_buffer[num_queued_words], &dword, 8);
        num_queued_words += 2;
        current += 8;
    } while (--num_dwords > 0);

    while (queue_word_offset < num_queued_words) {
        u32 cmd_first_word = cmd_buffer[queue_word_offset];
        u32 opcode = cmd_first_word & 0x3F;
        static constexpr std::array cmd_word_lengths = {
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            8,
            12,
            24,
            28,
            24,
            28,
            40,
            44,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            4,
            4,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
            2,
        };
        u32 cmd_word_len = cmd_word_lengths[opcode];
        if (queue_word_offset + cmd_word_len > num_queued_words) {
            /* partial command; keep data around for next processing call */
            dp.start = dp.current = dp.end;
            return;
        }
        if (opcode >= 8) {
            implementation->EnqueueCommand(cmd_word_len, &cmd_buffer[queue_word_offset]);
        }
        if (opcode == 0x29) { /* full sync command */
            implementation->OnFullSync();
            dp.status.pipe_busy = dp.status.start_gclk = false;
            mi::SetInterruptFlag(mi::InterruptType::DP);
        }
        queue_word_offset += cmd_word_len;
    }

    queue_word_offset = num_queued_words = 0;
    dp.current = dp.end;
}

Status MakeParallelRdp()
{
    if (implementation) {
        implementation->TearDown();
    }
    implementation = std::make_unique<ParallelRDPWrapper>();
    return implementation->Initialize();
}

u32 ReadReg(u32 addr)
{
    /* TODO: RCP will ignore the requested access size and will just put the requested 32-bit word on the bus.
    Luckily, this is the correct behavior for 8-bit and 16-bit accesses (as explained above), so the VR4300 will
    be able to extract the correct portion. 64-bit reads instead will completely freeze the VR4300
    (and thus the whole console), because it will stall waiting for the second word to appear on the bus that the RCP
    will never put. */
    static_assert(sizeof(dp) >> 2 == 8);
    u32 offset = addr >> 2 & 7;
    u32 ret;
    std::memcpy(&ret, (u32*)(&dp) + offset, 4);
    if constexpr (log_io_rdp) {
        log(std::format("RDP IO: {} => ${:08X}", RegOffsetToStr(offset), ret));
    }
    return ret;
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case StartReg: return "DPC_START";
    case EndReg: return "DPC_END";
    case CurrentReg: return "DPC_CURRENT";
    case StatusReg: return "DPC_STATUS";
    case ClockReg: return "DPC_CLOCK";
    case BufBusyReg: return "DPC_BUF_BUSY";
    case PipeBusyReg: return "DPC_PIPE_BUSY";
    case TmemReg: return "DPC_TMEM";
    default: std::unreachable();
    }
}

void WriteReg(u32 addr, u32 data)
{
    auto ProcessCommands = [&] {
        dp.status.cmd_source ? LoadExecuteCommands<CommandLocation::DMEM>()
                             : LoadExecuteCommands<CommandLocation::RDRAM>();
    };

    static_assert(sizeof(dp) >> 2 == 8);
    u32 offset = addr >> 2 & 7;
    if constexpr (log_io_rdp) {
        log(std::format("RDP IO: {} <= ${:08X}", RegOffsetToStr(offset), data));
    }

    switch (offset) {
    case Register::StartReg:
        if (!dp.status.start_valid) {
            dp.start = data & 0xFF'FFF8;
            dp.status.start_valid = true;
        }
        break;

    case Register::EndReg:
        dp.end = data & 0xFF'FFF8;
        if (dp.status.start_valid) {
            dp.current = dp.start;
            dp.status.start_valid = false;
        }
        if (!dp.status.freeze) {
            ProcessCommands();
        }
        break;

    case Register::StatusReg: {
        bool unfrozen = false;
        if (data & 1) {
            dp.status.cmd_source = 0;
        } else if (data & 2) {
            dp.status.cmd_source = 1;
        }
        if (data & 4) {
            dp.status.freeze = 0;
            unfrozen = true;
        } else if (data & 8) {
            dp.status.freeze = 1;
        }
        if (data & 0x10) {
            dp.status.flush = 0;
        } else if (data & 0x20) {
            dp.status.flush = 1;
        }
        if (data & 0x40) {
            dp.tmem = 0;
        }
        if (data & 0x80) {
            dp.pipebusy = 0;
        }
        if (data & 0x100) {
            dp.bufbusy = 0;
        }
        if (data & 0x200) {
            dp.clock = 0;
        }
        if (unfrozen) {
            ProcessCommands();
        }
    } break;
    }
}
} // namespace n64::rdp

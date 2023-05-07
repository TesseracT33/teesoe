#include "rsp.hpp"
#include "disassembler.hpp"
#include "interface.hpp"
#include "log.hpp"
#include "n64_build_options.hpp"
#include "rdp/rdp.hpp"

#include <bit>
#include <cstring>
#include <format>

namespace n64::rsp {

static u32 jump_addr;
static u32 p_cycle_counter;

static u32 FetchInstruction(u32 pc);

void AdvancePipeline(u64 cycles)
{
    p_cycle_counter += cycles;
}

u32 FetchInstruction(u32 pc)
{
    u32 instr;
    std::memcpy(&instr, &imem[pc], 4);
    instr = std::byteswap(instr);
    return instr;
}

u8* GetPointerToMemory(u32 addr)
{
    return mem.data() + (addr & 0x1FFF);
}

void Link(u32 reg)
{
    gpr.set(reg, (pc + 8) & 0xFFF);
}

void NotifyIllegalInstr(std::string_view instr)
{
    log_error(std::format("Illegal RSP instruction {} encountered.\n", instr));
}

void NotifyIllegalInstrCode(u32 instr_code)
{
    log_error(std::format("Illegal RSP instruction code {:08X} encountered.\n", instr_code));
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

u64 RdpReadCommand(u32 addr)
{ // The address is aligned to 8 bytes
    u32 words[2];
    std::memcpy(words, &dmem[addr & 0xFFF], 8);
    words[0] = std::byteswap(words[0]);
    words[1] = std::byteswap(words[1]);
    return std::bit_cast<u64>(words);
}

u64 RunInterpreter(u64 rsp_cycles)
{
    if (sp.status.halted) return 0;
    auto Instr = [] {
        AdvancePipeline(1);
        u32 instr = FetchInstruction(pc);
        disassembler::exec_rsp<CpuImpl::Interpreter>(instr);
        if (jump_is_pending) {
            pc = jump_addr;
            jump_is_pending = in_branch_delay_slot = false;
            return;
        }
        if (in_branch_delay_slot) {
            jump_is_pending = true;
        }
        pc = (pc + 4) & 0xFFC;
    };
    p_cycle_counter = 0;
    if (sp.status.sstep) {
        Instr();
        sp.status.halted = true;
    } else {
        while (p_cycle_counter < rsp_cycles && !sp.status.halted) {
            Instr();
        }
    }
    if (sp.status.halted) {
        if (jump_is_pending) { // note for future refactors: this makes rsp::op_break::BREAKWithinDelay pass
            pc = jump_addr;
            jump_is_pending = in_branch_delay_slot = false;
        }
    }
    return p_cycle_counter <= rsp_cycles ? 0 : p_cycle_counter - rsp_cycles;
}

u64 RunRecompiler(u64 rsp_cycles)
{
    // TODO
    return 0;
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

template s8 ReadDMEM<s8>(u32);
template s16 ReadDMEM<s16>(u32);
template s32 ReadDMEM<s32>(u32);
template s64 ReadDMEM<s64>(u32);
template void WriteDMEM<s8>(u32, s8);
template void WriteDMEM<s16>(u32, s16);
template void WriteDMEM<s32>(u32, s32);
template void WriteDMEM<s64>(u32, s64);

} // namespace n64::rsp

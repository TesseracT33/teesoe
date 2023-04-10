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

void AdvancePipeline(u64 cycles)
{
    p_cycle_counter += cycles;
}

void FetchDecodeExecuteInstruction()
{
    if constexpr (log_rsp_instructions) {
        current_instr_pc = pc;
    }
    u32 instr;
    std::memcpy(&instr, &imem[pc], 4); /* TODO: can pc be misaligned? */
    instr = std::byteswap(instr);
    pc = (pc + 4) & 0xFFF;
    disassembler::exec_rsp<CpuImpl::Interpreter>(instr);
    AdvancePipeline(1);
}

u8* GetPointerToMemory(u32 addr)
{
    return mem.data() + (addr & 0x1FFF);
}

void Jump(u32 target_address)
{
    jump_is_pending = true;
    instructions_until_jump = 1;
    jump_addr = target_address & 0xFFC;
}

void Link(u32 reg)
{
    gpr.set(reg, 0xFFF & (4 + (in_branch_delay_slot ? jump_addr : pc)));
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

template<std::signed_integral Int> Int ReadMemoryCpu(u32 addr)
{ /* CPU precondition; the address is always aligned */
    if (addr < 0x0404'0000) {
        Int ret;
        std::memcpy(&ret, mem.data() + (addr & 0x1FFF), sizeof(Int));
        return std::byteswap(ret);
    } else if constexpr (sizeof(Int) == 4) {
        return ReadReg(addr);
    } else {
        log_warn(
          std::format("Attempted to read RSP memory region at address ${:08X} for sized int {}", addr, sizeof(Int)));
        return {};
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

u64 Run(u64 rsp_cycles_to_run)
{
    if (sp.status.halted) {
        return 0;
    }
    auto Instr = [rsp_cycles_to_run] {
        if (jump_is_pending) {
            if (instructions_until_jump-- == 0) {
                pc = jump_addr;
                jump_is_pending = false;
                in_branch_delay_slot = false;
            } else {
                in_branch_delay_slot = true;
            }
        }
        FetchDecodeExecuteInstruction();
    };
    p_cycle_counter = 0;
    if (sp.status.sstep) {
        Instr();
        sp.status.halted = true;
        return p_cycle_counter <= rsp_cycles_to_run ? 0 : p_cycle_counter - rsp_cycles_to_run;
    } else {
        while (p_cycle_counter < rsp_cycles_to_run) {
            Instr();
            if (sp.status.halted) {
                return p_cycle_counter <= rsp_cycles_to_run ? 0 : p_cycle_counter - rsp_cycles_to_run;
            }
        }
        return p_cycle_counter - rsp_cycles_to_run;
    }
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
        to_write = std::byteswap(to_write);
        std::memcpy(&mem[addr & 0x1FFC], &to_write, 4);
    } else {
        WriteReg(addr, to_write);
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

template s8 ReadMemoryCpu<s8>(u32);
template s16 ReadMemoryCpu<s16>(u32);
template s32 ReadMemoryCpu<s32>(u32);
template s64 ReadMemoryCpu<s64>(u32);

template void WriteMemoryCpu<1>(u32, s64);
template void WriteMemoryCpu<2>(u32, s64);
template void WriteMemoryCpu<4>(u32, s64);
template void WriteMemoryCpu<8>(u32, s64);

} // namespace n64::rsp

#include "vr4300.hpp"
#include "cache.hpp"
#include "cop0.hpp"
#include "cop1.hpp"
#include "disassembler.hpp"
#include "exceptions.hpp"
#include "log.hpp"
#include "memory/rdram.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
#include "recompiler.hpp"

#include <bit>
#include <cstring>
#include <format>
#include <utility>

namespace n64::vr4300 {

static bool interrupt;

void AddInitialEvents()
{
    ReloadCountCompareEvent<true>();
}

void AdvancePipeline(u64 cycles)
{
    p_cycle_counter += cycles;
    cop0.count += cycles;
    // if constexpr (recompile_cpu) {
    //     recompiler::current_block_cycle_counter += cycles;
    // }
}

void CheckInterrupts()
{
    bool prev_interrupt = interrupt;
    interrupt = cop0.status.ie & !cop0.status.exl & !cop0.status.erl & bool(cop0.cause.ip & cop0.status.im);
    if (interrupt && !prev_interrupt) {
        if constexpr (log_interrupts) {
            log(
              std::format("INTERRUPT; cause.ip = ${:02X}; status.im = ${:02X}", u8(cop0.cause.ip), u8(cop0.status.im)));
        }
        SignalException<Exception::Interrupt>();
    }
}

/* Devices external to the CPU (e.g. the console's reset button) use this function to tell the CPU about interrupts. */
void ClearInterruptPending(ExternalInterruptSource interrupt)
{
    cop0.cause.ip &= ~std::to_underlying(interrupt);
}

void FetchDecodeExecuteInstruction()
{
    u32 instr = FetchInstruction(pc);
    pc += 4;
    disassembler::exec_cpu<CpuImpl::Interpreter>(instr);
    AdvancePipeline(1);
}

u64 GetElapsedCycles()
{
    return p_cycle_counter;
}

void InitializeRegisters()
{
    std::memset(&gpr, 0, sizeof(gpr));
    // std::memset(&fpr, 0, sizeof(fpr));
    gpr.set(29, 0xFFFF'FFFF'A400'1FF0);
    cop0.SetRaw(Cop0Reg::index, 0x3F);
    cop0.SetRaw(Cop0Reg::config, 0x7006'E463);
    cop0.SetRaw(Cop0Reg::context, 0x007F'FFF0);
    cop0.SetRaw(Cop0Reg::bad_v_addr, 0xFFFF'FFFF'FFFF'FFFF);
    cop0.SetRaw(Cop0Reg::cause, 0xB000'007C);
    cop0.SetRaw(Cop0Reg::epc, 0xFFFF'FFFF'FFFF'FFFF);
    cop0.SetRaw(Cop0Reg::status, 0x3400'0000);
    cop0.SetRaw(Cop0Reg::ll_addr, 0xFFFF'FFFF);
    cop0.SetRaw(Cop0Reg::watch_lo, 0xFFFF'FFFB);
    cop0.SetRaw(Cop0Reg::watch_hi, 0xF);
    cop0.SetRaw(Cop0Reg::x_context, 0xFFFF'FFFF'FFFF'FFF0);
    cop0.SetRaw(Cop0Reg::error_epc, 0xFFFF'FFFF'FFFF'FFFF);
}

void InitRun(bool hle_pif)
{
    if (hle_pif) {
        /* https://github.com/Dillonb/n64-resources/blob/master/bootn64.html */
        gpr.set(20, 1);
        gpr.set(22, 0x3F);
        gpr.set(29, 0xA400'1FF0);
        cop0.SetRaw(Cop0Reg::status, 0x3400'0000);
        cop0.SetRaw(Cop0Reg::config, 0x7006'E463);
        for (u64 i = 0; i < 0x1000; i += 4) {
            u64 src_addr = 0xFFFF'FFFF'B000'0000 + i;
            u64 dst_addr = 0xFFFF'FFFF'A400'0000 + i;
            WriteVirtual<4>(dst_addr, ReadVirtual<s32>(src_addr));
        }
        pc = 0xFFFF'FFFF'A400'0040;
    } else {
        SignalException<Exception::ColdReset>();
        HandleException();
    }
}

void Jump(u64 target_address)
{
    jump_is_pending = true;
    instructions_until_jump = 1;
    jump_addr = target_address & ~u64(3);
}

void Link(u32 reg)
{
    gpr.set(reg, 4 + (in_branch_delay_slot ? jump_addr : pc));
}

void NotifyIllegalInstrCode(u32 instr_code)
{
    log_error(std::format("Illegal CPU instruction code {:08X} encountered.\n", instr_code));
}

void PowerOn()
{
    exception_occurred = false;
    jump_is_pending = false;

    InitializeRegisters();
    InitCop1();
    InitializeMMU();
    InitCache();

    if constexpr (recompile_cpu) {
        recompiler::Initialize();
    }
}

void Reset()
{
    exception_occurred = false;
    jump_is_pending = false;
    SignalException<Exception::SoftReset>();
    HandleException();
}

u64 Run(u64 cpu_cycles_to_run)
{
    p_cycle_counter = 0;
    while (p_cycle_counter < cpu_cycles_to_run) {
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
        if (exception_occurred) {
            HandleException();
        }
    }
    return p_cycle_counter - cpu_cycles_to_run;
}

void SetInterruptPending(ExternalInterruptSource interrupt)
{
    cop0.cause.ip |= std::to_underlying(interrupt);
    CheckInterrupts();
}

void SignalInterruptFalse()
{
    interrupt = false;
}

} // namespace n64::vr4300

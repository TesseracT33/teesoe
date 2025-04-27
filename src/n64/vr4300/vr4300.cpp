#include "vr4300.hpp"
#include "cache.hpp"
#include "cop0.hpp"
#include "cop1.hpp"
#include "exceptions.hpp"
#include "log.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
#include "recompiler.hpp"

#include <utility>

namespace n64::vr4300 {

static bool interrupt;

static void InitializeRegisters();

void AddInitialEvents()
{
    ReloadCountCompareEvent<true>();
}

void AdvancePipeline(u32 cycles)
{
    cycle_counter += cycles;
    cop0.count += cycles;
}

void CheckInterrupts()
{
    bool prev_interrupt = interrupt;
    interrupt = cop0.status.ie & !cop0.status.exl & !cop0.status.erl & bool(cop0.cause.ip & cop0.status.im);
    if (interrupt && !prev_interrupt) {
        if constexpr (log_interrupts) {
            LogInfo("INTERRUPT; cause.ip = ${:02X}; status.im = ${:02X}", u8(cop0.cause.ip), u8(cop0.status.im));
        }
        InterruptException();
    }
}

/* Devices external to the CPU (e.g. the console's reset button) use this function to tell the CPU about interrupts. */
void ClearInterruptPending(ExternalInterruptSource interrupt_source)
{
    cop0.cause.ip &= ~std::to_underlying(interrupt_source);
    CheckInterrupts();
}

u64 GetElapsedCycles()
{
    return cycle_counter;
}

void InitializeRegisters()
{
    gpr = {};
    gpr.set(29, 0xFFFF'FFFF'A400'1FF0);
    cop0.SetRaw(Cop0Reg::index, 0x3F);
    cop0.SetRaw(Cop0Reg::config, 0x7006'E463);
    cop0.SetRaw(Cop0Reg::context, 0x007F'FFF0);
    cop0.SetRaw(Cop0Reg::bad_v_addr, s64(0xFFFF'FFFF'FFFF'FFFF));
    cop0.SetRaw(Cop0Reg::cause, s32(0xB000'007C));
    cop0.SetRaw(Cop0Reg::epc, s64(0xFFFF'FFFF'FFFF'FFFF));
    cop0.SetRaw(Cop0Reg::status, 0x3400'0000);
    cop0.SetRaw(Cop0Reg::ll_addr, s32(0xFFFF'FFFF));
    cop0.SetRaw(Cop0Reg::watch_lo, s32(0xFFFF'FFFB));
    cop0.SetRaw(Cop0Reg::watch_hi, 0xF);
    cop0.SetRaw(Cop0Reg::x_context, s64(0xFFFF'FFFF'FFFF'FFF0));
    cop0.SetRaw(Cop0Reg::error_epc, s64(0xFFFF'FFFF'FFFF'FFFF));
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
        ColdResetException();
    }
}

void NotifyIllegalInstrCode(u32 instr_code)
{
    LogError("Illegal CPU instruction code {:08X} encountered.\n", instr_code);
}

void PerformBranch()
{
    branch_state = mips::BranchState::NoBranch;
    pc = jump_addr;
    if constexpr (log_cpu_branches) {
        LogInfo("CPU branch to 0x{:016X}; RA = 0x{:016X}; SP = 0x{:016X}", u64(pc), u64(gpr[31]), u64(gpr[29]));
    }
    if (pc & 3) [[unlikely]] {
        AddressErrorException(pc, MemOp::InstrFetch);
    }
}

void PowerOn()
{
    exception_occurred = false;
    InitializeRegisters();
    InitCop1();
    InitializeMMU();
    InitCache();
}

void Reset()
{
    exception_occurred = false;
    branch_state = mips::BranchState::NoBranch;
    SoftResetException();
}

void SetActiveCpuImpl(CpuImpl cpu_impl)
{
    vr4300::cpu_impl = cpu_impl;
    if (cpu_impl == CpuImpl::Interpreter) {
        TearDownRecompiler();
    } else {
        Status status = InitRecompiler();
        if (!status.Ok()) {
            LogError(status.Message());
        }
    }
}

void SetInterruptPending(ExternalInterruptSource interrupt_source)
{
    cop0.cause.ip |= std::to_underlying(interrupt_source);
    CheckInterrupts();
}

void SignalInterruptFalse()
{
    interrupt = false;
}

} // namespace n64::vr4300

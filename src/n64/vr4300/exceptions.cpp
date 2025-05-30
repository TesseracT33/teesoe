#include "exceptions.hpp"
#include "cop0.hpp"
#include "interpreter.hpp"
#include "log.hpp"
#include "mips/types.hpp"
#include "n64_build_options.hpp"
#include "vr4300.hpp"

namespace n64::vr4300 {

static u64 GetCommonExceptionVector();
static void HandleException();

void AddressErrorException(u64 bad_vaddr, MemOp mem_op)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Address Error; vaddr {:016X}", bad_vaddr);
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = mem_op == MemOp::Write ? 5 : 4;
    cop0.bad_v_addr = bad_vaddr;
    cop0.context.bad_vpn2 = cop0.x_context.bad_vpn2 = cop0.entry_hi.vpn2 = bad_vaddr >> 13;
    cop0.entry_hi.r = cop0.x_context.r = bad_vaddr >> 62;
    cop0.cause.ce = 0;
}

void BreakpointException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Breakpoint");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 9;
    cop0.cause.ce = 0;
}

void BusErrorException(MemOp mem_op)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Bus Error");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = mem_op == MemOp::InstrFetch ? 6 : 7;
    cop0.cause.ce = 0;
}

void ColdResetException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Cold Reset");
    HandleException();
    pc = 0xFFFF'FFFF'BFC0'0000;
    cop0.status.rp = cop0.status.sr = cop0.status.ts = 0;
    cop0.status.erl = cop0.status.bev = 1;
    cop0.config.ep = 0;
    cop0.config.be = 1;
    cop0.random = 31;
    OnWriteToStatus();
}

void CoprocessorUnusableException(int cop)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Cop{} Unusable", cop);
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 11;
    cop0.cause.ce = cop;
}

void FloatingPointException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Floating-point");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 15;
    cop0.cause.ce = 0;
}

u64 GetCommonExceptionVector()
{
    return cop0.status.bev ? 0xFFFF'FFFF'BFC0'0380 : 0xFFFF'FFFF'8000'0180;
}

void IntegerOverflowException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Integer Overflow");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 12;
    cop0.cause.ce = 0;
}

void InterruptException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Interrupt");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 0;
    cop0.cause.ce = 0;
}

void HandleException()
{
    exception_occurred = true;
    if (!cop0.status.exl) {
        cop0.status.exl = 1;
        bool in_delay_slot =
          branch_state == BranchState::DelaySlotTaken || branch_state == BranchState::DelaySlotNotTaken;
        cop0.cause.bd = in_delay_slot;
        cop0.epc = in_delay_slot ? pc - 4 : pc;
        SignalInterruptFalse();
        SetVaddrToPaddrFuncs();
    }
    ResetBranch();
}

void NmiException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: NMI");
    HandleException();
    pc = cop0.error_epc; // TODO: not 0xFFFF'FFFF'BFC0'0000?
    cop0.status.ts = 0;
    cop0.status.erl = cop0.status.sr = cop0.status.bev = 1;
    cop0.cause.ce = 0;
}

void ReservedInstructionException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Reserved Instruction");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 10;
    cop0.cause.ce = 0;
}

void ReservedInstructionCop2Exception()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Reserved Instruction");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 10;
    cop0.cause.ce = 2;
}

void SoftResetException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Soft Reset");
    HandleException();
    pc = cop0.status.erl ? 0xFFFF'FFFF'BFC0'0000 : cop0.error_epc;
    cop0.status.rp = cop0.status.ts = 0;
    cop0.status.bev = cop0.status.erl = cop0.status.sr = 1;
}

void SyscallException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Syscall");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 8;
    cop0.cause.ce = 0;
}

void TlbInvalidException(u64 bad_vaddr, MemOp mem_op)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: TLB Invalid; vaddr {:016X}", bad_vaddr);
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = mem_op == MemOp::Write ? 3 : 2;
    cop0.bad_v_addr = bad_vaddr;
    cop0.context.bad_vpn2 = cop0.x_context.bad_vpn2 = cop0.entry_hi.vpn2 = bad_vaddr >> 13;
    cop0.cause.ce = 0;
}

void TlbMissException(u64 bad_vaddr, MemOp mem_op)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: TLB Miss; vaddr {:016X}", bad_vaddr);
    static constexpr s32 base_addr[2][2] = {
        0x8000'0000_s32,
        0x8000'0180_s32,
        0xBFC0'0200_s32,
        0xBFC0'0380_s32,
    };
    u64 vector = base_addr[cop0.status.bev][cop0.status.exl];
    HandleException(); // changes status.exl
    pc = vector;
    cop0.cause.exc_code = mem_op == MemOp::Write ? 3 : 2;
    cop0.bad_v_addr = bad_vaddr;
    cop0.context.bad_vpn2 = cop0.x_context.bad_vpn2 = cop0.entry_hi.vpn2 = bad_vaddr >> 13;
    cop0.cause.ce = 0;
}

void TlbModificationException(u64 bad_vaddr)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: TLB Modification; vaddr {:016X}", bad_vaddr);
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 1;
    cop0.bad_v_addr = bad_vaddr;
    cop0.context.bad_vpn2 = cop0.x_context.bad_vpn2 = cop0.entry_hi.vpn2 = bad_vaddr >> 13;
    cop0.cause.ce = 0;
}

void TrapException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Trap");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 13;
    cop0.cause.ce = 0;
}

void WatchException()
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: Watch");
    HandleException();
    pc = GetCommonExceptionVector();
    cop0.cause.exc_code = 23;
    cop0.cause.ce = 0;
}

void XtlbMissException(u64 bad_vaddr, MemOp mem_op)
{
    if constexpr (log_exceptions) LogInfo("EXCEPTION: XTLB Miss; vaddr {:016X}", bad_vaddr);
    static constexpr s32 base_addr[2][2] = {
        0x8000'0080_s32,
        0x8000'0180_s32,
        0xBFC0'0280_s32,
        0xBFC0'0380_s32,
    };
    u64 vector = base_addr[cop0.status.bev][cop0.status.exl];
    HandleException(); // changes status.exl
    pc = vector;
    cop0.cause.exc_code = mem_op == MemOp::Write ? 3 : 2;
    cop0.bad_v_addr = bad_vaddr;
    cop0.context.bad_vpn2 = cop0.x_context.bad_vpn2 = cop0.entry_hi.vpn2 = bad_vaddr >> 13;
    cop0.entry_hi.r = cop0.x_context.r = bad_vaddr >> 62;
    cop0.cause.ce = 0;
}

} // namespace n64::vr4300

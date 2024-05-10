#pragma once

#include "vr4300/cop0.hpp"
#include "vr4300/cop2.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

void Cop2Prolog()
{
    Label l_usable = c.newLabel();
    c.bt(JitPtr(cop0.status), 30); // cu2
    c.jc(l_usable);
    reg_alloc.ReserveArgs(1);
    c.mov(host_gpr_arg[0].r32(), 2);
    BlockEpilogWithPcFlushAndJmp(CoprocessorUnusableException);
    c.bind(l_usable);
    reg_alloc.FreeArgs(1);
}

void cfc2(u32 rt)
{
    Cop2Prolog();
    if (rt) c.movsxd(reg_alloc.GetHostGprMarkDirty(rt), JitPtr(cop2_latch, 4));
}

void cop2_reserved()
{
    Cop2Prolog();
    BlockEpilogWithPcFlushAndJmp(ReservedInstructionCop2Exception);
    compiler_exception_occurred = true;
}

void ctc2(u32 rt)
{
    Cop2Prolog();
    c.mov(JitPtr(cop2_latch), reg_alloc.GetHostGpr(rt));
}

void dcfc2()
{
    Cop2Prolog();
    BlockEpilogWithPcFlushAndJmp(ReservedInstructionCop2Exception);
    branched = true;
}

void dctc2()
{
    dcfc2();
}

void dmfc2(u32 rt)
{
    Cop2Prolog();
    if (rt) c.mov(reg_alloc.GetHostGprMarkDirty(rt), JitPtr(cop2_latch));
}

void dmtc2(u32 rt)
{
    ctc2(rt);
}

void mfc2(u32 rt)
{
    cfc2(rt);
}

void mtc2(u32 rt)
{
    ctc2(rt);
}

} // namespace n64::vr4300::x64

#pragma once

#include "vr4300/cop0.hpp"
#include "vr4300/cop2.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

void Cop2Prolog()
{
    Label l0 = c.newLabel();
    c.bt(ptr(cop0.status), 30);
    c.jc(l0);
    reg_alloc.Free(host_gpr_arg[0]);
    c.mov(host_gpr_arg[0].r32(), 2);
    BlockEpilogWithJmp(CoprocessorUnusableException);
    c.bind(l0);
}

void cfc2(u32 rt)
{
    Cop2Prolog();
    if (rt) c.movsxd(reg_alloc.GetHostGprMarkDirty(rt), dword_ptr(cop2_latch));
}

void ctc2(u32 rt)
{
    Cop2Prolog();
    c.movsxd(rax, reg_alloc.GetHostGpr(rt));
    c.mov(qword_ptr(cop2_latch), rax);
}

void dcfc2()
{
    Cop2Prolog();
    BlockEpilogWithJmp(ReservedInstructionCop2Exception);
    branched = true;
}

void dctc2()
{
    dcfc2();
}

void dmfc2(u32 rt)
{
    Cop2Prolog();
    if (rt) c.mov(reg_alloc.GetHostGprMarkDirty(rt), ptr(cop2_latch));
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

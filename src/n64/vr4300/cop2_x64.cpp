#include "vr4300/cop0.hpp"
#include "vr4300/cop2.hpp"
#include "vr4300/exceptions.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

void Cop2Prolog()
{
    reg_alloc.DestroyVolatile(host_gpr_arg[0]);
    Label l_noexception = c.newLabel();
    c.bt(JitPtr(cop0.status), 30); // cu2
    c.jc(l_noexception);
    c.mov(host_gpr_arg[0].r32(), 2);
    BlockEpilogWithPcFlushAndJmp((void*)CoprocessorUnusableException);
    c.bind(l_noexception);
}

void cfc2(u32 rt)
{
    Cop2Prolog();
    if (rt) {
        Gp ht = reg_alloc.GetDirtyGpr(rt);
        c.movsxd(ht, JitPtr(cop2_latch, 4));
    }
}

void cop2_reserved()
{
    Cop2Prolog();
    BlockEpilogWithPcFlushAndJmp((void*)ReservedInstructionCop2Exception);
    // compiler_exception_occurred = true; // TODO
}

void ctc2(u32 rt)
{
    Cop2Prolog();
    Gp ht = reg_alloc.GetGpr(rt);
    c.mov(JitPtr(cop2_latch), ht);
}

void dcfc2()
{
    Cop2Prolog();
    BlockEpilogWithPcFlushAndJmp((void*)ReservedInstructionCop2Exception);
    branched = true;
}

void dctc2()
{
    dcfc2();
}

void dmfc2(u32 rt)
{
    Cop2Prolog();
    if (rt) {
        Gp ht = reg_alloc.GetDirtyGpr(rt);
        c.mov(ht, JitPtr(cop2_latch));
    }
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

#include "cop2.hpp"
#include "cop0.hpp"
#include "exceptions.hpp"
#include "vr4300.hpp"

namespace n64::vr4300 {

static u64 cop2_latch;

void InitCop2()
{
    cop2_latch = 0;
}

void cfc2(u32 rt)
{
    if (cop0.status.cu2) {
        gpr.set(rt, s32(cop2_latch));

    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void ctc2(u32 rt)
{
    if (cop0.status.cu2) {
        cop2_latch = gpr[rt];
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void dcfc2()
{
    if (cop0.status.cu2) {
        SignalException<Exception::ReservedInstructionCop2>();
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void dctc2()
{
    if (cop0.status.cu2) {
        SignalException<Exception::ReservedInstructionCop2>();
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void dmfc2(u32 rt)
{
    if (cop0.status.cu2) {
        gpr.set(rt, cop2_latch);
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void dmtc2(u32 rt)
{
    if (cop0.status.cu2) {
        cop2_latch = gpr[rt];
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void mfc2(u32 rt)
{
    if (cop0.status.cu2) {
        gpr.set(rt, s32(cop2_latch));
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

void mtc2(u32 rt)
{
    if (cop0.status.cu2) {
        cop2_latch = gpr[rt];
    } else {
        SignalCoprocessorUnusableException(2);
    }
}

} // namespace n64::vr4300

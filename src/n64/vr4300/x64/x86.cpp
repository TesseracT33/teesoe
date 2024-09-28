#include "x86.hpp"
#include "recompiler.hpp"
#include "vr4300.hpp"

namespace n64::vr4300::x86 {

asmjit::x86::Mem GprPtr32(u32 gpr_idx)
{
    return JitPtrOffset(gpr.data(), gpr_idx, 4);
}

asmjit::x86::Mem GprPtr64(u32 gpr_idx)
{
    return JitPtrOffset(gpr.data(), gpr_idx, 8);
}

} // namespace n64::vr4300::x86

#pragma once

#include "vr4300/cop1.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

template<typename... Args> static void JitCallCop1InterpreterImpl(auto impl, Args... args)
{ /*
    Label l_noexception = c.newLabel();
    JitCallInterpreterImpl(impl, args...);
    c.cmp(ptr(exception_occurred), 0);
    c.je(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
    */
}

template<bool cond, bool likely> static void cop1_branch(s16 imm)
{ /*
    Label l_branch = c.newLabel(), l_end = c.newLabel();
    c.bt(ptr(fcr31), 23); // fcr31.c
    cond ? c.jc(l_branch) : c.jnc(l_branch);
    likely ? DiscardBranchJit() : OnBranchNotTakenJit();
    c.jmp(l_end);
    c.bind(l_branch);
    c.mov(rax, jit_pc + 4 + (imm << 2));
    TakeBranchJit(rax);
    c.bind(l_end);
    OnBranchJit();
    */
}

inline void bc1f(s16 imm)
{
    cop1_branch<false, false>(imm);
}

inline void bc1fl(s16 imm)
{
    cop1_branch<false, true>(imm);
}

inline void bc1t(s16 imm)
{
    cop1_branch<true, false>(imm);
}

inline void bc1tl(s16 imm)
{
    cop1_branch<true, true>(imm);
}

inline void cfc1(u32 fs, u32 rt)
{
    /*
    Gpq ht = rec_alloc.GetGprMarkDirty(rt);
    if (fs == 31) c.movsxd(ht, dword_ptr(fcr31));
    else if (fs == 0) c.mov(ht, 0xA00);
    else c.xor_(ht.r32(), ht.r32()); // TODO: what to do here?
    */
}

inline void ctc1(u32 fs, u32 rt)
{
    /*
    static constexpr u32 mask = 0x183'FFFF;
    if (fs != 31) return;
    Gpd ht = rec_alloc.GetGpr32(rt);
    c.mov(eax, ht);
    c.and_(eax, mask);
    c.and_(ptr(fcr31), ~mask);
    c.or_(ptr(fcr31), eax);
    */
}

inline void dcfc1()
{
}

inline void dctc1()
{
}

inline void dmfc1(u32 fs, u32 rt)
{
}

inline void dmtc1(u32 fs, u32 rt)
{
}

inline void ldc1(u32 base, u32 ft, s16 imm)
{
}

inline void lwc1(u32 base, u32 ft, s16 imm)
{
}

inline void mfc1(u32 fs, u32 rt)
{
}

inline void mtc1(u32 fs, u32 rt)
{
}

inline void sdc1(u32 base, u32 ft, s16 imm)
{
}

inline void swc1(u32 base, u32 ft, s16 imm)
{
}

template<Fmt fmt> inline void compare(u32 fs, u32 ft, u8 cond)
{
}

template<Fmt fmt> inline void ceil_l(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::ceil_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void ceil_w(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::ceil_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_d(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::cvt_d<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_l(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::cvt_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_s(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::cvt_s<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_w(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::cvt_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void floor_l(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::floor_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void floor_w(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::floor_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void round_l(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::round_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void round_w(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::round_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void trunc_l(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::trunc_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void trunc_w(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::trunc_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void abs(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::abs<fmt>, fs, fd);
}

template<Fmt fmt> inline void add(u32 fs, u32 ft, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::add<fmt>, fs, ft, fd);
}

template<Fmt fmt> inline void div(u32 fs, u32 ft, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::div<fmt>, fs, ft, fd);
}

template<Fmt fmt> inline void mov(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::mov<fmt>, fs, fd);
}

template<Fmt fmt> inline void mul(u32 fs, u32 ft, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::mul<fmt>, fs, ft, fd);
}

template<Fmt fmt> inline void neg(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::neg<fmt>, fs, fd);
}

template<Fmt fmt> inline void sqrt(u32 fs, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::sqrt<fmt>, fs, fd);
}

template<Fmt fmt> inline void sub(u32 fs, u32 ft, u32 fd)
{
    JitCallCop1InterpreterImpl(vr4300::sub<fmt>, fs, ft, fd);
}

} // namespace n64::vr4300::x64

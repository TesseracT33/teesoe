#pragma once

#include "vr4300/cop1.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

bool CheckCop1Usable();
void OnCop1Unusable();

template<typename... Args> static void CallCop1InterpreterImpl(auto impl, Args... args)
{
    Label l_noexception = c.newLabel();
    JitCallInterpreterImpl(impl, args...);
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
}

bool CheckCop1Usable()
{
    if (cop0.status.cu1) {
        c.and_(GlobalVarPtr(fcr31), 0xFFFC'0FFF); // clear all exceptions
        reg_alloc.Free(host_gpr_arg[0]);
        c.mov(host_gpr_arg[0].r32(), FE_ALL_EXCEPT);
        reg_alloc.Call(feclearexcept); // TODO: can we get rid of this?
        return true;
    } else {
        OnCop1Unusable();
        return false;
    }
}

template<bool cond, bool likely> static void Cop1Branch(s16 imm)
{
    if (!CheckCop1Usable()) return;
    Label l_branch = c.newLabel(), l_end = c.newLabel();
    c.bt(GlobalVarPtr(fcr31), 23); // fcr31.c
    cond ? c.jc(l_branch) : c.jnc(l_branch);
    likely ? DiscardBranchJit() : OnBranchNotTakenJit();
    c.jmp(l_end);
    c.bind(l_branch);
    c.mov(rax, jit_pc + 4 + (imm << 2));
    TakeBranchJit(rax);
    c.bind(l_end);
    branch_hit = true;
}

Gpq GetGpr(u32 idx)
{
    return reg_alloc.GetHostGpr(idx);
}

Gpq GetDirtyGpr(u32 idx)
{
    return reg_alloc.GetHostGprMarkDirty(idx);
}

template<size_t size> Xmm GetFs(u32 idx)
{
    if (!cop0.status.fr) idx &= ~1;
    if constexpr (size == 4) {
        // return reg_alloc.GetFpr32(idx);
        return xmm0;
    } else {
        // return reg_alloc.GetFpr64(idx);
        return xmm0;
    }
}

template<size_t size> Xmm GetFt(u32 idx)
{
    if constexpr (size == 4) {
        // return reg_alloc.GetFpr32(idx);
        return xmm0;
    } else {
        // return reg_alloc.GetFpr64(idx);
        return xmm0;
    }
}

template<size_t size> Xmm GetMoveLoadStore(u32 idx)
{
    if constexpr (size == 4) {
        // return reg_alloc.GetFpr32(idx);
        return xmm0;
    } else {
        // return reg_alloc.GetFpr64(idx);
        return xmm0;
    }
}

void OnCop1Unusable()
{
    reg_alloc.Free(host_gpr_arg[0]);
    c.mov(host_gpr_arg[0].r32(), 1);
    BlockEpilogWithJmp(CoprocessorUnusableException);
    branched = true;
}

static void OnInvalidFormat()
{
    if (!CheckCop1Usable()) return;
    c.bts(GlobalVarPtr(fcr31), FCR31BitIndex::CauseUnimplemented);
    block_cycles++;
    BlockEpilogWithJmp(FloatingPointException);
    branched = true;
}

inline void bc1f(s16 imm)
{
    Cop1Branch<false, false>(imm);
}

inline void bc1fl(s16 imm)
{
    Cop1Branch<false, true>(imm);
}

inline void bc1t(s16 imm)
{
    Cop1Branch<true, false>(imm);
}

inline void bc1tl(s16 imm)
{
    Cop1Branch<true, true>(imm);
}

inline void cfc1(u32 fs, u32 rt)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    Gpq ht = reg_alloc.GetHostGprMarkDirty(rt);
    if (fs == 31) c.movsxd(ht, GlobalVarPtr(fcr31));
    else if (fs == 0) c.mov(ht, 0xA00);
    else c.xor_(ht.r32(), ht.r32());
}

inline void ctc1(u32 fs, u32 rt)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (fs != 31) return;
    Gpd ht = reg_alloc.GetHostGpr(rt).r32();
    c.mov(eax, ht);
    c.and_(eax, fcr31_write_mask);
    c.and_(GlobalVarPtr(fcr31), ~fcr31_write_mask);
    c.or_(GlobalVarPtr(fcr31), eax);
    c.and_(eax, 3);
    c.mov(host_gpr_arg[0].r32(), GlobalArrPtrWithRegOffset(guest_to_host_rounding_mode, rax, 4));
    reg_alloc.Call(fesetround);
}

inline void dcfc1()
{
    if (!CheckCop1Usable()) return;
    c.bts(GlobalVarPtr(fcr31), FCR31BitIndex::CauseUnimplemented);
    block_cycles++;
    BlockEpilogWithJmp(FloatingPointException);
    branched = true;
}

inline void dctc1()
{
    dcfc1();
}

inline void dmfc1(u32 fs, u32 rt)
{
    // if (!cop0.status.cu1) return OnCop1Unusable();
    // Gpq hrt = GetDirtyGpr(rt);
    // if (!cop0.status.fr) fs &= ~1;
    // if (IsBoundAndDirty(fs)) {
    //     Xmm hfs = GetFpr<8>(fs);
    //     c.vcvtss2si(hrt, hfs);
    // } else {
    //     c.mov(hrt, GlobalArrPtrWithImmOffset(fpr, fs, 8));
    // }
}

inline void dmtc1(u32 fs, u32 rt)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
}

inline void ldc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
}

inline void lwc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
}

inline void mfc1(u32 fs, u32 rt)
{
    if (!CheckCop1Usable()) return;
}

inline void mtc1(u32 fs, u32 rt)
{
    if (!CheckCop1Usable()) return;
}

inline void sdc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    reg_alloc.Free<2>({ host_gpr_arg[0], host_gpr_arg[1] });
    // c.lea(host_gpr_arg[0], ptr(hbase, imm));
}

inline void swc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
}

template<Fmt fmt> inline void compare(u32 fs, u32 ft, u8 cond)
{
}

template<Fmt fmt> inline void ceil_l(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void ceil_w(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void cvt_d(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void cvt_l(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void cvt_s(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void cvt_w(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void floor_l(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void floor_w(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void round_l(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void round_w(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void trunc_l(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void trunc_w(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void abs(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void add(u32 fs, u32 ft, u32 fd)
{
}

template<Fmt fmt> inline void div(u32 fs, u32 ft, u32 fd)
{
}

template<Fmt fmt> inline void mov(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void mul(u32 fs, u32 ft, u32 fd)
{
}

template<Fmt fmt> inline void neg(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void sqrt(u32 fs, u32 fd)
{
}

template<Fmt fmt> inline void sub(u32 fs, u32 ft, u32 fd)
{
}

} // namespace n64::vr4300::x64

#pragma once

#include "vr4300/cop1.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

inline constexpr std::array abs_f32_mask{ 0x7FFF'FFFF, 0x7FFF'FFFF, 0x7FFF'FFFF, 0x7FFF'FFFF };
inline constexpr std::array abs_f64_mask{ 0x7FFF'FFFF'FFFF'FFFF, 0x7FFF'FFFF'FFFF'FFFF };
inline constexpr std::array neg_f32_mask{ 0x8000'0000, 0x8000'0000, 0x8000'0000, 0x8000'0000 };
inline constexpr std::array neg_f64_mask{ 0x8000'0000'0000'0000, 0x8000'0000'0000'0000 };

bool CheckCop1Usable();
void OnCop1Unusable();

void CallCop1InterpreterImpl(auto impl, u32 fs, u32 fd)
{
    Label l_noexception = c.newLabel();
    reg_alloc.FreeArgs(2);
    fs ? c.mov(host_gpr_arg[0].r32(), fs) : c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    fd ? c.mov(host_gpr_arg[1].r32(), fd) : c.xor_(host_gpr_arg[1].r32(), host_gpr_arg[1].r32());
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
}

void CallCop1InterpreterImpl(auto impl, u32 fs, u32 ft, u32 fd)
{
    Label l_noexception = c.newLabel();
    reg_alloc.FreeArgs(3);
    fs ? c.mov(host_gpr_arg[0].r32(), fs) : c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    ft ? c.mov(host_gpr_arg[1].r32(), ft) : c.xor_(host_gpr_arg[1].r32(), host_gpr_arg[1].r32());
    fd ? c.mov(host_gpr_arg[2].r32(), fd) : c.xor_(host_gpr_arg[2].r32(), host_gpr_arg[2].r32());
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
}

bool CheckCop1Usable()
{
    if (cop0.status.cu1) {
        c.and_(GlobalVarPtr(fcr31), 0xFFFC'0FFF); // clear all exceptions
        reg_alloc.ReserveArgs(1);
        c.mov(host_gpr_arg[0].r32(), FE_ALL_EXCEPT);
        reg_alloc.Call(feclearexcept); // TODO: can we get rid of this?
        return true;
    } else {
        OnCop1Unusable();
        return false;
    }
}

template<bool cond, bool likely> void Cop1Branch(s16 imm)
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

void OnCop1Unusable()
{
    reg_alloc.ReserveArgs(1);
    c.mov(host_gpr_arg[0].r32(), 1);
    BlockEpilogWithJmp(CoprocessorUnusableException);
    branched = true;
}

void OnInvalidFormat()
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
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) fs &= ~1;
    Gpq hrt = GetDirtyGpr(rt);
    c.mov(hrt, GlobalArrPtrWithImmOffset(fpr, fs * 8, 8));
    block_cycles++;
}

inline void dmtc1(u32 fs, u32 rt)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) fs &= ~1;
    Gpq hrt = GetGpr(rt);
    c.mov(GlobalArrPtrWithImmOffset(fpr, fs * 8, 8), hrt);
    block_cycles++;
}

inline void ldc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) ft &= ~1;
    Label l_no_exception = c.newLabel();
    reg_alloc.ReserveArgs(1);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    reg_alloc.Call(ReadVirtual<s64>);
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_no_exception);

    BlockEpilog();

    c.bind(l_no_exception);
    c.mov(GlobalArrPtrWithImmOffset(fpr, ft * 8, 8), rax);

    block_cycles++;
}

inline void lwc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    Label l_no_exception = c.newLabel();
    reg_alloc.ReserveArgs(1);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    reg_alloc.Call(ReadVirtual<s32>);
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_no_exception);

    BlockEpilog();

    c.bind(l_no_exception);
    if (cop0.status.fr || !(ft & 1)) {
        c.mov(GlobalArrPtrWithImmOffset(fpr, ft * 8, 4), eax);
    } else {
        c.mov(GlobalArrPtrWithImmOffset(fpr, (ft & ~1) * 8 + 4, 4), eax);
    }

    block_cycles++;
}

inline void mfc1(u32 fs, u32 rt)
{
    if (!CheckCop1Usable()) return;
    Gpq hrt = GetDirtyGpr(rt);
    if (cop0.status.fr || !(fs & 1)) {
        c.movsxd(hrt, GlobalArrPtrWithImmOffset(fpr, fs * 8, 4));
    } else {
        c.movsxd(hrt, GlobalArrPtrWithImmOffset(fpr, (fs & ~1) * 8 + 4, 4));
    }
    block_cycles++;
}

inline void mtc1(u32 fs, u32 rt)
{
    if (!CheckCop1Usable()) return;
    Gpd hrt = GetGpr(rt).r32();
    if (cop0.status.fr || !(fs & 1)) {
        c.mov(GlobalArrPtrWithImmOffset(fpr, fs * 8, 4), hrt);
    } else {
        c.mov(GlobalArrPtrWithImmOffset(fpr, (fs & ~1) * 8 + 4, 4), hrt);
    }
    block_cycles++;
}

inline void sdc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) ft &= ~1;
    Label l_end = c.newLabel();
    reg_alloc.ReserveArgs(2);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    c.mov(host_gpr_arg[1], GlobalArrPtrWithImmOffset(fpr, ft * 8, 8));
    reg_alloc.Call(WriteVirtual<8>);
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
    block_cycles++;
}

inline void swc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    Label l_end = c.newLabel();
    reg_alloc.ReserveArgs(2);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    if (cop0.status.fr || !(ft & 1)) {
        c.movsxd(host_gpr_arg[1], GlobalArrPtrWithImmOffset(fpr, ft * 8, 4));
    } else {
        c.movsxd(host_gpr_arg[1], GlobalArrPtrWithImmOffset(fpr, (ft & ~1) * 8 + 4, 4));
    }
    reg_alloc.Call(WriteVirtual<4>);
    c.cmp(GlobalVarPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
    block_cycles++;
}

template<Fmt fmt> inline void compare(u32 fs, u32 ft, u8 cond)
{
    CallCop1InterpreterImpl(vr4300::compare<fmt>, fs, cond);
}

template<Fmt fmt> inline void ceil_l(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::ceil_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void ceil_w(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::ceil_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_d(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::cvt_d<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_l(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::cvt_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_s(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::cvt_s<fmt>, fs, fd);
}

template<Fmt fmt> inline void cvt_w(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::cvt_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void floor_l(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::floor_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void floor_w(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::floor_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void round_l(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::round_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void round_w(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::round_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void trunc_l(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::trunc_l<fmt>, fs, fd);
}

template<Fmt fmt> inline void trunc_w(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::trunc_w<fmt>, fs, fd);
}

template<Fmt fmt> inline void abs(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::abs<fmt>, fs, fd);
}

template<Fmt fmt> inline void add(u32 fs, u32 ft, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::add<fmt>, fs, ft, fd);
}

template<Fmt fmt> inline void div(u32 fs, u32 ft, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::div<fmt>, fs, ft, fd);
}

template<Fmt fmt> inline void mov(u32 fs, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        if (cop0.status.cu1) {
            if (!cop0.status.fr) fs &= ~1;
            c.mov(rax, GlobalArrPtrWithImmOffset(fpr, 8 * fs, 8));
            c.mov(GlobalArrPtrWithImmOffset(fpr, 8 * fd, 8), rax);
        } else {
            reg_alloc.ReserveArgs(1);
            c.mov(host_gpr_arg[0], 1);
            reg_alloc.BlockEpilogWithJmp(CoprocessorUnusableException);
        }
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> inline void mul(u32 fs, u32 ft, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::mul<fmt>, fs, ft, fd);
}

template<Fmt fmt> inline void neg(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::neg<fmt>, fs, fd);
}

template<Fmt fmt> inline void sqrt(u32 fs, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::sqrt<fmt>, fs, fd);
}

template<Fmt fmt> inline void sub(u32 fs, u32 ft, u32 fd)
{
    CallCop1InterpreterImpl(vr4300::sub<fmt>, fs, ft, fd);
}

} // namespace n64::vr4300::x64

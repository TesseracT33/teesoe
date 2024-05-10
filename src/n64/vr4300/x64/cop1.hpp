#pragma once

#include "algorithm"
#include "vr4300/cop1.hpp"
#include "vr4300/recompiler.hpp"

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

alignas(16) inline constexpr std::array abs_f32_mask{ 0x7FFF'FFFF, 0x7FFF'FFFF, 0x7FFF'FFFF, 0x7FFF'FFFF };
alignas(16) inline constexpr std::array abs_f64_mask{ 0x7FFF'FFFF'FFFF'FFFF, 0x7FFF'FFFF'FFFF'FFFF };
alignas(16) inline constexpr std::array neg_f32_mask{ 0x8000'0000, 0x8000'0000, 0x8000'0000, 0x8000'0000 };
alignas(16) inline constexpr std::array neg_f64_mask{ 0x8000'0000'0000'0000, 0x8000'0000'0000'0000 };

bool CheckCop1Usable();
void OnCop1Unusable();

void CallCop1InterpreterImpl(auto impl, u32 fs, u32 fd)
{
    Label l_noexception = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(2);
    fs ? c.mov(host_gpr_arg[0].r32(), fs) : c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    fd ? c.mov(host_gpr_arg[1].r32(), fd) : c.xor_(host_gpr_arg[1].r32(), host_gpr_arg[1].r32());
    reg_alloc.Call(impl);
    reg_alloc.FreeArgs(2);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
}

void CallCop1InterpreterImpl(auto impl, u32 fs, u32 ft, u32 fd)
{
    Label l_noexception = c.newLabel();
    FlushPc();
    reg_alloc.ReserveArgs(3);
    fs ? c.mov(host_gpr_arg[0].r32(), fs) : c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    ft ? c.mov(host_gpr_arg[1].r32(), ft) : c.xor_(host_gpr_arg[1].r32(), host_gpr_arg[1].r32());
    fd ? c.mov(host_gpr_arg[2].r32(), fd) : c.xor_(host_gpr_arg[2].r32(), host_gpr_arg[2].r32());
    reg_alloc.Call(impl);
    reg_alloc.FreeArgs(3);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
}

bool CheckCop1Usable()
{
    if (cop0.status.cu1) {
        c.and_(JitPtr(fcr31), 0xFFFC'0FFF); // clear all exceptions
        c.vstmxcsr(dword_ptr(x86::rsp, -8));
        c.and_(dword_ptr(x86::rsp, -8), ~0x3D);
        c.vldmxcsr(dword_ptr(x86::rsp, -8));
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
    c.bt(JitPtr(fcr31), FCR31BitIndex::Condition);
    cond ? c.jc(l_branch) : c.jnc(l_branch);
    likely ? DiscardBranchJit() : OnBranchNotTakenJit();
    c.jmp(l_end);
    c.bind(l_branch);
    TakeBranchJit(jit_pc + 4 + (imm << 2));
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
    reg_alloc.FlushAll();
    c.mov(host_gpr_arg[0].r32(), 1);
    BlockEpilogWithPcFlushAndJmp(CoprocessorUnusableException);
    branched = true;
}

void OnInvalidFormat()
{
    if (!CheckCop1Usable()) return;
    c.bts(JitPtr(fcr31), FCR31BitIndex::CauseUnimplemented);
    block_cycles++;
    BlockEpilogWithPcFlushAndJmp(FloatingPointException);
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
    Gpq ht = GetDirtyGpr(rt);
    if (fs == 31) c.movsxd(ht, JitPtr(fcr31));
    else if (fs == 0) c.mov(ht.r32(), 0xA00);
    else c.xor_(ht.r32(), ht.r32());
}

inline void ctc1(u32 fs, u32 rt)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (fs != 31) return;
    reg_alloc.ReserveArgs(1);
    Label l_no_exception = c.newLabel();
    Gpd ht = GetGpr(rt).r32();
    c.mov(eax, ht);
    c.and_(eax, fcr31_write_mask);
    c.and_(JitPtr(fcr31), ~fcr31_write_mask);
    c.or_(JitPtr(fcr31), eax);
    c.and_(eax, 3);
    c.shl(eax, 2);
    c.mov(host_gpr_arg[0].r32(), JitPtrOffset(guest_to_host_rounding_mode, rax, 4));
    reg_alloc.Call(fesetround);
    reg_alloc.Call(TestExceptions<false>);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_no_exception);
    BlockEpilog();
    c.bind(l_no_exception);
    reg_alloc.FreeArgs(1);
}

inline void dcfc1()
{
    if (!CheckCop1Usable()) return;
    c.bts(JitPtr(fcr31), FCR31BitIndex::CauseUnimplemented);
    block_cycles++;
    BlockEpilogWithPcFlushAndJmp(FloatingPointException);
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
    c.mov(hrt, JitPtrOffset(fpr, fs * 8, 8));
    block_cycles++;
}

inline void dmtc1(u32 fs, u32 rt)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) fs &= ~1;
    Gpq hrt = GetGpr(rt);
    c.mov(JitPtrOffset(fpr, fs * 8, 8), hrt);
    block_cycles++;
}

inline void ldc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) ft &= ~1;
    FlushPc();
    Label l_no_exception = c.newLabel();
    reg_alloc.ReserveArgs(1);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    reg_alloc.Call(ReadVirtual<s64>);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_no_exception);

    BlockEpilog();

    c.bind(l_no_exception);
    c.mov(JitPtrOffset(fpr, ft * 8, 8), rax);

    block_cycles++;
    reg_alloc.FreeArgs(1);
}

inline void lwc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    FlushPc();
    Label l_no_exception = c.newLabel();
    reg_alloc.ReserveArgs(1);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    reg_alloc.Call(ReadVirtual<s32>);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_no_exception);

    BlockEpilog();

    c.bind(l_no_exception);
    if (cop0.status.fr || !(ft & 1)) {
        c.mov(JitPtrOffset(fpr, ft * 8, 4), eax);
    } else {
        c.mov(JitPtrOffset(fpr, (ft & ~1) * 8 + 4, 4), eax);
    }

    block_cycles++;
    reg_alloc.FreeArgs(1);
}

inline void mfc1(u32 fs, u32 rt)
{
    if (!CheckCop1Usable()) return;
    Gpq hrt = GetDirtyGpr(rt);
    if (cop0.status.fr || !(fs & 1)) {
        c.movsxd(hrt, JitPtrOffset(fpr, fs * 8, 4));
    } else {
        c.movsxd(hrt, JitPtrOffset(fpr, (fs & ~1) * 8 + 4, 4));
    }
    block_cycles++;
}

inline void mtc1(u32 fs, u32 rt)
{
    if (!CheckCop1Usable()) return;
    Gpd hrt = GetGpr(rt).r32();
    if (cop0.status.fr || !(fs & 1)) {
        c.mov(JitPtrOffset(fpr, fs * 8, 4), hrt);
    } else {
        c.mov(JitPtrOffset(fpr, (fs & ~1) * 8 + 4, 4), hrt);
    }
    block_cycles++;
}

inline void sdc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    if (!cop0.status.fr) ft &= ~1;
    FlushPc();
    Label l_end = c.newLabel();
    reg_alloc.ReserveArgs(2);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    c.mov(host_gpr_arg[1], JitPtrOffset(fpr, ft * 8, 8));
    reg_alloc.Call(WriteVirtual<8>);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
    block_cycles++;
    reg_alloc.FreeArgs(2);
}

inline void swc1(u32 base, u32 ft, s16 imm)
{
    if (!cop0.status.cu1) return OnCop1Unusable();
    FlushPc();
    Label l_end = c.newLabel();
    reg_alloc.ReserveArgs(2);
    Gpq hbase = GetGpr(base);
    c.lea(host_gpr_arg[0], ptr(hbase, imm));
    if (cop0.status.fr || !(ft & 1)) {
        c.movsxd(host_gpr_arg[1], JitPtrOffset(fpr, ft * 8, 4));
    } else {
        c.movsxd(host_gpr_arg[1], JitPtrOffset(fpr, (ft & ~1) * 8 + 4, 4));
    }
    reg_alloc.Call(WriteVirtual<4>);
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_end);

    BlockEpilog();

    c.bind(l_end);
    block_cycles++;
    reg_alloc.FreeArgs(2);
}

template<Fmt fmt> inline void compare(u32 fs, u32 ft, u8 cond)
{
    if constexpr (!OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        return OnInvalidFormat();
    }
    if (!CheckCop1Usable()) return;
    if (!cop0.status.fr) fs &= ~1;
    reg_alloc.Reserve<2>({ rcx, rdx });
    Label l_no_nans = c.newLabel(), l_exception = c.newLabel(), l_end = c.newLabel();

    c.movq(xmm0, JitPtrOffset(fpr, 8 * fs, 8));
    if (cond & 8) {
        fmt == Fmt::Float32 ? c.vcomiss(xmm0, JitPtrOffset(fpr, 8 * ft, 4))
                            : c.vcomisd(xmm0, JitPtrOffset(fpr, 8 * ft, 8));
    } else {
        fmt == Fmt::Float32 ? c.vucomiss(xmm0, JitPtrOffset(fpr, 8 * ft, 4))
                            : c.vucomisd(xmm0, JitPtrOffset(fpr, 8 * ft, 8));
    }
    c.setp(al);

    c.jnp(l_no_nans);
    c.vstmxcsr(dword_ptr(x86::rsp, -8));
    c.mov(eax, dword_ptr(x86::rsp, -8));
    c.and_(eax, 1); // invalid exception
    c.mov(ecx, eax);
    c.shl(ecx, FCR31BitIndex::CauseInvalid);
    c.or_(JitPtr(fcr31), ecx);
    c.bt(JitPtr(fcr31), FCR31BitIndex::EnableInvalid);
    c.setc(cl);
    c.mov(edx, ecx);
    c.not_(edx);
    c.and_(edx, 1);
    c.shl(edx, FCR31BitIndex::FlagInvalid);
    c.or_(JitPtr(fcr31), edx);
    c.and_(eax, ecx);
    c.and_(eax, 1);
    c.jnz(l_exception);
    cond & 1 ? c.bts(JitPtr(fcr31), FCR31BitIndex::Condition)
             : c.btr(JitPtr(fcr31), FCR31BitIndex::Condition);
    c.jmp(l_end);

    c.bind(l_exception);
    BlockEpilogWithPcFlushAndJmp(FloatingPointException);

    c.bind(l_no_nans);
    u8 cond12 = cond >> 1 & 3;
    if (cond12) {
        switch (cond12) {
        case 1: c.setz(al); break;
        case 2: c.setc(al); break;
        case 3:
            c.setz(al);
            c.setc(cl);
            c.or_(eax, ecx);
            break;
        default: std::unreachable();
        }
        c.and_(eax, 1);
        c.shl(eax, FCR31BitIndex::Condition);
        c.btr(JitPtr(fcr31), FCR31BitIndex::Condition);
        c.or_(JitPtr(fcr31), eax);
    } else {
        c.btr(JitPtr(fcr31), FCR31BitIndex::Condition);
    }

    c.bind(l_end);
    reg_alloc.Free<2>({ rcx, rdx });
}

template<Fmt fmt> void ceil_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::CEIL, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void ceil_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::CEIL, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
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

template<Fmt fmt> void floor_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::FLOOR, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void floor_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::FLOOR, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void round_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::ROUND, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void round_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::ROUND, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void trunc_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::TRUNC, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void trunc_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::TRUNC, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> inline void abs(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr1Op::ABS, typename FmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> inline void add(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::ADD, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> inline void div(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::DIV, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> inline void mov(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        if (cop0.status.cu1) {
            if (!cop0.status.fr) fs &= ~1;
            c.mov(rax, JitPtrOffset(fpr, 8 * fs, 8));
            c.mov(JitPtrOffset(fpr, 8 * fd, 8), rax);
        } else {
            reg_alloc.FlushAll();
            c.mov(host_gpr_arg[0].r32(), 1);
            BlockEpilogWithPcFlushAndJmp(CoprocessorUnusableException);
            branched = true;
        }
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void mul(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::MUL, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void neg(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr1Op::NEG, typename FmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void sqrt(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr1Op::SQRT, typename FmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void sub(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::SUB, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<ComputeInstr1Op instr, std::floating_point Float> void Compute(u32 fs, u32 fd)
{
    using enum ComputeInstr1Op;
    if (!CheckCop1Usable()) return;
    if (!cop0.status.fr) fs &= ~1;
    Label l_epilog = c.newLabel(), l_end = c.newLabel();

    FlushPc();
    c.sub(x86::rsp, 16);
    c.vmovq(xmm0, JitPtrOffset(fpr, 8 * fs, 8));
    c.vmovq(qword_ptr(x86::rsp), xmm0);
    reg_alloc.Call(IsValidInput<Float>);
    c.test(al, al);
    c.jz(l_epilog);
    if constexpr (sizeof(Float) == 4) {
        c.vmovd(xmm0, dword_ptr(x86::rsp));
    } else {
        c.vmovq(xmm0, qword_ptr(x86::rsp));
    }
    if constexpr (instr == ABS) {
        if constexpr (sizeof(Float) == 4) {
            c.vandps(xmm0, xmm0, JitPtr(abs_f32_mask));
        } else {
            c.vandpd(xmm0, xmm0, JitPtr(abs_f64_mask));
        }
    }
    if constexpr (instr == NEG) {
        if constexpr (sizeof(Float) == 4) {
            c.vxorps(xmm0, xmm0, JitPtr(neg_f32_mask));
        } else {
            c.vxorpd(xmm0, xmm0, JitPtr(neg_f64_mask));
        }
    }
    if constexpr (instr == SQRT) {
        if constexpr (sizeof(Float) == 4) {
            c.vsqrtps(xmm0, xmm0);
            block_cycles += 28;
        } else {
            c.vsqrtpd(xmm0, xmm0);
            block_cycles += 57;
        }
    }
    c.vmovq(qword_ptr(x86::rsp), xmm0);
    reg_alloc.Call(GetAndTestExceptions);
    c.test(al, al);
    c.jnz(l_epilog);
    c.mov(host_gpr_arg[0], x86::rsp);
    reg_alloc.Call(IsValidOutput<Float>);
    c.test(al, al);
    c.jz(l_epilog);
    if constexpr (sizeof(Float) == 4) {
        c.mov(eax, dword_ptr(x86::rsp));
    } else {
        c.mov(rax, qword_ptr(x86::rsp));
    }
    c.mov(JitPtrOffset(fpr, 8 * fd, 8), rax);
    c.add(x86::rsp, 16);
    c.jmp(l_end);

    c.bind(l_epilog);
    c.add(x86::rsp, 16);
    BlockEpilog();

    c.bind(l_end);
}

template<ComputeInstr2Op instr, std::floating_point Float> void Compute(u32 fs, u32 ft, u32 fd)
{
    using enum ComputeInstr2Op;
    if (!CheckCop1Usable()) return;
    if (!cop0.status.fr) fs &= ~1;
    Label l_epilog = c.newLabel(), l_end = c.newLabel();

    FlushPc();
    c.sub(x86::rsp, 16);
    c.vmovq(xmm0, JitPtrOffset(fpr, 8 * fs, 8));
    c.vmovq(qword_ptr(x86::rsp), xmm0);
    reg_alloc.Call(IsValidInput<Float>);
    c.test(al, al);
    c.jz(l_epilog);
    c.vmovq(xmm0, JitPtrOffset(fpr, 8 * ft, 8));
    c.vmovq(qword_ptr(x86::rsp, 8), xmm0);
    reg_alloc.Call(IsValidInput<Float>);
    c.test(al, al);
    c.jz(l_epilog);
    c.vmovq(xmm0, qword_ptr(x86::rsp));
    if constexpr (instr == ADD) {
        if constexpr (sizeof(Float) == 4) {
            c.vaddps(xmm0, xmm0, xmmword_ptr(x86::rsp, 8));
        } else {
            c.vaddpd(xmm0, xmm0, xmmword_ptr(x86::rsp, 8));
        }
        block_cycles += 2;
    }
    if constexpr (instr == SUB) {
        if constexpr (sizeof(Float) == 4) {
            c.vsubps(xmm0, xmm0, xmmword_ptr(x86::rsp, 8));
        } else {
            c.vsubpd(xmm0, xmm0, xmmword_ptr(x86::rsp, 8));
        }
        block_cycles += 2;
    }
    if constexpr (instr == MUL) {
        if constexpr (sizeof(Float) == 4) {
            c.vmulps(xmm0, xmm0, xmmword_ptr(x86::rsp, 8));
            block_cycles += 4;
        } else {
            c.vmulpd(xmm0, xmm0, xmmword_ptr(x86::rsp, 8));
            block_cycles += 7;
        }
    }
    if constexpr (instr == DIV) {
        c.vmovq(xmm1, qword_ptr(x86::rsp, 8));
        if constexpr (sizeof(Float) == 4) {
            static constexpr std::array div_or_vec = { 0, 1, 1, 1 };
            c.vpor(xmm1, xmm1, JitPtr(div_or_vec)); // avoid div by zero for "unused" lanes
            c.vdivps(xmm0, xmm0, xmm1);
            block_cycles += 28;
        } else {
            static constexpr std::array div_or_vec = { 0_u64, 1_u64 };
            c.vpor(xmm1, xmm1, JitPtr(div_or_vec));
            c.vdivpd(xmm0, xmm0, xmm1);
            block_cycles += 57;
        }
    }
    c.vmovq(qword_ptr(x86::rsp), xmm0);
    reg_alloc.Call(GetAndTestExceptions);
    c.test(al, al);
    c.jnz(l_epilog);
    c.mov(host_gpr_arg[0], x86::rsp);
    reg_alloc.Call(IsValidOutput<Float>);
    c.test(al, al);
    c.jz(l_epilog);
    if constexpr (sizeof(Float) == 4) {
        c.mov(eax, dword_ptr(x86::rsp));
    } else {
        c.mov(rax, qword_ptr(x86::rsp));
    }
    c.mov(JitPtrOffset(fpr, 8 * fd, 8), rax);
    c.add(x86::rsp, 16);
    c.jmp(l_end);

    c.bind(l_epilog);
    c.add(x86::rsp, 16);
    BlockEpilog();

    c.bind(l_end);
}
/*
 template<FpuNum From, FpuNum To> static void Convert(u32 fs, u32 fd)
{
    if (!CheckCop1Usable()) return;
    if (!cop0.status.fr) fs &= ~1;
    Label l_epilog = c.newLabel(), l_end = c.newLabel();

 FlushPc();
    c.sub(x86::rsp, 16);
    c.vmovq(xmm0, JitPtrOffset(fpr, 8 * fs, 8));
    c.vmovq(qword_ptr(x86::rsp), xmm0);
    reg_alloc.Call(IsValidInputCvtRound<To>);
    c.test(al, al);
    c.jz(l_epilog);
    c.vmovq(xmm0, qword_ptr(x86::rsp));
    static constexpr int rounding_mode = [] {
        if constexpr (instr == ROUND) return 0;
        if constexpr (instr == FLOOR) return 9;
        if constexpr (instr == CEIL) return 10;
        if constexpr (instr == TRUNC) return 11;
    }();
    if constexpr (sizeof(From) == 4) {
        c.vroundss(xmm0, xmm0, xmm0, rounding_mode);
    } else {
        c.vroundsd(xmm0, xmm0, xmm0, rounding_mode);
    }
    if constexpr (sizeof(To) == 4) {
        c.vcvttss2si(eax, xmm0);
        c.mov(dword_ptr(x86::rsp, 8), eax);
        reg_alloc.Call(GetAndTestExceptionsConvFloatToWord);
    } else {
        c.vcvttsd2si(rax, xmm0);
        c.mov(qword_ptr(x86::rsp, 8), rax);
        reg_alloc.Call(GetAndTestExceptions);
    }
    c.test(al, al);
    c.jnz(l_epilog);
        c.mov(host_gpr_arg[0], x86::rsp);
    reg_alloc.Call(IsValidOutput<Float>);
    c.test(al, al);
    c.jz(l_epilog);
    if constexpr (sizeof(From) == 4) {
        c.vcvtsi2ss(xmm0, xmm0, dword_ptr(x86::rsp, 8));
        c.vucomiss(xmm0, dword_ptr(x86::rsp));
    } else {
        c.vcvtsi2ss(xmm0, xmm0, qword_ptr(x86::rsp, 8));
        c.vucomisd(xmm0, qword_ptr(x86::rsp));
    }
    if constexpr (sizeof(Float) == 4) {
        c.mov(eax, dword_ptr(x86::rsp));
    } else {
        c.mov(rax, qword_ptr(x86::rsp));
    }
    c.mov(JitPtrOffset(fpr, 8 * fd, 8), rax);
    c.add(x86::rsp, 16);
    c.jmp(l_end);

    c.bind(l_epilog);
    c.add(x86::rsp, 16);
    BlockEpilog();

    c.bind(l_end);
    block_cycles += 4;
}
*/
template<RoundInstr instr, FpuNum From, FpuNum To> void Round(u32 fs, u32 fd)
{
    using enum RoundInstr;
    if (!CheckCop1Usable()) return;
    if (!cop0.status.fr) fs &= ~1;
    Label l_epilog = c.newLabel(), l_end = c.newLabel();

    FlushPc();
    c.sub(x86::rsp, 16);
    c.vmovq(xmm0, JitPtrOffset(fpr, 8 * fs, 8));
    c.vmovq(qword_ptr(x86::rsp), xmm0);
    reg_alloc.Call(IsValidInputCvtRound<To, From>);
    c.test(al, al);
    c.jz(l_epilog);
    c.vmovq(xmm0, qword_ptr(x86::rsp));
    static constexpr int rounding_mode = [] {
        if constexpr (instr == ROUND) return 0;
        if constexpr (instr == FLOOR) return 9;
        if constexpr (instr == CEIL) return 10;
        if constexpr (instr == TRUNC) return 11;
    }();
    if constexpr (sizeof(From) == 4) {
        c.vroundss(xmm0, xmm0, xmm0, rounding_mode);
    } else {
        c.vroundsd(xmm0, xmm0, xmm0, rounding_mode);
    }
    if constexpr (sizeof(To) == 4) {
        c.vcvttss2si(eax, xmm0);
        c.mov(dword_ptr(x86::rsp, 8), eax);
        reg_alloc.Call(GetAndTestExceptionsConvFloatToWord);
    } else {
        c.vcvttsd2si(rax, xmm0);
        c.mov(qword_ptr(x86::rsp, 8), rax);
        reg_alloc.Call(GetAndTestExceptions);
    }
    c.test(al, al);
    c.jnz(l_epilog);
    c.mov(host_gpr_arg[0], x86::rsp);
    // TODO: this doesn't compile, and I have no fucking idea why
    // reg_alloc.Call(IsValidOutput<To>);
    c.test(al, al);
    c.jz(l_epilog);
    if constexpr (sizeof(From) == 4) {
        c.vcvtsi2ss(xmm0, xmm0, dword_ptr(x86::rsp, 8));
        c.vucomiss(xmm0, dword_ptr(x86::rsp));
    } else {
        c.vcvtsi2ss(xmm0, xmm0, qword_ptr(x86::rsp, 8));
        c.vucomisd(xmm0, qword_ptr(x86::rsp));
    }
    if constexpr (sizeof(To) == 4) {
        c.mov(eax, dword_ptr(x86::rsp));
    } else {
        c.mov(rax, qword_ptr(x86::rsp));
    }
    c.mov(JitPtrOffset(fpr, 8 * fd, 8), rax);
    c.add(x86::rsp, 16);
    c.jmp(l_end);

    c.bind(l_epilog);
    c.add(x86::rsp, 16);
    BlockEpilog();

    c.bind(l_end);
    block_cycles += 4;
}

} // namespace n64::vr4300::x64

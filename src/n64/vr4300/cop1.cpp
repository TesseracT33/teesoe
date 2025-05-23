#include "cop1.hpp"
#include "algorithm.hpp"
#include "cop0.hpp"
#include "exceptions.hpp"
#include "mmu.hpp"
#include "platform.hpp"
#include "vr4300.hpp"
#include "vr4300/interpreter.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

#if PLATFORM_X64
#    include <immintrin.h>
#endif

namespace n64::vr4300 {

static void ClearAllExceptions();
template<ComputeInstr1Op, std::floating_point> static void Compute(u32 fs, u32 fd);
template<ComputeInstr2Op, std::floating_point> static void Compute(u32 fs, u32 ft, u32 fd);
template<FpuNum, FpuNum> static void Convert(u32 fs, u32 fd);
template<std::floating_point Float> static Float Flush(Float f);
bool FpuUsable();
static bool IsQuietNan(f32 f);
static bool IsQuietNan(f64 f);
static void OnInvalidFormat();
template<RoundInstr, FpuNum, FpuNum> static void Round(u32 fs, u32 fd);
template<std::signed_integral Int> Int Round(f32 f);
template<std::signed_integral Int> Int Round(f64 f);
template<std::signed_integral Int> Int RoundNearest(f32 f);
template<std::signed_integral Int> Int RoundNearest(f64 f);
static bool SignalInexactOp();
static bool SignalInvalidOp();
static bool SignalUnderflow();
static bool SignalUnimplementedOp();

/// 32 bit operations in 64 bit mode:
/// - LWC1 and MTC1 leave the upper 32 bit as-is
/// - MOV.S copies the upper 32 bits over (it might be identical to MOV.D)
/// - Everything else clears the upper 32 bits to 0
///
/// - For all operations, retrieving Fs clears bit0 of the register index if !status.fr
///
/// - For Load/Store/Move instructions, if !status.fr, clear bit0 of the register index
/// - For 32-bit such instructions, if !status.fr and bit0 of the register index is set, access the upper word of the
/// register instead of the lower as usual.
template<FpuNum T> T FGR::GetFs(u32 idx) const
{
    if (!cop0.status.fr) idx &= ~1;
    if constexpr (sizeof(T) == 4) {
        return std::bit_cast<T>(u32(fpr[idx]));
    } else {
        return std::bit_cast<T>(fpr[idx]);
    }
}

template<FpuNum T> T FGR::GetFt(u32 idx) const
{
    if constexpr (sizeof(T) == 4) {
        return std::bit_cast<T>(u32(fpr[idx]));
    } else {
        return std::bit_cast<T>(fpr[idx]);
    }
}

template<FpuNum T> T FGR::GetMoveLoadStore(u32 idx) const
{
    if constexpr (sizeof(T) == 4) {
        u32 data = [this, idx] {
            if (cop0.status.fr || !(idx & 1)) return u32(fpr[idx]);
            else return u32(fpr[idx & ~1] >> 32);
        }();
        return std::bit_cast<T>(data);
    } else {
        if (!cop0.status.fr) idx &= ~1;
        return std::bit_cast<T>(fpr[idx]);
    }
}

template<FpuNum T> void FGR::Set(u32 idx, T value)
{
    u8* dst = reinterpret_cast<u8*>(&fpr[idx]);
    if constexpr (sizeof(T) == 4) {
        std::memcpy(dst, &value, 4);
        std::memset(dst + 4, 0, 4);
    } else {
        std::memcpy(dst, &value, 8);
    }
}

template<FpuNum T> void FGR::SetMoveLoadStore(u32 idx, T value)
{
    if constexpr (sizeof(T) == 4) {
        if (cop0.status.fr || !(idx & 1)) {
            std::memcpy(&fpr[idx], &value, 4);
        } else {
            std::memcpy(reinterpret_cast<u8*>(&fpr[idx & ~1]) + 4, &value, 4);
        }
    } else {
        if (!cop0.status.fr) idx &= ~1;
        fpr[idx] = std::bit_cast<s64>(value);
    }
}

bool FpuUsable()
{
    if (cop0.status.cu1) {
        ClearAllExceptions();
        return true;
    } else {
        CoprocessorUnusableException(1);
        return false;
    }
}

void ClearAllExceptions()
{
    fcr31 = std::bit_cast<FCR31>(std::bit_cast<u32>(fcr31) & 0xFFFC'0FFF);
    std::feclearexcept(FE_ALL_EXCEPT);
}

void InitCop1()
{
    std::fesetround(FE_TONEAREST); /* corresponds to fcr31.rm == 0b00 */
}

bool IsValidInput(std::floating_point auto f)
{
    switch (std::fpclassify(f)) {
    case FP_NAN: {
        if (IsQuietNan(f) ? SignalInvalidOp() : SignalUnimplementedOp()) {
            FloatingPointException();
            return false;
        } else {
            return true;
        }
    }

    case FP_SUBNORMAL:
        SignalUnimplementedOp();
        FloatingPointException();
        return false;

    default: return true;
    }
}

bool IsValidInput(std::floating_point auto f1, std::floating_point auto f2)
{
    auto cl1 = std::fpclassify(f1), cl2 = std::fpclassify(f2);
    if ((cl1 == FP_NAN && !IsQuietNan(f1)) || (cl2 == FP_NAN && !IsQuietNan(f2))) {
        if (SignalUnimplementedOp()) {
            FloatingPointException();
            return false;
        }
    }
    if (cl1 == FP_SUBNORMAL || cl2 == FP_SUBNORMAL) {
        if (SignalUnimplementedOp()) {
            FloatingPointException();
            return false;
        }
    }
    if ((cl1 == FP_NAN && IsQuietNan(f1)) || (cl2 == FP_NAN && IsQuietNan(f2))) {
        if (SignalInvalidOp()) {
            FloatingPointException();
            return false;
        }
    }
    return true;
}

template<std::floating_point Float, std::signed_integral Int> bool IsValidInputCvtRound(Float f)
{
    if (OneOf(std::fpclassify(f), FP_INFINITE, FP_NAN, FP_SUBNORMAL)) {
        SignalUnimplementedOp();
        FloatingPointException();
        return false;
    }
    bool unimpl = [f] {
        if constexpr (sizeof(Int) == 4) return f >= Float(0x1p+31) || f < Float(-0x1p+31);
        if constexpr (sizeof(Int) == 8) return f >= Float(0x1p+53) || f <= Float(-0x1p+53);
    }();
    if (unimpl) {
        SignalUnimplementedOp();
        FloatingPointException();
        return false;
    }
    return true;
}

bool IsValidOutput(std::floating_point auto& f)
{
    switch (std::fpclassify(f)) {
    case FP_NAN:
        if constexpr (sizeof(f) == 4) f = std::bit_cast<f32>(0x7FBF'FFFF);
        if constexpr (sizeof(f) == 8) f = std::bit_cast<f64>(0x7FF7'FFFF'FFFF'FFFF);
        return true;

    case FP_SUBNORMAL:
        if (!fcr31.fs || fcr31.enable_underflow || fcr31.enable_inexact) {
            SignalUnimplementedOp();
            FloatingPointException();
            return false;
        } else {
            SignalUnderflow();
            SignalInexactOp();
            f = Flush(f);
            return true;
        }

    default: return true;
    }
}

void OnInvalidFormat()
{
    if (FpuUsable()) {
        AdvancePipeline(1);
        SignalUnimplementedOp();
        FloatingPointException();
    }
}

template<std::signed_integral Int> Int Round(f32 f)
{
    __m128 t = _mm_set_ss(f);
    t = _mm_round_ss(t, t, _MM_FROUND_CUR_DIRECTION);
    return static_cast<Int>(_mm_cvtss_f32(t));
}

template<std::signed_integral Int> Int Round(f64 f)
{
    __m128d t = _mm_set_sd(f);
    t = _mm_round_sd(t, t, _MM_FROUND_CUR_DIRECTION);
    return static_cast<Int>(_mm_cvtsd_f64(t));
}

template<std::signed_integral Int> Int RoundNearest(f32 f)
{
    __m128 t = _mm_set_ss(f);
    t = _mm_round_ss(t, t, _MM_FROUND_TO_NEAREST_INT);
    return static_cast<Int>(_mm_cvtss_f32(t));
}

template<std::signed_integral Int> Int RoundNearest(f64 f)
{
    __m128d t = _mm_set_sd(f);
    t = _mm_round_sd(t, t, _MM_FROUND_TO_NEAREST_INT);
    return static_cast<Int>(_mm_cvtsd_f64(t));
}

bool SignalInexactOp()
{
    fcr31.cause_inexact = 1;
    fcr31.flag_inexact |= !fcr31.enable_inexact;
    return fcr31.enable_inexact;
}

bool SignalInvalidOp()
{
    fcr31.cause_invalid = 1;
    fcr31.flag_invalid |= !fcr31.enable_invalid;
    return fcr31.enable_invalid;
}

bool SignalUnderflow()
{
    fcr31.cause_underflow = 1;
    fcr31.flag_underflow |= !fcr31.enable_underflow;
    return fcr31.enable_underflow;
}

bool SignalUnimplementedOp()
{
    fcr31.cause_unimplemented = 1;
    return 1;
}

template<bool update_flags> bool TestExceptions()
{
    /* * The Cause bits are updated by the floating-point operations (except load, store,
       and transfer instructions).

       * A floating-point exception is generated any time a Cause bit and the
       corresponding Enable bit are set. As soon as the Cause bit enabled through the
       Floating-point operation, an exception occurs. There is no enable bit for unimplemented
       operation instruction (E). An Unimplemented exception always generates a floating-point
       exception.

       * The Flag bits are cumulative and indicate the exceptions that were raised after
       reset. Flag bits are set to 1 if an IEEE754 exception is raised but the occurrence
       of the exception is prohibited. Otherwise, they remain unchanged.
    */
    u32 fcr31_u32 = std::bit_cast<u32>(fcr31);
    u32 enables = fcr31_u32 >> 7 & 31;
    u32 causes = fcr31_u32 >> 12 & 31;
    if constexpr (update_flags) {
        u32 flags = causes & ~enables;
        fcr31_u32 |= flags << 2;
        fcr31 = std::bit_cast<FCR31>(fcr31_u32);
    }
    if ((enables & causes) || fcr31.cause_unimplemented) {
        FloatingPointException();
        return true;
    } else {
        return false;
    }
}

bool GetAndTestExceptions()
{
    int const exc = std::fetestexcept(FE_ALL_EXCEPT);
    if (!exc) return false;
    if ((exc & FE_UNDERFLOW) && (!fcr31.fs || fcr31.enable_underflow || fcr31.enable_inexact)) {
        fcr31.cause_unimplemented = 1;
        FloatingPointException();
        return true;
    }
    fcr31.cause_underflow |= (exc & FE_UNDERFLOW) != 0;
    fcr31.cause_inexact |= (exc & FE_INEXACT) != 0;
    fcr31.cause_overflow |= (exc & FE_OVERFLOW) != 0;
    fcr31.cause_div_zero |= (exc & FE_DIVBYZERO) != 0;
    fcr31.cause_invalid |= (exc & FE_INVALID) != 0;
    return TestExceptions();
}

bool GetAndTestExceptionsConvFloatToWord()
{
    int const exc = std::fetestexcept(FE_ALL_EXCEPT);
    if (!exc) return false;
    if (exc & FE_INVALID) {
        fcr31.cause_unimplemented = 1;
        FloatingPointException();
        return true;
    }
    if ((exc & FE_UNDERFLOW) && (!fcr31.fs || fcr31.enable_underflow || fcr31.enable_inexact)) {
        fcr31.cause_unimplemented = 1;
        FloatingPointException();
        return true;
    }
    fcr31.cause_underflow |= (exc & FE_UNDERFLOW) != 0;
    fcr31.cause_inexact |= (exc & FE_INEXACT) != 0;
    fcr31.cause_overflow |= (exc & FE_OVERFLOW) != 0;
    fcr31.cause_div_zero |= (exc & FE_DIVBYZERO) != 0;
    fcr31.cause_invalid |= (exc & FE_INVALID) != 0;
    return TestExceptions();
}

template<std::floating_point Float> Float Flush(Float f)
{
    switch (fcr31.rm) {
    case 0: /* FE_TONEAREST */
    case 1: /* FE_TOWARDZERO */ return std::copysign(Float(), f);
    case 2: /* FE_UPWARD */ return std::signbit(f) ? -Float() : std::numeric_limits<Float>::min();
    case 3: /* FE_DOWNWARD */ return std::signbit(f) ? -std::numeric_limits<Float>::min() : Float();
    default: std::unreachable();
    }
}

bool IsQuietNan(f32 f)
{ /* Precondition: std::isnan(f) */
    return std::bit_cast<u32>(f) >> 22 & 1;
}

bool IsQuietNan(f64 f)
{ /* Precondition: std::isnan(f) */
    return std::bit_cast<u64>(f) >> 51 & 1;
}

void bc1f(s16 imm)
{
    if (!FpuUsable()) return;
    if (!fcr31.c) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bc1fl(s16 imm)
{
    if (!FpuUsable()) return;
    if (!fcr31.c) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

void bc1t(s16 imm)
{
    if (!FpuUsable()) return;
    if (fcr31.c) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        OnBranchNotTaken();
    }
}

void bc1tl(s16 imm)
{
    if (!FpuUsable()) return;
    if (fcr31.c) {
        TakeBranch(pc + 4 + (imm << 2));
    } else {
        DiscardBranch();
    }
}

template<FpuFmt fmt> void compare(u32 fs, u32 ft, u8 cond)
{
    if constexpr (!OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        OnInvalidFormat();
    } else {
        if (!FpuUsable()) return;
        using Float = FpuFmtToType<fmt>::type;
        /* See VR4300 User's Manual by NEC, p. 566
            Ordered instructions are: LE, LT, NGE, NGL, NGLE, NGT, SEQ, SF (cond.3 set)
            Unordered: EQ, F, OLE, OLT, UEQ, ULE, ULT, UN (cond.3 clear) */
        auto IsValidInput = [cond](std::floating_point auto f) {
            if (std::isnan(f) && ((cond & 8) || IsQuietNan(f))) {
                bool signal_fpu_exc = SignalInvalidOp();
                if (signal_fpu_exc) FloatingPointException();
                return !signal_fpu_exc;
            } else return true;
        };
        Float op1 = fpr.GetFs<Float>(fs);
        Float op2 = fpr.GetFt<Float>(ft);
        if (std::isunordered(op1, op2)) {
            if (!IsValidInput(op1)) {
                return AdvancePipeline(1);
            }
            if (!IsValidInput(op2)) {
                return AdvancePipeline(1);
            }
            fcr31.c = cond & 1;
        } else {
            fcr31.c = ((cond >> 2) & (op1 < op2) | (cond >> 1) & (op1 == op2)) & 1;
        }
    }
}

void cfc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        s32 data = [fs] {
            if (fs == 31) return std::bit_cast<s32>(fcr31);
            if (fs == 0) return 0xA00;
            return 0; /* Only #0 and #31 are "valid". */
        }();
        gpr.set(rt, data);
    } else {
        CoprocessorUnusableException(1);
    }
}

void ctc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        if (fs == 31) {
            fcr31 =
              std::bit_cast<FCR31>(u32(gpr[rt]) & fcr31_write_mask | std::bit_cast<u32>(fcr31) & ~fcr31_write_mask);
            std::fesetround(guest_to_host_rounding_mode[fcr31.rm]);
            TestExceptions<false>();
        }
    } else {
        CoprocessorUnusableException(1);
    }
}

void dcfc1()
{
    if (FpuUsable()) {
        SignalUnimplementedOp();
        FloatingPointException();
        AdvancePipeline(1);
    }
}

void dctc1()
{
    dcfc1();
}

void dmfc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        gpr.set(rt, fpr.GetMoveLoadStore<s64>(fs));
        AdvancePipeline(1);
    } else {
        CoprocessorUnusableException(1);
    }
}

void dmtc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        fpr.SetMoveLoadStore(fs, gpr[rt]);
        AdvancePipeline(1);
    } else {
        CoprocessorUnusableException(1);
    }
}

void ldc1(u32 base, u32 ft, s16 imm)
{
    if (cop0.status.cu1) {
        s64 val = ReadVirtual<s64>(gpr[base] + imm);
        if (!exception_occurred) {
            fpr.SetMoveLoadStore(ft, val);
        }
        AdvancePipeline(1);
    } else {
        CoprocessorUnusableException(1);
    }
}

void lwc1(u32 base, u32 ft, s16 imm)
{
    if (cop0.status.cu1) {
        s32 val = ReadVirtual<s32>(gpr[base] + imm);
        if (!exception_occurred) {
            fpr.SetMoveLoadStore<s32>(ft, val);
        }
        AdvancePipeline(1);
    } else {
        CoprocessorUnusableException(1);
    }
}

void mfc1(u32 fs, u32 rt)
{
    if (FpuUsable()) {
        gpr.set(rt, u64(fpr.GetMoveLoadStore<s32>(fs)));
        AdvancePipeline(1);
    }
}

void mtc1(u32 fs, u32 rt)
{
    if (FpuUsable()) {
        fpr.SetMoveLoadStore(fs, s32(gpr[rt]));
        AdvancePipeline(1);
    }
}

void sdc1(u32 base, u32 ft, s16 imm)
{
    if (cop0.status.cu1) {
        WriteVirtual<8>(gpr[base] + imm, fpr.GetMoveLoadStore<s64>(ft));
        AdvancePipeline(1);
    } else {
        CoprocessorUnusableException(1);
    }
}

void swc1(u32 base, u32 ft, s16 imm)
{
    if (cop0.status.cu1) {
        WriteVirtual<4>(gpr[base] + imm, fpr.GetMoveLoadStore<s32>(ft));
        AdvancePipeline(1);
    } else {
        CoprocessorUnusableException(1);
    }
}

template<FpuFmt fmt> void ceil_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::CEIL, typename FpuFmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void ceil_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::CEIL, typename FpuFmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void cvt_d(u32 fs, u32 fd)
{
    if constexpr (fmt == FpuFmt::Invalid) {
        OnInvalidFormat();
    } else {
        Convert<typename FpuFmtToType<fmt>::type, f64>(fs, fd);
    }
}

template<FpuFmt fmt> void cvt_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Convert<typename FpuFmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void cvt_s(u32 fs, u32 fd)
{
    if constexpr (fmt == FpuFmt::Invalid) {
        OnInvalidFormat();
    } else {
        Convert<typename FpuFmtToType<fmt>::type, f32>(fs, fd);
    }
}

template<FpuFmt fmt> void cvt_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Convert<typename FpuFmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void floor_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::FLOOR, typename FpuFmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void floor_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::FLOOR, typename FpuFmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void round_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::ROUND, typename FpuFmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void round_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::ROUND, typename FpuFmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void trunc_l(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::TRUNC, typename FpuFmtToType<fmt>::type, s64>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void trunc_w(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Round<RoundInstr::TRUNC, typename FpuFmtToType<fmt>::type, s32>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void abs(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr1Op::ABS, typename FpuFmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void add(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr2Op::ADD, typename FpuFmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void div(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr2Op::DIV, typename FpuFmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void mov(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        if (cop0.status.cu1) {
            fpr.Set<f64>(fd, fpr.GetFs<f64>(fs));
        } else {
            CoprocessorUnusableException(1);
        }
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void mul(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr2Op::MUL, typename FpuFmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void neg(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr1Op::NEG, typename FpuFmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void sqrt(u32 fs, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr1Op::SQRT, typename FpuFmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<FpuFmt fmt> void sub(u32 fs, u32 ft, u32 fd)
{
    if constexpr (OneOf(fmt, FpuFmt::Float32, FpuFmt::Float64)) {
        Compute<ComputeInstr2Op::SUB, typename FpuFmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<ComputeInstr1Op instr, std::floating_point Float> void Compute(u32 fs, u32 fd)
{
    using enum ComputeInstr1Op;
    if (!FpuUsable()) return;
    Float op = fpr.GetFs<Float>(fs);
    if (!IsValidInput(op)) {
        AdvancePipeline(2);
        return;
    }
    Float result = [op] {
        if constexpr (instr == ABS) {
            return std::abs(op);
        }
        if constexpr (instr == NEG) {
            return -op;
        }
        if constexpr (instr == SQRT) {
            AdvancePipeline(sizeof(Float) == 4 ? 28 : 57);
            return std::sqrt(op);
        }
    }();
    if (!GetAndTestExceptions() && IsValidOutput(result)) {
        fpr.Set<Float>(fd, result);
    }
}

template<ComputeInstr2Op instr, std::floating_point Float> void Compute(u32 fs, u32 ft, u32 fd)
{
    if (!FpuUsable()) return;
    Float op1 = fpr.GetFs<Float>(fs);
    Float op2 = fpr.GetFt<Float>(ft);
    if (!IsValidInput(op1, op2)) {
        return AdvancePipeline(1);
    }
    Float result = [op1, op2] {
        if constexpr (instr == ComputeInstr2Op::ADD) {
            AdvancePipeline(2);
            return op1 + op2;
        }
        if constexpr (instr == ComputeInstr2Op::SUB) {
            AdvancePipeline(2);
            return op1 - op2;
        }
        if constexpr (instr == ComputeInstr2Op::MUL) {
            AdvancePipeline(sizeof(Float) == 4 ? 4 : 7);
            return op1 * op2;
        }
        if constexpr (instr == ComputeInstr2Op::DIV) {
            AdvancePipeline(sizeof(Float) == 4 ? 28 : 57);
            return op1 / op2;
        }
    }();
    if (!GetAndTestExceptions() && IsValidOutput(result)) {
        fpr.Set<Float>(fd, result);
    }
}

template<FpuNum From, FpuNum To> static void Convert(u32 fs, u32 fd)
{
    if (!FpuUsable()) return;

    static constexpr int cycles = [] {
        if constexpr (std::same_as<From, To>) return 0;
        else if constexpr (std::same_as<From, f32> && std::same_as<To, f64>) return 0;
        else if constexpr (std::same_as<From, f64> && std::same_as<To, f32>) return 1;
        else if constexpr (std::integral<From> && std::integral<To>) return 1;
        else return 4;
    }();
    if constexpr (cycles > 0) {
        AdvancePipeline(cycles);
    }
    if constexpr (std::same_as<From, To> || std::integral<From> && std::integral<To>) {
        SignalUnimplementedOp();
        FloatingPointException();
        return;
    }

    From source = fpr.GetFs<From>(fs);
    if constexpr (std::floating_point<From> && std::floating_point<To>) {
        if (!IsValidInput(source)) return;
    }
    if constexpr (std::floating_point<From> && std::integral<To>) {
        if (!IsValidInputCvtRound<From, To>(source)) return;
    }
    if constexpr (std::same_as<From, s64> && std::floating_point<To>) {
        if (source >= 0x0080'0000'0000'0000_s64 || source < 0xFF80'0000'0000'0000_s64) {
            SignalUnimplementedOp();
            FloatingPointException();
            return;
        }
    }

    To result = [source] {
        if constexpr (std::integral<To> && std::floating_point<From>) return Round<To>(source);
        else return static_cast<To>(source);
    }();

    if constexpr (std::same_as<To, s32>) {
        if (GetAndTestExceptionsConvFloatToWord()) return;
    } else {
        if (GetAndTestExceptions()) return;
    }
    if constexpr (std::floating_point<To>) {
        if (!IsValidOutput(result)) return;
    }
    fpr.Set<To>(fd, result);
}

template<RoundInstr instr, FpuNum From, FpuNum To> void Round(u32 fs, u32 fd)
{
    if (!FpuUsable()) return;
    AdvancePipeline(4);
    From source = fpr.GetFs<From>(fs);
    if (!IsValidInputCvtRound<From, To>(source)) return;
    To result = To([source] {
        if constexpr (instr == RoundInstr::ROUND) return RoundNearest<To>(source);
        if constexpr (instr == RoundInstr::TRUNC) return std::trunc(source);
        if constexpr (instr == RoundInstr::CEIL) return std::ceil(source);
        if constexpr (instr == RoundInstr::FLOOR) return std::floor(source);
    }());
    if constexpr (std::same_as<To, s32>) {
        if (GetAndTestExceptionsConvFloatToWord()) return;
    } else {
        if (GetAndTestExceptions()) return;
    }
    if (source != From(result) && SignalInexactOp()) {
        FloatingPointException();
        return;
    }
    fpr.Set<To>(fd, result);
}

#define INST_FMT_SPEC(instr, ...)                      \
    template void instr<FpuFmt::Float32>(__VA_ARGS__); \
    template void instr<FpuFmt::Float64>(__VA_ARGS__); \
    template void instr<FpuFmt::Int32>(__VA_ARGS__);   \
    template void instr<FpuFmt::Int64>(__VA_ARGS__);   \
    template void instr<FpuFmt::Invalid>(__VA_ARGS__);

INST_FMT_SPEC(compare, u32, u32, u8);
INST_FMT_SPEC(ceil_l, u32, u32);
INST_FMT_SPEC(ceil_w, u32, u32);
INST_FMT_SPEC(cvt_d, u32, u32);
INST_FMT_SPEC(cvt_l, u32, u32);
INST_FMT_SPEC(cvt_s, u32, u32);
INST_FMT_SPEC(cvt_w, u32, u32);
INST_FMT_SPEC(floor_l, u32, u32);
INST_FMT_SPEC(floor_w, u32, u32);
INST_FMT_SPEC(round_l, u32, u32);
INST_FMT_SPEC(round_w, u32, u32);
INST_FMT_SPEC(trunc_l, u32, u32);
INST_FMT_SPEC(trunc_w, u32, u32);
INST_FMT_SPEC(abs, u32, u32);
INST_FMT_SPEC(add, u32, u32, u32);
INST_FMT_SPEC(div, u32, u32, u32);
INST_FMT_SPEC(mov, u32, u32);
INST_FMT_SPEC(mul, u32, u32, u32);
INST_FMT_SPEC(neg, u32, u32);
INST_FMT_SPEC(sqrt, u32, u32);
INST_FMT_SPEC(sub, u32, u32, u32);

template bool IsValidInput<f32>(f32);
template bool IsValidInput<f64>(f64);
template bool IsValidInput<f32, f32>(f32, f32);
template bool IsValidInput<f64, f64>(f64, f64);
template bool IsValidInputCvtRound<f32, s32>(f32);
template bool IsValidInputCvtRound<f64, s32>(f64);
template bool IsValidInputCvtRound<f32, s64>(f32);
template bool IsValidInputCvtRound<f64, s64>(f64);
template bool IsValidOutput<f32>(f32&);
template bool IsValidOutput<f64>(f64&);
template bool TestExceptions<false>();
template bool TestExceptions<true>();

} // namespace n64::vr4300

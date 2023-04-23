#pragma fenv_access(on)

#include "cop1.hpp"
#include "cop0.hpp"
#include "exceptions.hpp"
#include "mmu.hpp"
#include "util.hpp"
#include "vr4300.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <concepts>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace n64::vr4300 {

template<typename T>
concept FpuNum = std::same_as<f32, T> || std::same_as<f64, T> || std::same_as<s32, T> || std::same_as<s64, T>;

template<Fmt> struct FmtToType {};

template<> struct FmtToType<Fmt::Float32> {
    using type = f32;
};

template<> struct FmtToType<Fmt::Float64> {
    using type = f64;
};

template<> struct FmtToType<Fmt::Int32> {
    using type = s32;
};

template<> struct FmtToType<Fmt::Int64> {
    using type = s64;
};

enum class ComputeInstr1Op {
    ABS,
    NEG,
    SQRT,
};

enum class ComputeInstr2Op {
    ADD,
    SUB,
    MUL,
    DIV,
};

enum class RoundInstr {
    CEIL,
    FLOOR,
    ROUND,
    TRUNC,
};

enum class FpuException {
    InexactOp,
    Underflow,
    Overflow,
    DivByZero,
    InvalidOp,
    UnimplementedOp
};

static void ClearAllExceptions();
template<ComputeInstr1Op, std::floating_point> static void Compute(u32 fs, u32 fd);
template<ComputeInstr2Op, std::floating_point> static void Compute(u32 fs, u32 ft, u32 fd);
template<FpuNum, FpuNum> static void Convert(u32 fs, u32 fd);
template<std::floating_point Float> static Float Flush(Float f);
constexpr char FmtToChar(u32 fmt);
bool FpuUsable();
static bool GetAndTestExceptions();
static bool GetAndTestExceptionsConvFloatToWord();
static bool IsQuietNan(f32 f);
static bool IsQuietNan(f64 f);
static bool IsValidInput(std::floating_point auto f);
template<std::signed_integral Int> static bool IsValidInputCvtRound(std::floating_point auto f);
static bool IsValidOutput(std::floating_point auto& f);
static void OnInvalidFormat();
template<RoundInstr, FpuNum, FpuNum> static void Round(u32 fs, u32 fd);
static bool SignalDivZero();
static bool SignalInexactOp();
static bool SignalInvalidOp();
static bool SignalOverflow();
static bool SignalUnderflow();
static bool SignalUnimplementedOp();
template<bool update_flags = true> static bool TestExceptions();

/* Floating point control register #31 */
struct FCR31 {
    u32 rm : 2; /* Rounding mode */

    u32 flag_inexact   : 1;
    u32 flag_underflow : 1;
    u32 flag_overflow  : 1;
    u32 flag_div_zero  : 1;
    u32 flag_invalid   : 1;

    u32 enable_inexact   : 1;
    u32 enable_underflow : 1;
    u32 enable_overflow  : 1;
    u32 enable_div_zero  : 1;
    u32 enable_invalid   : 1;

    u32 cause_inexact       : 1;
    u32 cause_underflow     : 1;
    u32 cause_overflow      : 1;
    u32 cause_div_zero      : 1;
    u32 cause_invalid       : 1;
    u32 cause_unimplemented : 1;

    u32    : 5;
    u32 c  : 1; /* Condition bit; set/cleared by the Compare instruction (or CTC1). */
    u32 fs : 1; /* Flush subnormals: if set, and underflow and invalid exceptions are disabled,
            an fp operation resulting in a denormalized number does not cause the unimplemented operation to trigger. */
    u32    : 7;
} static fcr31;

/* Floating point control registers. Only #0 and #31 are "valid", and #0 is read-only. */
struct FPUControl {
    u32 Get(u32 idx) const;
    void Set(u32 idx, u32 value);
} static fpu_control;

/* General-purpose floating point registers. */
struct FGR {
    template<FpuNum T> T GetFs(u32 idx) const;
    template<FpuNum T> T GetFt(u32 idx) const;
    template<FpuNum T> T GetMoveLoadStore(u32 idx) const;
    template<FpuNum T> void Set(u32 idx, T data);
    template<FpuNum T> void SetMoveLoadStore(u32 idx, T data);

private:
    std::array<s64, 32> fpr;
} static fpr;

[[maybe_unused]] constexpr std::array compare_cond_strings = {
    "F",
    "UN",
    "EQ",
    "UEQ",
    "OLT",
    "ULT",
    "OLE",
    "ULE",
    "SF",
    "NGLE",
    "SEQ",
    "NGL",
    "LT",
    "NGE",
    "LE",
    "NGT",
};

u32 FPUControl::Get(u32 idx) const
{
    if (idx == 31) return std::bit_cast<u32>(fcr31);
    if (idx == 0) return 0xA00;
    return 0; /* Only #0 and #31 are "valid". */
}

void FPUControl::Set(u32 idx, u32 data)
{
    if (idx != 31) return;
    static constexpr u32 mask = 0x183'FFFF;
    fcr31 = std::bit_cast<FCR31>(data & mask | std::bit_cast<u32>(fcr31) & ~mask);
    auto new_rounding_mode = [&] {
        switch (fcr31.rm) {
        case 0: return FE_TONEAREST; /* RN */
        case 1: return FE_TOWARDZERO; /* RZ */
        case 2: return FE_UPWARD; /* RP */
        case 3: return FE_DOWNWARD; /* RM */
        default: std::unreachable();
        }
    }();
    std::fesetround(new_rounding_mode);
    TestExceptions<false>();
}

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

constexpr char FmtToChar(Fmt fmt)
{
    switch (fmt) {
    case Fmt::Float32: return 'S';
    case Fmt::Float64: return 'D';
    case Fmt::Int32: return 'W';
    case Fmt::Int64: return 'L';
    case Fmt::Invalid: return '?';
    }
}

bool FpuUsable()
{
    if (cop0.status.cu1) {
        ClearAllExceptions();
        return true;
    } else {
        SignalCoprocessorUnusableException(1);
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
        bool signal_fpu_exc = IsQuietNan(f) ? SignalInvalidOp() : SignalUnimplementedOp();
        if (signal_fpu_exc) {
            SignalException<Exception::FloatingPoint>();
            return false;
        } else {
            return true;
        }
    }

    case FP_SUBNORMAL:
        SignalUnimplementedOp();
        SignalException<Exception::FloatingPoint>();
        return false;

    default: return true;
    }
}

template<std::signed_integral Int> bool IsValidInputCvtRound(std::floating_point auto f)
{
    auto classify = std::fpclassify(f);
    if (classify == FP_INFINITE || classify == FP_NAN || classify == FP_SUBNORMAL) {
        SignalUnimplementedOp();
        SignalException<Exception::FloatingPoint>();
        return false;
    }
    if constexpr (sizeof(Int) == 4) {
        if (f >= 0x1p+31 || f < -0x1p+31) {
            SignalUnimplementedOp();
            SignalException<Exception::FloatingPoint>();
            return false;
        }
        return true;
    } else {
        if (f >= 0x1p+53 || f < -0x1p+53) {
            SignalUnimplementedOp();
            SignalException<Exception::FloatingPoint>();
            return false;
        }
        return true;
    }
}

bool IsValidOutput(std::floating_point auto& f)
{
    switch (std::fpclassify(f)) {
    case FP_NAN:
        if constexpr (sizeof(f) == 4) f = std::bit_cast<f32>(0x7FBF'FFFF);
        else f = std::bit_cast<f64>(0x7FF7'FFFF'FFFF'FFFF);
        return true;

    case FP_SUBNORMAL:
        if (!fcr31.fs || fcr31.enable_underflow || fcr31.enable_inexact) {
            SignalUnimplementedOp();
            SignalException<Exception::FloatingPoint>();
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
    if (!FpuUsable()) return;
    AdvancePipeline(1);
    SignalUnimplementedOp();
    SignalException<Exception::FloatingPoint>();
}

bool SignalDivZero()
{ /* return true if floatingpoint exception should be raised */
    fcr31.cause_div_zero = 1;
    fcr31.flag_div_zero |= !fcr31.enable_div_zero;
    return fcr31.enable_div_zero;
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

bool SignalOverflow()
{
    fcr31.cause_overflow = 1;
    fcr31.flag_overflow |= !fcr31.enable_overflow;
    return fcr31.enable_overflow;
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
        SignalException<Exception::FloatingPoint>();
        return true;
    } else {
        return false;
    }
}

bool GetAndTestExceptions()
{
    // TODO: store the flags as they come from fetestexcept, until they are read by the program?
    bool underflow = std::fetestexcept(FE_UNDERFLOW);
    if (underflow && (!fcr31.fs || fcr31.enable_underflow || fcr31.enable_inexact)) {
        fcr31.cause_unimplemented = 1;
        SignalException<Exception::FloatingPoint>();
        return true;
    }

    fcr31.cause_underflow |= underflow;
    fcr31.cause_inexact |= std::fetestexcept(FE_INEXACT) != 0;
    fcr31.cause_overflow |= std::fetestexcept(FE_OVERFLOW) != 0;
    fcr31.cause_div_zero |= std::fetestexcept(FE_DIVBYZERO) != 0;
    fcr31.cause_invalid |= std::fetestexcept(FE_INVALID) != 0;

    return TestExceptions();
}

bool GetAndTestExceptionsConvFloatToWord()
{
    bool invalid = std::fetestexcept(FE_INVALID);
    if (invalid) {
        fcr31.cause_unimplemented = 1;
        SignalException<Exception::FloatingPoint>();
        return true;
    }
    bool underflow = std::fetestexcept(FE_UNDERFLOW);
    if (underflow && (!fcr31.fs || fcr31.enable_underflow || fcr31.enable_inexact)) {
        fcr31.cause_unimplemented = 1;
        SignalException<Exception::FloatingPoint>();
        return true;
    }
    fcr31.cause_invalid |= invalid;
    fcr31.cause_underflow |= underflow;
    fcr31.cause_inexact |= std::fetestexcept(FE_INEXACT) != 0;
    fcr31.cause_overflow |= std::fetestexcept(FE_OVERFLOW) != 0;
    fcr31.cause_div_zero |= std::fetestexcept(FE_DIVBYZERO) != 0;

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
    s64 offset = s64(imm) << 2;
    if (!fcr31.c) {
        Jump(pc + offset);
    }
}

void bc1fl(s16 imm)
{
    if (!FpuUsable()) return;
    s64 offset = s64(imm) << 2;
    if (!fcr31.c) {
        Jump(pc + offset);
    } else {
        pc += 4;
    }
}

void bc1t(s16 imm)
{
    if (!FpuUsable()) return;
    s64 offset = s64(imm) << 2;
    if (fcr31.c) {
        Jump(pc + offset);
    }
}

void bc1tl(s16 imm)
{
    if (!FpuUsable()) return;
    s64 offset = s64(imm) << 2;
    if (fcr31.c) {
        Jump(pc + offset);
    } else {
        pc += 4;
    }
}

template<Fmt fmt> void c(u32 fs, u32 ft, u8 cond)
{
    if constexpr (!one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        OnInvalidFormat();
    } else {
        if (!FpuUsable()) return;
        using Float = FmtToType<fmt>::type;
        /* See VR4300 User's Manual by NEC, p. 566
            Ordered instructions are: LE, LT, NGE, NGL, NGLE, NGT, SEQ, SF (cond.3 set)
            Unordered: EQ, F, OLE, OLT, UEQ, ULE, ULT, UN (cond.3 clear) */
        auto IsValidInput = [cond](std::floating_point auto f) {
            if (std::isnan(f) && ((cond & 8) || IsQuietNan(f))) {
                bool signal_fpu_exc = SignalInvalidOp();
                if (signal_fpu_exc) SignalException<Exception::FloatingPoint>();
                return !signal_fpu_exc;
            } else return true;
        };
        Float op1 = fpr.GetFs<Float>(fs);
        Float op2 = fpr.GetFt<Float>(ft);
        if (std::isnan(op1) || std::isnan(op2)) {
            if (!IsValidInput(op1)) {
                AdvancePipeline(1);
                return;
            }
            if (!IsValidInput(op2)) {
                AdvancePipeline(1);
                return;
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
        gpr.set(rt, s32(fpu_control.Get(fs)));
    } else {
        SignalCoprocessorUnusableException(1);
    }
}

void ctc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        fpu_control.Set(fs, u32(gpr[rt]));
    } else {
        SignalCoprocessorUnusableException(1);
    }
}

void dcfc1()
{
    if (FpuUsable()) {
        SignalUnimplementedOp();
        SignalException<Exception::FloatingPoint>();
        AdvancePipeline(1);
    }
}

void dctc1()
{
    if (FpuUsable()) {
        SignalUnimplementedOp();
        SignalException<Exception::FloatingPoint>();
        AdvancePipeline(1);
    }
}

void dmfc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        gpr.set(rt, fpr.GetMoveLoadStore<s64>(fs));
        AdvancePipeline(1);
    } else {
        SignalCoprocessorUnusableException(1);
    }
}

void dmtc1(u32 fs, u32 rt)
{
    if (cop0.status.cu1) {
        fpr.SetMoveLoadStore(fs, s64(gpr[rt]));
        AdvancePipeline(1);
    } else {
        SignalCoprocessorUnusableException(1);
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
        SignalCoprocessorUnusableException(1);
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
        SignalCoprocessorUnusableException(1);
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
        SignalCoprocessorUnusableException(1);
    }
}

void swc1(u32 base, u32 ft, s16 imm)
{
    if (cop0.status.cu1) {
        WriteVirtual<4>(gpr[base] + imm, fpr.GetMoveLoadStore<s32>(ft));
        AdvancePipeline(1);
    } else {
        SignalCoprocessorUnusableException(1);
    }
}

template<Fmt fmt> void ceil_l(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::CEIL, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void ceil_w(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::CEIL, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void cvt_d(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else {
        Convert<typename FmtToType<fmt>::type, f64>(fs, fd);
    }
}

template<Fmt fmt> void cvt_l(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else {
        Convert<typename FmtToType<fmt>::type, s64>(fs, fd);
    }
}

template<Fmt fmt> void cvt_s(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else {
        Convert<typename FmtToType<fmt>::type, f32>(fs, fd);
    }
}

template<Fmt fmt> void cvt_w(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else {
        Convert<typename FmtToType<fmt>::type, s32>(fs, fd);
    }
}

template<Fmt fmt> void floor_l(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::FLOOR, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void floor_w(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::FLOOR, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void round_l(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::ROUND, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void round_w(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::ROUND, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void trunc_l(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::TRUNC, typename FmtToType<fmt>::type, s64>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void trunc_w(u32 fs, u32 fd)
{
    if constexpr (fmt == Fmt::Invalid) {
        OnInvalidFormat();
    } else if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Round<RoundInstr::TRUNC, typename FmtToType<fmt>::type, s32>(fs, fd);
    } else {
        // Result is undefined
    }
}

template<Fmt fmt> void abs(u32 fs, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr1Op::ABS, typename FmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void add(u32 fs, u32 ft, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::ADD, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void div(u32 fs, u32 ft, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::DIV, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void mov(u32 fs, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        if (cop0.status.cu1) {
            fpr.Set<f64>(fd, fpr.GetFs<f64>(fs));
        } else {
            SignalCoprocessorUnusableException(1);
        }
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void mul(u32 fs, u32 ft, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::MUL, typename FmtToType<fmt>::type>(fs, ft, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void neg(u32 fs, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr1Op::NEG, typename FmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void sqrt(u32 fs, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr1Op::SQRT, typename FmtToType<fmt>::type>(fs, fd);
    } else {
        OnInvalidFormat();
    }
}

template<Fmt fmt> void sub(u32 fs, u32 ft, u32 fd)
{
    if constexpr (one_of(fmt, Fmt::Float32, Fmt::Float64)) {
        Compute<ComputeInstr2Op::SUB, typename FmtToType<fmt>::type>(fs, ft, fd);
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
    using enum ComputeInstr2Op;
    if (!FpuUsable()) return;
    Float op1 = fpr.GetFs<Float>(fs);
    if (!IsValidInput(op1)) {
        AdvancePipeline(1);
        return;
    }
    Float op2 = fpr.GetFt<Float>(ft);
    if (!IsValidInput(op2)) {
        AdvancePipeline(1);
        return;
    }
    Float result = [op1, op2] {
        if constexpr (instr == ADD) {
            AdvancePipeline(2);
            return op1 + op2;
        }
        if constexpr (instr == SUB) {
            AdvancePipeline(2);
            return op1 - op2;
        }
        if constexpr (instr == MUL) {
            AdvancePipeline(sizeof(Float) == 4 ? 4 : 7);
            return op1 * op2;
        }
        if constexpr (instr == DIV) {
            AdvancePipeline(sizeof(Float) == 4 ? 28 : 57);
            return op1 / op2;
        }
    }();
    if (!GetAndTestExceptions() && IsValidOutput(result)) {
        fpr.Set<Float>(fd, result);
    }
}

template<RoundInstr instr, FpuNum From, FpuNum To> void Round(u32 fs, u32 fd)
{
    using enum RoundInstr;
    if (!FpuUsable()) return;
    AdvancePipeline(4);
    From source = fpr.GetFs<From>(fs);
    if (!IsValidInputCvtRound<To>(source)) return;
    To result = To([source] {
        if constexpr (instr == ROUND) return std::nearbyint(source);
        if constexpr (instr == TRUNC) return std::trunc(source);
        if constexpr (instr == CEIL) return std::ceil(source);
        if constexpr (instr == FLOOR) return std::floor(source);
    }());
    if constexpr (std::same_as<To, s32>) {
        if (GetAndTestExceptionsConvFloatToWord()) return;
    } else {
        if (GetAndTestExceptions()) return;
    }
    if constexpr (instr == ROUND || instr == TRUNC) {
        if (fs != fd && SignalInexactOp()) {
            SignalException<Exception::FloatingPoint>();
            return;
        }
    }
    fpr.Set<To>(fd, result);
}

template<FpuNum From, FpuNum To> static void Convert(u32 fs, u32 fd)
{
    if (!FpuUsable()) return;

    static constexpr int cycles = [&] {
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
        SignalException<Exception::FloatingPoint>();
        return;
    }

    From source = fpr.GetFs<From>(fs);
    if constexpr (std::floating_point<From> && std::floating_point<To>) {
        if (!IsValidInput(source)) return;
    }
    if constexpr (std::floating_point<From> && std::integral<To>) {
        if (!IsValidInputCvtRound<To>(source)) return;
    }
    if constexpr (std::same_as<From, s64> && std::floating_point<To>) {
        if (source >= s64(0x0080'0000'0000'0000) || source < s64(0xFF80'0000'0000'0000)) {
            SignalUnimplementedOp();
            SignalException<Exception::FloatingPoint>();
            return;
        }
    }

    To result = To(source);
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

#define INST_FMT_SPEC(instr, ...)                   \
    template void instr<Fmt::Float32>(__VA_ARGS__); \
    template void instr<Fmt::Float64>(__VA_ARGS__); \
    template void instr<Fmt::Int32>(__VA_ARGS__);   \
    template void instr<Fmt::Int64>(__VA_ARGS__);   \
    template void instr<Fmt::Invalid>(__VA_ARGS__);

INST_FMT_SPEC(c, u32, u32, u8);
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

} // namespace n64::vr4300

#pragma once

#include "types.hpp"

#include <array>
#include <cfenv>
#include <concepts>

namespace n64::vr4300 {

enum class Fmt {
    Float32 = 16,
    Float64 = 17,
    Int32 = 20,
    Int64 = 21,
    Invalid,
};

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

/* General-purpose floating point registers. */
struct FGR {
    template<FpuNum T> T GetFs(u32 idx) const;
    template<FpuNum T> T GetFt(u32 idx) const;
    template<FpuNum T> T GetMoveLoadStore(u32 idx) const;
    template<FpuNum T> void Set(u32 idx, T data);
    template<FpuNum T> void SetMoveLoadStore(u32 idx, T data);

private:
    std::array<s64, 32> fpr;
} inline fpr;

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
} inline fcr31;

struct FCR31BitIndex {
    enum {
        FlagInexact = 2,
        FlagUnderflow,
        FlagOverflow,
        FlagDivZero,
        FlagInvalid,

        EnableInexact,
        EnableUnderflow,
        EnableOverflow,
        EnableDivZero,
        EnableInvalid,

        CauseInexact,
        CauseUnderflow,
        CauseOverflow,
        CauseDivZero,
        CauseInvalid,
        CauseUnimplemented,
    };
};

constexpr std::array guest_to_host_rounding_mode{ FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD, FE_DOWNWARD };

constexpr u32 fcr31_write_mask = 0x183'FFFF;

void bc1f(s16 imm);
void bc1fl(s16 imm);
void bc1t(s16 imm);
void bc1tl(s16 imm);

void cfc1(u32 fs, u32 rt);
void ctc1(u32 fs, u32 rt);
void dcfc1();
void dctc1();
void dmfc1(u32 fs, u32 rt);
void dmtc1(u32 fs, u32 rt);
void ldc1(u32 base, u32 ft, s16 imm);
void lwc1(u32 base, u32 ft, s16 imm);
void mfc1(u32 fs, u32 rt);
void mtc1(u32 fs, u32 rt);
void sdc1(u32 base, u32 ft, s16 imm);
void swc1(u32 base, u32 ft, s16 imm);

template<Fmt> void compare(u32 fs, u32 ft, u8 cond);

template<Fmt> void ceil_l(u32 fs, u32 fd);
template<Fmt> void ceil_w(u32 fs, u32 fd);
template<Fmt> void cvt_d(u32 fs, u32 fd);
template<Fmt> void cvt_l(u32 fs, u32 fd);
template<Fmt> void cvt_s(u32 fs, u32 fd);
template<Fmt> void cvt_w(u32 fs, u32 fd);
template<Fmt> void floor_l(u32 fs, u32 fd);
template<Fmt> void floor_w(u32 fs, u32 fd);
template<Fmt> void round_l(u32 fs, u32 fd);
template<Fmt> void round_w(u32 fs, u32 fd);
template<Fmt> void trunc_l(u32 fs, u32 fd);
template<Fmt> void trunc_w(u32 fs, u32 fd);

template<Fmt> void abs(u32 fs, u32 fd);
template<Fmt> void add(u32 fs, u32 ft, u32 fd);
template<Fmt> void div(u32 fs, u32 ft, u32 fd);
template<Fmt> void mov(u32 fs, u32 fd);
template<Fmt> void mul(u32 fs, u32 ft, u32 fd);
template<Fmt> void neg(u32 fs, u32 fd);
template<Fmt> void sqrt(u32 fs, u32 fd);
template<Fmt> void sub(u32 fs, u32 ft, u32 fd);

void InitCop1();

} // namespace n64::vr4300

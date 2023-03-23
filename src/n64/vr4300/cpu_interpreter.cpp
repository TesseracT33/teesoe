#include "cpu_interpreter.hpp"
#include "cop0.hpp"
#include "host.hpp"
#include "memory/memory.hpp"
#include "mmu.hpp"

#include <array>
#include <limits>

namespace n64::vr4300 {

// TODO: compute these at runtime instead, should be faster
constexpr std::array right_load_mask = {
    0xFFFF'FFFF'FFFF'FF00ull,
    0xFFFF'FFFF'FFFF'0000ull,
    0xFFFF'FFFF'FF00'0000ull,
    0xFFFF'FFFF'0000'0000ull,
    0xFFFF'FF00'0000'0000ull,
    0xFFFF'0000'0000'0000ull,
    0xFF00'0000'0000'0000ull,
    0ull,
};

void Interpreter::break_() const
{
    SignalException<Exception::Breakpoint>();
}

void Interpreter::ddiv(u32 rs, u32 rt) const
{
    s64 op1 = gpr[rs];
    s64 op2 = gpr[rt];
    if (op2 == 0) { /* Peter Lemon N64 CPUTest>CPU>DDIV */
        lo_reg = op1 >= 0 ? -1 : 1;
        hi_reg = op1;
    } else if (op1 == std::numeric_limits<s64>::min() && op2 == -1) {
        lo_reg = op1;
        hi_reg = 0;
    } else {
        lo_reg = op1 / op2;
        hi_reg = op1 % op2;
    }
}

void Interpreter::ddivu(u32 rs, u32 rt) const
{
    u64 op1 = u64(gpr[rs]);
    u64 op2 = u64(gpr[rt]);
    if (op2 == 0) {
        lo_reg = -1;
        hi_reg = op1;
    } else {
        lo_reg = op1 / op2;
        hi_reg = op1 % op2;
    }
}

void Interpreter::dmult(u32 rs, u32 rt) const
{
#if INT128_AVAILABLE
    s128 prod = s128(gpr[rs]) * s128(gpr[rt]);
    lo_reg = prod & s64(-1);
    hi_reg = prod >> 64;
#elif defined _MSC_VER
    s64 hi;
    s64 lo = _mul128(gpr[rs], gpr[rt], &hi);
    lo_reg = lo;
    hi_reg = hi;
#else
#error DMULT unimplemented on targets where INT128 or MSVC _mul128 is unavailable
#endif
}

void Interpreter::dmultu(u32 rs, u32 rt) const
{
#if INT128_AVAILABLE
    u128 prod = u128(gpr[rs]) * u128(gpr[rt]);
    lo_reg = prod & u64(-1);
    hi_reg = prod >> 64;
#elif defined _MSC_VER
    u64 hi;
    u64 lo = _umul128(gpr[rs], gpr[rt], &hi);
    lo_reg = lo;
    hi_reg = hi;
#else
#error DMULTU unimplemented on targets where UINT128 or MSVC _umul128 is unavailable
#endif
}

void Interpreter::lb(u32 rs, u32 rt, s16 imm) const
{
    s8 val = ReadVirtual<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lbu(u32 rs, u32 rt, s16 imm) const
{
    u8 val = ReadVirtual<s8>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::ld(u32 rs, u32 rt, s16 imm) const
{
    s64 val = ReadVirtual<s64>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::ldl(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    s64 val = ReadVirtual<s64, Alignment::UnalignedLeft>(addr);
    if (!exception_occurred) {
        u32 bits_from_last_boundary = (addr & 7) << 3;
        val <<= bits_from_last_boundary;
        s64 untouched_gpr = gpr[rt] & ((1ll << bits_from_last_boundary) - 1);
        gpr.set(rt, val | untouched_gpr);
    }
}

void Interpreter::ldr(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    s64 val = ReadVirtual<s64, Alignment::UnalignedRight>(addr);
    if (!exception_occurred) {
        u32 bytes_from_last_boundary = addr & 7;
        val >>= 8 * (7 - bytes_from_last_boundary);
        s64 untouched_gpr = gpr[rt] & right_load_mask[bytes_from_last_boundary];
        gpr.set(rt, val | untouched_gpr);
    }
}

void Interpreter::lh(u32 rs, u32 rt, s16 imm) const
{
    s16 val = ReadVirtual<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lhu(u32 rs, u32 rt, s16 imm) const
{
    u16 val = ReadVirtual<s16>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::ll(u32 rs, u32 rt, s16 imm) const
{
    s32 val = ReadVirtual<s32>(gpr[rs] + imm);
    cop0.ll_addr = last_physical_address_on_load >> 4;
    ll_bit = 1;
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lld(u32 rs, u32 rt, s16 imm) const
{
    s64 val = ReadVirtual<s64>(gpr[rs] + imm);
    cop0.ll_addr = last_physical_address_on_load >> 4;
    ll_bit = 1;
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lw(u32 rs, u32 rt, s16 imm) const
{
    s32 val = ReadVirtual<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::lwl(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    s32 val = ReadVirtual<s32, Alignment::UnalignedLeft>(addr);
    if (!exception_occurred) {
        u32 bits_from_last_boundary = (addr & 3) << 3;
        val <<= bits_from_last_boundary;
        s32 untouched_gpr = s32(gpr[rt] & ((1 << bits_from_last_boundary) - 1));
        gpr.set(rt, val | untouched_gpr);
    }
}

void Interpreter::lwr(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    s32 val = ReadVirtual<s32, Alignment::UnalignedRight>(addr);
    if (!exception_occurred) {
        u32 bytes_from_last_boundary = addr & 3;
        val >>= 8 * (3 - bytes_from_last_boundary);
        s32 untouched_gpr = s32(gpr[rt] & right_load_mask[bytes_from_last_boundary]);
        gpr.set(rt, val | untouched_gpr);
    }
}

void Interpreter::lwu(u32 rs, u32 rt, s16 imm) const
{
    u32 val = ReadVirtual<s32>(gpr[rs] + imm);
    if (!exception_occurred) {
        gpr.set(rt, val);
    }
}

void Interpreter::sb(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<1>(gpr[rs] + imm, s8(gpr[rt]));
}

void Interpreter::sc(u32 rs, u32 rt, s16 imm) const
{
    if (ll_bit) {
        WriteVirtual<4>(gpr[rs] + imm, s32(gpr[rt]));
    }
    gpr.set(rt, ll_bit);
}

void Interpreter::scd(u32 rs, u32 rt, s16 imm) const
{
    if (ll_bit) {
        WriteVirtual<8>(gpr[rs] + imm, gpr[rt]);
    }
    gpr.set(rt, ll_bit);
}

void Interpreter::sd(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<8>(gpr[rs] + imm, gpr[rt]);
}

void Interpreter::sdl(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<8, Alignment::UnalignedLeft>(addr, gpr[rt] >> (8 * (addr & 7)));
}

void Interpreter::sdr(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<8, Alignment::UnalignedRight>(addr, gpr[rt] << (8 * (7 - (addr & 7))));
}

void Interpreter::sh(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<2>(gpr[rs] + imm, s16(gpr[rt]));
}

void Interpreter::sync() const
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void Interpreter::syscall() const
{
    SignalException<Exception::Syscall>();
}

void Interpreter::sw(u32 rs, u32 rt, s16 imm) const
{
    WriteVirtual<4>(gpr[rs] + imm, s32(gpr[rt]));
}

void Interpreter::swl(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<4, Alignment::UnalignedLeft>(addr, u32(gpr[rt]) >> (8 * (addr & 3)));
}

void Interpreter::swr(u32 rs, u32 rt, s16 imm) const
{
    s64 addr = gpr[rs] + imm;
    WriteVirtual<4, Alignment::UnalignedRight>(addr, gpr[rt] << (8 * (3 - (addr & 3))));
}

} // namespace n64::vr4300

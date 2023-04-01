#include "cpu_recompiler.hpp"
#include "cop0.hpp"
#include "host.hpp"
#include "memory/memory.hpp"
#include "mmu.hpp"

#include <array>

namespace n64::vr4300 {

using namespace asmjit;
using namespace asmjit::x86;

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

void Recompiler::break_() const
{
    c.call(SignalException<Exception::Breakpoint>);
    jit.branched = 1;
}

void Recompiler::ddiv(u32 rs, u32 rt) const
{
    // TODO
}

void Recompiler::ddivu(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    c.mov(rax, gpr_ptr(rs));
    c.mov(rcx, gpr_ptr(rt));
    c.test(rcx, rcx);
    c.jne(l_div);
    c.mov(lo_mem(), -1);
    c.mov(hi_mem(), rax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.div(rax, rcx);
    c.mov(lo_mem(), rax);
    c.mov(hi_mem(), rdx);
    c.bind(l_end);
}

void Recompiler::dmult(u32 rs, u32 rt) const
{
    multiply64<false>(rs, rt);
}

void Recompiler::dmultu(u32 rs, u32 rt) const
{
    multiply64<true>(rt, rt);
}

void Recompiler::lb(u32 rs, u32 rt, s16 imm) const
{
    load<s8>(rs, rt, imm);
}

void Recompiler::lbu(u32 rs, u32 rt, s16 imm) const
{
    load<u8>(rs, rt, imm);
}

void Recompiler::ld(u32 rs, u32 rt, s16 imm) const
{
    load<s64>(rs, rt, imm);
}

void Recompiler::ldl(u32 rs, u32 rt, s16 imm) const
{
    load_left<s64>(rs, rt, imm);
}

void Recompiler::ldr(u32 rs, u32 rt, s16 imm) const
{
    load_right<s64>(rs, rt, imm);
}

void Recompiler::lh(u32 rs, u32 rt, s16 imm) const
{
    load<s16>(rs, rt, imm);
}

void Recompiler::lhu(u32 rs, u32 rt, s16 imm) const
{
    load<u16>(rs, rt, imm);
}

void Recompiler::ll(u32 rs, u32 rt, s16 imm) const
{
    load_linked<s32>(rs, rt, imm);
}

void Recompiler::lld(u32 rs, u32 rt, s16 imm) const
{
    load_linked<s64>(rs, rt, imm);
}

void Recompiler::lw(u32 rs, u32 rt, s16 imm) const
{
    load<s32>(rs, rt, imm);
}

void Recompiler::lwl(u32 rs, u32 rt, s16 imm) const
{
    load_left<s32>(rs, rt, imm);
}

void Recompiler::lwr(u32 rs, u32 rt, s16 imm) const
{
    load_right<s32>(rs, rt, imm);
}

void Recompiler::lwu(u32 rs, u32 rt, s16 imm) const
{
    load<u32>(rs, rt, imm);
}

void Recompiler::sb(u32 rs, u32 rt, s16 imm) const
{
    store<s8>(rs, rt, imm);
}

void Recompiler::sc(u32 rs, u32 rt, s16 imm) const
{
    store_conditional<s32>(rs, rt, imm);
}

void Recompiler::scd(u32 rs, u32 rt, s16 imm) const
{
    store_conditional<s64>(rs, rt, imm);
}

void Recompiler::sd(u32 rs, u32 rt, s16 imm) const
{
    store<s64>(rs, rt, imm);
}

void Recompiler::sdl(u32 rs, u32 rt, s16 imm) const
{
    store_left<s64>(rs, rt, imm);
}

void Recompiler::sdr(u32 rs, u32 rt, s16 imm) const
{
    store_right<s64>(rs, rt, imm);
}

void Recompiler::sh(u32 rs, u32 rt, s16 imm) const
{
    store<s16>(rs, rt, imm);
}

void Recompiler::sync() const
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void Recompiler::syscall() const
{
    c.call(SignalException<Exception::Syscall>);
    jit.branched = 1;
}

void Recompiler::sw(u32 rs, u32 rt, s16 imm) const
{
    store<s32>(rs, rt, imm);
}

void Recompiler::swl(u32 rs, u32 rt, s16 imm) const
{
    store_left<s32>(rs, rt, imm);
}

void Recompiler::swr(u32 rs, u32 rt, s16 imm) const
{
    store_right<s32>(rs, rt, imm);
}

template<std::integral Int> void Recompiler::load(u32 rs, u32 rt, s16 imm) const
{
    c.mov(r[0], gpr_ptr(rs));
    c.add(r[0], imm);
    c.call(ReadVirtual<s8>);
    if (rt) {
        Label l_end = c.newLabel();
        c.cmp(mem(&exception_occurred), 0);
        c.jne(l_end);
        if constexpr (std::same_as<Int, s32>) {
            c.cdqe(rax);
            c.mov(gpr_ptr(rt), rax);
        } else if constexpr (sizeof(Int) == 8) {
            c.mov(gpr_ptr(rt), rax);
        } else {
            if constexpr (std::same_as<Int, s8>) c.movsx(r[0], al);
            if constexpr (std::same_as<Int, u8>) c.movzx(r[0], al);
            if constexpr (std::same_as<Int, s16>) c.movsx(r[0], ax);
            if constexpr (std::same_as<Int, u16>) c.movzx(r[0], ax);
            if constexpr (std::same_as<Int, u32>) c.mov(r[0].r32(), eax);
            c.mov(gpr_ptr(rt), r[0]);
        }
        c.bind(l_end);
    }
}

template<std::integral Int> void Recompiler::load_left(u32 rs, u32 rt, s16 imm) const
{
    if (rt) {
        Label l_end = c.newLabel();
        c.mov(r[0], gpr_ptr(rs));
        c.add(r[0], imm);
        c.push(rbx);
        c.mov(rbx, r[0]);
        c.call(ReadVirtual<Int, Alignment::UnalignedLeft>);
        c.cmp(mem(&exception_occurred), 0);
        c.jne(l_end);
        c.mov(ecx, ebx);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        c.shl(rax, cl);
        c.mov(r8d, 1);
        c.shl(r8, cl);
        c.dec(r8);
        c.and_(r8, gpr_ptr(rt));
        c.or_(rax, r8);
        c.mov(gpr_ptr(rt), rax);
        c.bind(l_end);
        c.pop(rbx);
    } else {
        c.mov(r[0], gpr_ptr(rs));
        c.add(r[0], imm);
        c.call(ReadVirtual<Int, Alignment::UnalignedLeft>);
    }
}

template<std::integral Int> void Recompiler::load_linked(u32 rs, u32 rt, s16 imm) const
{
    load<Int>(rs, rt, imm);
    cop0.ll_addr = last_physical_address_on_load >> 4; // TODO
    c.mov(mem(&ll_bit), 1);
}

template<std::integral Int> void Recompiler::load_right(u32 rs, u32 rt, s16 imm) const
{
    if (rt) {
        Label l_end = c.newLabel();
        c.mov(r[0], gpr_ptr(rs));
        c.add(r[0], imm);
        c.push(rbx);
        c.mov(rbx, r[0]);
        c.call(ReadVirtual<Int, Alignment::UnalignedRight>);
        c.cmp(mem(&exception_occurred), 0);
        c.jne(l_end);
        c.mov(ecx, ebx);
        c.not_(ecx);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        if constexpr (sizeof(Int) == 4) {
            c.sar(eax, cl);
            c.mov(ebx, 0xFFFF'FF00);
        } else {
            c.sar(rax, cl);
        }

        // TODO
        // c.mov(r8d, 1);
        // c.shl(r8, cl);
        // c.dec(r8);
        // c.and_(r8, gpr_ptr(rt));
        // c.or_(rax, r8);
        // c.mov(gpr_ptr(rt), rax);
        // c.bind(l_end);
        // c.pop(rbx);
    } else {
        c.mov(r[0], gpr_ptr(rs));
        c.add(r[0], imm);
        c.call(ReadVirtual<Int, Alignment::UnalignedRight>);
    }
}

template<std::integral Int> void Recompiler::store(u32 rs, u32 rt, s16 imm) const
{
    c.mov(r[0], gpr_ptr(rs));
    c.add(r[0], imm);
    c.mov(r[1], gpr_ptr(rt));
    c.call(WriteVirtual<sizeof(Int)>);
}

template<std::integral Int> void Recompiler::store_conditional(u32 rs, u32 rt, s16 imm) const
{
    Label l_end = c.newLabel();
    c.cmp(mem(&ll_bit), 0);
    c.je(l_end);
    store<Int>(rs, rt, imm);
    c.bind(l_end);
    if (rt) {
        c.movzx(eax, mem(&ll_bit));
        c.mov(gpr_ptr(rt), rax);
    }
}

template<std::integral Int> void Recompiler::store_left(u32 rs, u32 rt, s16 imm) const
{
#ifdef _WIN32
    c.mov(eax, gpr_ptr(rs));
    c.add(eax, imm);
    c.mov(ecx, eax);
    c.and_(ecx, sizeof(Int) - 1);
    c.shl(ecx, 3);
    c.mov(r[1], gpr_ptr(rt));
    c.shr(r[1], cl);
    c.mov(r[0], eax);
#else
    c.mov(r[0], gpr_ptr(rs));
    c.add(r[0], imm);
    c.mov(ecx, r[0]);
    c.and_(ecx, sizeof(Int) - 1);
    c.shl(ecx, 3);
    c.mov(r[1], gpr_ptr(rt));
    c.shr(r[1], cl);
#endif
    c.call(WriteVirtual<sizeof(Int), Alignment::UnalignedLeft>);
}

template<std::integral Int> void Recompiler::store_right(u32 rs, u32 rt, s16 imm) const
{
#ifdef _WIN32
    c.mov(eax, gpr_ptr(rs));
    c.add(eax, imm);
    c.mov(ecx, eax);
    c.not_(ecx);
    c.and_(ecx, sizeof(Int) - 1);
    c.shl(ecx, 3);
    c.mov(r[1], gpr_ptr(rt));
    c.shl(r[1], cl);
    c.mov(r[0], eax);
#else
    c.mov(r[0], gpr_ptr(rs));
    c.add(r[0], imm);
    c.mov(ecx, r[0]);
    c.not_(ecx);
    c.and_(ecx, sizeof(Int) - 1);
    c.shl(ecx, 3);
    c.mov(r[1], gpr_ptr(rt));
    c.shl(r[1], cl);
#endif
    c.call(WriteVirtual<sizeof(Int), Alignment::UnalignedRight>);
}

template<bool unsig> void Recompiler::multiply64(u32 rs, u32 rt) const
{
    Gp v = get_gpr(rt);
    c.mov(rax, gpr_ptr(rs));
    if constexpr (unsig) c.mul(rax, v);
    else c.imul(rax, v);
    c.mov(lo_mem(), rax);
    c.mov(hi_mem(), rdx);
}

} // namespace n64::vr4300

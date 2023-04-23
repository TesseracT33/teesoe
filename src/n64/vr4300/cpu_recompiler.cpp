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
    call(c, SignalException<Exception::Breakpoint>);
    jit.branched = 1;
}

void Recompiler::ddiv(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    c.mov(rax, gpr_ptr(rs));
    c.mov(rcx, gpr_ptr(rt));
    c.test(rcx, rcx);
    c.je(l_divzero);
    c.mov(r8, rax);
    c.mov(r9, rcx);
    c.xor_(r8, 1LL << 63);
    c.not_(r9);
    c.or_(r8, r9);
    c.jne(l_div);
    c.mov(lo_ptr(), 1LL << 63);
    c.mov(hi_ptr(), 0);
    c.jmp(l_end);
    c.bind(l_divzero);
    c.mov(hi_ptr(), rax);
    c.not_(rax);
    c.sar(rax, 63);
    c.or_(rax, 1);
    c.mov(lo_ptr(), rax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.idiv(rax, rcx);
    c.mov(lo_ptr(), rax);
    c.mov(hi_ptr(), rdx);
    c.bind(l_end);
}

void Recompiler::ddivu(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    c.mov(rax, gpr_ptr(rs));
    c.mov(rcx, gpr_ptr(rt));
    c.test(rcx, rcx);
    c.jne(l_div);
    c.mov(lo_ptr(), -1);
    c.mov(hi_ptr(), rax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.div(rax, rcx);
    c.mov(lo_ptr(), rax);
    c.mov(hi_ptr(), rdx);
    c.bind(l_end);
}

void Recompiler::div(u32 rs, u32 rt) const
{
    asmjit::Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    c.mov(asmjit::x86::eax, gpr_ptr32(rs));
    c.mov(asmjit::x86::ecx, gpr_ptr32(rt));
    c.test(asmjit::x86::ecx, asmjit::x86::ecx);
    c.je(l_divzero);
    c.mov(asmjit::x86::r8d, asmjit::x86::eax);
    c.mov(asmjit::x86::r9d, asmjit::x86::ecx);
    c.add(asmjit::x86::r8d, s32(0x8000'0000));
    c.not_(asmjit::x86::r9d);
    c.or_(asmjit::x86::r8d, asmjit::x86::r9d);
    c.jne(l_div);
    c.mov(lo_ptr(), s32(0x8000'0000));
    c.mov(hi_ptr(), 0);
    c.jmp(l_end);
    c.bind(l_divzero);
    set_hi32(asmjit::x86::eax);
    c.not_(asmjit::x86::eax);
    c.sar(asmjit::x86::eax, 31);
    c.or_(asmjit::x86::eax, 1);
    set_lo32(asmjit::x86::eax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(asmjit::x86::edx, asmjit::x86::edx);
    c.idiv(asmjit::x86::eax, asmjit::x86::ecx);
    set_lo32(asmjit::x86::eax);
    set_hi32(asmjit::x86::edx);
    c.bind(l_end);
}

void Recompiler::divu(u32 rs, u32 rt) const
{
    asmjit::Label l_div = c.newLabel(), l_end = c.newLabel();
    c.mov(asmjit::x86::eax, gpr_ptr32(rs));
    c.mov(asmjit::x86::ecx, gpr_ptr32(rt));
    c.test(asmjit::x86::ecx, asmjit::x86::ecx);
    c.jne(l_div);
    c.mov(lo_ptr(), -1);
    set_hi32(asmjit::x86::eax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(asmjit::x86::edx, asmjit::x86::edx);
    c.div(asmjit::x86::eax, asmjit::x86::ecx);
    set_lo32(asmjit::x86::eax);
    set_hi32(asmjit::x86::edx);
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

void Recompiler::j(u32 instr) const
{
    // TODO
}

void Recompiler::jal(u32 instr) const
{
    // TODO
}

void Recompiler::jalr(u32 rs, u32 rd) const
{
    // TODO
}

void Recompiler::jr(u32 rs) const
{
    // TODO
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

void Recompiler::mult(u32 rs, u32 rt) const
{
    multiply32<false>(rs, rt);
}

void Recompiler::multu(u32 rs, u32 rt) const
{
    multiply32<true>(rs, rt);
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
    call(c, SignalException<Exception::Syscall>);
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
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    call(c, ReadVirtual<std::make_signed_t<Int>>);
    if (rt) {
        Label l_end = c.newLabel();
        c.cmp(ptr(exception_occurred), 0);
        c.jne(l_end);
        if constexpr (std::same_as<Int, s32>) {
            c.cdqe(rax);
            c.mov(gpr_ptr(rt), rax);
        } else if constexpr (sizeof(Int) == 8) {
            c.mov(gpr_ptr(rt), rax);
        } else {
            if constexpr (std::same_as<Int, s8>) c.movsx(rax, al);
            if constexpr (std::same_as<Int, u8>) c.movzx(rax, al);
            if constexpr (std::same_as<Int, s16>) c.movsx(rax, ax);
            if constexpr (std::same_as<Int, u16>) c.movzx(rax, ax);
            if constexpr (std::same_as<Int, u32>) c.mov(eax, eax);
            c.mov(gpr_ptr(rt), rax);
        }
        c.bind(l_end);
    }
}

template<std::integral Int> void Recompiler::load_left(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    if (rt) {
        Label l_end = c.newLabel();
        c.push(rbx);
        c.mov(rbx, gp[0]);
        call<1>(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedLeft>);
        c.cmp(ptr(exception_occurred), 0);
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
        call(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedLeft>);
    }
}

template<std::integral Int> void Recompiler::load_linked(u32 rs, u32 rt, s16 imm) const
{
    load<Int>(rs, rt, imm);
    c.mov(eax, ptr(last_physical_address_on_load));
    c.shl(eax, 4);
    c.mov(ptr(cop0.ll_addr), eax);
    c.mov(ptr(ll_bit), 1);
}

template<std::integral Int> void Recompiler::load_right(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    if (rt) {
        Label l_end = c.newLabel();
        c.push(rbx);
        c.mov(rbx, gp[0]);
        call<1>(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedRight>);
        c.cmp(ptr(exception_occurred), 0);
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
        call(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedRight>);
    }
}

template<bool unsig> void Recompiler::multiply32(u32 rs, u32 rt) const
{
    asmjit::x86::Gp v = get_gpr32(rt);
    c.mov(asmjit::x86::eax, gpr_ptr32(rs));
    if constexpr (unsig) c.mul(asmjit::x86::eax, v);
    else c.imul(asmjit::x86::eax, v);
    set_lo32(asmjit::x86::eax);
    set_hi32(asmjit::x86::edx);
}

template<bool unsig> void Recompiler::multiply64(u32 rs, u32 rt) const
{
    Gp v = get_gpr(rt);
    c.mov(rax, gpr_ptr(rs));
    if constexpr (unsig) c.mul(rax, v);
    else c.imul(rax, v);
    c.mov(lo_ptr(), rax);
    c.mov(hi_ptr(), rdx);
}

template<std::integral Int> void Recompiler::store(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    c.mov(gp[1], gpr_ptr(rt));
    call(c, WriteVirtual<sizeof(Int)>);
}

template<std::integral Int> void Recompiler::store_conditional(u32 rs, u32 rt, s16 imm) const
{
    Label l_end = c.newLabel();
    c.cmp(ptr(ll_bit), 0);
    c.je(l_end);
    store<Int>(rs, rt, imm);
    c.bind(l_end);
    if (rt) {
        c.movzx(eax, ptr(ll_bit));
        set_gpr(rt, rax);
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
    c.mov(gp[1], gpr_ptr(rt));
    c.shr(gp[1], cl);
    c.mov(gp[0], eax);
#else
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    c.mov(ecx, gp[0]);
    c.and_(ecx, sizeof(Int) - 1);
    c.shl(ecx, 3);
    c.mov(gp[1], gpr_ptr(rt));
    c.shr(gp[1], cl);
#endif
    call(c, WriteVirtual<sizeof(Int), Alignment::UnalignedLeft>);
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
    c.mov(gp[1], gpr_ptr(rt));
    c.shl(gp[1], cl);
    c.mov(gp[0], eax);
#else
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    c.mov(ecx, gp[0]);
    c.not_(ecx);
    c.and_(ecx, sizeof(Int) - 1);
    c.shl(ecx, 3);
    c.mov(gp[1], gpr_ptr(rt));
    c.shl(gp[1], cl);
#endif
    call(c, WriteVirtual<sizeof(Int), Alignment::UnalignedRight>);
}

} // namespace n64::vr4300

#pragma once

#include "cpu.hpp"
#include "host.hpp"
#include "jit/jit.hpp"

#include <bit>

namespace mips {

template<typename GprInt, typename LoHiInt, std::integral PcInt, typename GprBaseInt = GprInt>
struct Recompiler : public Cpu<GprInt, LoHiInt, PcInt, GprBaseInt> {
    using ExceptionHandler = void (*)();
    using LinkHandler = void (*)(u32 reg);
    template<std::integral Int> using JumpHandler = void (*)(PcInt target);

    consteval Recompiler(Jit& jit,
      Gpr<GprInt>& gpr,
      LoHiInt& lo,
      LoHiInt& hi,
      PcInt& pc,
      bool& in_branch_delay_slot,
      JumpHandler<PcInt> jump_handler,
      LinkHandler link_handler,
      ExceptionHandler integer_overflow_exception = nullptr,
      ExceptionHandler trap_exception = nullptr)
      : jit(jit),
        c(jit.compiler),
        Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>(gpr,
          lo,
          hi,
          pc,
          in_branch_delay_slot,
          jump_handler,
          link_handler,
          integer_overflow_exception,
          trap_exception)
    {
    }

    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::gpr;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::lo;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::hi;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::pc;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::in_branch_delay_slot;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::jump;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::link;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::integer_overflow_exception;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::trap_exception;

    static constexpr bool mips32 = sizeof(GprBaseInt) == 4;
    static constexpr bool mips64 = sizeof(GprBaseInt) == 8;

    Jit& jit;
    asmjit::x86::Compiler& c;

    void add(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr32(rs), v1 = get_gpr32(rt);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v0, v1);
        c.jno(l_noexception);
        c.call(integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr32(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        jit.branch_hit = 1;
    }

    void addi(u32 rs, u32 rt, s16 imm) const {}

    void addiu(u32 rs, u32 rt, s16 imm) const {}

    void addu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr32(rs), v1 = get_gpr32(rt);
        c.add(v0, v1);
        set_gpr32(rd, v0);
    }

    void and_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        if (!rs || !rt) {
            // c.mov(rd, 0);
        } else {
            asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
            c.and_(v0, v1);
            set_gpr(rd, v0);
        }
    }

    void andi(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v0 = get_gpr(rs);
        c.and_(v0, imm);
        set_gpr(rt, v0);
    }

    void beq(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_nobranch = c.newLabel();
        c.cmp(v0, v1);
        c.jne(l_nobranch);
        c.mov(v0, imm);
        c.shl(v0, 2);
        c.add(v0, pc_ptr());
        c.call(jump);
        c.bind(l_nobranch);
        jit.branch_hit = 1;
    }

    void beql(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_nobranch = c.newLabel(), l_end = c.newLabel();
        c.cmp(v0, v1);
        c.jne(l_nobranch);
        c.mov(v0, imm);
        c.shl(v0, 2);
        c.add(v0, pc_ptr());
        c.call(jump);
        c.jmp(l_end);
        c.bind(l_nobranch);
        c.add(pc_ptr(), 4);
        c.ret();
        c.bind(l_end);
        jit.branch_hit = 1;
    }

    void bgez(u32 rs, s16 imm) const {}

    void bgezal(u32 rs, s16 imm) const {}

    void bgezall(u32 rs, s16 imm) const {}

    void bgezl(u32 rs, s16 imm) const {}

    void bgtz(u32 rs, s16 imm) const {}

    void bgtzl(u32 rs, s16 imm) const {}

    void blez(u32 rs, s16 imm) const {}

    void blezl(u32 rs, s16 imm) const {}

    void bltz(u32 rs, s16 imm) const {}

    void bltzal(u32 rs, s16 imm) const {}

    void bltzall(u32 rs, s16 imm) const {}

    void bltzl(u32 rs, s16 imm) const {}

    void bne(u32 rs, u32 rt, s16 imm) const {}

    void bnel(u32 rs, u32 rt, s16 imm) const {}

    void dadd(u32 rs, u32 rt, u32 rd) const {}

    void daddi(u32 rs, u32 rt, s16 imm) const {}

    void daddiu(u32 rs, u32 rt, s16 imm) const {}

    void daddu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.add(v0, v1);
        set_gpr(rd, v0);
    }

    void div(u32 rs, u32 rt) const {}

    void divu(u32 rs, u32 rt) const
    {
        // asmjit::Label l_div = c.newLabel(), l_end = c.newLabel();
        // c.mov(asmjit::x86::eax, gpr_ptr32(rs));
        // c.mov(asmjit::x86::edx, gpr_ptr32(rt));
        // c.cmp(asmjit::x86::edx, 0);
        // c.jne(l_div);
        // c.mov(lo_ptr(), -1);
        // c.mov(hi_ptr(), asmjit::x86::eax);
        // c.jmp(l_end);
        // c.bind(l_div);
        // c.div(asmjit::x86::edx);
        // if constexpr (mips32) {
        //     c.mov(lo_ptr(), asmjit::x86::eax);
        //     c.mov(hi_ptr(), asmjit::x86::edx);
        // } else {
        //     c.cdqe(asmjit::x86::rax);
        //     c.movsxd(asmjit::x86::rdx, asmjit::x86::edx);
        //     c.mov(lo_ptr(), asmjit::x86::eax);
        //     c.mov(hi_ptr(), asmjit::x86::edx);
        // }
        // c.bind(l_end);
    }

    void dsll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        c.shl(vrt, sa);
        set_gpr(rd, vrt);
    }

    void dsll32(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        c.shl(vrt, sa + 32);
        set_gpr(rd, vrt);
    }

    void dsllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        // c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.shl(vrt, asmjit::x86::ecx);
        set_gpr(rd, vrt);
    }

    void dsra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        c.sar(vrt, sa);
        set_gpr(rd, vrt);
    }

    void dsra32(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        c.sar(vrt, sa + 32);
        set_gpr(rd, vrt);
    }

    void dsrav(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        // c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.sar(vrt, asmjit::x86::ecx);
        set_gpr(rd, vrt);
    }

    void dsrl(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        c.shr(vrt, sa);
        set_gpr(rd, vrt);
    }

    void dsrl32(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        c.shr(vrt, sa + 32);
        set_gpr(rd, vrt);
    }

    void dsrlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp vrt = get_gpr(rt);
        // c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.shr(vrt, asmjit::x86::ecx);
        set_gpr(rd, vrt);
    }

    void dsub(u32 rs, u32 rt, u32 rd) const {}

    void dsubu(u32 rs, u32 rt, u32 rd) const {}

    void j(u32 instr) const {}

    void jal(u32 instr) const {}

    void jalr(u32 rs, u32 rd) const {}

    void jr(u32 rs) const {}

    void lui(u32 rt, s16 imm) const
    {
        if (!rt) return;
    }

    void mfhi(u32 rd) const {}

    void mflo(u32 rd) const {}

    void movn(u32 rs, u32 rt, u32 rd) const {}

    void movz(u32 rs, u32 rt, u32 rd) const {}

    void mthi(u32 rs) const {}

    void mtlo(u32 rs) const {}

    void mult(u32 rs, u32 rt) const {}

    void multu(u32 rs, u32 rt) const {}

    void nor(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.or_(v0, v1);
        c.not_(v0);
        set_gpr(rd, v0);
    }

    void or_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.or_(v0, v1);
        set_gpr(rd, v0);
    }

    void ori(u32 rs, u32 rt, u16 imm) const {}

    void sll(u32 rt, u32 rd, u32 sa) const {}

    void sllv(u32 rs, u32 rt, u32 rd) const {}

    void slt(u32 rs, u32 rt, u32 rd) const {}

    void slti(u32 rs, u32 rt, s16 imm) const {}

    void sltiu(u32 rs, u32 rt, s16 imm) const {}

    void sltu(u32 rs, u32 rt, u32 rd) const {}

    void sra(u32 rt, u32 rd, u32 sa) const {}

    void srav(u32 rs, u32 rt, u32 rd) const {}

    void srl(u32 rt, u32 rd, u32 sa) const {}

    void srlv(u32 rs, u32 rt, u32 rd) const {}

    void sub(u32 rs, u32 rt, u32 rd) const {}

    void subu(u32 rs, u32 rt, u32 rd) const {}

    void teq(u32 rs, u32 rt) const {}

    void teqi(u32 rs, s16 imm) const {}

    void tge(u32 rs, u32 rt) const {}

    void tgei(u32 rs, s16 imm) const {}

    void tgeu(u32 rs, u32 rt) const {}

    void tgeiu(u32 rs, s16 imm) const {}

    void tlt(u32 rs, u32 rt) const {}

    void tlti(u32 rs, s16 imm) const {}

    void tltu(u32 rs, u32 rt) const {}

    void tltiu(u32 rs, s16 imm) const {}

    void tne(u32 rs, u32 rt) const {}

    void tnei(u32 rs, s16 imm) const {}

    void xor_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.xor_(v0, v1);
        set_gpr(rd, v0);
    }

    void xori(u32 rs, u32 rt, u16 imm) const {}

private:
    asmjit::x86::Mem gpr_ptr(u32 idx) const
    {
        return asmjit::x86::ptr(std::bit_cast<u64>(gpr.ptr(idx)), sizeof(GprBaseInt));
    }

    asmjit::x86::Gp get_gpr32(u32 idx) const { return c.newGpd(); }

    asmjit::x86::Gp get_gpr(u32 idx) const
    {
        asmjit::x86::Gp v = mips32 ? c.newGpd() : c.newGpq();
        c.mov(v, gpr_ptr(idx));
        return v;
    }

    asmjit::x86::Mem lo_ptr() const { return asmjit::x86::ptr(std::bit_cast<u64>(&lo), sizeof(LoHiInt)); }

    asmjit::x86::Mem hi_ptr() const { return asmjit::x86::ptr(std::bit_cast<u64>(&hi), sizeof(LoHiInt)); }

    asmjit::x86::Mem pc_ptr() const { return asmjit::x86::ptr(std::bit_cast<u64>(&pc), sizeof(PcInt)); }

    void set_gpr(u32 idx, asmjit::x86::Gp vreg) const { c.mov(gpr_ptr(idx), vreg); }

    void set_gpr32(u32 idx, asmjit::x86::Gp vreg) const
    {
        if constexpr (mips32) {
            set_gpr(idx, vreg);
        } else {
            asmjit::x86::Gp v0 = c.newGpq();
            c.movsxd(v0, vreg);
            set_gpr(idx, v0);
        }
    }
};

} // namespace mips

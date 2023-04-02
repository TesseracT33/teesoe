#pragma once

#include "cpu.hpp"
#include "host.hpp"
#include "jit/jit.hpp"
#include "jit/util.hpp"

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
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::mips32;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::mips64;

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

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr32(rs);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v, imm);
        c.jno(l_noexception);
        c.call(integer_overflow_exception);
        if (rt) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr32(rt, v);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        jit.branch_hit = 1;
    }

    void addiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v = get_gpr32(rs);
        c.add(v, imm);
        set_gpr32(rt, v);
    }

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
            c.mov(gpr_ptr(rd), 0);
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

    void beq(u32 rs, u32 rt, s16 imm) const { branch<Cond::Eq>(rs, rt, imm); }

    void beql(u32 rs, u32 rt, s16 imm) const { branch_likely<Cond::Eq>(rs, rt, imm); }

    void bgez(u32 rs, s16 imm) const { branch<Cond::Ge>(rs, imm); }

    void bgezal(u32 rs, s16 imm) const { branch_and_link<Cond::Ge>(rs, imm); }

    void bgezall(u32 rs, s16 imm) const { branch_and_link_likely<Cond::Ge>(rs, imm); }

    void bgezl(u32 rs, s16 imm) const { branch_likely<Cond::Ge>(rs, imm); }

    void bgtz(u32 rs, s16 imm) const { branch<Cond::Gt>(rs, imm); }

    void bgtzl(u32 rs, s16 imm) const { branch_likely<Cond::Gt>(rs, imm); }

    void blez(u32 rs, s16 imm) const { branch<Cond::Le>(rs, imm); }

    void blezl(u32 rs, s16 imm) const { branch_likely<Cond::Le>(rs, imm); }

    void bltz(u32 rs, s16 imm) const { branch<Cond::Lt>(rs, imm); }

    void bltzal(u32 rs, s16 imm) const { branch_and_link<Cond::Lt>(rs, imm); }

    void bltzall(u32 rs, s16 imm) const { branch_and_link_likely<Cond::Lt>(rs, imm); }

    void bltzl(u32 rs, s16 imm) const { branch_likely<Cond::Lt>(rs, imm); }

    void bne(u32 rs, u32 rt, s16 imm) const { branch<Cond::Ne>(rs, rt, imm); }

    void bnel(u32 rs, u32 rt, s16 imm) const { branch_likely<Cond::Ne>(rs, rt, imm); }

    void dadd(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v0, v1);
        c.jno(l_noexception);
        c.call(integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        jit.branch_hit = 1;
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr(rs);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v, imm);
        c.jno(l_noexception);
        c.call(integer_overflow_exception);
        if (rt) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr(rt, v);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        jit.branch_hit = 1;
    }

    void daddiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v = get_gpr(rs);
        c.add(v, imm);
        set_gpr(rt, v);
    }

    void daddu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.add(v0, v1);
        set_gpr(rd, v0);
    }

    void div(u32 rs, u32 rt) const
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

    void divu(u32 rs, u32 rt) const
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

    void dsll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.shl(v, sa);
        set_gpr(rd, v);
    }

    void dsll32(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.shl(v, sa + 32);
        set_gpr(rd, v);
    }

    void dsllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.shl(v, asmjit::x86::ecx);
        set_gpr(rd, v);
    }

    void dsra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.sar(v, sa);
        set_gpr(rd, v);
    }

    void dsra32(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.sar(v, sa + 32);
        set_gpr(rd, v);
    }

    void dsrav(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.sar(v, asmjit::x86::ecx);
        set_gpr(rd, v);
    }

    void dsrl(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.shr(v, sa);
        set_gpr(rd, v);
    }

    void dsrl32(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.shr(v, sa + 32);
        set_gpr(rd, v);
    }

    void dsrlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr(rt);
        c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.shr(v, asmjit::x86::ecx);
        set_gpr(rd, v);
    }

    void dsub(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_noexception = c.newLabel();
        c.sub(v0, v1);
        c.jno(l_noexception);
        c.call(integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        jit.branch_hit = 1;
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.sub(v0, v1);
        set_gpr(rd, v0);
    }

    void j(u32 instr) const
    {
        asmjit::Label l_nojump = c.newLabel();
        c.cmp(ptr(in_branch_delay_slot), 0);
        c.jne(l_nojump);
        c.mov(r[0], pc_ptr());
        c.and_(r[0], s32(0xF000'0000));
        c.or_(r[0], instr << 2 & 0xFFF'FFFF);
        c.call(jump);
        c.bind(l_nojump);
        jit.branch_hit = 1;
    }

    void jal(u32 instr) const
    {
        asmjit::Label l_nojump = c.newLabel();
        c.cmp(ptr(in_branch_delay_slot), 0);
        c.jne(l_nojump);
        c.mov(r[0], pc_ptr());
        c.and_(r[0], s32(0xF000'0000));
        c.or_(r[0], instr << 2 & 0xFFF'FFFF);
        c.call(jump);
        c.bind(l_nojump);
        c.mov(r[0].r32(), 31);
        c.call(link);
        jit.branch_hit = 1;
    }

    void jalr(u32 rs, u32 rd) const
    {
        asmjit::Label l_nojump = c.newLabel();
        c.cmp(ptr(in_branch_delay_slot), 0);
        c.jne(l_nojump);
        c.mov(r[0], gpr_ptr(rs));
        c.call(jump);
        c.bind(l_nojump);
        c.mov(r[0].r32(), rd);
        c.call(link);
        jit.branch_hit = 1;
    }

    void jr(u32 rs) const
    {
        asmjit::Label l_nojump = c.newLabel();
        c.cmp(ptr(in_branch_delay_slot), 0);
        c.jne(l_nojump);
        c.mov(r[0], gpr_ptr(rs));
        c.call(jump);
        c.bind(l_nojump);
        jit.branch_hit = 1;
    }

    void lui(u32 rt, s16 imm) const
    {
        if (!rt) return;
        c.mov(gpr_ptr(rt), imm << 16);
    }

    void mfhi(u32 rd) const
    {
        if (!rd) return;
        c.mov(r[0], hi_ptr());
        set_gpr(rd, r[0]);
    }

    void mflo(u32 rd) const
    {
        if (!rd) return;
        c.mov(r[0], lo_ptr());
        set_gpr(rd, r[0]);
    }

    void movn(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::Label l_end = c.newLabel();
        asmjit::x86::Gp v = get_gpr(rs);
        c.cmp(gpr_ptr(rt), 0);
        c.je(l_end);
        set_gpr(rd, v);
        c.bind(l_end);
    }

    void movz(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::Label l_end = c.newLabel();
        asmjit::x86::Gp v = get_gpr(rs);
        c.cmp(gpr_ptr(rt), 0);
        c.jne(l_end);
        set_gpr(rd, v);
        c.bind(l_end);
    }

    void mthi(u32 rs) const
    {
        if (rs) {
            asmjit::x86::Gp v = get_gpr(rs);
            c.mov(hi_ptr(), v);
        } else {
            c.mov(hi_ptr(), 0);
        }
    }

    void mtlo(u32 rs) const
    {
        if (rs) {
            asmjit::x86::Gp v = get_gpr(rs);
            c.mov(lo_ptr(), v);
        } else {
            c.mov(lo_ptr(), 0);
        }
    }

    void mult(u32 rs, u32 rt) const { multiply<false>(rs, rt); }

    void multu(u32 rs, u32 rt) const { multiply<true>(rs, rt); }

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

    void ori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v = get_gpr(rs);
        c.or_(v, imm);
        set_gpr(rt, v);
    }

    void sll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr32(rt);
        c.shl(v, sa);
        set_gpr32(rd, v);
    }

    void sllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr32(rt);
        c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.shl(v, asmjit::x86::ecx);
        set_gpr32(rd, v);
    }

    void slt(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.cmp(v0, v1);
        c.setl(v0);
        set_gpr(rd, v0);
    }

    void slti(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v = get_gpr(rs);
        c.cmp(v, imm);
        c.setl(v);
        set_gpr(rt, v);
    }

    void sltiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v = get_gpr(rs);
        c.cmp(v, imm);
        c.setb(v);
        set_gpr(rt, v);
    }

    void sltu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.cmp(v0, v1);
        c.setb(v0);
        set_gpr(rd, v0);
    }

    void sra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr32(rt);
        c.sar(v, sa);
        set_gpr32(rd, v);
    }

    void srav(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr32(rt);
        c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.sar(v, asmjit::x86::ecx);
        set_gpr32(rd, v);
    }

    void srl(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr32(rt);
        c.shr(v, sa);
        set_gpr32(rd, v);
    }

    void srlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v = get_gpr32(rt);
        c.mov(asmjit::x86::ecx, gpr_ptr32(rs));
        c.shr(v, asmjit::x86::ecx);
        set_gpr32(rd, v);
    }

    void sub(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr32(rs), v1 = get_gpr32(rt);
        asmjit::Label l_noexception = c.newLabel();
        c.sub(v0, v1);
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

    void subu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        asmjit::x86::Gp v0 = get_gpr32(rs), v1 = get_gpr32(rt);
        c.add(v0, v1);
        set_gpr32(rd, v0);
    }

    void teq(u32 rs, u32 rt) const { trap<Cond::Eq>(rs, rt); }

    void teqi(u32 rs, s16 imm) const { trap<Cond::Eq>(rs, imm); }

    void tge(u32 rs, u32 rt) const { trap<Cond::Ge>(rs, rt); }

    void tgei(u32 rs, s16 imm) const { trap<Cond::Ge>(rs, imm); }

    void tgeu(u32 rs, u32 rt) const { trap<Cond::Geu>(rs, rt); }

    void tgeiu(u32 rs, s16 imm) const { trap<Cond::Geu>(rs, imm); }

    void tlt(u32 rs, u32 rt) const { trap<Cond::Lt>(rs, rt); }

    void tlti(u32 rs, s16 imm) const { trap<Cond::Lt>(rs, imm); }

    void tltu(u32 rs, u32 rt) const { trap<Cond::Ltu>(rs, rt); }

    void tltiu(u32 rs, s16 imm) const { trap<Cond::Ltu>(rs, imm); }

    void tne(u32 rs, u32 rt) const { trap<Cond::Ne>(rs, rt); }

    void tnei(u32 rs, s16 imm) const { trap<Cond::Ne>(rs, imm); }

    void xor_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        if (rs == rt) {
            c.mov(gpr_ptr(rd), 0);
        } else {
            asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
            c.xor_(v0, v1);
            set_gpr(rd, v0);
        }
    }

    void xori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        asmjit::x86::Gp v = get_gpr(rs);
        c.xor_(v, imm);
        set_gpr(rt, v);
    }

protected:
    enum class Cond {
        Eq,
        Ge,
        Geu,
        Gt,
        Le,
        Lt,
        Ltu,
        Ne
    };

    static constexpr std::array r = {
#ifdef _WIN32
        asmjit::x86::rcx,
        asmjit::x86::rdx,
        asmjit::x86::r8,
        asmjit::x86::r9,
        asmjit::x86::r10,
        asmjit::x86::r11,
        asmjit::x86::rax,
#else
        asmjit::x86::rdi,
        asmjit::x86::rsi,
        asmjit::x86::rdx,
        asmjit::x86::rcx,
        asmjit::x86::r8,
        asmjit::x86::r9,
        asmjit::x86::r10,
        asmjit::x86::r11,
        asmjit::x86::rax,
#endif
    };

    asmjit::x86::Mem gpr_ptr(u32 idx) const
    {
        return asmjit::x86::ptr(std::bit_cast<u64>(gpr.ptr(idx)), sizeof(GprBaseInt));
    }

    asmjit::x86::Mem gpr_ptr32(u32 idx) const { return asmjit::x86::ptr(std::bit_cast<u64>(gpr.ptr(idx)), 4); }

    asmjit::x86::Gp get_gpr32(u32 idx) const { return c.newGpd(); }

    asmjit::x86::Gp get_gpr(u32 idx) const
    {
        asmjit::x86::Gp v = mips32 ? c.newGpd() : c.newGpq();
        c.mov(v, gpr_ptr(idx));
        return v;
    }

    asmjit::x86::Mem lo_ptr() const { return ptr(lo); }

    asmjit::x86::Mem hi_ptr() const { return ptr(hi); }

    asmjit::x86::Mem pc_ptr() const { return ptr(pc); }

    void set_gpr(u32 idx, asmjit::x86::Gp r) const { c.mov(gpr_ptr(idx), r); }

    void set_gpr32(u32 idx, asmjit::x86::Gp r) const
    {
        if constexpr (mips32) {
            set_gpr(idx, r);
        } else {
            c.movsxd(r.r64(), r.r32());
            set_gpr(idx, r);
        }
    }

    template<Cond cc> void branch(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_nobranch = c.newLabel();
        c.cmp(v0, v1);
        if constexpr (cc == Cond::Eq) c.jne(l_nobranch);
        if constexpr (cc == Cond::Ne) c.je(l_nobranch);
        c.mov(v0, imm);
        c.shl(v0, 2);
        c.add(v0, pc_ptr());
        c.call(jump);
        c.bind(l_nobranch);
        jit.branch_hit = 1;
    }

    template<Cond cc> void branch(u32 rs, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr(rs);
        asmjit::Label l_nobranch = c.newLabel();
        c.cmp(v, v);
        if constexpr (cc == Cond::Ge) c.jl(l_nobranch);
        if constexpr (cc == Cond::Gt) c.jle(l_nobranch);
        if constexpr (cc == Cond::Le) c.jg(l_nobranch);
        if constexpr (cc == Cond::Lt) c.jge(l_nobranch);
        c.mov(v, imm);
        c.shl(v, 2);
        c.add(v, pc_ptr());
        c.call(jump);
        c.bind(l_nobranch);
        jit.branch_hit = 1;
    }

    template<Cond cc> void branch_likely(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_nobranch = c.newLabel(), l_end = c.newLabel();
        c.cmp(v0, v1);
        if constexpr (cc == Cond::Eq) c.jne(l_nobranch);
        if constexpr (cc == Cond::Ne) c.je(l_nobranch);
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

    template<Cond cc> void branch_likely(u32 rs, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr(rs);
        asmjit::Label l_nobranch = c.newLabel(), l_end = c.newLabel();
        c.cmp(v, v);
        if constexpr (cc == Cond::Ge) c.jl(l_nobranch);
        if constexpr (cc == Cond::Gt) c.jle(l_nobranch);
        if constexpr (cc == Cond::Le) c.jg(l_nobranch);
        if constexpr (cc == Cond::Lt) c.jge(l_nobranch);
        c.mov(v, imm);
        c.shl(v, 2);
        c.add(v, pc_ptr());
        c.call(jump);
        c.jmp(l_end);
        c.bind(l_nobranch);
        c.add(pc_ptr(), 4);
        c.ret();
        c.bind(l_end);
        jit.branch_hit = 1;
    }

    template<Cond cc> void branch_and_link(u32 rs, s16 imm) const
    {
        c.mov(r[0].r32(), 31);
        c.call(link);
        branch<cc>(rs, imm);
    }

    template<Cond cc> void branch_and_link_likely(u32 rs, s16 imm) const
    {
        c.mov(r[0].r32(), 31);
        c.call(link);
        branch_likely<cc>(rs, imm);
    }

    void set_lo32(asmjit::x86::Gpd v) const
    {
        if constexpr (mips32) {
            c.mov(lo_ptr(), v);
        } else {
            if (v == asmjit::x86::eax) {
                c.cdqe(asmjit::x86::rax);
            } else {
                c.movsxd(v.r64(), v);
            }
            c.mov(lo_ptr(), v.r64());
        }
    }

    void set_hi32(asmjit::x86::Gpd v) const
    {
        if constexpr (mips32) {
            c.mov(hi_ptr(), v);
        } else {
            if (v == asmjit::x86::eax) {
                c.cdqe(asmjit::x86::rax);
            } else {
                c.movsxd(v.r64(), v);
            }
            c.mov(hi_ptr(), v.r64());
        }
    }

    template<bool unsig> void multiply(u32 rs, u32 rt) const
    {
        asmjit::x86::Gp v = get_gpr32(rt);
        c.mov(asmjit::x86::eax, gpr_ptr32(rs));
        if constexpr (unsig) c.mul(asmjit::x86::eax, v);
        else c.imul(asmjit::x86::eax, v);
        set_lo32(asmjit::x86::eax);
        set_hi32(asmjit::x86::edx);
    }

    template<Cond cc> void trap(u32 rs, u32 rt) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        asmjit::Label l_end = c.newLabel();
        c.cmp(v0, v1);
        if constexpr (cc == Cond::Eq) c.jne(l_end);
        if constexpr (cc == Cond::Ge) c.jl(l_end);
        if constexpr (cc == Cond::Geu) c.jb(l_end);
        if constexpr (cc == Cond::Lt) c.jge(l_end);
        if constexpr (cc == Cond::Ltu) c.jae(l_end);
        if constexpr (cc == Cond::Ne) c.je(l_end);
        c.call(trap_exception);
        c.bind(l_end);
        jit.branched = 1;
    }

    template<Cond cc> void trap(u32 rs, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr(rs);
        asmjit::Label l_end = c.newLabel();
        c.cmp(v, imm);
        if constexpr (cc == Cond::Eq) c.jne(l_end);
        if constexpr (cc == Cond::Ge) c.jl(l_end);
        if constexpr (cc == Cond::Geu) c.jb(l_end);
        if constexpr (cc == Cond::Lt) c.jge(l_end);
        if constexpr (cc == Cond::Ltu) c.jae(l_end);
        if constexpr (cc == Cond::Ne) c.je(l_end);
        c.call(trap_exception);
        c.bind(l_end);
        jit.branched = 1;
    }
};

} // namespace mips

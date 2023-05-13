#pragma once

#include "cpu.hpp"
#include "host.hpp"
#include "jit_util.hpp"

#include <bit>

namespace mips {

template<std::signed_integral GprInt, std::signed_integral LoHiInt, std::integral PcInt>
struct Recompiler : public Cpu<GprInt, LoHiInt, PcInt> {
    using ExceptionHandler = void (*)();
    template<std::integral Int> using JumpHandler = void (*)(PcInt target);
    using LinkHandler = void (*)(u32 reg);
    using OnBranchHandler = void (*)();

    consteval Recompiler(asmjit::x86::Compiler& compiler,
      Gpr<GprInt>& gpr,
      LoHiInt& lo,
      LoHiInt& hi,
      PcInt& pc,
      bool const& dword_op_cond,
      JumpHandler<PcInt> jump_handler,
      LinkHandler link_handler,
      OnBranchHandler on_branch_handler,
      ExceptionHandler integer_overflow_exception = nullptr,
      ExceptionHandler reserved_instruction_exception = nullptr,
      ExceptionHandler trap_exception = nullptr)
      : c(compiler),
        on_branch(on_branch_handler),
        Cpu<GprInt, LoHiInt, PcInt>(gpr,
          lo,
          hi,
          pc,
          dword_op_cond,
          jump_handler,
          link_handler,
          integer_overflow_exception,
          reserved_instruction_exception,
          trap_exception)
    {
    }

    using Cpu<GprInt, LoHiInt, PcInt>::gpr;
    using Cpu<GprInt, LoHiInt, PcInt>::lo;
    using Cpu<GprInt, LoHiInt, PcInt>::hi;
    using Cpu<GprInt, LoHiInt, PcInt>::pc;
    using Cpu<GprInt, LoHiInt, PcInt>::dword_op_cond;
    using Cpu<GprInt, LoHiInt, PcInt>::jump;
    using Cpu<GprInt, LoHiInt, PcInt>::link;
    using Cpu<GprInt, LoHiInt, PcInt>::integer_overflow_exception;
    using Cpu<GprInt, LoHiInt, PcInt>::reserved_instruction_exception;
    using Cpu<GprInt, LoHiInt, PcInt>::trap_exception;
    using Cpu<GprInt, LoHiInt, PcInt>::mips32;
    using Cpu<GprInt, LoHiInt, PcInt>::mips64;

    asmjit::x86::Compiler& c;
    OnBranchHandler on_branch;

    void add(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr32(rs), v1 = get_gpr32(rt);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v0, v1);
        c.jno(l_noexception);
        call(c, integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr32(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        on_branch();
    }

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr32(rs);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v, imm);
        c.jno(l_noexception);
        call(c, integer_overflow_exception);
        if (rt) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr32(rt, v);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        on_branch();
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
        // TODO: check dword_op_cond. Possibly compile for it being true/false, then invalidate if it changes?
        asmjit::Label l_noexception = c.newLabel();
        asmjit::x86::Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
        c.add(v0, v1);
        c.jno(l_noexception);
        call(c, integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        on_branch();
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        asmjit::x86::Gp v = get_gpr(rs);
        asmjit::Label l_noexception = c.newLabel();
        c.add(v, imm);
        c.jno(l_noexception);
        call(c, integer_overflow_exception);
        if (rt) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr(rt, v);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        on_branch();
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
        call(c, integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        on_branch();
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
        c.mov(gp[0], ptr(pc));
        c.and_(gp[0], s32(0xF000'0000));
        c.or_(gp[0], instr << 2 & 0xFFF'FFFF);
        call(c, jump);
        on_branch();
    }

    void jal(u32 instr) const
    {
        c.mov(gp[0], ptr(pc));
        c.and_(gp[0], s32(0xF000'0000));
        c.or_(gp[0], instr << 2 & 0xFFF'FFFF);
        call(c, jump);
        c.mov(gp[0], ptr(pc));
        c.add(gp[0], 4);
        c.mov(gpr_ptr(31), gp[0]);
        on_branch();
    }

    void jalr(u32 rs, u32 rd) const
    {
        c.mov(gp[0], gpr_ptr(rs));
        call(c, jump);
        c.mov(gp[0], ptr(pc));
        c.add(gp[0], 4);
        c.mov(gpr_ptr(rd), gp[0]);
        on_branch();
    }

    void jr(u32 rs) const
    {
        c.mov(gp[0], gpr_ptr(rs));
        call(c, jump);
        on_branch();
    }

    void lui(u32 rt, s16 imm) const
    {
        if (!rt) return;
        c.mov(gpr_ptr(rt), imm << 16);
    }

    void mfhi(u32 rd) const
    {
        if (!rd) return;
        c.mov(gp[0], ptr(hi));
        set_gpr(rd, gp[0]);
    }

    void mflo(u32 rd) const
    {
        if (!rd) return;
        c.mov(gp[0], ptr(lo));
        set_gpr(rd, gp[0]);
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
            c.mov(ptr(hi), v);
        } else {
            c.mov(ptr(hi), 0);
        }
    }

    void mtlo(u32 rs) const
    {
        if (rs) {
            asmjit::x86::Gp v = get_gpr(rs);
            c.mov(ptr(lo), v);
        } else {
            c.mov(ptr(lo), 0);
        }
    }

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
        call(c, integer_overflow_exception);
        if (rd) {
            asmjit::Label l_end = c.newLabel();
            c.jmp(l_end);
            c.bind(l_noexception);
            set_gpr32(rd, v0);
            c.bind(l_end);
        } else {
            c.bind(l_noexception);
        }
        on_branch();
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

    asmjit::x86::Mem gpr_ptr(u32 idx) const
    {
        return asmjit::x86::ptr(std::bit_cast<u64>(gpr.ptr(idx)), sizeof(GprInt));
    }

    asmjit::x86::Mem gpr_ptr32(u32 idx) const { return asmjit::x86::ptr(std::bit_cast<u64>(gpr.ptr(idx)), 4); }

    asmjit::x86::Gp get_gpr32(u32 idx) const { return c.newGpd(); }

    asmjit::x86::Gp get_gpr(u32 idx) const
    {
        asmjit::x86::Gp v = mips32 ? c.newGpd() : c.newGpq();
        c.mov(v, gpr_ptr(idx));
        return v;
    }

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
        c.add(v0, ptr(pc));
        call(c, jump);
        c.bind(l_nobranch);
        on_branch();
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
        c.add(v, ptr(pc));
        call(c, jump);
        c.bind(l_nobranch);
        on_branch();
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
        c.add(v0, ptr(pc));
        call(c, jump);
        c.jmp(l_end);
        c.bind(l_nobranch);
        c.add(ptr(pc), 4);
        c.ret();
        c.bind(l_end);
        on_branch();
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
        c.add(v, ptr(pc));
        call(c, jump);
        c.jmp(l_end);
        c.bind(l_nobranch);
        c.add(ptr(pc), 4);
        c.ret();
        c.bind(l_end);
        on_branch();
    }

    template<Cond cc> void branch_and_link(u32 rs, s16 imm) const
    {
        branch<cc>(rs, imm);
        c.mov(gp[0], ptr(pc));
        c.add(gp[0], 4);
        c.mov(gpr_ptr(31), gp[0]);
    }

    template<Cond cc> void branch_and_link_likely(u32 rs, s16 imm) const
    {
        branch_likely<cc>(rs, imm);
        c.mov(gp[0], ptr(pc));
        c.add(gp[0], 4);
        c.mov(gpr_ptr(31), gp[0]);
    }

    void set_lo32(asmjit::x86::Gpd v) const
    {
        if constexpr (mips32) {
            c.mov(ptr(lo), v);
        } else {
            if (v == asmjit::x86::eax) {
                c.cdqe(asmjit::x86::rax);
            } else {
                c.movsxd(v.r64(), v);
            }
            c.mov(ptr(lo), v.r64());
        }
    }

    void set_hi32(asmjit::x86::Gpd v) const
    {
        if constexpr (mips32) {
            c.mov(ptr(hi), v);
        } else {
            if (v == asmjit::x86::eax) {
                c.cdqe(asmjit::x86::rax);
            } else {
                c.movsxd(v.r64(), v);
            }
            c.mov(ptr(hi), v.r64());
        }
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
        call(c, trap_exception);
        c.bind(l_end);
        on_branch();
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
        call(c, trap_exception);
        c.bind(l_end);
        on_branch();
    }
};

} // namespace mips

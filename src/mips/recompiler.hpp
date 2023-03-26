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

    asmjit::x86::Mem gpr_ptr(u32 idx) const
    {
        if constexpr (mips32) {
            return asmjit::x86::dword_ptr(std::bit_cast<u64>(gpr.ptr(idx)));
        } else {
            return asmjit::x86::qword_ptr(std::bit_cast<u64>(gpr.ptr(idx)));
        }
    }

    asmjit::x86::Gp get_gpr32(u32 idx) const { return c.newGpd(); }

    asmjit::x86::Gp get_gpr(u32 idx) const
    {
        asmjit::x86::Gp v = mips32 ? c.newGpd() : c.newGpq();
        c.mov(v, gpr_ptr(idx));
        return v;
    }

    void set_gpr(u32 idx, asmjit::x86::Gp vreg) const { c.mov(gpr_ptr(idx), vreg); }

    void add(u32 rs, u32 rt, u32 rd) const {}

    void addi(u32 rs, u32 rt, s16 imm) const {}

    void addiu(u32 rs, u32 rt, s16 imm) const {}

    void addu(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr32(rs);
        asmjit::x86::Gp v1 = get_gpr32(rt);
        c.add(v0, v1);
        if constexpr (mips32) {
            set_gpr(rd, v0);
        } else {
            asmjit::x86::Gp v2 = c.newGpq();
            c.movsxd(v2, v0);
            set_gpr(rd, v2);
        }
    }

    void and_(u32 rs, u32 rt, u32 rd) const
    {
        asmjit::x86::Gp v0 = get_gpr(rs);
        asmjit::x86::Gp v1 = get_gpr(rt);
        c.and_(v0, v1);
        set_gpr(rd, v0);
    }

    void andi(u32 rs, u32 rt, u16 imm) const {}

    void beq(u32 rs, u32 rt, s16 imm) const {}

    void beql(u32 rs, u32 rt, s16 imm) const {}

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

    void daddu(u32 rs, u32 rt, u32 rd) const {}

    void div(u32 rs, u32 rt) const {}

    void divu(u32 rs, u32 rt) const {}

    void dsll(u32 rt, u32 rd, u32 sa) const {}

    void dsll32(u32 rt, u32 rd, u32 sa) const {}

    void dsllv(u32 rs, u32 rt, u32 rd) const {}

    void dsra(u32 rt, u32 rd, u32 sa) const {}

    void dsra32(u32 rt, u32 rd, u32 sa) const {}

    void dsrav(u32 rs, u32 rt, u32 rd) const {}

    void dsrl(u32 rt, u32 rd, u32 sa) const {}

    void dsrl32(u32 rt, u32 rd, u32 sa) const {}

    void dsrlv(u32 rs, u32 rt, u32 rd) const {}

    void dsub(u32 rs, u32 rt, u32 rd) const {}

    void dsubu(u32 rs, u32 rt, u32 rd) const {}

    void j(u32 instr) const {}

    void jal(u32 instr) const {}

    void jalr(u32 rs, u32 rd) const {}

    void jr(u32 rs) const {}

    void lui(u32 rt, s16 imm) const {}

    void mfhi(u32 rd) const {}

    void mflo(u32 rd) const {}

    void movn(u32 rs, u32 rt, u32 rd) const {}

    void movz(u32 rs, u32 rt, u32 rd) const {}

    void mthi(u32 rs) const {}

    void mtlo(u32 rs) const {}

    void mult(u32 rs, u32 rt) const {}

    void multu(u32 rs, u32 rt) const {}

    void nor(u32 rs, u32 rt, u32 rd) const {}

    void or_(u32 rs, u32 rt, u32 rd) const {}

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

    void xor_(u32 rs, u32 rt, u32 rd) const {}

    void xori(u32 rs, u32 rt, u16 imm) const {}
};

} // namespace mips

#pragma once

#include "cpu.hpp"
#include "host.hpp"

namespace mips {

template<typename GprInt, typename LoHiInt, std::integral PcInt, typename GprBaseInt = GprInt>
struct Interpreter : public Cpu<GprInt, LoHiInt, PcInt, GprBaseInt> {
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::Cpu;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::gpr;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::lo;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::hi;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::pc;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::in_branch_delay_slot;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::jump;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::link;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::integer_overflow_exception;
    using Cpu<GprInt, LoHiInt, PcInt, GprBaseInt>::trap_exception;

    void add(u32 rs, u32 rt, u32 rd) const
    {
        s32 sum;
        bool overflow;
#if HAS_BUILTIN_ADD_OVERFLOW
        overflow = __builtin_add_overflow(s32(gpr[rs]), s32(gpr[rt]), &sum);
#else
        sum = s32(gpr[rs]) + s32(gpr[rt]);
        overflow = (s32(gpr[rs]) ^ sum) & (s32(gpr[rt]) ^ sum) & 0x8000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rd, sum);
        }
    }

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        s32 sum;
        bool overflow;
#if HAS_BUILTIN_ADD_OVERFLOW
        overflow = __builtin_add_overflow(s32(gpr[rs]), imm, &sum);
#else
        sum = s32(gpr[rs]) + imm;
        overflow = (s32(gpr[rs]) ^ sum) & (imm ^ sum) & 0x8000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rt, sum);
        }
    }

    void addiu(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, s32(gpr[rs]) + imm); }

    void addu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(gpr[rs]) + s32(gpr[rt])); }

    void and_(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rs]) & s64(gpr[rt])); }

    void andi(u32 rs, u32 rt, u16 imm) const { gpr.set(rt, s64(gpr[rs]) & imm); }

    void beq(u32 rs, u32 rt, s16 imm) const
    {
        if (s64(gpr[rs]) == s64(gpr[rt])) {
            jump(pc + (imm << 2));
        }
    }

    void beql(u32 rs, u32 rt, s16 imm) const
    {
        if (s64(gpr[rs]) == s64(gpr[rt])) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bgez(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) >= 0) {
            jump(pc + (imm << 2));
        }
    }

    void bgezal(u32 rs, s16 imm) const
    {
        link(31);
        if (s64(gpr[rs]) >= 0) {
            jump(pc + (imm << 2));
        }
    }

    void bgezall(u32 rs, s16 imm) const
    {
        link(31);
        if (s64(gpr[rs]) >= 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bgezl(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) >= 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bgtz(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) > 0) {
            jump(pc + (imm << 2));
        }
    }

    void bgtzl(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) > 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void blez(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) <= 0) {
            jump(pc + (imm << 2));
        }
    }

    void blezl(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) <= 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bltz(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) < 0) {
            jump(pc + (imm << 2));
        }
    }

    void bltzal(u32 rs, s16 imm) const
    {
        link(31);
        if (s64(gpr[rs]) < 0) {
            jump(pc + (imm << 2));
        }
    }

    void bltzall(u32 rs, s16 imm) const
    {
        link(31);
        if (s64(gpr[rs]) < 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bltzl(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) < 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bne(u32 rs, u32 rt, s16 imm) const
    {
        if (s64(gpr[rs]) != s64(gpr[rt])) {
            jump(pc + (imm << 2));
        }
    }

    void bnel(u32 rs, u32 rt, s16 imm) const
    {
        if (s64(gpr[rs]) != s64(gpr[rt])) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void dadd(u32 rs, u32 rt, u32 rd) const
    {
        s64 sum;
        bool overflow;
#if HAS_BUILTIN_ADD_OVERFLOW
        overflow = __builtin_add_overflow(s64(gpr[rs]), s64(gpr[rt]), &sum);
#else
        sum = s64(gpr[rs]) + s64(gpr[rt]);
        overflow = (s64(gpr[rs]) ^ sum) & (s64(gpr[rt]) ^ sum) & 0x8000'0000'0000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rd, sum);
        }
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        s64 sum;
        bool overflow;
#if HAS_BUILTIN_ADD_OVERFLOW
        overflow = __builtin_add_overflow(s64(gpr[rs]), imm, &sum);
#else
        sum = s64(gpr[rs]) + imm;
        overflow = (s64(gpr[rs]) ^ sum) & (imm ^ sum) & 0x8000'0000'0000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rt, sum);
        }
    }

    void daddiu(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, s64(gpr[rs]) + imm); }

    void daddu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rs]) + s64(gpr[rt])); }

    void div(u32 rs, u32 rt) const
    {
        s32 op1 = s32(gpr[rs]);
        s32 op2 = s32(gpr[rt]);
        if (op2 == 0) {
            lo = op1 >= 0 ? -1 : 1;
            hi = op1;
        } else if (op1 == std::numeric_limits<s32>::min() && op2 == -1) {
            lo = std::numeric_limits<s32>::min();
            hi = 0;
        } else [[likely]] {
            lo = op1 / op2;
            hi = op1 % op2;
        }
    }

    void divu(u32 rs, u32 rt) const
    {
        u32 op1 = u32(gpr[rs]);
        u32 op2 = u32(gpr[rt]);
        if (op2 == 0) {
            lo = -1;
            hi = s32(op1);
        } else {
            lo = s32(op1 / op2);
            hi = s32(op1 % op2);
        }
    }

    void dsll(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s64(gpr[rt]) << sa); }

    void dsll32(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s64(gpr[rt]) << (sa + 32)); }

    void dsllv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rt]) << (s32(gpr[rs]) & 63)); }

    void dsra(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s64(gpr[rt]) >> sa); }

    void dsra32(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s64(gpr[rt]) >> (sa + 32)); }

    void dsrav(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rt]) >> (s32(gpr[rs]) & 63)); }

    void dsrl(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, u64(gpr[rt]) >> sa); }

    void dsrl32(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, u64(gpr[rt]) >> (sa + 32)); }

    void dsrlv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, u64(gpr[rt]) >> (s32(gpr[rs]) & 63)); }

    void dsub(u32 rs, u32 rt, u32 rd) const
    {
        s64 sum;
        bool overflow;
#if HAS_BUILTIN_SUB_OVERFLOW
        overflow = __builtin_sub_overflow(s64(gpr[rs]), s64(gpr[rt]), &sum);
#else
        sum = s64(gpr[rs]) - s64(gpr[rt]);
        overflow = (s64(gpr[rs]) ^ s64(gpr[rt])) & ~(s64(gpr[rt]) ^ sum) & 0x8000'0000'0000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rd, sum);
        }
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rs]) - s64(gpr[rt])); }

    void j(u32 instr) const
    {
        if (!in_branch_delay_slot) {
            jump(pc & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        }
    }

    void jal(u32 instr) const
    {
        if (!in_branch_delay_slot) {
            jump(pc & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        }
        link(31);
    }

    void jalr(u32 rs, u32 rd) const
    {
        if (!in_branch_delay_slot) {
            jump(gpr[rs]);
        }
        link(rd);
    }

    void jr(u32 rs) const
    {
        if (!in_branch_delay_slot) {
            jump(gpr[rs]);
        }
    }

    void lui(u32 rt, s16 imm) const { gpr.set(rt, imm << 16); }

    void mfhi(u32 rd) const { gpr.set(rd, hi); }

    void mflo(u32 rd) const { gpr.set(rd, lo); }

    void movn(u32 rs, u32 rt, u32 rd) const
    {
        if (gpr[rt]) {
            gpr.set(rd, s64(gpr[rs]));
        }
    }

    void movz(u32 rs, u32 rt, u32 rd) const
    {
        if (!gpr[rt]) {
            gpr.set(rd, s64(gpr[rs]));
        }
    }

    void mthi(u32 rs) const { hi = s64(gpr[rs]); }

    void mtlo(u32 rs) const { lo = s64(gpr[rs]); }

    void mult(u32 rs, u32 rt) const
    {
        s64 prod = s64(s32(gpr[rs])) * s64(s32(gpr[rt]));
        lo = s32(prod);
        hi = prod >> 32;
    }

    void multu(u32 rs, u32 rt) const
    {
        u64 prod = u64(u32(gpr[rs])) * u64(u32(gpr[rt]));
        lo = s32(prod);
        hi = s32(prod >> 32);
    }

    void nor(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, ~(s64(gpr[rs]) | s64(gpr[rt]))); }

    void or_(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rs]) | s64(gpr[rt])); }

    void ori(u32 rs, u32 rt, u16 imm) const { gpr.set(rt, s64(gpr[rs]) | imm); }

    void sll(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s32(gpr[rt]) << sa); }

    void sllv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(gpr[rt]) << (s32(gpr[rs]) & 31)); }

    void slt(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rs]) < s64(gpr[rt])); }

    void slti(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, s64(gpr[rs]) < imm); }

    void sltiu(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, u64(gpr[rs]) < u64(imm)); }

    void sltu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, u64(gpr[rs]) < u64(gpr[rt])); }

    void sra(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s32(gpr[rt] >> sa)); }

    void srav(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(gpr[rt] >> (s32(gpr[rs]) & 31))); }

    void srl(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s32(u32(gpr[rt]) >> sa)); }

    void srlv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(u32(gpr[rt]) >> (s32(gpr[rs]) & 31))); }

    void sub(u32 rs, u32 rt, u32 rd) const
    {
        s32 sum;
        bool overflow;
#if HAS_BUILTIN_SUB_OVERFLOW
        overflow = __builtin_sub_overflow(s32(gpr[rs]), s32(gpr[rt]), &sum);
#else
        sum = s32(gpr[rs]) - s32(gpr[rt]);
        overflow = (s32(gpr[rs]) ^ s32(gpr[rt])) & ~(s32(gpr[rt]) ^ sum) & 0x8000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rd, sum);
        }
    }

    void subu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(gpr[rs]) - s32(gpr[rt])); }

    void teq(u32 rs, u32 rt) const
    {
        if (s64(gpr[rs]) == s64(gpr[rt])) {
            trap_exception();
        }
    }

    void teqi(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) == imm) {
            trap_exception();
        }
    }

    void tge(u32 rs, u32 rt) const
    {
        if (s64(gpr[rs]) >= s64(gpr[rt])) {
            trap_exception();
        }
    }

    void tgei(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) >= imm) {
            trap_exception();
        }
    }

    void tgeu(u32 rs, u32 rt) const
    {
        if (u64(gpr[rs]) >= u64(gpr[rt])) {
            trap_exception();
        }
    }

    void tgeiu(u32 rs, s16 imm) const
    {
        if (u64(gpr[rs]) >= u64(imm)) {
            trap_exception();
        }
    }

    void tlt(u32 rs, u32 rt) const
    {
        if (s64(gpr[rs]) < s64(gpr[rt])) {
            trap_exception();
        }
    }

    void tlti(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) < imm) {
            trap_exception();
        }
    }

    void tltu(u32 rs, u32 rt) const
    {
        if (u64(gpr[rs]) < u64(gpr[rt])) {
            trap_exception();
        }
    }

    void tltiu(u32 rs, s16 imm) const
    {
        if (u64(gpr[rs]) < u64(imm)) {
            trap_exception();
        }
    }

    void tne(u32 rs, u32 rt) const
    {
        if (s64(gpr[rs]) != s64(gpr[rt])) {
            trap_exception();
        }
    }

    void tnei(u32 rs, s16 imm) const
    {
        if (s64(gpr[rs]) != imm) {
            trap_exception();
        }
    }

    void xor_(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s64(gpr[rs]) ^ s64(gpr[rt])); }

    void xori(u32 rs, u32 rt, u16 imm) const { gpr.set(rt, s64(gpr[rs]) ^ imm); }
};

} // namespace mips

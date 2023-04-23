#pragma once

#include "cpu.hpp"
#include "host.hpp"

namespace mips {

template<std::signed_integral GprInt, std::signed_integral LoHiInt, std::integral PcInt>
struct Interpreter : public Cpu<GprInt, LoHiInt, PcInt> {
    using Cpu<GprInt, LoHiInt, PcInt>::Cpu;
    using Cpu<GprInt, LoHiInt, PcInt>::gpr;
    using Cpu<GprInt, LoHiInt, PcInt>::lo;
    using Cpu<GprInt, LoHiInt, PcInt>::hi;
    using Cpu<GprInt, LoHiInt, PcInt>::pc;
    using Cpu<GprInt, LoHiInt, PcInt>::jump;
    using Cpu<GprInt, LoHiInt, PcInt>::integer_overflow_exception;
    using Cpu<GprInt, LoHiInt, PcInt>::trap_exception;

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

    void and_(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rs] & gpr[rt]); }

    void andi(u32 rs, u32 rt, u16 imm) const { gpr.set(rt, gpr[rs] & imm); }

    void beq(u32 rs, u32 rt, s16 imm) const
    {
        if (gpr[rs] == gpr[rt]) {
            jump(pc + (imm << 2));
        }
    }

    void beql(u32 rs, u32 rt, s16 imm) const
    {
        if (gpr[rs] == gpr[rt]) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bgez(u32 rs, s16 imm) const
    {
        if (gpr[rs] >= 0) {
            jump(pc + (imm << 2));
        }
    }

    void bgezal(u32 rs, s16 imm) const
    {
        if (gpr[rs] >= 0) {
            jump(pc + (imm << 2));
        }
        gpr.set(31, pc + 4);
    }

    void bgezall(u32 rs, s16 imm) const
    {
        if (gpr[rs] >= 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
        gpr.set(31, pc + 4);
    }

    void bgezl(u32 rs, s16 imm) const
    {
        if (gpr[rs] >= 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bgtz(u32 rs, s16 imm) const
    {
        if (gpr[rs] > 0) {
            jump(pc + (imm << 2));
        }
    }

    void bgtzl(u32 rs, s16 imm) const
    {
        if (gpr[rs] > 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void blez(u32 rs, s16 imm) const
    {
        if (gpr[rs] <= 0) {
            jump(pc + (imm << 2));
        }
    }

    void blezl(u32 rs, s16 imm) const
    {
        if (gpr[rs] <= 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bltz(u32 rs, s16 imm) const
    {
        if (gpr[rs] < 0) {
            jump(pc + (imm << 2));
        }
    }

    void bltzal(u32 rs, s16 imm) const
    {
        if (gpr[rs] < 0) {
            jump(pc + (imm << 2));
        }
        gpr.set(31, pc + 4);
    }

    void bltzall(u32 rs, s16 imm) const
    {
        if (gpr[rs] < 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bltzl(u32 rs, s16 imm) const
    {
        if (gpr[rs] < 0) {
            jump(pc + (imm << 2));
        } else {
            pc += 4;
        }
    }

    void bne(u32 rs, u32 rt, s16 imm) const
    {
        if (gpr[rs] != gpr[rt]) {
            jump(pc + (imm << 2));
        }
    }

    void bnel(u32 rs, u32 rt, s16 imm) const
    {
        if (gpr[rs] != gpr[rt]) {
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
        overflow = __builtin_add_overflow(gpr[rs], gpr[rt], &sum);
#else
        sum = gpr[rs] + gpr[rt];
        overflow = (gpr[rs] ^ sum) & (gpr[rt] ^ sum) & 0x8000'0000'0000'0000;
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
        overflow = __builtin_add_overflow(gpr[rs], imm, &sum);
#else
        sum = gpr[rs] + imm;
        overflow = (gpr[rs] ^ sum) & (imm ^ sum) & 0x8000'0000'0000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rt, sum);
        }
    }

    void daddiu(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, gpr[rs] + imm); }

    void daddu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rs] + gpr[rt]); }

    void dsll(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, gpr[rt] << sa); }

    void dsll32(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, gpr[rt] << (sa + 32)); }

    void dsllv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rt] << (gpr[rs] & 63)); }

    void dsra(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, gpr[rt] >> sa); }

    void dsra32(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, gpr[rt] >> (sa + 32)); }

    void dsrav(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rt] >> (gpr[rs] & 63)); }

    void dsrl(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, u64(gpr[rt]) >> sa); }

    void dsrl32(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, u64(gpr[rt]) >> (sa + 32)); }

    void dsrlv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, u64(gpr[rt]) >> (gpr[rs] & 63)); }

    void dsub(u32 rs, u32 rt, u32 rd) const
    {
        s64 sum;
        bool overflow;
#if HAS_BUILTIN_SUB_OVERFLOW
        overflow = __builtin_sub_overflow(gpr[rs], gpr[rt], &sum);
#else
        sum = gpr[rs] - gpr[rt];
        overflow = (gpr[rs] ^ gpr[rt]) & ~(gpr[rt] ^ sum) & 0x8000'0000'0000'0000;
#endif
        if (overflow) {
            integer_overflow_exception();
        } else {
            gpr.set(rd, sum);
        }
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rs] - gpr[rt]); }

    void j(u32 instr) const { jump(pc & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF); }

    void jal(u32 instr) const
    {
        jump(pc & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        gpr.set(31, pc + 4);
    }

    void jalr(u32 rs, u32 rd) const
    {
        jump(gpr[rs]);
        gpr.set(rd, pc + 4);
    }

    void jr(u32 rs) const { jump(gpr[rs]); }

    void lui(u32 rt, s16 imm) const { gpr.set(rt, imm << 16); }

    void mfhi(u32 rd) const { gpr.set(rd, hi); }

    void mflo(u32 rd) const { gpr.set(rd, lo); }

    void movn(u32 rs, u32 rt, u32 rd) const
    {
        if (gpr[rt]) {
            gpr.set(rd, gpr[rs]);
        }
    }

    void movz(u32 rs, u32 rt, u32 rd) const
    {
        if (!gpr[rt]) {
            gpr.set(rd, gpr[rs]);
        }
    }

    void mthi(u32 rs) const { hi = gpr[rs]; }

    void mtlo(u32 rs) const { lo = gpr[rs]; }

    void nor(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, ~(gpr[rs] | gpr[rt])); }

    void or_(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rs] | gpr[rt]); }

    void ori(u32 rs, u32 rt, u16 imm) const { gpr.set(rt, gpr[rs] | imm); }

    void sll(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s32(gpr[rt]) << sa); }

    void sllv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(gpr[rt]) << (gpr[rs] & 31)); }

    void slt(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rs] < gpr[rt]); }

    void slti(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, gpr[rs] < imm); }

    void sltiu(u32 rs, u32 rt, s16 imm) const { gpr.set(rt, u64(gpr[rs]) < u64(imm)); }

    void sltu(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, u64(gpr[rs]) < u64(gpr[rt])); }

    void sra(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s32(gpr[rt] >> sa)); }

    void srav(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(gpr[rt] >> (gpr[rs] & 31))); }

    void srl(u32 rt, u32 rd, u32 sa) const { gpr.set(rd, s32(u32(gpr[rt]) >> sa)); }

    void srlv(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, s32(u32(gpr[rt]) >> (gpr[rs] & 31))); }

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
        if (gpr[rs] == gpr[rt]) {
            trap_exception();
        }
    }

    void teqi(u32 rs, s16 imm) const
    {
        if (gpr[rs] == imm) {
            trap_exception();
        }
    }

    void tge(u32 rs, u32 rt) const
    {
        if (gpr[rs] >= gpr[rt]) {
            trap_exception();
        }
    }

    void tgei(u32 rs, s16 imm) const
    {
        if (gpr[rs] >= imm) {
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
        if (gpr[rs] < gpr[rt]) {
            trap_exception();
        }
    }

    void tlti(u32 rs, s16 imm) const
    {
        if (gpr[rs] < imm) {
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
        if (gpr[rs] != gpr[rt]) {
            trap_exception();
        }
    }

    void tnei(u32 rs, s16 imm) const
    {
        if (gpr[rs] != imm) {
            trap_exception();
        }
    }

    void xor_(u32 rs, u32 rt, u32 rd) const { gpr.set(rd, gpr[rs] ^ gpr[rt]); }

    void xori(u32 rs, u32 rt, u16 imm) const { gpr.set(rt, gpr[rs] ^ imm); }
};

} // namespace mips

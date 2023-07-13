#pragma once

#include "host.hpp"
#include "recompiler.hpp"

// WIP

namespace mips {

using namespace asmjit;
using namespace asmjit::a64;

template<std::signed_integral GprInt, std::integral PcInt, typename RegisterAllocator>
struct RecompilerA64 : public Recompiler<GprInt, PcInt, RegisterAllocator> {
    using Base = Recompiler<GprInt, PcInt, RegisterAllocator>;
    using Base::Base;
    using Base::block_epilog_with_jmp_and_pc_flush_and_pc_flush;
    using Base::branch_hit;
    using Base::branched;
    using Base::c;
    using Base::check_can_exec_dword_instr;
    using Base::get_hi_ptr;
    using Base::get_lo_ptr;
    using Base::GetDirtyGpr;
    using Base::GetDirtyGpr32;
    using Base::GetGpr;
    using Base::GetGpr32;
    using Base::indirect_jump;
    using Base::integer_overflow_exception;
    using Base::jit_pc;
    using Base::link;
    using Base::mips32;
    using Base::mips64;
    using Base::reg_alloc;
    using Base::take_branch;
    using Base::trap_exception;

    void add(u32 rs, u32 rt, u32 rd) const
    {
        Label l_exception = c.newLabel(), l_end = c.newLabel();
        Gpw hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.adds(w0, hs, ht);
        c.b_vs(l_exception);
        if (rd) {
            Gp hd = GetDirtyGpr(rd);
            mips32 ? c.mov(hd, w0) : c.sxtw(hd, w0);
        }
        c.b(l_end);
        c.bind(l_exception);
        block_epilog_with_jmp_and_pc_flush(integer_overflow_exception);
        c.bind(l_end);
    }

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        Label l_exception = c.newLabel(), l_end = c.newLabel();
        Gpw hs = GetGpr32(rs);
        if ((imm & 0xFFF) == imm) {
            c.adds(w0, hs, imm);
        } else {
            c.mov(w0, imm);
            c.adds(w0, hs, w0);
        }
        c.b_vs(l_exception);
        if (rt) {
            Gp ht = GetDirtyGpr(rt);
            mips32 ? c.mov(ht, w0) : c.sxtw(ht, w0);
        }
        c.b(l_end);
        c.bind(l_exception);
        block_epilog_with_jmp_and_pc_flush(integer_overflow_exception);
        c.bind(l_end);
    }

    void addiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpw ht = GetDirtyGpr32(rt), hs = GetGpr32(rs);
        if ((imm & 0xFFF) == imm) {
            c.add(ht, hs, imm);
        } else {
            c.mov(w0, imm);
            c.add(ht, hs, w0);
        }
        if constexpr (mips64) c.sxtw(ht.x(), ht);
    }

    void addu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.add(hd, hs, ht);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void and_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.and_(hd, hs, ht);
    }

    void andi(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr32(rd), hs = GetGpr32(rs);
        if ((imm & 0xFFF) == imm) {
            c.and_(ht, hs, imm);
        } else {
            Gp t = mips32 ? w0 : x0;
            c.mov(t, imm);
            c.and_(ht, hs, t);
        }
    }

    void beq(u32 rs, u32 rt, s16 imm) const { branch<Cond::Eq>(rs, rt, imm); }

    void bgez(u32 rs, s16 imm) const { branch<Cond::Ge>(rs, imm); }

    void bgezal(u32 rs, s16 imm) const { branch_and_link<Cond::Ge>(rs, imm); }

    void bgtz(u32 rs, s16 imm) const { branch<Cond::Gt>(rs, imm); }

    void blez(u32 rs, s16 imm) const { branch<Cond::Le>(rs, imm); }

    void bltz(u32 rs, s16 imm) const { branch<Cond::Lt>(rs, imm); }

    void bltzal(u32 rs, s16 imm) const { branch_and_link<Cond::Lt>(rs, imm); }

    void bne(u32 rs, u32 rt, s16 imm) const { branch<Cond::Ne>(rs, rt, imm); }

    void dadd(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_exception = c.newLabel(), l_end = c.newLabel();
        Gpx hs = GetGpr(rs), ht = GetGpr(rt);
        c.adds(x0, hs, ht);
        c.b_vs(l_exception);
        if (rd) c.mov(GetDirtyGpr(rd), x0);
        c.b(l_end);
        c.bind(l_exception);
        block_epilog_with_jmp_and_pc_flush(integer_overflow_exception);
        c.bind(l_end);
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_exception = c.newLabel(), l_end = c.newLabel();
        Gpx hs = GetGpr(rs);
        if ((imm & 0xFFF) == imm) {
            c.adds(x0, hs, imm);
        } else {
            c.mov(x0, imm);
            c.adds(x0, hs, x0);
        }
        c.b_vs(l_exception);
        if (rt) c.mov(GetDirtyGpr(rt), x0);
        c.b(l_end);
        c.bind(l_exception);
        block_epilog_with_jmp_and_pc_flush(integer_overflow_exception);
        c.bind(l_end);
    }

    void daddiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rt) return;
        Gpx ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if ((imm & 0xFFF) == imm) {
            c.add(ht, hs, imm);
        } else {
            c.mov(x0, imm);
            c.add(ht, hs, x0);
        }
    }

    void daddu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.add(hd, hs, ht);
    }

    void dsll(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        c.lsl(hd, ht, sa);
    }

    void dsll32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        c.lsl(hd, ht, sa + 32);
    }

    void dsllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.and_(x0, hs, 63);
        c.lsl(hd, ht, x0);
    }

    void dsra(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        c.asr(hd, ht, sa);
    }

    void dsra32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        c.asr(hd, ht, sa + 32);
    }

    void dsrav(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.and_(x0, hs, 63);
        c.asr(hd, ht, x0);
    }

    void dsrl(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        c.lsr(hd, ht, sa);
    }

    void dsrl32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        c.lsr(hd, ht, sa + 32);
    }

    void dsrlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.and_(x0, hs, 63);
        c.lsr(hd, ht, x0);
    }

    void dsub(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_exception = c.newLabel(), l_end = c.newLabel();
        Gpx hs = GetGpr(rs), ht = GetGpr(rt);
        c.subs(x0, hs, ht);
        c.b_vs(l_exception);
        if (rd) c.mov(GetDirtyGpr(rd), x0);
        c.b(l_end);
        c.bind(l_exception);
        block_epilog_with_jmp_and_pc_flush(integer_overflow_exception);
        c.bind(l_end);
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpx hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.sub(rd, rs, rt);
    }

    void j(u32 instr) const
    {
        take_branch((jit_pc + 4) & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        branch_hit = true;
    }

    void jal(u32 instr) const
    {
        take_branch((jit_pc + 4) & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        link(31);
        branch_hit = true;
    }

    void jalr(u32 rs, u32 rd) const
    {
        indirect_jump(GetGpr(rs));
        link(rd);
        branch_hit = true;
    }

    void jr(u32 rs) const
    {
        indirect_jump(GetGpr(rs));
        branch_hit = true;
    }

    void lui(u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt);
        c.mov(ht, imm << 16);
    }

    void mfhi(u32 rd) const
    {
        // if (rd) {
        //     c.mov(GetDirtyGpr(rd), GlobalVarPtr(hi));
        // }
    }

    void mflo(u32 rd) const
    {
        /*if (rd) {
            c.mov(GetDirtyGpr(rd), GlobalVarPtr(lo));
        }*/
    }

    void movn(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(ht, 0);
        // c.csel(hd, hs, );
    }

    void movz(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(ht, 0);
        // c.csel(hd, hs, );
    }

    void mthi(u32 rs) const
    { /*c.mov(GlobalVarPtr(hi), GetGpr(rs));*/
    }

    void mtlo(u32 rs) const
    { /*
        c.mov(GlobalVarPtr(lo), GetGpr(rs));*/
    }

    void nor(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.orr(hd, hs, ht);
        c.mvn(hd, hd);
    }

    void or_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.orr(hd, hs, ht);
    }

    void ori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if ((imm & 0xFFF) == imm) {
            c.orr(ht, hs, imm);
        } else {
            Gp t = mips32 ? w0 : x0;
            c.mov(t, imm);
            c.orr(ht, hs, t);
        }
    }

    void sll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        c.lsl(hd, ht, sa);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void sllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.and_(w0, hs, 31);
        c.lsl(hd, ht, w0);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void slt(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        c.cset(hd, arm::CondCode::kLT);
    }

    void slti(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if ((imm & 0xFFF) == imm) {
            c.cmp(hs, imm);
        } else {
            Gp t = mips32 ? w0 : x0;
            c.mov(t, imm);
            c.cmp(hs, t);
        }
        c.cset(hd, arm::CondCode::kLT);
    }

    void sltiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if ((imm & 0xFFF) == imm) {
            c.cmp(hs, imm);
        } else {
            Gp t = mips32 ? w0 : x0;
            c.mov(t, imm);
            c.cmp(hs, t);
        }
        c.cset(hd, arm::CondCode::kLO);
    }

    void sltu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        c.cset(hd, arm::CondCode::kLO);
    }

    void sra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        c.asr(hd, ht, sa);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void srav(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.and_(w0, hs, 31);
        c.asr(hd, ht, w0);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void srl(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        c.lsr(hd, ht, sa);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void srlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.and_(w0, hs, 31);
        c.lsr(hd, ht, w0);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
    }

    void sub(u32 rs, u32 rt, u32 rd) const
    {
        Label l_exception = c.newLabel(), l_end = c.newLabel();
        Gpw hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.subs(w0, hs, ht);
        c.b_vs(l_exception);
        if (rd) {
            Gp hd = GetDirtyGpr(rd);
            mips32 ? c.mov(hd, w0) : c.sxtw(hd, w0);
        }
        c.b(l_end);
        c.bind(l_exception);
        block_epilog_with_jmp_and_pc_flush(integer_overflow_exception);
        c.bind(l_end);
    }

    void subu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpw hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.sub(rd, rs, rt);
        if constexpr (mips64) c.sxtw(hd.x(), hd);
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
        Gp hd = GetDirtyGpr(rd);
        if (rs == rt) {
            c.mov(hd, 0);
        } else {
            Gp hs = GetGpr(rs), ht = GetGpr(rt);
            c.eor(hd, hs, ht);
        }
    }

    void xori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if ((imm & 0xFFF) == imm) {
            c.eor(ht, hs, imm);
        } else {
            Gp t = mips32 ? w0 : x0;
            c.mov(t, imm);
            c.eor(ht, hs, t);
        }
    }

protected:
    template<Cond cc> void branch(u32 rs, u32 rt, s16 imm) const
    {
        Label l_nobranch = c.newLabel();
        Gp hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        if constexpr (cc == Cond::Eq) c.jne(l_nobranch);
        if constexpr (cc == Cond::Ne) c.je(l_nobranch);
        take_branch(jit_pc + 4 + (imm << 2));
        c.bind(l_nobranch);
        branch_hit = true;
    }

    template<Cond cc> void branch(u32 rs, s16 imm) const
    {
        Label l_nobranch = c.newLabel();
        Gp hs = GetGpr(rs);
        c.test(hs, hs);
        if constexpr (cc == Cond::Ge) c.js(l_nobranch);
        if constexpr (cc == Cond::Gt) c.jle(l_nobranch);
        if constexpr (cc == Cond::Le) c.jg(l_nobranch);
        if constexpr (cc == Cond::Lt) c.jns(l_nobranch);
        take_branch(jit_pc + 4 + (imm << 2));
        c.bind(l_nobranch);
        branch_hit = true;
    }

    template<Cond cc> void branch_and_link(auto... args) const
    {
        branch<cc>(args...);
        link(31);
    }

    template<Cond cc>
    void trap(u32 rs, auto rt_or_imm) const
        requires(std::same_as<decltype(rt_or_imm), s16> || std::same_as<decltype(rt_or_imm), u32>)
    {
        Label l_end = c.newLabel();
        if constexpr (sizeof(rt_or_imm) == 2) c.cmp(GetGpr(rs), rt_or_imm);
        else c.cmp(GetGpr(rs), GetGpr(rt_or_imm));
        if constexpr (cc == Cond::Eq) c.jne(l_end);
        if constexpr (cc == Cond::Ge) c.jl(l_end);
        if constexpr (cc == Cond::Geu) c.jb(l_end);
        if constexpr (cc == Cond::Lt) c.jge(l_end);
        if constexpr (cc == Cond::Ltu) c.jae(l_end);
        if constexpr (cc == Cond::Ne) c.je(l_end);
        block_epilog_with_jmp_and_pc_flush(trap_exception);
        c.bind(l_end);
        branched = true;
    }
};

} // namespace mips

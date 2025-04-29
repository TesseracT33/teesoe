#pragma once

#include "platform.hpp"
#include "recompiler.hpp"

namespace mips {

using namespace asmjit;
using namespace asmjit::x86;

template<std::signed_integral GprInt, std::integral PcInt, typename RegisterAllocator>
struct RecompilerX64 : public Recompiler<GprInt, PcInt, RegisterAllocator> {
    using Base = Recompiler<GprInt, PcInt, RegisterAllocator>;
    using Base::Base;
    using Base::block_epilog_with_pc_flush_and_jmp;
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
    using Base::last_instr_was_branch;
    using Base::link;
    using Base::mips32;
    using Base::mips64;
    using Base::reg_alloc;
    using Base::take_branch;
    using Base::trap_exception;

    void add(u32 rs, u32 rt, u32 rd) const
    {
        Label l_noexception = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.mov(eax, hs);
        c.add(eax, ht);
        c.jno(l_noexception);
        block_epilog_with_pc_flush_and_jmp((void*)integer_overflow_exception, 0);
        c.bind(l_noexception);
        if (rd) {
            Gp hd = GetDirtyGpr(rd);
            mips32 ? c.mov(hd, eax) : c.movsxd(hd, eax);
        }
    }

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();
        Gpd hs = GetGpr32(rs);
        c.mov(eax, hs);
        c.add(eax, imm);
        c.jno(l_noexception);
        block_epilog_with_pc_flush_and_jmp((void*)integer_overflow_exception, 0);
        c.bind(l_noexception);
        if (rt) {
            Gp ht = GetDirtyGpr(rt);
            mips32 ? c.mov(ht, eax) : c.movsxd(ht, eax);
        }
    }

    void addiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gpd ht = GetDirtyGpr32(rt), hs = GetGpr32(rs);
        c.lea(ht, ptr(hs, imm));
        if constexpr (mips64) c.movsxd(ht.r64(), ht);
    }

    void addu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.lea(hd, ptr(hs, ht));
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void and_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if (rt == rd) {
            c.and_(hd, hs);
        } else {
            if (rs != rd) c.mov(hd, hs);
            c.and_(hd, ht);
        }
    }

    void andi(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if (rs != rt) c.mov(ht, hs);
        c.and_(ht, imm);
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
        Label l_noexception = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.mov(rax, hs);
        c.add(rax, ht);
        c.jno(l_noexception);
        block_epilog_with_pc_flush_and_jmp((void*)integer_overflow_exception, 0);
        c.bind(l_noexception);
        if (rd) c.mov(GetDirtyGpr(rd), rax);
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_noexception = c.newLabel();
        Gpq hs = GetGpr(rs);
        c.mov(rax, hs);
        c.add(rax, imm);
        c.jno(l_noexception);
        block_epilog_with_pc_flush_and_jmp((void*)integer_overflow_exception, 0);
        c.bind(l_noexception);
        if (rt) c.mov(GetDirtyGpr(rt), rax);
    }

    void daddiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rt) return;
        Gpq ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.lea(ht, ptr(hs, imm));
    }

    void daddu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(hd, ptr(hs, ht));
    }

    void dsll(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        if (sa) c.shl(hd, sa);
    }

    void dsll32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        c.shl(hd, sa + 32);
    }

    void dsllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.shlx(hd, ht, hs);
    }

    void dsra(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        if (sa) c.sar(hd, sa);
    }

    void dsra32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        c.sar(hd, sa + 32);
    }

    void dsrav(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.sarx(hd, ht, hs);
    }

    void dsrl(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        if (sa) c.shr(hd, sa);
    }

    void dsrl32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        c.shr(hd, sa + 32);
    }

    void dsrlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.shrx(hd, ht, hs);
    }

    void dsub(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_noexception = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.mov(rax, hs);
        c.sub(rax, ht);
        c.jno(l_noexception);
        block_epilog_with_pc_flush_and_jmp((void*)integer_overflow_exception, 0);
        c.bind(l_noexception);
        if (rd) c.mov(GetDirtyGpr(rd), rax);
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if (rt == rd) {
            c.neg(hd);
            c.add(hd, hs);
        } else {
            if (rs != rd) c.mov(hd, hs);
            c.sub(hd, ht);
        }
    }

    void j(u32 instr) const
    {
        take_branch((jit_pc + 4) & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        last_instr_was_branch = true;
    }

    void jal(u32 instr) const
    {
        take_branch((jit_pc + 4) & ~PcInt(0xFFF'FFFF) | instr << 2 & 0xFFF'FFFF);
        link(31);
        last_instr_was_branch = true;
    }

    void jalr(u32 rs, u32 rd) const
    {
        indirect_jump(GetGpr(rs).r64());
        link(rd);
        last_instr_was_branch = true;
    }

    void jr(u32 rs) const
    {
        indirect_jump(GetGpr(rs).r64());
        last_instr_was_branch = true;
    }

    void lui(u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt);
        imm ? c.mov(ht, imm << 16) : c.xor_(ht.r32(), ht.r32());
    }

    void mfhi(u32 rd) const
    {
        if (rd) {
            c.mov(GetDirtyGpr(rd), get_hi_ptr());
        }
    }

    void mflo(u32 rd) const
    {
        if (rd) {
            c.mov(GetDirtyGpr(rd), get_lo_ptr());
        }
    }

    void movn(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.test(ht, ht);
        c.cmovne(hd, hs);
    }

    void movz(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.test(ht, ht);
        c.cmove(hd, hs);
    }

    void mthi(u32 rs) const { c.mov(get_hi_ptr(), GetGpr(rs)); }

    void mtlo(u32 rs) const { c.mov(get_lo_ptr(), GetGpr(rs)); }

    void nor(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if (rs != rd) c.mov(hd, hs);
        c.or_(hd, ht);

        c.not_(hd);
    }

    void or_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if (rs != rd) c.mov(hd, hs);
        c.or_(hd, ht);
    }

    void ori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if (rs != rt) c.mov(ht, hs);
        if (imm) c.or_(ht, imm);
    }

    void sll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        if (rt != rd) c.mov(hd, ht);
        if (sa) c.shl(hd, sa);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void sllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.shlx(hd, ht, hs);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void slt(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        c.setl(hd.r8());
        c.and_(hd.r32(), 1);
    }

    void slti(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.cmp(hs, imm);
        c.setl(ht.r8());
        c.and_(ht.r32(), 1);
    }

    void sltiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.cmp(hs, imm);
        c.setb(ht.r8());
        c.and_(ht.r32(), 1);
    }

    void sltu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        c.setb(hd.r8());
        c.and_(hd.r32(), 1);
    }

    void sra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), ht = GetGpr(rt);
        if (rt != rd) c.mov(hd, ht);
        if (sa) c.sar(hd, sa);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void srav(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if constexpr (mips32) {
            c.sarx(hd, ht, hs);
        } else {
            c.mov(eax, hs.r32());
            c.and_(al, 31);
            c.sarx(hd, ht, rax);
            c.movsxd(hd, hd.r32());
        }
    }

    void srl(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        if (rt != rd) c.mov(hd, ht); // TODO: on mips64 only, move into eax
        if (sa) c.shr(hd, sa);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
        // if constexpr (mips64) {
        //     if (rt != rd) {
        //         c.mov(eax, ht);
        //         c.shr(eax, sa);
        //         c.movsxd(hd.r64(), eax);
        //     } else {
        //         c.shr(hd, sa);
        //         c.movsxd(hd.r64(), rd);
        //     }
        // }
    }

    void srlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.shrx(hd, ht, hs);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void sub(u32 rs, u32 rt, u32 rd) const
    {
        Label l_noexception = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.mov(eax, hs);
        c.sub(eax, ht);
        c.jno(l_noexception);
        block_epilog_with_pc_flush_and_jmp((void*)integer_overflow_exception, 0);
        c.bind(l_noexception);
        if (rd) {
            Gp hd = GetDirtyGpr(rd);
            mips32 ? c.mov(hd, eax) : c.movsxd(hd, eax);
        }
    }

    void subu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        if (rt == rd) {
            c.neg(hd);
            c.add(hd, hs);
        } else {
            if (rs != rd) c.mov(hd, hs);
            c.sub(hd, ht);
        }
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
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
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if (rs != rd) c.mov(hd, hs);
        c.xor_(hd, ht);
    }

    void xori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if (rs != rt) c.mov(ht, hs);
        c.xor_(ht, imm);
    }

protected:
    template<Cond cc> void branch(u32 rs, u32 rt, s16 imm) const
    {
        Label l_nobranch = c.newLabel();
        if (!rs) {
            Gp ht = GetGpr(rt);
            c.test(ht, ht);
        } else if (!rt) {
            Gp hs = GetGpr(rs);
            c.test(hs, hs);
        } else {
            Gp hs = GetGpr(rs), ht = GetGpr(rt);
            c.cmp(hs, ht);
        }
        if constexpr (cc == Cond::Eq) c.jne(l_nobranch);
        if constexpr (cc == Cond::Ne) c.je(l_nobranch);
        take_branch(jit_pc + 4 + (imm << 2));
        c.bind(l_nobranch);
        last_instr_was_branch = true;
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
        last_instr_was_branch = true;
    }

    template<Cond cc> void branch_and_link(auto... args) const
    {
        branch<cc>(args...);
        link(31);
    }

    template<Cond cc> void trap(u32 rs, s16 imm) const
    {
        Label l_notrap = c.newLabel();
        Gp hs = GetGpr(rs);
        c.cmp(hs, imm);
        if constexpr (cc == Cond::Eq) c.jne(l_notrap);
        if constexpr (cc == Cond::Ge) c.jl(l_notrap);
        if constexpr (cc == Cond::Geu) c.jb(l_notrap);
        if constexpr (cc == Cond::Lt) c.jge(l_notrap);
        if constexpr (cc == Cond::Ltu) c.jae(l_notrap);
        if constexpr (cc == Cond::Ne) c.je(l_notrap);
        block_epilog_with_pc_flush_and_jmp((void*)trap_exception, 0);
        c.bind(l_notrap);
        branched = true;
    }

    template<Cond cc> void trap(u32 rs, u32 rt) const
    {
        Label l_notrap = c.newLabel();
        Gp hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        if constexpr (cc == Cond::Eq) c.jne(l_notrap);
        if constexpr (cc == Cond::Ge) c.jl(l_notrap);
        if constexpr (cc == Cond::Geu) c.jb(l_notrap);
        if constexpr (cc == Cond::Lt) c.jge(l_notrap);
        if constexpr (cc == Cond::Ltu) c.jae(l_notrap);
        if constexpr (cc == Cond::Ne) c.je(l_notrap);
        block_epilog_with_pc_flush_and_jmp((void*)trap_exception, 0);
        c.bind(l_notrap);
        branched = true;
    }
};

} // namespace mips

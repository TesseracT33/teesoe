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
        Label l_no_ov = c.newLabel();
        c.mov(eax, GprPtr32(rs));
        if (rs == rt) {
            c.add(eax, eax);
        } else {
            c.add(eax, GprPtr32(rt));
        }
        c.jno(l_no_ov);
        block_epilog_with_pc_flush_and_jmp(integer_overflow_exception, 0);
        c.bind(l_no_ov);
        if (rd) {
            c.cdqe(rax);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        Label l_no_ov = c.newLabel();
        c.mov(eax, GprPtr32(rs));
        if (imm == 1) {
            c.inc(eax);
        } else if (imm == -1) {
            c.dec(eax);
        } else {
            c.add(eax, imm);
        }
        c.jno(l_no_ov);
        block_epilog_with_pc_flush_and_jmp(integer_overflow_exception, 0);
        c.bind(l_no_ov);
        if (rt) {
            c.cdqe(rax);
            c.mov(GprPtr64(rt), rax);
        }
    }

    void addiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        c.mov(eax, GprPtr32(rs));
        if (imm == 1) {
            c.inc(eax);
        } else if (imm == -1) {
            c.dec(eax);
        } else {
            c.add(eax, imm);
        }
        c.cdqe(rax);
        c.mov(GprPtr64(rt), rax);
    }

    void addu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        c.mov(eax, GprPtr32(rs));
        if (rs == rt) {
            c.add(eax, eax);
        } else {
            c.add(eax, GprPtr32(rt));
        }
        c.cdqe(rax);
        c.mov(GprPtr64(rd), rax);

        Gpq hd = GetDirtyGpr64(rd);
        Gpd hs = GetGpr32(rs);
        if (GprIsLoaded(rt)) {
            Gpd ht = GetGpr32(rt);
            c.add(hs, ht);
        } else {
            c.add(hs, GprPtr32(rt));
        }
        c.movsxd(hd, hs);
    }

    void and_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        if (rd == rs) {
            c.mov(rax, GprPtr64(rt));
            c.and_(GprPtr64(rd), rax);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.and_(rax, GprPtr64(rt));
            c.mov(GprPtr64(rd), rax);
        }

        Gpq hd = GetDirtyGpr64(rd);
        Gpq hs = GetGpr64(rs);
        if (GprIsLoaded(rt)) {
            c.and_(hs, GetGpr64(rt));
        } else {
            c.and_(hs, GprPtr64(rt));
        }
        if (rd != rs) {
            c.mov(hd, hs);
        }
    }

    void andi(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        if (rs == rt) {
            c.and_(GprPtr64(rt), imm);
        } else {
            c.mov(eax, GprPtr32(rs));
            c.and_(eax, imm);
            c.mov(GprPtr64(rt), rax);
        }

        Gpd ht = GetDirtyGpr32(rt);
        if (rs == rt) {
            c.and_(ht, imm);
        } else {
            Gpd hs = GetGpr32(rs);
            c.and_(hs, imm);
            c.mov(ht, hs);
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
        Label l_no_ov = c.newLabel();
        c.mov(rax, GprPtr64(rs));
        if (rs == rt) {
            c.add(rax, rax);
        } else {
            c.add(rax, GprPtr64(rt));
        }
        c.jno(l_no_ov);
        block_epilog_with_pc_flush_and_jmp(integer_overflow_exception, 0);
        c.bind(l_no_ov);
        if (rd) {
            c.mov(GprPtr64(rd), rax);
        }
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_no_ov = c.newLabel();
        c.mov(rax, GprPtr64(rs));
        c.add(rax, imm);
        c.jno(l_no_ov);
        block_epilog_with_pc_flush_and_jmp(integer_overflow_exception, 0);
        c.bind(l_no_ov);
        if (rt) {
            c.mov(GprPtr64(rt), rax);
        }
    }

    void daddiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rt || !imm) return;
        if (rs == rt) {
            if (imm == 1) {
                c.inc(GprPtr64(rt));
            } else if (imm == -1) {
                c.dec(GprPtr64(rt));
            } else {
                c.add(GprPtr64(rt), imm);
            }
        } else {
            c.mov(rax, GprPtr64(rs));
            c.add(rax, imm);
            c.mov(GprPtr64(rt), rax);
        }
    }

    void daddu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rs) {
            if (rs == rt) {
                c.shl(GprPtr64(rd), 1);
            } else {
                c.mov(rax, GprPtr64(rt));
                c.add(GprPtr64(rd), rax);
            }
        } else {
            c.mov(rax, GprPtr64(rs));
            if (rs == rt) {
                c.add(rax, rax);
            } else {
                c.add(rax, GprPtr64(rt));
            }
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsll(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rt) {
            if (sa) {
                c.shl(GprPtr64(rd), sa);
            }
        } else {
            c.mov(rax, GprPtr64(rt));
            if (sa) {
                c.shl(rax, sa);
            }
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsll32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rt) {
            c.shl(GprPtr64(rd), sa + 32);
        } else {
            c.mov(eax, GprPtr32(rt));
            c.shl(rax, sa + 32);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        c.mov(ecx, GprPtr32(rs));
        if (rd == rt) {
            c.shl(GprPtr64(rd), cl);
        } else {
            c.shlx(rax, GprPtr64(rt), rcx);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsra(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rt) {
            if (sa) {
                c.sar(GprPtr64(rd), sa);
            }
        } else {
            c.mov(rax, GprPtr64(rt));
            if (sa) {
                c.sar(rax, sa);
            }
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsra32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rt) {
            c.sar(GprPtr64(rd), sa + 32);
        } else {
            c.mov(rax, GprPtr64(rt));
            c.sar(rax, sa + 32);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsrav(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        c.mov(ecx, GprPtr32(rs));
        if (rd == rt) {
            c.sar(GprPtr64(rd), cl);
        } else {
            c.sarx(rax, GprPtr64(rt), rcx);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsrl(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rt) {
            if (sa) {
                c.shr(GprPtr64(rd), sa);
            }
        } else {
            c.mov(rax, GprPtr64(rt));
            if (sa) {
                c.shr(rax, sa);
            }
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsrl32(u32 rt, u32 rd, u32 sa) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rt) {
            c.shr(GprPtr64(rd), sa + 32);
        } else {
            c.mov(rax, GprPtr64(rt));
            c.shr(rax, sa + 32);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsrlv(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        c.mov(ecx, GprPtr32(rs));
        if (rd == rt) {
            c.shr(GprPtr64(rd), cl);
        } else {
            c.shrx(rax, GprPtr64(rt), rcx);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsub(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_no_ov = c.newLabel();
        c.mov(rax, GprPtr64(rs));
        c.sub(rax, GprPtr64(rt));
        c.jno(l_no_ov);
        block_epilog_with_pc_flush_and_jmp(integer_overflow_exception, 0);
        c.bind(l_no_ov);
        if (rd) {
            c.mov(GprPtr64(rd), rax);
        }
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        if (rd == rs) {
            c.mov(rax, GprPtr64(rt));
            c.sub(GprPtr64(rd), rax);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.sub(rax, GprPtr64(rt));
            c.mov(GprPtr64(rd), rax);
        }
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
        c.mov(GprPtr64(rt), imm << 16);
    }

    void mfhi(u32 rd) const
    {
        if (rd) {
            c.mov(rax, get_hi_ptr());
            c.mov(GprPtr64(rd), rax);
        }
    }

    void mflo(u32 rd) const
    {
        if (rd) {
            c.mov(rax, get_lo_ptr());
            c.mov(GprPtr64(rd), rax);
        }
    }

    void mthi(u32 rs) const
    {
        c.mov(rax, GprPtr64(rs));
        c.mov(get_hi_ptr(), rax);
    }

    void mtlo(u32 rs) const
    {
        c.mov(rax, GprPtr64(rs));
        c.mov(get_lo_ptr(), rax);
    }

    void nor(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        c.mov(rax, GprPtr64(rs));
        c.or_(rax, GprPtr64(rt));
        c.not_(rax);
        c.mov(GprPtr64(rd), rax);
    }

    void or_(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        if (rd == rs) {
            c.mov(rax, GprPtr64(rt));
            c.or_(GprPtr64(rd), rax);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.or_(rax, GprPtr64(rt));
            c.mov(GprPtr64(rd), rax);
        }
    }

    void ori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        if (rs == rt) {
            c.or_(GprPtr32(rt), imm);
        } else {
            c.mov(eax, GprPtr32(rs));
            c.or_(eax, imm);
            c.mov(GprPtr32(rt), eax);
        }
    }

    void sll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        if (sa) {
            c.mov(eax, GprPtr32(rt));
            c.shl(eax, sa);
            c.cdqe(rax);
            c.mov(GprPtr64(rd), rax);
        } else {
            c.movsxd(rax, GprPtr32(rt));
            c.mov(GprPtr64(rd), rax);
        }
    }

    void sllv(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        c.mov(ecx, GprPtr32(rs));
        c.shlx(eax, GprPtr32(rt), ecx);
        c.cdqe(rax);
        c.mov(GprPtr64(rd), rax);
    }

    void slt(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        if (!rs) {
            c.cmp(GprPtr64(rt), 0);
            c.setg(al);
        } else if (!rt) {
            c.cmp(GprPtr64(rs), 0);
            c.setl(al);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.cmp(rax, GprPtr64(rt));
            c.setl(al);
        }
        c.and_(eax, 1);
        c.mov(GprPtr64(rd), rax);
    }

    void slti(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        c.cmp(GprPtr64(rs), imm);
        c.setl(al);
        c.and_(eax, 1);
        c.mov(GprPtr64(rd), rax);
    }

    void sltiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        c.cmp(GprPtr64(rs), imm);
        c.setb(al);
        c.and_(eax, 1);
        c.mov(GprPtr64(rd), rax);
    }

    void sltu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        if (!rs) {
            c.cmp(GprPtr64(rt), 0);
            c.seta(al);
        } else if (!rt) {
            c.cmp(GprPtr64(rs), 0);
            c.setb(al);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.cmp(rax, GprPtr64(rt));
            c.setb(al);
        }
        c.and_(eax, 1);
        c.mov(GprPtr64(rd), rax);
    }

    void sra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        if (sa) {
            c.mov(eax, GprPtr32(rt));
            c.sar(eax, sa);
            c.cdqe(rax);
            c.mov(GprPtr64(rd), rax);
        } else {
            c.movsxd(rax, GprPtr32(rt));
            c.mov(GprPtr64(rd), rax);
        }
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
        if (rt != rd) c.mov(hd, ht);
        if (sa) c.shr(hd, sa);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
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
        Label l_no_ov = c.newLabel();
        c.mov(eax, GprPtr32(rs));
        c.sub(eax, GprPtr32(rt));
        c.jno(l_no_ov);
        block_epilog_with_pc_flush_and_jmp(integer_overflow_exception, 0);
        c.bind(l_no_ov);
        if (rd) {
            c.cdqe(rax);
            c.mov(GprPtr64(rd), rax);
        }
    }

    void subu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        c.mov(eax, GprPtr32(rs));
        c.sub(eax, GprPtr32(rt));
        c.cdqe(rax);
        c.mov(GprPtr64(rd), rax);
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
        if (rd == rs) {
            c.mov(rax, GprPtr64(rt));
            c.xor_(GprPtr64(rd), rax);
        } else {
            c.mov(rax, GprPtr64(rs));
            c.xor_(rax, GprPtr64(rt));
            c.mov(GprPtr64(rd), rax);
        }
    }

    void xori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        if (rs == rt) {
            c.xor_(GprPtr32(rt), imm);
        } else {
            c.mov(eax, GprPtr32(rs));
            c.xor_(eax, imm);
            c.mov(GprPtr32(rt), eax);
        }
    }

protected:
    template<Cond cc> void branch(u32 rs, u32 rt, s16 imm) const
    {
        if (!rs && !rt) {
            if constexpr (cc == mips::Cond::Eq) take_branch(jit_pc + 4 + (imm << 2));
        } else {
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
        }
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
        block_epilog_with_pc_flush_and_jmp(trap_exception, 0);
        c.bind(l_end);
        branched = true;
    }
};

} // namespace mips

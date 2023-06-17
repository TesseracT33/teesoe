#pragma once

#include <bit>
#include <concepts>

#include "host.hpp"
#include "jit_util.hpp"
#include "mips/types.hpp"

namespace mips {

using namespace asmjit;
using namespace asmjit::x86;

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

template<std::signed_integral GprInt, std::signed_integral LoHiInt, std::integral PcInt, typename RegisterAllocator>
struct Recompiler {
    using BlockEpilogHandler = void (*)();
    using BlockEpilogWithJmpHandler = void (*)(void*);
    using CheckCanExecDwordInstrHandler = bool (*)();
    using ExceptionHandler = void (*)();
    using IndirectJumpHandler = void (*)(Gp target);
    using LinkHandler = void (*)(u32 reg);
    using TakeBranchHandler = void (*)(PcInt target);

    consteval Recompiler(AsmjitCompiler& compiler,
      RegisterAllocator& reg_alloc,
      LoHiInt& lo,
      LoHiInt& hi,
      PcInt& jit_pc,
      bool& branch_hit,
      bool& branched,
      TakeBranchHandler take_branch_handler,
      IndirectJumpHandler indirect_jump_handler,
      LinkHandler link_handler,
      BlockEpilogHandler block_epilog = nullptr, // only for cpus supporting exceptions
      BlockEpilogWithJmpHandler block_epilog_with_jmp = nullptr, // only for cpus supporting exceptions
      ExceptionHandler integer_overflow_exception = nullptr,
      ExceptionHandler trap_exception = nullptr,
      CheckCanExecDwordInstrHandler check_can_exec_dword_instr = nullptr) // MIPS64 only
      : c(compiler),
        reg_alloc(reg_alloc),
        lo(lo),
        hi(hi),
        jit_pc(jit_pc),
        branch_hit(branch_hit),
        branched(branched),
        take_branch(take_branch_handler),
        indirect_jump(indirect_jump_handler),
        link(link_handler),
        check_can_exec_dword_instr(check_can_exec_dword_instr),
        block_epilog(block_epilog),
        block_epilog_with_jmp(block_epilog_with_jmp),
        integer_overflow_exception(integer_overflow_exception),
        trap_exception(trap_exception)
    {
    }

    LoHiInt& lo;
    LoHiInt& hi;
    PcInt& jit_pc;
    AsmjitCompiler& c;
    RegisterAllocator& reg_alloc;
    bool& branch_hit;
    bool& branched;
    TakeBranchHandler const take_branch;
    IndirectJumpHandler const indirect_jump;
    LinkHandler const link;
    CheckCanExecDwordInstrHandler const check_can_exec_dword_instr;
    BlockEpilogHandler const block_epilog;
    BlockEpilogWithJmpHandler const block_epilog_with_jmp;
    ExceptionHandler const integer_overflow_exception, trap_exception;

    static constexpr bool mips32 = sizeof(GprInt) == 4;
    static constexpr bool mips64 = sizeof(GprInt) == 8;

    // rax can be used freely
    // rbx has been calle-saved and is used for $zero. Can be used a temporary, but must be zeroed afterwards
    // the remaining gprs, aside from rbp and rsi, are used by the register allocator.

    void add(u32 rs, u32 rt, u32 rd) const
    {
        Label l_noexception = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.lea(eax, ptr(hs, ht));
        c.jno(l_noexception);
        block_epilog_with_jmp(integer_overflow_exception);
        c.bind(l_noexception);
        if (rd) {
            Gp hd = GetDirtyGpr(rd);
            if constexpr (mips32) c.mov(hd, eax);
            else c.movsxd(hd, eax);
        }
    }

    void addi(u32 rs, u32 rt, s16 imm) const
    {
        Label l_noexception = c.newLabel();
        Gpd hs = GetGpr32(rs);
        c.lea(eax, ptr(hs, imm));
        c.jno(l_noexception);
        block_epilog_with_jmp(integer_overflow_exception);
        c.bind(l_noexception);
        if (rt) {
            Gp ht = GetDirtyGpr(rt);
            if constexpr (mips32) c.mov(ht, eax);
            else c.movsxd(ht, eax);
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
        Gp hd = GetDirtyGpr(rd);
        if (!rs || !rt) {
            c.xor_(hd.r32(), hd.r32());
        } else {
            Gp hs = GetGpr(rs), ht = GetGpr(rt);
            if (rs != rd) c.mov(hd, hs);
            c.and_(hd, ht);
        }
    }

    void andi(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt);
        if (!rs) {
            c.xor_(ht.r32(), ht.r32());
        } else {
            Gp hs = GetGpr(rs);
            if (rs != rt) c.mov(ht, hs);
            c.and_(ht, imm);
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
        Label l_noexception = c.newLabel();
        Gpq hs = GetGpr(rs), ht = GetGpr(rt);
        c.lea(rax, ptr(hs, ht));
        c.jno(l_noexception);
        block_epilog_with_jmp(integer_overflow_exception);
        c.bind(l_noexception);
        if (rd) c.mov(GetDirtyGpr(rd), rax);
    }

    void daddi(u32 rs, u32 rt, s16 imm) const
    {
        if (!check_can_exec_dword_instr()) return;
        Label l_noexception = c.newLabel();
        Gpq hs = GetGpr(rs);
        c.lea(rax, ptr(hs, imm));
        c.jno(l_noexception);
        block_epilog_with_jmp(integer_overflow_exception);
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
        c.shl(hd, sa);
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
        c.sar(hd, sa);
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
        c.shr(hd, sa);
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
        block_epilog_with_jmp(integer_overflow_exception);
        c.bind(l_noexception);
        if (rd) c.mov(GetDirtyGpr(rd), rax);
    }

    void dsubu(u32 rs, u32 rt, u32 rd) const
    {
        if (!check_can_exec_dword_instr()) return;
        if (!rd) return;
        Gpq hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        if (rs != rd) c.mov(hd, hs);
        c.sub(hd, ht);
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
        imm ? c.mov(ht, imm << 16) : c.xor_(ht.r32(), ht.r32());
    }

    void mfhi(u32 rd) const
    {
        if (rd) {
            if constexpr (mips32) {
                c.mov(eax, ptr(hi));
                c.mov(GetDirtyGpr(rd), eax);
            } else {
                c.mov(rax, ptr(hi));
                c.mov(GetDirtyGpr(rd), rax);
            }
        }
    }

    void mflo(u32 rd) const
    {
        if (rd) {
            if constexpr (mips32) {
                c.mov(eax, ptr(lo));
                c.mov(GetDirtyGpr(rd), eax);
            } else {
                c.mov(rax, ptr(lo));
                c.mov(GetDirtyGpr(rd), rax);
            }
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

    void mthi(u32 rs) const
    {
        if constexpr (mips32) {
            c.mov(eax, GetGpr(rs));
            c.mov(ptr(hi), eax);
        } else {
            c.mov(rax, GetGpr(rs));
            c.mov(ptr(hi), rax);
        }
    }

    void mtlo(u32 rs) const
    {
        if constexpr (mips32) {
            c.mov(eax, GetGpr(rs));
            c.mov(ptr(lo), eax);
        } else {
            c.mov(rax, GetGpr(rs));
            c.mov(ptr(lo), rax);
        }
    }

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
        c.or_(ht, imm);
    }

    void sll(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        if (rt != rd) c.mov(hd, ht);
        c.shl(hd, sa);
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
        c.setl(hd);
    }

    void slti(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.cmp(hs, imm);
        c.setl(ht);
    }

    void sltiu(u32 rs, u32 rt, s16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        c.cmp(hs, imm);
        c.setb(ht);
    }

    void sltu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gp hd = GetDirtyGpr(rd), hs = GetGpr(rs), ht = GetGpr(rt);
        c.cmp(hs, ht);
        c.setb(hd);
    }

    void sra(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        if (rt != rd) c.mov(hd, ht);
        c.sar(hd, sa);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void srav(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.sarx(hd, ht, hs);
        if constexpr (mips64) c.movsxd(hd.r64(), hd);
    }

    void srl(u32 rt, u32 rd, u32 sa) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), ht = GetGpr32(rt);
        if (rt != rd) c.mov(hd, ht);
        c.shr(hd, sa);
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
        Label l_noexception = c.newLabel();
        Gpd hs = GetGpr32(rs), ht = GetGpr32(rt);
        c.mov(eax, hs);
        c.sub(eax, ht);
        c.jno(l_noexception);
        block_epilog_with_jmp(integer_overflow_exception);
        c.bind(l_noexception);
        if (rd) {
            Gp hd = GetDirtyGpr(rd);
            if constexpr (mips32) c.mov(hd, eax);
            else c.movsxd(hd, eax);
        }
    }

    void subu(u32 rs, u32 rt, u32 rd) const
    {
        if (!rd) return;
        Gpd hd = GetDirtyGpr32(rd), hs = GetGpr32(rs), ht = GetGpr32(rt);
        if (rs != rd) c.mov(hd, hs);
        c.sub(hd, ht);
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
        Gp hd = GetDirtyGpr(rd);
        if (rs == rt) {
            c.xor_(hd.r32(), hd.r32());
        } else {
            Gp hs = GetGpr(rs), ht = GetGpr(rt);
            if (rs != rd) c.mov(hd, hs);
            c.xor_(hd, ht);
        }
    }

    void xori(u32 rs, u32 rt, u16 imm) const
    {
        if (!rt) return;
        Gp ht = GetDirtyGpr(rt), hs = GetGpr(rs);
        if (rs != rt) c.mov(ht, hs);
        c.xor_(ht, imm);
    }

protected:
    Gpd GetGpr32(u32 idx) const { return reg_alloc.GetHost(idx).r32(); }

    auto GetGpr(u32 idx) const
    {
        auto r = reg_alloc.GetHost(idx);
        if constexpr (mips32) {
            return r.r32();
        } else {
            return r.r64();
        }
    }

    Gpd GetDirtyGpr32(u32 idx) const { return reg_alloc.GetHostMarkDirty(idx).r32(); }

    auto GetDirtyGpr(u32 idx) const
    {
        auto r = reg_alloc.GetHostMarkDirty(idx);
        if constexpr (mips32) {
            return r.r32();
        } else {
            return r.r64();
        }
    }

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
        block_epilog_with_jmp(trap_exception);
        c.bind(l_end);
        branched = true;
    }
};

} // namespace mips

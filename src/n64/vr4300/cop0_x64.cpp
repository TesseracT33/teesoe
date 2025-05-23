#include "jit_common.hpp"
#include "vr4300/cache.hpp"
#include "vr4300/cop0.hpp"
#include "vr4300/exceptions.hpp"
#include "vr4300/interpreter.hpp"
#include "vr4300/recompiler.hpp"

#include <concepts>
#include <utility>

namespace n64::vr4300::x64 {

using namespace asmjit;
using namespace asmjit::x86;

static u32 ReadRandomJit();

void OnCop0Unusable()
{
    reg_alloc.DestroyVolatile(host_gpr_arg[0]);
    c.xor_(host_gpr_arg[0].r32(), host_gpr_arg[0].r32());
    BlockEpilogWithPcFlushAndJmp((void*)CoprocessorUnusableException);
    branched = true;
}

template<size_t size> void ReadCop0(Gpq dst, u32 idx)
{
    auto Read = [dst](auto const& cop0_reg) {
        if constexpr (size == 4) {
            c.movsxd(dst, JitPtr(cop0_reg, 4));
        } else if constexpr (sizeof(cop0_reg) == 4) {
            c.mov(dst.r32(), JitPtr(cop0_reg));
        } else {
            c.mov(dst, JitPtr(cop0_reg));
        }
    };

    switch (idx & 31) {
    case Cop0Reg::index: Read(cop0.index); break;
    case Cop0Reg::random:
        reg_alloc.Call((void*)ReadRandomJit);
        if constexpr (size == 4) {
            c.movsxd(dst, eax);
        } else {
            c.mov(dst.r32(), eax);
        }
        break;
    case Cop0Reg::entry_lo_0: Read(cop0.entry_lo[0]); break;
    case Cop0Reg::entry_lo_1: Read(cop0.entry_lo[1]); break;
    case Cop0Reg::context: Read(cop0.context); break;
    case Cop0Reg::page_mask: Read(cop0.page_mask); break;
    case Cop0Reg::wired: Read(cop0.wired); break;
    case Cop0Reg::bad_v_addr: Read(cop0.bad_v_addr); break;
    case Cop0Reg::count: /* See the declaration of 'count' */
        c.mov(rax, JitPtr(cop0.count));
        c.add(rax, block_cycles);
        c.shr(rax, 1);
        if constexpr (size == 4) {
            c.movsxd(dst, eax);
        } else {
            c.mov(dst.r32(), eax);
        }
        break;
    case Cop0Reg::entry_hi: Read(cop0.entry_hi); break;
    case Cop0Reg::compare: /* See the declaration of 'compare' */
        c.mov(rax, JitPtr(cop0.compare));
        c.shr(rax, 1);
        if constexpr (size == 4) {
            c.movsxd(dst, eax);
        } else {
            c.mov(dst.r32(), eax);
        }
        break;
    case Cop0Reg::status: Read(cop0.status); break;
    case Cop0Reg::cause: Read(cop0.cause); break;
    case Cop0Reg::epc: Read(cop0.epc); break;
    case Cop0Reg::pr_id: Read(cop0.pr_id); break;
    case Cop0Reg::config: Read(cop0.config); break;
    case Cop0Reg::ll_addr: Read(cop0.ll_addr); break;
    case Cop0Reg::watch_lo: Read(cop0.watch_lo); break;
    case Cop0Reg::watch_hi: Read(cop0.watch_hi); break;
    case Cop0Reg::x_context: Read(cop0.x_context); break;
    case Cop0Reg::parity_error: Read(cop0.parity_error); break;
    case Cop0Reg::cache_error: Read(cop0.cache_error); break;
    case Cop0Reg::tag_lo: Read(cop0.tag_lo); break;
    case Cop0Reg::tag_hi: Read(cop0.tag_hi); break;
    case Cop0Reg::error_epc: Read(cop0.error_epc); break;
    case 7:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 31: Read(last_cop0_write); break;
    default: c.xor_(dst.r32(), dst.r32());
    }
}

inline u32 ReadRandomJit()
{
    return random_generator.Generate();
}

template<size_t size> void WriteCop0(Gpq src, u32 idx)
{
    auto Write = [src](auto& cop0_reg) {
        if constexpr (sizeof(cop0_reg) == 4) {
            c.mov(JitPtr(cop0_reg), src.r32());
        } else if constexpr (size == 4) {
            c.movsxd(rax, src.r32());
            c.mov(JitPtr(cop0_reg), rax);
        } else {
            c.mov(JitPtr(cop0_reg), src);
        }
    };

    auto WriteMasked = [src](auto& cop0_reg, std::integral auto mask) {
        static_assert(sizeof(cop0_reg) == sizeof(mask));
        if constexpr (sizeof(cop0_reg) == 4) {
            c.mov(eax, src.r32());
            c.and_(eax, mask);
            c.and_(JitPtr(cop0_reg), ~mask);
            c.or_(JitPtr(cop0_reg), eax);
        } else {
            size == 8 ? c.mov(rax, src) : c.movsxd(rax, src.r32());
            c.push(rbx);
            c.mov(rbx, mask);
            c.and_(rax, rbx);
            c.not_(rbx);
            c.and_(JitPtr(cop0_reg), rbx);
            c.or_(JitPtr(cop0_reg), rax);
            c.pop(rbx);
        }
    };

    c.mov(JitPtr(last_cop0_write), src.r32());

    switch (idx & 31) {
    case Cop0Reg::index: WriteMasked(cop0.index, 0x8000'003F); break;
    case Cop0Reg::random: WriteMasked(cop0.random, 0x20); break;
    case Cop0Reg::entry_lo_0: WriteMasked(cop0.entry_lo[0], 0x3FFF'FFFF); break;
    case Cop0Reg::entry_lo_1: WriteMasked(cop0.entry_lo[1], 0x3FFF'FFFF); break;
    case Cop0Reg::context: WriteMasked(cop0.context, 0xFFFF'FFFF'FF80'0000); break;
    case Cop0Reg::page_mask: WriteMasked(cop0.page_mask, 0x01FF'E000); break;
    case Cop0Reg::wired:
        WriteMasked(cop0.wired, 0x3F);
        reg_alloc.Call((void*)OnWriteToWired);
        break;
    case Cop0Reg::bad_v_addr: break;
    case Cop0Reg::count:
        c.lea(rax, ptr(src, src));
        c.mov(JitPtr(cop0.count), rax);
        reg_alloc.Call((void*)OnWriteToCount);
        break;
    case Cop0Reg::entry_hi: WriteMasked(cop0.entry_hi, 0xC000'00FF'FFFF'E0FF); break;
    case Cop0Reg::compare: {
        Label l_noexception = c.newLabel();
        c.lea(rax, ptr(src, src));
        c.mov(JitPtr(cop0.compare), rax);
        FlushPc();
        reg_alloc.Call((void*)OnWriteToCompare);
        c.cmp(JitPtr(exception_occurred), 0);
        c.je(l_noexception);
        BlockEpilog();
        c.bind(l_noexception);
    } break;
    case Cop0Reg::status:
        branched = true;
        WriteMasked(cop0.status, 0xFF57'FFFF);
        BlockEpilogWithPcFlushAndJmp((void*)OnWriteToStatus, 4);
        break;
    case Cop0Reg::cause: {
        WriteMasked(cop0.cause, 0x300);
        Label l_noexception = c.newLabel();
        FlushPc();
        reg_alloc.Call((void*)OnWriteToCause);
        c.cmp(JitPtr(exception_occurred), 0);
        c.je(l_noexception);
        BlockEpilog();
        c.bind(l_noexception);
    } break;
    case Cop0Reg::epc: Write(cop0.epc); break;
    case Cop0Reg::config: WriteMasked(cop0.config, 0xF00'800F); break;
    case Cop0Reg::ll_addr: Write(cop0.ll_addr); break;
    case Cop0Reg::watch_lo: WriteMasked(cop0.watch_lo, 0xFFFF'FFFB); break;
    case Cop0Reg::watch_hi: Write(cop0.watch_hi); break;
    case Cop0Reg::x_context: WriteMasked(cop0.x_context, 0xFFFF'FFFE'0000'0000); break;
    case Cop0Reg::parity_error: WriteMasked(cop0.parity_error, 0xFF); break;
    case Cop0Reg::tag_lo: WriteMasked(cop0.tag_lo, 0x0FFF'FFC0); break;
    case Cop0Reg::error_epc: Write(cop0.error_epc); break;
    }
}

void cache(u32 rs, u32 rt, s16 imm)
{
    Label l_no_exception = c.newLabel();
    FlushPc();
    reg_alloc.FlushAll(); // flush nonvolatiles as well, since cache reads gpr[rs]
    Gpd arg0 = host_gpr_arg[0].r32(), arg1 = host_gpr_arg[1].r32(), arg2 = host_gpr_arg[2].r32();
    rs ? c.mov(arg0, rs) : c.xor_(arg0, arg0);
    rt ? c.mov(arg1, rt) : c.xor_(arg1, arg1);
    imm ? c.mov(arg2, imm) : c.xor_(arg2, arg2);
    jit_call_no_stack_alignment(c,
      (void*)vr4300::cache); // todo: this used instead of reg_alloc.Call, since we have args
    c.cmp(JitPtr(exception_occurred), 0);
    c.je(l_no_exception);
    BlockEpilog();

    c.bind(l_no_exception);
}

void dmfc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        Label l_noexception = c.newLabel();
        c.cmp(JitPtr(operating_mode), OperatingMode::Kernel);
        c.je(l_noexception);
        c.cmp(JitPtr(addressing_mode), AddressingMode::Dword);
        c.je(l_noexception);
        BlockEpilogWithPcFlushAndJmp((void*)ReservedInstructionException);

        c.bind(l_noexception);
        if (rt) {
            Gpq ht = reg_alloc.GetDirtyGpr(rt);
            ReadCop0<8>(ht, rd);
        }
    } else {
        OnCop0Unusable();
    }
}

void dmtc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        Label l_noexception = c.newLabel();
        c.cmp(JitPtr(operating_mode), OperatingMode::Kernel);
        c.je(l_noexception);
        c.cmp(JitPtr(addressing_mode), AddressingMode::Dword);
        c.je(l_noexception);
        BlockEpilogWithPcFlushAndJmp((void*)ReservedInstructionException);

        c.bind(l_noexception);
        Gpq ht = reg_alloc.GetGpr(rt);
        WriteCop0<8>(ht, rd);
    } else {
        OnCop0Unusable();
    }
}

void eret()
{
    branched = true;
    c.mov(JitPtr(branch_state), BranchState::NoBranch);
    BlockEpilogWithPcFlushAndJmp(
      (void*)vr4300::eret); // pc flush needed since exception handler may be called, and it reads the pc
}

void mfc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        if (!rt) return;
        Gpq ht = reg_alloc.GetDirtyGpr(rt);
        ReadCop0<4>(ht, rd);
    } else {
        OnCop0Unusable();
    }
}

void mtc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        Gpq ht = reg_alloc.GetGpr(rt);
        WriteCop0<4>(ht, rd);
    } else {
        OnCop0Unusable();
    }
}

bool tlbr()
{
    FlushPc();
    Label l_noexception = c.newLabel();
    reg_alloc.Call((void*)vr4300::tlbr);
    c.test(al, al);
    c.jz(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
    return true; // todo: interpreter version returns bool based on exception. not needed here
}

bool tlbwi()
{
    FlushPc();
    Label l_noexception = c.newLabel();
    reg_alloc.Call((void*)vr4300::tlbwi);
    c.test(al, al);
    c.jz(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
    return true; // todo: interpreter version returns bool based on exception. not needed here
}

bool tlbwr()
{
    FlushPc();
    Label l_noexception = c.newLabel();
    reg_alloc.Call((void*)vr4300::tlbwr);
    c.test(al, al);
    c.jz(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
    return true; // todo: interpreter version returns bool based on exception. not needed here
}

bool tlbp()
{
    FlushPc();
    Label l_noexception = c.newLabel();
    reg_alloc.Call((void*)vr4300::tlbp);
    c.test(al, al);
    c.jz(l_noexception);
    BlockEpilog();
    c.bind(l_noexception);
    return true; // todo: interpreter version returns bool based on exception. not needed here
}

} // namespace n64::vr4300::x64

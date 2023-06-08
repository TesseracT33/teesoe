#include "recompiler.hpp"
#include "interface/mi.hpp"
#include "rdp/rdp.hpp"
#include "rsp.hpp"

using namespace asmjit;
using namespace asmjit::x86;

namespace n64::rsp {

Status InitRecompiler()
{
    return status_unimplemented();
}

void Invalidate(u32 addr)
{
}

void InvalidateRange(u32 addr_lo, u32 addr_hi)
{
}

void LinkJit(u32 reg)
{
    compiler.mov(reg_alloc.GetHostMarkDirty(reg), (jit_pc + 8) & 0xFFF);
}

void OnBranchJit()
{
}

u64 RunRecompiler(u64 rsp_cycles)
{
    return 0;
}

void TakeBranchJit(asmjit::x86::Gp target)
{
}

void Recompiler::add(u32 rs, u32 rt, u32 rd) const
{
    addu(rs, rt, rd);
}

void Recompiler::addi(u32 rs, u32 rt, s16 imm) const
{
    addiu(rs, rt, imm);
}

void Recompiler::break_() const
{
    Label l_end = c.newLabel();
    c.or_(ptr(sp.status), 3); // set halted, broke
    c.bt(ptr(sp.status), 7); // intbreak
    c.jnb(l_end);
    c.mov(host_gpr_arg[0].r32(), std::to_underlying(mi::InterruptType::SP));
    call(c, mi::RaiseInterrupt);
    c.bind(l_end);
}

void Recompiler::j(u32 instr) const
{
    c.mov(eax, (instr << 2) & 0xFFF);
    TakeBranchJit(eax);
    branch_hit = true;
}

void Recompiler::jal(u32 instr) const
{
    c.mov(eax, (instr << 2) & 0xFFF);
    TakeBranchJit(eax);
    LinkJit(31);
    branch_hit = true;
}

void Recompiler::lb(u32 rs, u32 rt, s16 imm) const
{
    load<s8>(rs, rt, imm);
}

void Recompiler::lbu(u32 rs, u32 rt, s16 imm) const
{
    load<u8>(rs, rt, imm);
}

void Recompiler::lh(u32 rs, u32 rt, s16 imm) const
{
    load<s16>(rs, rt, imm);
}

void Recompiler::lhu(u32 rs, u32 rt, s16 imm) const
{
    load<u16>(rs, rt, imm);
}

void Recompiler::lw(u32 rs, u32 rt, s16 imm) const
{
    load<s32>(rs, rt, imm);
}

void Recompiler::lwu(u32 rs, u32 rt, s16 imm) const
{
    load<u32>(rs, rt, imm);
}

void Recompiler::mfc0(u32 rt, u32 rd) const
{
    reg_alloc.Flush(host_gpr_arg[0]);
    c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
    reg_alloc.Call(rd & 8 ? rdp::ReadReg : rsp::ReadReg);
    if (rt) c.mov(GetGprMarkDirty(rt), eax);
}

void Recompiler::mtc0(u32 rt, u32 rd) const
{
    reg_alloc.Flush(host_gpr_arg[0]);
    reg_alloc.Flush(host_gpr_arg[1]);
    c.mov(host_gpr_arg[0].r32(), (rd & 7) << 2);
    c.mov(host_gpr_arg[1].r32(), GetGpr(rt));
    reg_alloc.Call(rd & 8 ? rdp::WriteReg : rsp::WriteReg);
}

void Recompiler::sb(u32 rs, u32 rt, s16 imm) const
{
    store<s8>(rs, rt, imm);
}

void Recompiler::sh(u32 rs, u32 rt, s16 imm) const
{
    store<s16>(rs, rt, imm);
}

void Recompiler::sub(u32 rs, u32 rt, u32 rd) const
{
    subu(rs, rt, rd);
}

void Recompiler::sw(u32 rs, u32 rt, s16 imm) const
{
    store<s32>(rs, rt, imm);
}

template<std::integral Int> void Recompiler::load(u32 rs, u32 rt, s16 imm) const
{
    if (!rt) return;
    c.mov(host_gpr_arg[0].r32(), GetGpr(rs));
    c.add(host_gpr_arg[0].r32(), imm);
    call(c, ReadDMEM<std::make_signed_t<Int>>);
    if constexpr (std::same_as<Int, s8>) c.movsx(eax, al);
    if constexpr (std::same_as<Int, u8>) c.movzx(eax, al);
    if constexpr (std::same_as<Int, s16>) c.cwde(eax);
    if constexpr (std::same_as<Int, u16>) c.movzx(eax, ax);
    c.mov(GetGprMarkDirty(rt), eax);
}

template<std::integral Int> void Recompiler::store(u32 rs, u32 rt, s16 imm) const
{
    c.mov(host_gpr_arg[0].r32(), GetGpr(rs));
    c.add(host_gpr_arg[0].r32(), imm);
    c.mov(host_gpr_arg[1].r32(), GetGpr(rt));
    call(c, WriteDMEM<std::make_signed_t<Int>>);
}

} // namespace n64::rsp

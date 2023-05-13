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

void OnBranchJit()
{
}

u64 RunRecompiler(u64 rsp_cycles)
{
    return 0;
}

void Recompiler::add(u32 rs, u32 rt, u32 rd) const
{
    if (!rd) return;
    Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
    c.add(v0, v1);
    set_gpr(rd, v0);
}

void Recompiler::addi(u32 rs, u32 rt, s16 imm) const
{
    if (!rt) return;
    Gp v = get_gpr(rs);
    c.add(v, imm);
    set_gpr(rt, v);
}

void Recompiler::break_() const
{
    Label l_end = c.newLabel();
    c.or_(ptr(sp.status), 3); // set halted, broke
    c.bt(ptr(sp.status), 7); // intbreak
    c.jnb(l_end);
    c.mov(gp[0], std::to_underlying(mi::InterruptType::SP));
    call(c, mi::RaiseInterrupt);
    c.bind(l_end);
}

void Recompiler::j(u32 instr) const
{
    c.mov(gp[0], instr << 2);
    call(c, jump);
    on_branch();
}

void Recompiler::jal(u32 instr) const
{
    c.mov(gp[0], instr << 2);
    call(c, jump);
    c.mov(gp[0], ptr(pc));
    c.add(gp[0], 4);
    c.mov(gpr_ptr(31), gp[0]);
    on_branch();
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
    c.mov(gp[0], (rd & 7) << 2);
    call(c, rd & 8 ? rdp::ReadReg : rsp::ReadReg); // read regardless of rt, since read can have side-effects
    if (rt) set_gpr(rt, eax);
}

void Recompiler::mtc0(u32 rt, u32 rd) const
{
    c.mov(gp[0], (rd & 7) << 2);
    c.mov(gp[1], gpr_ptr(rt));
    call(c, rd & 8 ? rdp::WriteReg : rsp::WriteReg);
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
    if (!rd) return;
    Gp v0 = get_gpr(rs), v1 = get_gpr(rt);
    c.sub(v0, v1);
    set_gpr(rd, v0);
}

void Recompiler::sw(u32 rs, u32 rt, s16 imm) const
{
    store<s32>(rs, rt, imm);
}

template<std::integral Int> void Recompiler::load(u32 rs, u32 rt, s16 imm) const
{
    if (!rt) return;
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    call(c, ReadDMEM<std::make_signed_t<Int>>);
    if constexpr (std::same_as<Int, s8>) c.movsx(eax, al);
    if constexpr (std::same_as<Int, u8>) c.movzx(eax, al);
    if constexpr (std::same_as<Int, s16>) c.cwde(eax);
    if constexpr (std::same_as<Int, u16>) c.movzx(eax, ax);
    set_gpr(rt, eax);
}

template<std::integral Int> void Recompiler::store(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    c.mov(gp[1], gpr_ptr(rt)); // TODO: is it enough?
    call(c, WriteDMEM<std::make_signed_t<Int>>);
}

} // namespace n64::rsp

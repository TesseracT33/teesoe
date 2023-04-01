#include "cpu_recompiler.hpp"
#include "interface.hpp"
#include "interface/mi.hpp"
#include "rsp.hpp"

namespace n64::rsp {

using namespace asmjit;
using namespace asmjit::x86;

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
    c.or_(mem(&sp.status), 3); // set halted, broke
    c.bt(mem(&sp.status), 7); // intbreak
    c.jnb(l_end);
    c.mov(r[0], std::to_underlying(mi::InterruptType::SP));
    c.call(mi::RaiseInterrupt);
    c.bind(l_end);
}

void Recompiler::j(u32 instr) const
{
    Label l_end = c.newLabel();
    c.cmp(mem(&in_branch_delay_slot), 0);
    c.jne(l_end);
    c.mov(r[0], instr << 2);
    c.call(jump);
    c.bind(l_end);
    jit.branch_hit = 1;
}

void Recompiler::jal(u32 instr) const
{
    Label l_end = c.newLabel();
    c.cmp(mem(&in_branch_delay_slot), 0);
    c.jne(l_end);
    c.mov(r[0], instr << 2);
    c.bind(l_end);
    c.mov(r[0], 31);
    c.call(link);
    jit.branch_hit = 1;
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

void Recompiler::ll(u32 rs, u32 rt, s16 imm) const
{
    load<s16>(rs, rt, imm);
    c.mov(mem(&ll_bit), 1);
}

void Recompiler::lw(u32 rs, u32 rt, s16 imm) const
{
    load<s32>(rs, rt, imm);
}

void Recompiler::lwu(u32 rs, u32 rt, s16 imm) const
{
    load<u32>(rs, rt, imm);
}

void Recompiler::sb(u32 rs, u32 rt, s16 imm) const
{
    store<s8>(rs, rt, imm);
}

void Recompiler::sc(u32 rs, u32 rt, s16 imm) const
{
    Label l_end = c.newLabel();
    c.cmp(mem(&ll_bit), 0);
    c.je(l_end);
    store<s32>(rs, rt, imm);
    c.bind(l_end);
    if (rt) {
        c.movzx(eax, mem(&ll_bit));
        set_gpr(rt, eax);
    }
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
    c.mov(r[0], gpr_ptr(rs));
    c.add(r[0], imm);
    c.call(ReadDMEM<std::make_signed_t<Int>>);
    if constexpr (std::same_as<Int, s8>) c.movsx(eax, al);
    if constexpr (std::same_as<Int, u8>) c.movzx(eax, al);
    if constexpr (std::same_as<Int, s16>) c.cwde(eax);
    if constexpr (std::same_as<Int, u16>) c.movzx(eax, ax);
    set_gpr(rt, eax);
}

template<std::integral Int> void Recompiler::store(u32 rs, u32 rt, s16 imm) const
{
    c.mov(r[0], gpr_ptr(rs));
    c.add(r[0], imm);
    c.mov(r[1], gpr_ptr(rt)); // TODO: is it enough?
    c.call(WriteDMEM<std::make_signed_t<Int>>);
}

} // namespace n64::rsp

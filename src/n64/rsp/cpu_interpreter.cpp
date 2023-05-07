#include "cpu_interpreter.hpp"
#include "interface.hpp"
#include "interface/mi.hpp"
#include "rdp/rdp.hpp"
#include "rsp.hpp"

namespace n64::rsp {

void Interpreter::add(u32 rs, u32 rt, u32 rd) const
{
    gpr.set(rd, gpr[rs] + gpr[rt]);
}

void Interpreter::addi(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, gpr[rs] + imm);
}

void Interpreter::break_() const
{
    sp.status.halted = sp.status.broke = true;
    if (sp.status.intbreak) {
        mi::RaiseInterrupt(mi::InterruptType::SP);
    }
}

void Interpreter::j(u32 instr) const
{
    if (!in_branch_delay_slot) {
        jump(instr << 2);
    }
}

void Interpreter::jal(u32 instr) const
{
    if (!in_branch_delay_slot) {
        jump(instr << 2);
    }
    gpr.set(31, (pc + 8) & 0xFFF);
}

void Interpreter::lb(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, ReadDMEM<s8>(gpr[rs] + imm));
}

void Interpreter::lbu(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, u8(ReadDMEM<s8>(gpr[rs] + imm)));
}

void Interpreter::lh(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, ReadDMEM<s16>(gpr[rs] + imm));
}

void Interpreter::lhu(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, u16(ReadDMEM<s16>(gpr[rs] + imm)));
}

void Interpreter::lw(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, ReadDMEM<s32>(gpr[rs] + imm));
}

void Interpreter::lwu(u32 rs, u32 rt, s16 imm) const
{
    gpr.set(rt, ReadDMEM<s32>(gpr[rs] + imm));
}

void Interpreter::mfc0(u32 rt, u32 rd) const
{
    u32 reg_addr = (rd & 7) << 2;
    rd & 8 ? gpr.set(rt, rdp::ReadReg(reg_addr)) : gpr.set(rt, rsp::ReadReg(reg_addr));
}

void Interpreter::mtc0(u32 rt, u32 rd) const
{
    u32 reg_addr = (rd & 7) << 2;
    rd & 8 ? rdp::WriteReg(reg_addr, gpr[rt]) : rsp::WriteReg(reg_addr, gpr[rt]);
}

void Interpreter::sb(u32 rs, u32 rt, s16 imm) const
{
    WriteDMEM(gpr[rs] + imm, s8(gpr[rt]));
}

void Interpreter::sh(u32 rs, u32 rt, s16 imm) const
{
    WriteDMEM(gpr[rs] + imm, s16(gpr[rt]));
}

void Interpreter::sub(u32 rs, u32 rt, u32 rd) const
{
    gpr.set(rd, gpr[rs] - gpr[rt]);
}

void Interpreter::sw(u32 rs, u32 rt, s16 imm) const
{
    WriteDMEM(gpr[rs] + imm, gpr[rt]);
}

} // namespace n64::rsp

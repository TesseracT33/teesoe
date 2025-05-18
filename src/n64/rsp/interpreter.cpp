#include "interpreter.hpp"
#include "decoder.hpp"
#include "interface/mi.hpp"
#include "rdp/rdp.hpp"
#include "rsp.hpp"

namespace n64::rsp {

void InterpretOneInstruction()
{
    AdvancePipeline(1);
    u32 instr = FetchInstruction(pc);
    decode_and_interpret_rsp(instr);
    if (jump_is_pending) {
        PerformBranch();
        return;
    }
    if (in_branch_delay_slot) {
        jump_is_pending = true;
    }
    pc = (pc + 4) & 0xFFC;
}

void OnSingleStep()
{
    InterpretOneInstruction();
    sp.status.halted = true;
}

u32 RunInterpreter(u32 rsp_cycles)
{
    if (sp.status.halted) return 0;
    cycle_counter = 0;
    if (sp.status.sstep) {
        OnSingleStep();
    } else {
        while (cycle_counter < rsp_cycles && !sp.status.halted && !sp.status.sstep) {
            InterpretOneInstruction();
        }
        if (sp.status.halted) {
            if (jump_is_pending) { // note for future refactors: this makes rsp::op_break::BREAKWithinDelay pass
                PerformBranch();
            }
        } else if (sp.status.sstep) {
            OnSingleStep();
        }
    }
    return cycle_counter <= rsp_cycles ? 0 : cycle_counter - rsp_cycles;
}

void add(u32 rs, u32 rt, u32 rd)
{
    addu(rs, rt, rd);
}

void addi(u32 rs, u32 rt, s16 imm)
{
    addiu(rs, rt, imm);
}

void addiu(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, gpr[rs] + imm);
}

void addu(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] + gpr[rt]);
}

void and_(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] & gpr[rt]);
}

void andi(u32 rs, u32 rt, u16 imm)
{
    gpr.set(rt, gpr[rs] & imm);
}

void beq(u32 rs, u32 rt, s16 imm)
{
    if (gpr[rs] == gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    }
}
void bgez(u32 rs, s16 imm)
{
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    }
}

void bgezal(u32 rs, s16 imm)
{
    if (gpr[rs] >= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    }
    Link(31);
}

void bgtz(u32 rs, s16 imm)
{
    if (gpr[rs] > 0) {
        TakeBranch(pc + 4 + (imm << 2));
    }
}

void blez(u32 rs, s16 imm)
{
    if (gpr[rs] <= 0) {
        TakeBranch(pc + 4 + (imm << 2));
    }
}

void bltz(u32 rs, s16 imm)
{
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    }
}

void bltzal(u32 rs, s16 imm)
{
    if (gpr[rs] < 0) {
        TakeBranch(pc + 4 + (imm << 2));
    }
    Link(31);
}

void bne(u32 rs, u32 rt, s16 imm)
{
    if (gpr[rs] != gpr[rt]) {
        TakeBranch(pc + 4 + (imm << 2));
    }
}

void break_()
{
    sp.status.halted = sp.status.broke = true;
    if (sp.status.intbreak) {
        mi::RaiseInterrupt(mi::InterruptType::SP);
    }
}

void j(u32 instr)
{
    TakeBranch(instr << 2);
}

void jal(u32 instr)
{
    TakeBranch(instr << 2);
    gpr.set(31, (pc + 8) & 0xFFF);
}

void jalr(u32 rs, u32 rd)
{
    TakeBranch(gpr[rs]);
    Link(rd);
}

void jr(u32 rs)
{
    TakeBranch(gpr[rs]);
}

void lb(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, ReadDMEM<s8>(gpr[rs] + imm));
}

void lbu(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, u8(ReadDMEM<s8>(gpr[rs] + imm)));
}

void lh(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, ReadDMEM<s16>(gpr[rs] + imm));
}

void lhu(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, u16(ReadDMEM<s16>(gpr[rs] + imm)));
}

void lui(u32 rt, s16 imm)
{
    gpr.set(rt, imm << 16);
}

void lw(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, ReadDMEM<s32>(gpr[rs] + imm));
}

void lwu(u32 rs, u32 rt, s16 imm)
{
    lw(rs, rt, imm);
}

void mfc0(u32 rt, u32 rd)
{
    u32 reg_addr = (rd & 7) << 2;
    if (rd & 8) {
        gpr.set(rt, rdp::ReadReg(reg_addr));
    } else {
        gpr.set(rt, rsp::ReadReg(reg_addr));
    }
}

void mtc0(u32 rt, u32 rd)
{
    u32 reg_addr = (rd & 7) << 2;
    if (rd & 8) {
        rdp::WriteReg(reg_addr, gpr[rt]);
    } else {
        rsp::WriteReg(reg_addr, gpr[rt]);
    }
}

void nor(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, ~(gpr[rs] | gpr[rt]));
}

void or_(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] | gpr[rt]);
}

void ori(u32 rs, u32 rt, u16 imm)
{
    gpr.set(rt, gpr[rs] | imm);
}

void sb(u32 rs, u32 rt, s16 imm)
{
    WriteDMEM(gpr[rs] + imm, s8(gpr[rt]));
}

void sh(u32 rs, u32 rt, s16 imm)
{
    WriteDMEM(gpr[rs] + imm, s16(gpr[rt]));
}

void sll(u32 rt, u32 rd, u32 sa)
{
    gpr.set(rd, gpr[rt] << sa);
}

void sllv(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rt] << (gpr[rs] & 31));
}

void slt(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] < gpr[rt]);
}

void slti(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, gpr[rs] < imm);
}

void sltiu(u32 rs, u32 rt, s16 imm)
{
    gpr.set(rt, u32(gpr[rs]) < u32(imm));
}

void sltu(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, u32(gpr[rs]) < u32(gpr[rt]));
}

void sra(u32 rt, u32 rd, u32 sa)
{
    gpr.set(rd, gpr[rt] >> sa);
}

void srav(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rt] >> (gpr[rs] & 31));
}

void srl(u32 rt, u32 rd, u32 sa)
{
    gpr.set(rd, u32(gpr[rt]) >> sa);
}

void srlv(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, u32(gpr[rt]) >> (gpr[rs] & 31));
}

void sub(u32 rs, u32 rt, u32 rd)
{
    subu(rs, rt, rd);
}

void subu(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] - gpr[rt]);
}

void sw(u32 rs, u32 rt, s16 imm)
{
    WriteDMEM(gpr[rs] + imm, gpr[rt]);
}

void xor_(u32 rs, u32 rt, u32 rd)
{
    gpr.set(rd, gpr[rs] ^ gpr[rt]);
}

void xori(u32 rs, u32 rt, u16 imm)
{
    gpr.set(rt, gpr[rs] ^ imm);
}

} // namespace n64::rsp

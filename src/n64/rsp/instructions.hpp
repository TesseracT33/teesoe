#include "numtypes.hpp"

#ifndef RSP_INSTRUCTIONS_NAMESPACE
#    error "RSP_INSTRUCTIONS_NAMESPACE is not defined. Please define it before including this file."
#endif

namespace RSP_INSTRUCTIONS_NAMESPACE {

// scalar
void add(u32 rs, u32 rt, u32 rd);
void addi(u32 rs, u32 rt, s16 imm);
void addiu(u32 rs, u32 rt, s16 imm);
void addu(u32 rs, u32 rt, u32 rd);
void and_(u32 rs, u32 rt, u32 rd);
void andi(u32 rs, u32 rt, u16 imm);
void beq(u32 rs, u32 rt, s16 imm);
void bgez(u32 rs, s16 imm);
void bgezal(u32 rs, s16 imm);
void bgtz(u32 rs, s16 imm);
void blez(u32 rs, s16 imm);
void bltz(u32 rs, s16 imm);
void bltzal(u32 rs, s16 imm);
void bne(u32 rs, u32 rt, s16 imm);
void break_();
void j(u32 instr);
void jal(u32 instr);
void jalr(u32 rs, u32 rd);
void jr(u32 rs);
void lb(u32 rs, u32 rt, s16 imm);
void lbu(u32 rs, u32 rt, s16 imm);
void lh(u32 rs, u32 rt, s16 imm);
void lhu(u32 rs, u32 rt, s16 imm);
void lui(u32 rt, s16 imm);
void lw(u32 rs, u32 rt, s16 imm);
void lwu(u32 rs, u32 rt, s16 imm);
void mfc0(u32 rt, u32 rd);
void mtc0(u32 rt, u32 rd);
void nor(u32 rs, u32 rt, u32 rd);
void or_(u32 rs, u32 rt, u32 rd);
void ori(u32 rs, u32 rt, u16 imm);
void sb(u32 rs, u32 rt, s16 imm);
void sh(u32 rs, u32 rt, s16 imm);
void sll(u32 rt, u32 rd, u32 sa);
void sllv(u32 rs, u32 rt, u32 rd);
void slt(u32 rs, u32 rt, u32 rd);
void slti(u32 rs, u32 rt, s16 imm);
void sltiu(u32 rs, u32 rt, s16 imm);
void sltu(u32 rs, u32 rt, u32 rd);
void sra(u32 rt, u32 rd, u32 sa);
void srav(u32 rs, u32 rt, u32 rd);
void srl(u32 rt, u32 rd, u32 sa);
void srlv(u32 rs, u32 rt, u32 rd);
void sub(u32 rs, u32 rt, u32 rd);
void subu(u32 rs, u32 rt, u32 rd);
void sw(u32 rs, u32 rt, s16 imm);
void xor_(u32 rs, u32 rt, u32 rd);
void xori(u32 rs, u32 rt, u16 imm);

// vector
void cfc2(u32 rt, u32 vs);
void ctc2(u32 rt, u32 vs);
void mfc2(u32 rt, u32 vs, u32 e);
void mtc2(u32 rt, u32 vs, u32 e);
void lbv(u32 base, u32 vt, u32 e, s32 offset);
void ldv(u32 base, u32 vt, u32 e, s32 offset);
void lfv(u32 base, u32 vt, u32 e, s32 offset);
void lhv(u32 base, u32 vt, u32 e, s32 offset);
void llv(u32 base, u32 vt, u32 e, s32 offset);
void lpv(u32 base, u32 vt, u32 e, s32 offset);
void lqv(u32 base, u32 vt, u32 e, s32 offset);
void lrv(u32 base, u32 vt, u32 e, s32 offset);
void lsv(u32 base, u32 vt, u32 e, s32 offset);
void ltv(u32 base, u32 vt, u32 e, s32 offset);
void luv(u32 base, u32 vt, u32 e, s32 offset);
void sbv(u32 base, u32 vt, u32 e, s32 offset);
void sdv(u32 base, u32 vt, u32 e, s32 offset);
void sfv(u32 base, u32 vt, u32 e, s32 offset);
void shv(u32 base, u32 vt, u32 e, s32 offset);
void slv(u32 base, u32 vt, u32 e, s32 offset);
void spv(u32 base, u32 vt, u32 e, s32 offset);
void sqv(u32 base, u32 vt, u32 e, s32 offset);
void srv(u32 base, u32 vt, u32 e, s32 offset);
void ssv(u32 base, u32 vt, u32 e, s32 offset);
void stv(u32 base, u32 vt, u32 e, s32 offset);
void suv(u32 base, u32 vt, u32 e, s32 offset);
void swv(u32 base, u32 vt, u32 e, s32 offset);
void vabs(u32 vs, u32 vt, u32 vd, u32 e);
void vadd(u32 vs, u32 vt, u32 vd, u32 e);
void vaddc(u32 vs, u32 vt, u32 vd, u32 e);
void vand(u32 vs, u32 vt, u32 vd, u32 e);
void vch(u32 vs, u32 vt, u32 vd, u32 e);
void vcl(u32 vs, u32 vt, u32 vd, u32 e);
void vcr(u32 vs, u32 vt, u32 vd, u32 e);
void veq(u32 vs, u32 vt, u32 vd, u32 e);
void vge(u32 vs, u32 vt, u32 vd, u32 e);
void vlt(u32 vs, u32 vt, u32 vd, u32 e);
void vmacf(u32 vs, u32 vt, u32 vd, u32 e);
void vmacq(u32 vd);
void vmacu(u32 vs, u32 vt, u32 vd, u32 e);
void vmadh(u32 vs, u32 vt, u32 vd, u32 e);
void vmadl(u32 vs, u32 vt, u32 vd, u32 e);
void vmadm(u32 vs, u32 vt, u32 vd, u32 e);
void vmadn(u32 vs, u32 vt, u32 vd, u32 e);
void vmov(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vmrg(u32 vs, u32 vt, u32 vd, u32 e);
void vmudh(u32 vs, u32 vt, u32 vd, u32 e);
void vmudl(u32 vs, u32 vt, u32 vd, u32 e);
void vmudm(u32 vs, u32 vt, u32 vd, u32 e);
void vmudn(u32 vs, u32 vt, u32 vd, u32 e);
void vmulf(u32 vs, u32 vt, u32 vd, u32 e);
void vmulq(u32 vs, u32 vt, u32 vd, u32 e);
void vmulu(u32 vs, u32 vt, u32 vd, u32 e);
void vnand(u32 vs, u32 vt, u32 vd, u32 e);
void vne(u32 vs, u32 vt, u32 vd, u32 e);
void vnop();
void vnor(u32 vs, u32 vt, u32 vd, u32 e);
void vnxor(u32 vs, u32 vt, u32 vd, u32 e);
void vor(u32 vs, u32 vt, u32 vd, u32 e);
void vrcp(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrcph(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrcpl(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrndn(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrndp(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrsq(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrsqh(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vrsql(u32 vt, u32 vt_e, u32 vd, u32 vd_e);
void vsar(u32 vd, u32 e);
void vsub(u32 vs, u32 vt, u32 vd, u32 e);
void vsubc(u32 vs, u32 vt, u32 vd, u32 e);
void vxor(u32 vs, u32 vt, u32 vd, u32 e);
void vzero(u32 vs, u32 vt, u32 vd, u32 e);

} // namespace RSP_INSTRUCTIONS_NAMESPACE

#include "cop1.hpp"
#include "numtypes.hpp"

#ifndef VR4300_INSTRUCTIONS_NAMESPACE
#    error "VR4300_INSTRUCTIONS_NAMESPACE is not defined. Please define it before including this file."
#endif

namespace VR4300_INSTRUCTIONS_NAMESPACE {

// integer
void add(u32 rs, u32 rt, u32 rd);
void addi(u32 rs, u32 rt, s16 imm);
void addiu(u32 rs, u32 rt, s16 imm);
void addu(u32 rs, u32 rt, u32 rd);
void and_(u32 rs, u32 rt, u32 rd);
void andi(u32 rs, u32 rt, u16 imm);
void beq(u32 rs, u32 rt, s16 imm);
void beql(u32 rs, u32 rt, s16 imm);
void bgez(u32 rs, s16 imm);
void bgezal(u32 rs, s16 imm);
void bgezall(u32 rs, s16 imm);
void bgezl(u32 rs, s16 imm);
void bgtz(u32 rs, s16 imm);
void bgtzl(u32 rs, s16 imm);
void blez(u32 rs, s16 imm);
void blezl(u32 rs, s16 imm);
void bltz(u32 rs, s16 imm);
void bltzal(u32 rs, s16 imm);
void bltzall(u32 rs, s16 imm);
void bltzl(u32 rs, s16 imm);
void bne(u32 rs, u32 rt, s16 imm);
void bnel(u32 rs, u32 rt, s16 imm);
void break_();
void dadd(u32 rs, u32 rt, u32 rd);
void daddi(u32 rs, u32 rt, s16 imm);
void daddiu(u32 rs, u32 rt, s16 imm);
void daddu(u32 rs, u32 rt, u32 rd);
void ddiv(u32 rs, u32 rt);
void ddivu(u32 rs, u32 rt);
void div(u32 rs, u32 rt);
void divu(u32 rs, u32 rt);
void dmult(u32 rs, u32 rt);
void dmultu(u32 rs, u32 rt);
void dsll(u32 rt, u32 rd, u32 sa);
void dsll32(u32 rt, u32 rd, u32 sa);
void dsllv(u32 rs, u32 rt, u32 rd);
void dsra(u32 rt, u32 rd, u32 sa);
void dsra32(u32 rt, u32 rd, u32 sa);
void dsrav(u32 rs, u32 rt, u32 rd);
void dsrl(u32 rt, u32 rd, u32 sa);
void dsrl32(u32 rt, u32 rd, u32 sa);
void dsrlv(u32 rs, u32 rt, u32 rd);
void dsub(u32 rs, u32 rt, u32 rd);
void dsubu(u32 rs, u32 rt, u32 rd);
void j(u32 instr);
void jal(u32 instr);
void jalr(u32 rs, u32 rd);
void jr(u32 rs);
void lb(u32 rs, u32 rt, s16 imm);
void lbu(u32 rs, u32 rt, s16 imm);
void ld(u32 rs, u32 rt, s16 imm);
void ldl(u32 rs, u32 rt, s16 imm);
void ldr(u32 rs, u32 rt, s16 imm);
void lh(u32 rs, u32 rt, s16 imm);
void lhu(u32 rs, u32 rt, s16 imm);
void ll(u32 rs, u32 rt, s16 imm);
void lld(u32 rs, u32 rt, s16 imm);
void lui(u32 rt, s16 imm);
void lw(u32 rs, u32 rt, s16 imm);
void lwl(u32 rs, u32 rt, s16 imm);
void lwr(u32 rs, u32 rt, s16 imm);
void lwu(u32 rs, u32 rt, s16 imm);
void mfhi(u32 rd);
void mflo(u32 rd);
void mthi(u32 rs);
void mtlo(u32 rs);
void mult(u32 rs, u32 rt);
void multu(u32 rs, u32 rt);
void nor(u32 rs, u32 rt, u32 rd);
void or_(u32 rs, u32 rt, u32 rd);
void ori(u32 rs, u32 rt, u16 imm);
void sb(u32 rs, u32 rt, s16 imm);
void sc(u32 rs, u32 rt, s16 imm);
void scd(u32 rs, u32 rt, s16 imm);
void sd(u32 rs, u32 rt, s16 imm);
void sdl(u32 rs, u32 rt, s16 imm);
void sdr(u32 rs, u32 rt, s16 imm);
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
void swl(u32 rs, u32 rt, s16 imm);
void swr(u32 rs, u32 rt, s16 imm);
void sync();
void syscall();
void teq(u32 rs, u32 rt);
void teqi(u32 rs, s16 imm);
void tge(u32 rs, u32 rt);
void tgei(u32 rs, s16 imm);
void tgeu(u32 rs, u32 rt);
void tgeiu(u32 rs, s16 imm);
void tlt(u32 rs, u32 rt);
void tlti(u32 rs, s16 imm);
void tltu(u32 rs, u32 rt);
void tltiu(u32 rs, s16 imm);
void tne(u32 rs, u32 rt);
void tnei(u32 rs, s16 imm);
void xor_(u32 rs, u32 rt, u32 rd);
void xori(u32 rs, u32 rt, u16 imm);

// cop0
void cache(u32 rs, u32 rt, s16 imm);
void dmfc0(u32 rt, u32 rd);
void dmtc0(u32 rt, u32 rd);
void eret();
void mfc0(u32 rt, u32 rd);
void mtc0(u32 rt, u32 rd);
bool tlbr();
bool tlbwi();
bool tlbwr();
bool tlbp();

// cop1
void bc1f(s16 imm);
void bc1fl(s16 imm);
void bc1t(s16 imm);
void bc1tl(s16 imm);

void cfc1(u32 fs, u32 rt);
void ctc1(u32 fs, u32 rt);
void dcfc1();
void dctc1();
void dmfc1(u32 fs, u32 rt);
void dmtc1(u32 fs, u32 rt);
void ldc1(u32 base, u32 ft, s16 imm);
void lwc1(u32 base, u32 ft, s16 imm);
void mfc1(u32 fs, u32 rt);
void mtc1(u32 fs, u32 rt);
void sdc1(u32 base, u32 ft, s16 imm);
void swc1(u32 base, u32 ft, s16 imm);

template<FpuFmt> void compare(u32 fs, u32 ft, u8 cond);

template<FpuFmt> void ceil_l(u32 fs, u32 fd);
template<FpuFmt> void ceil_w(u32 fs, u32 fd);
template<FpuFmt> void cvt_d(u32 fs, u32 fd);
template<FpuFmt> void cvt_l(u32 fs, u32 fd);
template<FpuFmt> void cvt_s(u32 fs, u32 fd);
template<FpuFmt> void cvt_w(u32 fs, u32 fd);
template<FpuFmt> void floor_l(u32 fs, u32 fd);
template<FpuFmt> void floor_w(u32 fs, u32 fd);
template<FpuFmt> void round_l(u32 fs, u32 fd);
template<FpuFmt> void round_w(u32 fs, u32 fd);
template<FpuFmt> void trunc_l(u32 fs, u32 fd);
template<FpuFmt> void trunc_w(u32 fs, u32 fd);

template<FpuFmt> void abs(u32 fs, u32 fd);
template<FpuFmt> void add(u32 fs, u32 ft, u32 fd);
template<FpuFmt> void div(u32 fs, u32 ft, u32 fd);
template<FpuFmt> void mov(u32 fs, u32 fd);
template<FpuFmt> void mul(u32 fs, u32 ft, u32 fd);
template<FpuFmt> void neg(u32 fs, u32 fd);
template<FpuFmt> void sqrt(u32 fs, u32 fd);
template<FpuFmt> void sub(u32 fs, u32 ft, u32 fd);

// cop2
void cfc2(u32 rt);
void cop2_reserved();
void ctc2(u32 rt);
void dcfc2();
void dctc2();
void dmfc2(u32 rt);
void dmtc2(u32 rt);
void mfc2(u32 rt);
void mtc2(u32 rt);

} // namespace VR4300_INSTRUCTIONS_NAMESPACE

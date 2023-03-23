#pragma once

#include "types.hpp"

namespace n64::vr4300 {

enum class Fmt {
    Float32 = 16,
    Float64 = 17,
    Int32 = 20,
    Int64 = 21,
    Invalid,
};

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

template<Fmt> void c(u32 fs, u32 ft, u8 cond);

template<Fmt> void ceil_l(u32 fs, u32 fd);
template<Fmt> void ceil_w(u32 fs, u32 fd);
template<Fmt> void cvt_d(u32 fs, u32 fd);
template<Fmt> void cvt_l(u32 fs, u32 fd);
template<Fmt> void cvt_s(u32 fs, u32 fd);
template<Fmt> void cvt_w(u32 fs, u32 fd);
template<Fmt> void floor_l(u32 fs, u32 fd);
template<Fmt> void floor_w(u32 fs, u32 fd);
template<Fmt> void round_l(u32 fs, u32 fd);
template<Fmt> void round_w(u32 fs, u32 fd);
template<Fmt> void trunc_l(u32 fs, u32 fd);
template<Fmt> void trunc_w(u32 fs, u32 fd);

template<Fmt> void abs(u32 fs, u32 fd);
template<Fmt> void add(u32 fs, u32 ft, u32 fd);
template<Fmt> void div(u32 fs, u32 ft, u32 fd);
template<Fmt> void mov(u32 fs, u32 fd);
template<Fmt> void mul(u32 fs, u32 ft, u32 fd);
template<Fmt> void neg(u32 fs, u32 fd);
template<Fmt> void sqrt(u32 fs, u32 fd);
template<Fmt> void sub(u32 fs, u32 ft, u32 fd);

void InitCop1();

} // namespace n64::vr4300

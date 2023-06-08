#pragma once

#include "types.hpp"

namespace n64::vr4300 {

void cfc2(u32 rt);
void ctc2(u32 rt);
void dcfc2();
void dctc2();
void dmfc2(u32 rt);
void dmtc2(u32 rt);
void mfc2(u32 rt);
void mtc2(u32 rt);

void InitCop2();

inline u64 cop2_latch;

} // namespace n64::vr4300

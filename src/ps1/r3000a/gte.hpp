#pragma once

#include "types.hpp"

namespace ps1::r3000a {

void cfc2(u32 rt, u32 rd);
void ctc2(u32 rt, u32 rd);
void mfc2(u32 rt, u32 rd);
void mtc2(u32 rt, u32 rd);

void avsz3();
void avsz4();
void cc();
void cdp();
void dcpl();
void dpcs();
void dpct();
void gpf();
void gpl();
void intpl();
void op();
void mvmva();
void nccs();
void ncct();
void ncds();
void ncdt();
void nclip();
void ncs();
void nct();
void rtps(bool sf);
void rtpt(bool sf);
void sqr(bool fs);

} // namespace ps1::r3000a

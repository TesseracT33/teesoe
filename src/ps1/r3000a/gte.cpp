#include "gte.hpp"

#include <algorithm>

namespace ps1::r3000a {

struct {
    u16 otz;
    s16 ir0, ir1, ir2, ir3;
    s16 sx0, sy0, sx1, sy1, sx2, sy2, sxp, syp;
    u16 sz0, sz1, sz2, sz3;
    u32 mac0, mac1, mac2, mac3;
    s16 zsf3, zsf4;
    struct {
        u32                   : 12;
        u32 ir0_saturated     : 1;
        u32 sy2_saturated     : 1;
        u32 sx2_saturated     : 1;
        u32 mac0_overflow_neg : 1;
        u32 mac0_overflow_pos : 1;
        u32 divide_overflow   : 1;
        u32 sz3_otz_saturated : 1;
        u32 fifo_b_saturated  : 1;
        u32 fifo_g_saturated  : 1;
        u32 fifo_r_saturated  : 1;
        u32 ir3_saturated     : 1;
        u32 ir2_saturated     : 1;
        u32 ir1_saturated     : 1;
        u32 mac3_overflow_neg : 1;
        u32 mac2_overflow_neg : 1;
        u32 mac1_overflow_neg : 1;
        u32 mac3_overflow_pos : 1;
        u32 mac2_overflow_pos : 1;
        u32 mac1_overflow_pos : 1;
        u32 error             : 1;
    } flag;
} cop2;

static void set_mac0_overflow(s64 result);
static void set_sz3_otz_saturated(s64 result);

void cfc2(u32 rt, u32 rd)
{
}

void ctc2(u32 rt, u32 rd)
{
}

void mfc2(u32 rt, u32 rd)
{
}

void mtc2(u32 rt, u32 rd)
{
}

void avsz3()
{
    s32 mac0_result = s32(cop2.zsf3) * (u32(cop2.sz1) + cop2.sz2 + cop2.sz3);
    s32 otz_result = mac0_result / 0x1000;
    otz_result = std::min(std::max(otz_result, 0), 0xFFFF);
    cop2.otz = u16(otz_result);
    set_sz3_otz_saturated(otz_result);
}

void avsz4()
{
    s32 mac0_result = s32(cop2.zsf3) * (u32(cop2.sz0) + cop2.sz1 + cop2.sz2 + cop2.sz3);
    s32 otz_result = mac0_result / 0x1000;
    otz_result = std::min(std::max(otz_result, 0), 0xFFFF);
    cop2.otz = u16(otz_result);
    set_sz3_otz_saturated(otz_result);
}

void cc()
{
}

void cdp()
{
}

void dcpl()
{
}

void dpcs()
{
}

void dpct()
{
}

void gpf()
{
}

void gpl()
{
}

void intpl()
{
}

void op()
{
}

void mvmva()
{
}

void nccs()
{
}

void ncct()
{
}

void ncds()
{
}

void ncdt()
{
}

void nclip()
{
    s64 result = s64(cop2.sx0) * s64(cop2.sy1) + s64(cop2.sx1) * s64(cop2.sy2) + s64(cop2.sx2) * s64(cop2.sy0)
               - s64(cop2.sx0) * s64(cop2.sy2) - s64(cop2.sx1) * s64(cop2.sy0) - s64(cop2.sx2) * s64(cop2.sy1);
    cop2.mac0 = s32(result);
    set_mac0_overflow(result);
}

void ncs()
{
}

void nct()
{
}

void rtps(bool sf)
{
}

void rtpt(bool sf)
{
}

void sqr(bool fs)
{
}

void set_mac0_overflow(s64 result)
{
    if (s32(result) != result) {
        if (result & 1_s64 << 63) {
            cop2.flag.mac0_overflow_neg = 1;
        } else {
            cop2.flag.mac0_overflow_pos = 1;
        }
    }
}

void set_sz3_otz_saturated(s64 result)
{
    cop2.flag.sz3_otz_saturated = result < 0 || result > 0xFFFF;
}

} // namespace ps1::r3000a

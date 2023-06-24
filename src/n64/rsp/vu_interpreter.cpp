#include "host.hpp"
#include "rsp.hpp"
#include "util.hpp"
#include "vu.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstring>

#if X64
#include "sse_util.hpp"
#endif

#define vco ctrl_reg[0]
#define vcc ctrl_reg[1]
#define vce ctrl_reg[2].lo

namespace n64::rsp {

static void AddToAcc(m128i low);
static void AddToAcc(m128i low, m128i mid);
static void AddToAcc(m128i low, m128i mid, m128i high);
static void AddToAccCond(m128i low, m128i cond);
static void AddToAccCond(m128i low, m128i mid, m128i cond);
static void AddToAccCond(m128i low, m128i mid, m128i high, m128i cond);
static void AddToAccFromMid(m128i mid, m128i high);
static m128i ClampSigned(m128i low, m128i high);
static m128i ClampUnsigned(m128i low, m128i high);
static m128i GetVTBroadcast(uint vt, uint element);

template<bool vmulf> static void vmulfu(u32 vs, u32 vt, u32 vd, u32 e);

void AddToAcc(m128i low)
{
    acc.low = _mm_add_epi16(acc.low, low);
    m128i low_carry = _mm_cmplt_epu16(acc.low, low);
    acc.mid = _mm_sub_epi16(acc.mid, low_carry);
    m128i mid_carry = _mm_and_si128(low_carry, _mm_cmpeq_epi16(acc.mid, _mm_setzero_si128()));
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAcc(m128i low, m128i mid)
{
    AddToAcc(low);
    acc.mid = _mm_add_epi16(acc.mid, mid);
    m128i mid_carry = _mm_cmplt_epu16(acc.mid, mid);
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAcc(m128i low, m128i mid, m128i high)
{
    AddToAcc(low, mid);
    acc.high = _mm_add_epi16(acc.high, high);
}

void AddToAccCond(m128i low, m128i cond)
{
    /* Like AddToAcc(m128i), but only perform the operation if the corresponding lane in 'cond' is 0xFFFF */
    m128i prev_acc_low = acc.low;
    acc.low = _mm_blendv_epi8(acc.low, _mm_add_epi16(acc.low, low), cond);
    m128i low_carry = _mm_cmplt_epu16(acc.low, prev_acc_low);
    acc.mid = _mm_sub_epi16(acc.mid, low_carry);
    m128i mid_carry = _mm_and_si128(low_carry, _mm_cmpeq_epi16(acc.mid, _mm_setzero_si128()));
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAccCond(m128i low, m128i mid, m128i cond)
{
    AddToAccCond(low, cond);
    m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_blendv_epi8(acc.mid, _mm_add_epi16(acc.mid, mid), cond);
    m128i mid_carry = _mm_cmplt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAccCond(m128i low, m128i mid, m128i high, m128i cond)
{
    AddToAccCond(low, mid, cond);
    acc.high = _mm_blendv_epi8(acc.high, _mm_add_epi16(acc.high, high), cond);
}

void AddToAccFromMid(m128i mid, m128i high)
{
    acc.mid = _mm_add_epi16(acc.mid, mid);
    m128i mid_carry = _mm_cmplt_epu16(acc.mid, mid);
    acc.high = _mm_add_epi16(acc.high, high);
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

m128i ClampSigned(m128i lo, m128i hi)
{
    /* for i in 0..7
            value = high<i> << 16 | low<i>
            if value < -32768
                result<i> = -32768
            elif value > 32767
                result<i> = 32767
            else
                result<i> = value(15..0)
            endif
        endfor
    */
    return _mm_packs_epi32(_mm_unpacklo_epi16(lo, hi), _mm_unpackhi_epi16(lo, hi));
}

m128i ClampUnsigned(m128i lo, m128i hi)
{
    /* for i in 0..7
            value = high<i> << 16 | low<i>
            if value < 0
                result<i> = 0
            elif value > 32767
                result<i> = 65535
            else
                result<i> = value(15..0)
            endif
        endfor
    */
    m128i words1 = _mm_unpacklo_epi16(lo, hi);
    m128i words2 = _mm_unpackhi_epi16(lo, hi);
    m128i clamp_sse = _mm_packus_epi32(words1, words2);
    return _mm_blendv_epi8(clamp_sse, _mm_set1_epi64x(s64(-1)), _mm_srai_epi16(clamp_sse, 15));
}

m128i GetVTBroadcast(uint vt /* 0-31 */, uint element /* 0-15 */)
{
    return _mm_shuffle_epi8(vpr[vt], broadcast_mask[element]);
}

// LBV, LSV, LLV, LDV
template<std::signed_integral Int> void LoadUpToDword(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * sizeof(Int);
    if constexpr (sizeof(Int) == 1) {
        vpr_dst[e ^ 1] = dmem[addr & 0xFFF];
    } else {
        u32 num_bytes = std::min((u32)sizeof(Int), 16 - e);
        for (u32 i = 0; i < num_bytes; ++i) {
            vpr_dst[e + i ^ 1] = dmem[addr + i & 0xFFF];
        }
    }
}

// SBV, SSV, SLV, SDV
template<std::signed_integral Int> void StoreUpToDword(u32 base, u32 vt, u32 e, s32 offset)
{
    u8 const* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * sizeof(Int);
    for (size_t i = 0; i < sizeof(Int); ++i) {
        dmem[addr + i & 0xFFF] = vpr_src[e + i & 15 ^ 1];
    }
}

s32 Rcp(s32 input)
{
    s32 sinput = input;
    s32 mask = input >> 31;
    input ^= mask;
    if (sinput > -32768) input -= mask;
    if (input == 0) return 0x7FFF'FFFF;
    if (sinput == -32768) return -65536;
    u32 shift = std::countl_zero(u32(input));
    u32 index = (u64(input) << shift & 0x7FC0'0000) >> 22;
    s32 result = (0x10000 | rcp_rom[index]) << 14;
    return result >> 31 - shift ^ mask;
}

s32 Rsq(s32 input)
{
    if (input == 0) return 0x7FFF'FFFF;
    if (input == 0xFFFF8000) return 0xFFFF0000;
    if (input > 0xFFFF8000) --input;
    s32 mask = input >> 31;
    input ^= mask;
    u32 lshift = std::countl_zero(u32(input)) + 1;
    u32 rshift = 32 - lshift >> 1;
    u32 index = u32(input) << lshift >> 24 | (lshift & 1) << 8;
    return (0x400'00000 | rsq_rom[index] << 14) >> rshift ^ mask;
}

void cfc2(u32 rt, u32 vs)
{
    /* GPR(31..0) = sign_extend(CTRL(15..0)) */
    vs = std::min(vs & 3, 2u);
    int lo = _mm_movemask_epi8(_mm_packs_epi16(ctrl_reg[vs].lo, _mm_setzero_si128()));
    int hi = _mm_movemask_epi8(_mm_packs_epi16(ctrl_reg[vs].hi, _mm_setzero_si128()));
    gpr.set(rt, s16(hi << 8 | lo));
}

void ctc2(u32 rt, u32 vs)
{
    /* CTRL(15..0) = GPR(15..0) */
    vs = std::min(vs & 3, 2u);
    s32 r = gpr[rt];
    ctrl_reg[vs].lo = _mm_set_epi64x(ctc2_table[r >> 4 & 0xF], ctc2_table[r >> 0 & 0xF]);
    if (vs < 2) {
        ctrl_reg[vs].hi = _mm_set_epi64x(ctc2_table[r >> 12 & 0xF], ctc2_table[r >> 8 & 0xF]);
    }
}

void mfc2(u32 rt, u32 vs, u32 e)
{
    /* GPR[rt](31..0) = sign_extend(VS<elem>(15..0)) */
    u8* v = (u8*)(&vpr[vs]);
    if (e & 1) {
        gpr.set(rt, s16(v[e ^ 1] << 8 | v[e + 1 & 15 ^ 1]));
    } else {
        gpr.set(rt, s16(v[e] | v[e + 1] << 8));
    }
}

void mtc2(u32 rt, u32 vs, u32 e)
{
    /* VS<elem>(15..0) = GPR[rt](15..0) */
    u8* v = (u8*)(&vpr[vs]);
    if (e & 1) {
        v[e ^ 1] = u8(gpr[rt] >> 8);
        if (e < 15) v[e + 1 ^ 1] = u8(gpr[rt]);
    } else {
        v[e] = u8(gpr[rt]);
        v[e + 1] = u8(gpr[rt] >> 8);
    }
}

void lbv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s8>(base, vt, e, offset);
}

void ldv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s64>(base, vt, e, offset);
}

void lfv(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto addr_offset = (addr & 7) - e; // todo: what if e > 8?
    addr &= ~7;
    s16 tmp[8];
    for (int i = 0; i < 4; ++i) {
        tmp[i] = dmem[addr + (addr_offset + 4 * i & 15) & 0xFFF] << 7;
        tmp[i + 4] = dmem[addr + (addr_offset + 4 * i + 8 & 15) & 0xFFF] << 7;
    }
    for (auto byte = e; byte < std::min(e + 8, 16u); ++byte) {
        vpr_dst[byte ^ 1] = reinterpret_cast<u8*>(tmp)[byte ^ 1];
    }
}

void lhv(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_dst = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto mem_offset = (addr & 7) - e; // todo: what if e > 8?
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        vpr_dst[i] = dmem[addr + (mem_offset + 2 * i & 15) & 0xFFF] << 7;
    }
}

void llv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s32>(base, vt, e, offset);
}

void lpv(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_dst = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    auto mem_offset = (addr & 7) - e; // todo: what if e > 8?
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        vpr_dst[i] = dmem[addr + (mem_offset + i & 15) & 0xFFF] << 8;
    }
}

void lqv(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    u32 addr = gpr[base] + offset * 16;
    u32 num_bytes = 16 - std::max(addr & 0xF, e); // == std::min(16 - (addr & 0xF), 16 - element)
    addr &= 0xFFF;
    for (u32 i = 0; i < num_bytes; ++i) {
        vpr_dst[e++ ^ 1] = dmem[addr++];
    }
}

void lrv(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    u32 addr = gpr[base] + offset * 16;
    e += 16 - (addr & 15);
    addr &= 0xFF0;
    while (e < 16) {
        vpr_dst[e++ ^ 1] = dmem[addr++];
    }
}

void lsv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s16>(base, vt, e, offset);
}

void ltv(u32 base, u32 vt, u32 e, s32 offset)
{
    auto addr = gpr[base] + offset * 16;
    auto const wrap_addr = addr & ~7;
    addr = wrap_addr + (e + (addr & 8) & 15);
    auto const reg_base = vt & 0x18;
    auto reg_off = e >> 1;
    for (int i = 0; i < 8; ++i) {
        for (int j = 1; j >= 0; --j) {
            reinterpret_cast<u8*>(&vpr[reg_base + reg_off])[2 * i + j] = dmem[addr & 0xFFF];
            addr = addr == wrap_addr + 15 ? wrap_addr : addr + 1;
        }
        reg_off = reg_off + 1 & 7;
    }
}

void luv(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_dst = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    auto mem_offset = (addr & 7) - e; // todo: what if e > 8?
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        vpr_dst[i] = dmem[addr + (mem_offset + i & 15) & 0xFFF] << 7;
    }
}

void sbv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s8>(base, vt, e, offset);
}

void sdv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s64>(base, vt, e, offset);
}

void sfv(u32 base, u32 vt, u32 e, s32 offset)
{
    u16* vpr_src = (u16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto addr_offset = addr & 7;
    addr &= ~7;
    auto store = [addr, addr_offset](std::array<u8, 4> values) {
        for (int i = 0; i < 4; ++i) {
            dmem[addr + (addr_offset + 4 * i & 15) & 0xFFF] = values[i];
        }
    };
    auto store_elems = [vpr_src, store](std::array<u8, 4> elems) {
        store({
          u8(vpr_src[elems[0]] >> 7),
          u8(vpr_src[elems[1]] >> 7),
          u8(vpr_src[elems[2]] >> 7),
          u8(vpr_src[elems[3]] >> 7),
        });
    };
    switch (e) {
    case 0:
    case 15: store_elems({ 0, 1, 2, 3 }); break;
    case 1: store_elems({ 6, 7, 4, 5 }); break;
    case 4: store_elems({ 1, 2, 3, 0 }); break;
    case 5: store_elems({ 7, 4, 5, 6 }); break;
    case 8: store_elems({ 4, 5, 6, 7 }); break;
    case 11: store_elems({ 3, 0, 1, 2 }); break;
    case 12: store_elems({ 5, 6, 7, 4 }); break;
    default: store({ 0, 0, 0, 0 }); break;
    }
}

void shv(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto addr_offset = addr & 7;
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        auto byte = e + 2 * i;
        s16 val = vpr_src[byte & 15 ^ 1] << 1 | vpr_src[byte + 1 & 15 ^ 1] >> 7;
        dmem[addr + (addr_offset + 2 * i & 15) & 0xFFF] = u8(val);
    }
}

void slv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s32>(base, vt, e, offset);
}

void spv(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    for (auto elem = e; elem < e + 8; ++elem) {
        u8 val;
        if ((elem & 15) < 8) {
            val = vpr_src[elem << 1 & 0xE ^ 1];
        } else {
            val = reinterpret_cast<s16*>(vpr_src)[elem & 7] >> 7;
        }
        dmem[addr++ & 0xFFF] = val;
    }
}

void sqv(u32 base, u32 vt, u32 e, s32 offset)
{
    ASSUME(e < 16);
    u8 const* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    u32 addr_offset = addr & 15;
    for (auto elem = e; elem < e + (16 - addr_offset); ++elem) {
        dmem[addr++ & 0xFFF] = vpr_src[elem & 15 ^ 1];
    }
}

void srv(u32 base, u32 vt, u32 e, s32 offset)
{
    ASSUME(e < 16);
    u8 const* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    u32 addr_offset = addr & 15;
    u32 base_e = 16 - addr_offset;
    addr &= 0xFF0;
    for (auto elem = e; elem < e + addr_offset; ++elem) {
        dmem[addr++] = vpr_src[base_e + elem & 15 ^ 1];
    }
}

void ssv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s16>(base, vt, e, offset);
}

void stv(u32 base, u32 vt, u32 e, s32 offset)
{
    auto addr = gpr[base] + offset * 16;
    auto offset_addr = (addr & 7) - (e & ~1);
    auto elem = 16 - (e & ~1);
    addr &= ~7;
    auto const reg_start = vt & 0x18;
    for (auto reg = reg_start; reg < reg_start + 8; ++reg) {
        for (int i = 0; i < 2; ++i) {
            dmem[addr + (offset_addr++ & 15) & 0xFFF] = reinterpret_cast<u8*>(&vpr[reg])[elem++ & 15 ^ 1];
        }
    }
}

void suv(u32 base, u32 vt, u32 e, s32 offset)
{
    ASSUME(e < 16);
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    for (auto elem = e; elem < e + 8; ++elem) {
        u8 val;
        if ((elem & 15) < 8) {
            val = reinterpret_cast<s16*>(vpr_src)[elem & 7] >> 7;
        } else {
            val = vpr_src[elem << 1 & 0xE ^ 1];
        }
        dmem[addr++ & 0xFFF] = val;
    }
}

void swv(u32 base, u32 vt, u32 e, s32 offset)
{
    ASSUME(e < 16);
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    base = addr & 7;
    addr &= ~7;
    for (auto elem = e; elem < e + 16; ++elem) {
        dmem[(addr + (base++ & 15)) & 0xFFF] = vpr_src[elem & 15 ^ 1];
    }
}

void vabs(u32 vs, u32 vt, u32 vd, u32 e)
{
    /* If a lane is 0x8000, store 0x7FFF to vpr[vd], and 0x8000 to the accumulator. */
    m128i eq0 = _mm_cmpeq_epi16(vpr[vs], _mm_setzero_si128());
    m128i slt = _mm_srai_epi16(vpr[vs], 15);
    vpr[vd] = _mm_andnot_si128(eq0, GetVTBroadcast(vt, e));
    vpr[vd] = _mm_xor_si128(vpr[vd], slt);
    acc.low = _mm_sub_epi16(vpr[vd], slt);
    vpr[vd] = _mm_subs_epi16(vpr[vd], slt);
}

void vadd(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_add_epi16(vpr[vs], _mm_sub_epi16(vt_op, vco.lo));
    m128i op1 = _mm_subs_epi16(_mm_min_epi16(vpr[vs], vt_op), vco.lo);
    m128i op2 = _mm_max_epi16(vpr[vs], vt_op);
    vpr[vd] = _mm_adds_epi16(op1, op2);
    vco.lo = vco.hi = _mm_setzero_si128();
}

void vaddc(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    vpr[vd] = acc.low = _mm_add_epi16(vpr[vs], vt_op);
    vco.lo = _mm_cmplt_epu16(vpr[vd], vt_op); /* check carry */
    vco.hi = _mm_setzero_si128();
}

void vand(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_and_si128(vpr[vs], GetVTBroadcast(vt, e));
}

void vch(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    vco.lo = _mm_xor_si128(vpr[vs], vt_op);
    vco.lo = _mm_cmplt_epi16(vco.lo, _mm_setzero_si128());
    m128i nvt = _mm_xor_si128(vt_op, vco.lo);
    nvt = _mm_sub_epi16(nvt, vco.lo);
    m128i diff = _mm_sub_epi16(vpr[vs], nvt);
    m128i diff0 = _mm_cmpeq_epi16(diff, _mm_setzero_si128());
    m128i dlez = _mm_cmpgt_epi16(diff, _mm_setzero_si128());
    m128i dgez = _mm_or_si128(dlez, diff0);
    dlez = _mm_cmpeq_epi16(dlez, _mm_setzero_si128());
    m128i vtn = _mm_cmplt_epi16(vt_op, _mm_setzero_si128());
    vcc.hi = _mm_blendv_epi8(dgez, vtn, vco.lo);
    vcc.lo = _mm_blendv_epi8(vtn, dlez, vco.lo);
    vce = _mm_cmpeq_epi16(diff, vco.lo);
    vce = _mm_and_si128(vce, vco.lo);
    vco.hi = _mm_or_si128(diff0, vce);
    vco.hi = _mm_cmpeq_epi16(vco.hi, _mm_setzero_si128());
    m128i mask = _mm_blendv_epi8(vcc.hi, vcc.lo, vco.lo);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], nvt, mask);
}

void vcl(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i nvt = _mm_xor_si128(vt_op, vco.lo);
    nvt = _mm_sub_epi16(nvt, vco.lo);
    m128i diff = _mm_sub_epi16(vpr[vs], nvt);
    m128i ncarry = _mm_adds_epu16(vpr[vs], vt_op);
    ncarry = _mm_cmpeq_epi16(diff, ncarry);
    m128i nvce = _mm_cmpeq_epi16(vce, _mm_setzero_si128());
    m128i diff0 = _mm_cmpeq_epi16(diff, _mm_setzero_si128());
    m128i lec1 = _mm_and_si128(diff0, ncarry);
    lec1 = _mm_and_si128(nvce, lec1);
    m128i lec2 = _mm_or_si128(diff0, ncarry);
    lec2 = _mm_and_si128(vce, lec2);
    m128i leeq = _mm_or_si128(lec1, lec2);
    m128i geeq = _mm_subs_epu16(vt_op, vpr[vs]);
    geeq = _mm_cmpeq_epi16(geeq, _mm_setzero_si128());
    m128i le = _mm_andnot_si128(vco.hi, vco.lo);
    le = _mm_blendv_epi8(vcc.lo, leeq, le);
    m128i ge = _mm_or_si128(vco.lo, vco.hi);
    ge = _mm_blendv_epi8(geeq, vcc.hi, ge);
    m128i mask = _mm_blendv_epi8(ge, le, vco.lo);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], nvt, mask);
    vcc.hi = ge;
    vcc.lo = le;
    vco.lo = vco.hi = vce = _mm_setzero_si128();
}

void vcr(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i sign = _mm_srai_epi16(_mm_xor_si128(vpr[vs], vt_op), 15);
    m128i dlez = _mm_add_epi16(_mm_and_si128(vpr[vs], sign), vt_op);
    vcc.lo = _mm_srai_epi16(dlez, 15);
    m128i dgez = _mm_min_epi16(_mm_or_si128(vpr[vs], sign), vt_op);
    vcc.hi = _mm_cmpeq_epi16(dgez, vt_op);
    m128i mask = _mm_blendv_epi8(vcc.hi, vcc.lo, sign);
    m128i nvt = _mm_xor_si128(vt_op, sign);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], nvt, mask);
    vco.lo = vco.hi = vce = _mm_setzero_si128();
}

void veq(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_andnot_si128(vco.hi, eq);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    vcc.hi = vco.lo = vco.hi = _mm_setzero_si128();
}

void vge(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    m128i neg = _mm_andnot_si128(_mm_and_si128(vco.lo, vco.hi), eq);
    m128i gt = _mm_cmpgt_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_or_si128(neg, gt);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    vcc.hi = vco.lo = vco.hi = _mm_setzero_si128();
}

void vlt(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    m128i neg = _mm_and_si128(_mm_and_si128(vco.lo, vco.hi), eq);
    m128i lt = _mm_cmplt_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_or_si128(neg, lt);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    vcc.hi = vco.lo = vco.hi = _mm_setzero_si128();
}

template<bool vmacf> void vmacfu(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two to get a 33-bit product. Sign-extend to 48 bits, and add to the accumulator. */
    m128i low_carry = _mm_srli_epi16(low, 15);
    m128i high_carry = _mm_srai_epi16(high, 15);
    low = _mm_add_epi16(low, low);
    high = _mm_add_epi16(high, high);
    high = _mm_add_epi16(high, low_carry);
    AddToAcc(low, high, high_carry);
    if constexpr (vmacf) {
        vpr[vd] = ClampSigned(acc.mid, acc.high);
    } else { // vmacu
        vpr[vd] = ClampUnsigned(acc.mid, acc.high);
    }
}

void vmacf(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmacfu<true>(vs, vt, vd, e);
}

void vmacq(u32 vd)
{
    /* Given result = acc.mid | acc.high << 16: if !result.5, add 32 if result < 0, else if result >= 32,
     * subtract 32. */
    m128i mask = _mm_set1_epi16(32);
    m128i addend = _mm_and_si128(_mm_not_si128(acc.mid), mask); /* 0 or 32 */
    m128i acc_high_gtz = _mm_cmpgt_epi16(acc.high, _mm_setzero_si128());
    m128i acc_high_ltz = _mm_cmplt_epi16(acc.high, _mm_setzero_si128());
    m128i acc_high_eqz = _mm_cmpeq_epi16(acc.high, _mm_setzero_si128());
    m128i acc_mid_geu32 = _mm_cmpgt_epi16(_mm_srli_epi16(acc.mid, 5), _mm_setzero_si128());
    /* Possibly subtract 32. */
    m128i neg_addend = _mm_and_si128(addend, _mm_or_si128(acc_high_gtz, _mm_and_si128(acc_high_eqz, acc_mid_geu32)));
    m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_sub_epi16(acc.mid, neg_addend);
    m128i borrow = _mm_cmpgt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_add_epi16(acc.high, borrow); /* same as subtracting 0 or 1 */
    /* Possibly add 32. No carry in acc.mid, since bit 5 is clear if addend != 0. */
    m128i pos_addend = _mm_and_si128(addend, acc_high_ltz);
    acc.mid = _mm_add_epi16(acc.mid, pos_addend);
    m128i clamp_input_low = _mm_or_si128(_mm_srli_epi16(acc.mid, 1), _mm_slli_epi16(acc.high, 15));
    m128i clamp_input_high = _mm_srai_epi16(acc.high, 1);
    vpr[vd] = _mm_and_si128(_mm_set1_epi16(~0xF), ClampSigned(clamp_input_low, clamp_input_high));
}

void vmacu(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmacfu<false>(vs, vt, vd, e);
}

void vmadh(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    AddToAccFromMid(low, high);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

void vmadl(u32 vs, u32 vt, u32 vd, u32 e)
{
    AddToAcc(_mm_mulhi_epu16(vpr[vs], GetVTBroadcast(vt, e)));
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    m128i acc_high_neg = _mm_srai_epi16(acc.high, 15);
    m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

void vmadm(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epu16_epi16(vt_op, vpr[vs]);
    m128i sign_ext = _mm_srai_epi16(high, 15);
    AddToAcc(low, high, sign_ext);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

void vmadn(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epu16_epi16(vpr[vs], vt_op);
    m128i sign_ext = _mm_srai_epi16(high, 15);
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    AddToAcc(low, high, sign_ext);
    m128i acc_high_neg = _mm_srai_epi16(acc.high, 15);
    m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

void vmov(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    m128i vte = GetVTBroadcast(vt, vt_e);
    reinterpret_cast<s16*>(&vpr[vd])[vd_e] = reinterpret_cast<s16*>(&vte)[vd_e];
    acc.low = vte;
}

void vmrg(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_blendv_epi8(GetVTBroadcast(vt, e), vpr[vs], vcc.lo);
    std::memset(&vco, 0, sizeof(vco));
}

void vmudh(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_setzero_si128(); /* seems necessary given tests */
    acc.mid = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.high = _mm_mulhi_epi16(vpr[vs], vt_op);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

void vmudl(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_mulhi_epu16(vpr[vs], GetVTBroadcast(vt, e));
    acc.mid = acc.high = _mm_setzero_si128();
}

void vmudm(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.mid = _mm_mulhi_epu16_epi16(vt_op, vpr[vs]);
    acc.high = _mm_srai_epi16(acc.mid, 15);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

void vmudn(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.mid = _mm_mulhi_epu16_epi16(vpr[vs], vt_op);
    acc.high = _mm_srai_epi16(acc.mid, 15);
    vpr[vd] = acc.low;
}

void vmulf(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmulfu<true>(vs, vt, vd, e);
}

// vmulf, vmulu
template<bool vmulf> void vmulfu(u32 vs, u32 vt, u32 vd, u32 e)
{
    /* Tests indicate the following behavior. The product is 33-bits, and product + 0x8000 is
           also kept in 33 bits. Example:
           oper1 = 0xFFEE, oper2 = 0x0011 => prod(32..0) = 0x1'FFFF'FD9C.
           prod(32..0) + 0x8000 = 0x0'0000'7D9C. */
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two */
    m128i low_carry_mul = _mm_srli_epi16(low, 15); /* note: either 0 or 1 */
    m128i high_carry_mul = _mm_srai_epi16(high, 15); /* note: either 0 or 0xFFFF */
    low = _mm_add_epi16(low, low);
    high = _mm_add_epi16(high, high);
    high = _mm_add_epi16(high, low_carry_mul);
    /* add $8000 */
    low = _mm_add_epi16(low, m128i_epi16_sign_mask);
    m128i low_carry_add = _mm_cmpge_epi16(low, _mm_setzero_si128()); /* carry if low >= 0 */
    high = _mm_sub_epi16(high, low_carry_add);
    m128i high_carry_add = _mm_and_si128(_mm_cmpeq_epi16(high, _mm_setzero_si128()), low_carry_add);
    acc.low = low;
    acc.mid = high;
    /* The XOR achieves the correct 33-bit overflow behaviour and subsequent sign-extension to 48 bits.
       E.g., if prod(32) = 1, but the mid lane overflowed when adding 0x8000, then acc(47..32) = 0.
       Notice that high carries are always computed as either 0 or 0xFFFF. */
    acc.high = _mm_xor_si128(high_carry_mul, high_carry_add);
    if constexpr (vmulf) {
        vpr[vd] = ClampSigned(acc.mid, acc.high);
    } else {
        vpr[vd] = ClampUnsigned(acc.mid, acc.high);
    }
}

void vmulq(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* add 31 to product if product < 0 */
    m128i addend = _mm_and_si128(_mm_srai_epi16(high, 15), _mm_set1_epi16(0x1F));
    low = _mm_add_epi16(low, addend);
    m128i low_carry = _mm_cmplt_epu16(low, addend);
    high = _mm_sub_epi16(high, low_carry);
    acc.low = _mm_setzero_si128();
    acc.mid = low;
    acc.high = high;
    vpr[vd] = _mm_and_si128(_mm_set1_epi16(~0xF), ClampSigned(_mm_srai_epi16(acc.mid, 1), _mm_srai_epi16(acc.high, 1)));
}

void vmulu(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmulfu<false>(vs, vt, vd, e);
}

void vnand(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_nand_si128(vpr[vs], GetVTBroadcast(vt, e));
}

void vne(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_or_si128(vco.hi, _mm_not_si128(eq));
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    std::memset(&vco, 0, sizeof(vco));
    std::memset(&vcc.hi, 0, sizeof(vcc.hi));
}

void vnop()
{
}

void vnor(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_nor_si128(vpr[vs], GetVTBroadcast(vt, e));
}

void vnxor(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_nxor_si128(vpr[vs], GetVTBroadcast(vt, e));
}

void vor(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_or_si128(vpr[vs], GetVTBroadcast(vt, e));
}

void vrcp(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    acc.low = GetVTBroadcast(vt, vt_e);
    s32 input = _mm_getlane_epi16(&vpr[vt], vt_e & 7);
    s32 result = Rcp(input);
    _mm_setlane_epi16(&vpr[vd], vd_e & 7, s16(result));
    div_out = u16(result >> 16);
    div_dp = 0;
}

void vrcph(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    acc.low = GetVTBroadcast(vt, vt_e);
    _mm_setlane_epi16(&vpr[vd], vd_e & 7, div_out);
    div_in = _mm_getlane_epi16(&vpr[vt], vt_e & 7);
    div_dp = 1;
}

void vrcpl(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    acc.low = GetVTBroadcast(vt, vt_e);
    u16 vte = _mm_getlane_epi16(&vpr[vt], vt_e & 7);
    s32 input = div_dp ? vte | div_in << 16 : s16(vte);
    s32 result = Rcp(input);
    _mm_setlane_epi16(&vpr[vd], vd_e & 7, s16(result));
    div_out = u16(result >> 16);
    div_in = div_dp = 0;
}

template<bool p> void vrnd(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    m128i low, mid, high, cond;
    if (vd_e & 1) { /* sign_extend(VT << 16) */
        low = _mm_setzero_si128();
        mid = GetVTBroadcast(vt, vt_e);
        high = _mm_srai_epi16(mid, 16);
    } else { /* sign_extend(VT) */
        low = GetVTBroadcast(vt, vt_e);
        mid = high = _mm_srai_epi16(low, 16);
    }
    cond = _mm_srai_epi16(acc.high, 16);
    if constexpr (p) { // vrndp
        cond = _mm_cmpeq_epi16(cond, _mm_setzero_si128());
    }
    AddToAccCond(low, mid, high, cond);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

void vrndn(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrnd<false>(vt, vt_e, vd, vd_e);
}

void vrndp(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrnd<true>(vt, vt_e, vd, vd_e);
}

void vrsq(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    acc.low = GetVTBroadcast(vt, vt_e);
    s32 input = _mm_getlane_epi16(&vpr[vt], vt_e & 7);
    s32 result = Rsq(input);
    _mm_setlane_epi16(&vpr[vd], vd_e & 7, s16(result));
    div_out = result >> 16;
    div_dp = 0;
}

void vrsqh(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrcph(vt, vt_e, vd, vd_e);
}

void vrsql(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    acc.low = GetVTBroadcast(vt, vt_e);
    u16 vte = _mm_getlane_epi16(&vpr[vt], vt_e & 7);
    s32 input = div_dp ? vte | div_in << 16 : s16(vte);
    s32 result = Rsq(input);
    _mm_setlane_epi16(&vpr[vd], vd_e & 7, s16(result));
    div_out = u16(result >> 16);
    div_in = div_dp = 0;
}

void vsar(u32 vd, u32 e)
{
    vpr[vd] = [e] {
        switch (e) {
        case 8: return acc.high;
        case 9: return acc.mid;
        case 10: return acc.low;
        default: return _mm_setzero_si128();
        }
    }();
}

void vsub(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i diff = _mm_sub_epi16(vt_op, vco.lo);
    acc.low = _mm_sub_epi16(vpr[vs], diff);
    m128i clamped_diff = _mm_subs_epi16(vt_op, vco.lo);
    m128i overflow = _mm_cmpgt_epi16(clamped_diff, diff);
    vpr[vd] = _mm_subs_epi16(vpr[vs], clamped_diff);
    vpr[vd] = _mm_adds_epi16(vpr[vd], overflow);
    vco.lo = vco.hi = _mm_setzero_si128();
}

void vsubc(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    vco.lo = _mm_cmplt_epu16(vpr[vs], vt_op); /* check borrow */
    vpr[vd] = acc.low = _mm_sub_epi16(vpr[vs], vt_op);
    vco.hi = _mm_or_si128(vco.lo, _mm_cmpneq_epi16(vpr[vd], _mm_setzero_si128()));
}

void vxor(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_xor_si128(vpr[vs], GetVTBroadcast(vt, e));
}

void vzero(u32 vs, u32 vt, u32 vd, u32 e)
{
    acc.low = _mm_add_epi16(vpr[vs], GetVTBroadcast(vt, e));
    vpr[vd] = _mm_setzero_si128();
}

} // namespace n64::rsp

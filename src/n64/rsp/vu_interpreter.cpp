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

using m128i = __m128i;

namespace n64::rsp {

using enum CpuImpl;

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
    /* for i in 0..7
            accumulator<i>(47..0) += low<i>
        endfor
    */
    m128i prev_acc_low = acc.low;
    acc.low = _mm_add_epi16(acc.low, low);
    m128i low_carry = _mm_cmplt_epu16(acc.low, prev_acc_low);
    acc.mid = _mm_sub_epi16(acc.mid, low_carry);
    m128i mid_carry = _mm_and_si128(low_carry, _mm_cmpeq_epi16(acc.mid, _mm_setzero_si128()));
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAcc(m128i low, m128i mid)
{
    /* for i in 0..7
            accumulator<i>(47..0) += mid<i> << 16 | low<i>
       endfor
    */
    AddToAcc(low);
    m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_add_epi16(acc.mid, mid);
    m128i mid_carry = _mm_cmplt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAcc(m128i low, m128i mid, m128i high)
{
    /* for i in 0..7
            accumulator<i>(47..0) += high<i> << 32 | mid<i> << 16 | low<i>
       endfor
    */
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
    /* for i in 0..7
            accumulator<i>(47..0) += high<i> << 32 | mid<i> << 16
       endfor
    */
    m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_add_epi16(acc.mid, mid);
    m128i mid_carry = _mm_cmplt_epu16(acc.mid, prev_acc_mid);
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
    s32 mask = input >> 31;
    s32 data = input ^ mask;
    if (input > -32768) {
        data -= mask;
    }
    if (data == 0) {
        return 0x7FFF'FFFF;
    } else if (input == -32768) {
        return 0xFFFF'0000;
    } else {
        u32 shift = std::countl_zero(u32(data));
        u32 index = (u64(data) << shift & 0x7FC0'0000) >> 22;
        s32 result = rcp_rom[index];
        result = (0x10000 | result) << 14;
        return (result >> (31 - shift)) ^ mask;
    }
}

s32 Rsq(s32 input)
{
    if (input == 0) {
        return 0x7FFF'FFFF;
    } else if (input == -32768) {
        return 0xFFFF'0000;
    } else {
        auto unsigned_input = to_unsigned(std::abs(input));
        auto lshift = std::countl_zero(unsigned_input) + 1;
        auto rshift = (32 - lshift) >> 1;
        auto index = (unsigned_input << lshift) >> 24;
        auto rom = rsq_rom[(index | ((lshift & 1) << 8))];
        s32 result = ((0x10000 | rom) << 14) >> rshift;
        if (unsigned_input != input) {
            return ~result;
        } else {
            return result;
        }
    }
    // if (input == 0) {
    //     return 0x7FFF'FFFF;
    // } else if (input == 0xFFFF'8000) {
    //     return 0xFFFF0000;
    // } else if (input > 0xFFFF'8000) {
    //     input--;
    // }
    // s32 mask = input >> 31;
    // input ^= mask;
    // u32 shift = std::countl_zero(u32(input));
    // u32 index = input << shift >> 24 | (shift & 1) << 8;
    // u32 rom = u16(rsq_rom[index]) << 14;
    // u32 r_shift = (32 - shift) >> 1;
    // u32 result = (0x4000'0000 | rom) >> r_shift;
    // return result ^ mask;
}

template<> void cfc2<Interpreter>(u32 rt, u32 vs)
{
    /* GPR(31..0) = sign_extend(CTRL(15..0)) */
    vs = std::min(vs & 3, 2u);
    int lo = _mm_movemask_epi8(_mm_packs_epi16(ctrl_reg[vs].lo, _mm_setzero_si128()));
    int hi = _mm_movemask_epi8(_mm_packs_epi16(ctrl_reg[vs].hi, _mm_setzero_si128()));
    gpr.set(rt, s16(hi << 8 | lo));
}

template<> void ctc2<Interpreter>(u32 rt, u32 vs)
{
    /* CTRL(15..0) = GPR(15..0) */
    /* Control registers (16-bit) are encoded in two m128i. Each lane represents one bit. */
    static constexpr std::array lanes = {
        s64(0x0000'0000'0000'0000),
        s64(0x0000'0000'0000'FFFF),
        s64(0x0000'0000'FFFF'0000),
        s64(0x0000'0000'FFFF'FFFF),
        s64(0x0000'FFFF'0000'0000),
        s64(0x0000'FFFF'0000'FFFF),
        s64(0x0000'FFFF'FFFF'0000),
        s64(0x0000'FFFF'FFFF'FFFF),
        s64(0xFFFF'0000'0000'0000),
        s64(0xFFFF'0000'0000'FFFF),
        s64(0xFFFF'0000'FFFF'0000),
        s64(0xFFFF'0000'FFFF'FFFF),
        s64(0xFFFF'FFFF'0000'0000),
        s64(0xFFFF'FFFF'0000'FFFF),
        s64(0xFFFF'FFFF'FFFF'0000),
        s64(0xFFFF'FFFF'FFFF'FFFF),
    };
    vs = std::min(vs & 3, 2u);
    s32 r = gpr[rt];
    ctrl_reg[vs].lo = _mm_set_epi64x(lanes[r >> 4 & 0xF], lanes[r >> 0 & 0xF]);
    if (vs < 2) {
        ctrl_reg[vs].hi = _mm_set_epi64x(lanes[r >> 12 & 0xF], lanes[r >> 8 & 0xF]);
    }
}

template<> void mfc2<Interpreter>(u32 rt, u32 vs, u32 e)
{
    /* GPR[rt](31..0) = sign_extend(VS<elem>(15..0)) */
    u8* v = (u8*)(&vpr[vs]);
    gpr.set(rt, s16(v[e] << 8 | v[e + 1 & 15]));
}

template<> void mtc2<Interpreter>(u32 rt, u32 vs, u32 e)
{
    /* VS<elem>(15..0) = GPR[rt](15..0) */
    u8* v = (u8*)(&vpr[vs]);
    v[e] = u8(gpr[rt] >> 8);
    if (e < 15) v[e + 1] = u8(gpr[rt]);
}

template<> void lbv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s8>(base, vt, e, offset);
}

template<> void ldv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s64>(base, vt, e, offset);
}

template<> void lfv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto mem_offset = (addr & 7) - e;
    addr &= ~7;
    s16 tmp[8];
    for (int i = 0; i < 4; ++i) {
        tmp[i] = dmem[(addr + (mem_offset + 4 * i & 15) ^ 1) & 0xFFF] << 7;
        tmp[i + 4] = dmem[(addr + (mem_offset + 4 * i + 8 & 15) ^ 1) & 0xFFF] << 7;
    }
    for (auto elem = e; elem < std::min(e + 8, 16u); ++elem) {
        vpr_dst[elem] = reinterpret_cast<u8*>(tmp)[elem];
    }
}

template<> void lhv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_dst = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto mem_offset = (addr & 7) - e;
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        vpr_dst[i] = dmem[addr + (mem_offset + 2 * i & 15) & 0xFFF] << 7;
    }
}

template<> void llv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s32>(base, vt, e, offset);
}

template<> void lpv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_dst = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    auto mem_offset = (addr & 7) - e;
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        vpr_dst[i] = dmem[addr + (mem_offset + i & 15) & 0xFFF] << 8;
    }
}

template<> void lqv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    u32 addr = gpr[base] + offset * 16;
    u32 num_bytes = 16 - std::max(addr & 0xF, e); // == std::min(16 - (addr & 0xF), 16 - element)
    addr &= 0xFFF;
    for (u32 i = 0; i < num_bytes; ++i) {
        vpr_dst[e++ ^ 1] = dmem[addr++];
    }
}

template<> void lrv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    u32 addr = gpr[base] + offset * 16;
    e += 16 - (addr & 0xF);
    if (e < 16) {
        u32 num_bytes = 16 - e;
        addr &= 0xFF0;
        for (u32 i = 0; i < num_bytes; ++i) {
            vpr_dst[e++ ^ 1] = dmem[addr++];
        }
    }
}

template<> void lsv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s16>(base, vt, e, offset);
}

template<> void ltv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    auto addr = gpr[base] + offset * 16;
    auto const wrap_addr = addr & ~7;
    addr = wrap_addr + (e + (addr & 8) & 15);
    auto const reg_base = vt & 0x18;
    auto reg_off = e >> 1;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 2; ++j) {
            reinterpret_cast<u8*>(&vpr[reg_base + reg_off])[2 * i + j] = dmem[addr & 0xFFF];
            addr = addr == wrap_addr + 15 ? wrap_addr : addr + 1;
        }
        reg_off = reg_off + 1 & 7;
    }
}

template<> void luv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_dst = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    auto mem_offset = (addr & 7) - e;
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        vpr_dst[i] = dmem[addr + (mem_offset + i & 15) & 0xFFF] << 7;
    }
}

template<> void lwv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    for (auto elem = 16 - e; elem < 16 + e; ++elem) {
        vpr_dst[elem & 15 ^ 1] = dmem[addr & 0xFFF ^ 1];
        addr += 4;
    }
}

template<> void sbv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s8>(base, vt, e, offset);
}

template<> void sdv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s64>(base, vt, e, offset);
}

template<> void sfv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    auto addr = gpr[base] + offset * 16;
    auto mem_offset = addr & 7;
    addr &= ~7;
    auto store = [addr, mem_offset, vt](std::array<u8, 4> elems) {
        for (int i = 0; i < 4; ++i) {
            dmem[addr + (mem_offset + 4 * i & 15) & 0xFFF] = reinterpret_cast<u16*>(&vpr[vt])[elems[i]] >> 7;
        }
    };
    switch (e) {
    case 0:
    case 15: store({ 0, 1, 2, 3 }); break;
    case 1: store({ 6, 7, 4, 5 }); break;
    case 4: store({ 1, 2, 3, 0 }); break;
    case 5: store({ 7, 4, 5, 6 }); break;
    case 8: store({ 4, 5, 6, 7 }); break;
    case 11: store({ 3, 0, 1, 2 }); break;
    case 12: store({ 5, 6, 7, 4 }); break;
    default: store({ 0, 0, 0, 0 }); break;
    }
}

template<> void shv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    auto addr_offset = addr & 7;
    addr &= ~7;
    for (int i = 0; i < 8; ++i) {
        auto src_e = e + 2 * i;
        s16 val;
        std::memcpy(&val, vpr_src + src_e, 2);
        if (src_e & 1) val = std::byteswap(val);
        dmem[addr + (addr_offset + 2 * i & 15) & 0xFFF] = val >> 7;
    }
}

template<> void slv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s32>(base, vt, e, offset);
}

template<> void spv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    for (auto elem = e; elem < e + 8; ++elem) {
        u8 val;
        if ((elem & 15) < 8) {
            val = vpr_src[elem << 1 & 0xE ^ 1];
        } else {
            val = *(reinterpret_cast<s16*>(vpr_src) + (elem & 7)) >> 7;
        }
        dmem[addr++ & 0xFFF] = val;
    }
}

template<> void sqv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8 const* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    u32 num_bytes = 16 - (addr & 0xF);
    u32 base_element = 0;
    addr &= 0xFFF;
    for (u32 i = 0; i < num_bytes; ++i) {
        dmem[addr++] = *(vpr_src + ((base_element + e++ & 0xF) ^ 1));
    }
}

template<> void srv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8 const* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    u32 num_bytes = addr & 0xF;
    u32 base_element = 16 - (addr & 0xF);
    addr &= 0xFF0;
    for (u32 i = 0; i < num_bytes; ++i) {
        dmem[addr++] = *(vpr_src + ((base_element + e++ & 0xF) ^ 1));
    }
}

template<> void ssv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s16>(base, vt, e, offset);
}

template<> void stv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    auto addr = gpr[base] + offset * 16;
    auto offset_addr = (addr & 7) - (e & ~1);
    auto elem = 16 - (e & ~1);
    for (auto reg = vt & 0x18; reg < (vt & 0x18) + 8; ++reg) {
        for (int i = 0; i < 2; ++i) {
            dmem[addr + (offset_addr++ & 15) & 0xFFF] = reinterpret_cast<u8*>(&vpr[reg])[elem++ & 15];
        }
    }
}

template<> void suv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8;
    for (auto elem = e; elem < e + 8; ++elem) {
        u8 val;
        if ((elem & 15) < 8) {
            val = *(reinterpret_cast<s16*>(vpr_src) + (elem & 7)) >> 7;
        } else {
            val = vpr_src[elem << 1 & 0xE ^ 1];
        }
        dmem[addr++ & 0xFFF] = val;
    }
}

template<> void swv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    base = addr & 7;
    addr &= ~7;
    for (auto current_elem = e; current_elem < e + 16; ++current_elem) {
        dmem[(addr + (base++ & 0xF)) & 0xFFF] = *(vpr_src + ((current_elem & 0xF) ^ 1));
    }
}

template<> void vabs<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    /* If a lane is 0x8000, store 0x7FFF to vpr[vd], and 0x8000 to the accumulator. */
    m128i eq0 = _mm_cmpeq_epi16(vpr[vs], _mm_setzero_si128());
    m128i slt = _mm_srai_epi16(vpr[vs], 15);
    vpr[vd] = _mm_andnot_si128(eq0, GetVTBroadcast(vt, e));
    vpr[vd] = _mm_xor_si128(vpr[vd], slt);
    acc.low = _mm_sub_epi16(vpr[vd], slt);
    vpr[vd] = _mm_subs_epi16(vpr[vd], slt);
}

template<> void vadd<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_add_epi16(vpr[vs], _mm_sub_epi16(vt_op, vco.lo));
    m128i op1 = _mm_subs_epi16(_mm_min_epi16(vpr[vs], vt_op), vco.lo);
    m128i op2 = _mm_max_epi16(vpr[vs], vt_op);
    vpr[vd] = _mm_adds_epi16(op1, op2);
    vco.lo = vco.hi = _mm_setzero_si128();
}

template<> void vaddc<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    vpr[vd] = acc.low = _mm_add_epi16(vpr[vs], vt_op);
    vco.lo = _mm_cmplt_epu16(vpr[vd], vt_op); /* check carry */
    vco.hi = _mm_setzero_si128();
}

template<> void vand<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_and_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vch<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i neg_vt = _mm_neg_epi16(vt_op);
    vco.lo = _mm_srai_epi16(_mm_xor_si128(vpr[vs], vt_op), 15);
    m128i vt_abs = _mm_blendv_epi8(vt_op, neg_vt, vco.lo);
    vce = _mm_and_si128(vco.lo, _mm_cmpeq_epi16(vpr[vs], _mm_sub_epi16(neg_vt, _mm_set1_epi16(1))));
    vco.hi = _mm_not_si128(_mm_or_si128(vce, _mm_cmpeq_epi16(vpr[vs], vt_abs)));
    vcc.lo = _mm_cmple_epi16(vpr[vs], neg_vt);
    vcc.hi = _mm_cmpge_epi16(vpr[vs], vt_op);
    m128i clip = _mm_blendv_epi8(vcc.hi, vcc.lo, vco.lo);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], vt_abs, clip);
}

template<> void vcl<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    vcc.hi = _mm_blendv_epi8(_mm_cmpge_epu16(vpr[vs], vt_op), vcc.hi, _mm_or_si128(vco.lo, vco.hi));
    m128i neg_vt = _mm_neg_epi16(vt_op);
    m128i le = _mm_cmple_epu16(vpr[vs], neg_vt);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], neg_vt);
    vcc.lo = _mm_blendv_epi8(vcc.lo, _mm_blendv_epi8(eq, le, vce), _mm_andnot_si128(vco.hi, vco.lo));
    m128i clip = _mm_blendv_epi8(vcc.hi, vcc.lo, vco.lo);
    m128i vt_abs = _mm_blendv_epi8(vt_op, neg_vt, vco.lo);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], vt_abs, clip);
    vco.lo = vco.hi = vcc.lo = _mm_setzero_si128();
}

template<> void vcr<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i sign = _mm_srai_epi16(_mm_xor_si128(vpr[vs], vt_op), 15);
    m128i dlez = _mm_add_epi16(_mm_and_si128(vpr[vs], sign), vt_op);
    vcc.lo = _mm_srai_epi16(dlez, 15);
    m128i dgez = _mm_min_epi16(_mm_or_si128(vpr[vs], sign), vt_op);
    vcc.hi = _mm_cmpeq_epi16(dgez, vt_op);
    m128i nvt = _mm_xor_si128(vt_op, sign);
    m128i mask = _mm_blendv_epi8(vcc.hi, vcc.lo, sign);
    acc.low = _mm_blendv_epi8(vpr[vs], nvt, mask);
    vpr[vd] = acc.low;
    vco.lo = vco.hi = vce = _mm_setzero_si128();
}

template<> void veq<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_andnot_si128(vco.hi, eq);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    vcc.hi = vco.lo = vco.hi = _mm_setzero_si128();
}

template<> void vge<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    m128i neg = _mm_andnot_si128(_mm_and_si128(vco.lo, vco.hi), eq);
    m128i gt = _mm_cmpgt_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_or_si128(neg, gt);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    vcc.hi = vco.lo = vco.hi = _mm_setzero_si128();
}

template<> void vlt<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    m128i neg = _mm_and_si128(_mm_and_si128(vco.lo, vco.hi), eq);
    m128i lt = _mm_cmplt_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_or_si128(neg, lt);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    vcc.hi = vco.lo = vco.hi = _mm_setzero_si128();
}

template<> void vmacf<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two to get a 33-bit product. Sign-extend to 48 bits, and add to the accumulator. */
    m128i low_carry = _mm_srli_epi16(low, 15);
    m128i high_carry = _mm_srai_epi16(high, 15);
    low = _mm_slli_epi16(low, 1);
    high = _mm_slli_epi16(high, 1);
    high = _mm_add_epi16(high, low_carry);
    AddToAcc(low, high, high_carry);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmacq<Interpreter>(u32 vd)
{
    /* Given result = acc.mid | acc.high << 16: if !result.5, add 32 if result < 0, else if result >= 32,
     * subtract 32. */
    m128i mask = _mm_set1_epi16(32);
    m128i nott = _mm_not_si128(acc.mid);
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

template<> void vmacu<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two to get a 33-bit product. Sign-extend to 48 bits, and add to the accumulator. */
    m128i low_carry = _mm_srli_epi16(low, 15);
    m128i high_carry = _mm_srai_epi16(high, 15);
    low = _mm_slli_epi16(low, 1);
    high = _mm_slli_epi16(high, 1);
    high = _mm_add_epi16(high, low_carry);
    AddToAcc(low, high, high_carry);
    vpr[vd] = ClampUnsigned(acc.mid, acc.high);
}

template<> void vmadh<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    AddToAccFromMid(low, high);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmadl<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    AddToAcc(_mm_mulhi_epu16(vpr[vs], GetVTBroadcast(vt, e)));
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

template<> void vmadm<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epu16_epi16(vt_op, vpr[vs]);
    m128i sign_ext = _mm_srai_epi16(high, 15);
    AddToAcc(low, high, sign_ext);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmadn<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epu16_epi16(vpr[vs], vt_op);
    m128i sign_ext = _mm_srai_epi16(high, 15);
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    AddToAcc(low, high, sign_ext);
    m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

template<> void vmov<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    reinterpret_cast<s16*>(&vpr[vd])[vd_e] = reinterpret_cast<s16*>(&vpr[vt])[vt_e];
    acc.low = vpr[vt];
}

template<> void vmrg<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_blendv_epi8(GetVTBroadcast(vt, e), vpr[vs], vcc.lo);
    std::memset(&vco, 0, sizeof(vco));
}

template<> void vmudh<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_setzero_si128(); /* seems necessary given tests */
    acc.mid = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.high = _mm_mulhi_epi16(vpr[vs], vt_op);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmudl<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_mulhi_epu16(vpr[vs], GetVTBroadcast(vt, e));
    acc.mid = acc.high = _mm_setzero_si128();
}

template<> void vmudm<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.mid = _mm_mulhi_epu16_epi16(vt_op, vpr[vs]);
    acc.high = _mm_srai_epi16(acc.mid, 15);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmudn<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.mid = _mm_mulhi_epu16_epi16(vpr[vs], vt_op);
    acc.high = _mm_srai_epi16(acc.mid, 15);
    m128i is_sign_ext = _mm_set1_epi64x(s64(-1));
    m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

template<> void vmulf<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
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
    low = _mm_slli_epi16(low, 1);
    high = _mm_slli_epi16(high, 1);
    high = _mm_add_epi16(high, low_carry_mul);
    /* add $8000 */
    low = _mm_add_epi16(low, m128i_epi16_sign_mask);
    m128i low_carry_add = _mm_cmpgt_epi16(low, _mm_setzero_si128()); /* carry if low >= 0 */
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

template<> void vmulq<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* add 31 to product if product < 0 */
    m128i addend = _mm_and_si128(_mm_srai_epi16(high, 15), _mm_set1_epi16(0x1F));
    low = _mm_add_epi16(low, addend);
    m128i low_carry = _mm_srli_epi16(_mm_cmplt_epu16(low, addend), 15);
    high = _mm_add_epi16(high, low_carry);
    acc.low = _mm_setzero_si128();
    acc.mid = low;
    acc.high = high;
    vpr[vd] = _mm_and_si128(_mm_set1_epi16(~0xF), ClampSigned(_mm_srai_epi16(acc.mid, 1), _mm_srai_epi16(acc.high, 1)));
}

template<> void vmulu<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmulfu<false>(vs, vt, vd, e);
}

template<> void vnand<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_nand_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vne<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    vcc.lo = _mm_or_si128(vco.hi, _mm_cmpneq_epi16(vpr[vs], vt_op));
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.lo); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    std::memset(&vco, 0, sizeof(vco));
    std::memset(&vcc.hi, 0, sizeof(vcc.hi));
}

template<> void vnop<Interpreter>()
{
}

template<> void vnor<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_nor_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vnxor<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_nxor_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vor<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_or_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vrcp<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    s32 input = _mm_getlane_epi16(&vpr[vt], vt_e);
    s32 result = Rcp(input);
    _mm_setlane_epi16(&vpr[vd], vd_e, s16(result));
    div_out = result >> 16 & 0xFFFF;
    div_dp = 0;
    acc.low = vpr[vt];
}

template<> void vrcph<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    _mm_setlane_epi16(&vpr[vd], vd_e, div_out);
    div_in = _mm_getlane_epi16(&vpr[vt], vt_e);
    div_dp = 1;
    acc.low = vpr[vt];
}

template<> void vrcpl<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    u16 vte = _mm_getlane_epi16(&vpr[vt], vt_e);
    s32 input = div_dp ? vte | div_in << 16 : s16(vte);
    s32 result = Rcp(input);
    _mm_setlane_epi16(&vpr[vd], vd_e, s16(result));
    div_out = result >> 16 & 0xFFFF;
    div_in = div_dp = 0;
    acc.low = vpr[vt];
}

template<bool p> void vrnd(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    m128i low, mid, high, cond;
    if (vd_e & 1) { /* sign_extend(VT << 16) */
        low = _mm_setzero_si128();
        mid = vpr[vt];
        high = _mm_srai_epi16(mid, 16);
    } else { /* sign_extend(VT) */
        low = vpr[vt];
        mid = high = _mm_srai_epi16(low, 16);
    }
    cond = _mm_srai_epi16(acc.high, 16);
    if constexpr (p) { // vrndp
        cond = _mm_cmpeq_epi16(cond, _mm_setzero_si128());
    }
    AddToAccCond(low, mid, high, cond);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vrndn<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrnd<false>(vt, vt_e, vd, vd_e);
}

template<> void vrndp<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrnd<true>(vt, vt_e, vd, vd_e);
}

template<> void vrsq<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    s32 input = _mm_getlane_epi16(&vpr[vt], vt_e);
    s32 result = Rsq(input);
    _mm_setlane_epi16(&vpr[vd], vd_e, s16(result));
    div_out = result >> 16;
    div_dp = 0;
    acc.low = vpr[vt];
}

template<> void vrsqh<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    _mm_setlane_epi16(&vpr[vd], vd_e, div_out);
    div_in = _mm_getlane_epi16(&vpr[vt], vt_e);
    div_dp = 1;
    acc.low = vpr[vt];
}

template<> void vrsql<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    u16 vte = _mm_getlane_epi16(&vpr[vt], vt_e);
    s32 input = div_dp ? vte | div_in << 16 : s16(vte);
    s32 result = Rsq(input);
    _mm_setlane_epi16(&vpr[vd], vd_e, s16(result));
    div_out = result >> 16;
    div_in = div_dp = 0;
    acc.low = vpr[vt];
}

template<> void vsar<Interpreter>(u32 vd, u32 e)
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

template<> void vsub<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    m128i diff = _mm_sub_epi16(vt_op, vco.lo);
    m128i clamped_diff = _mm_subs_epi16(vt_op, vco.lo);
    acc.low = _mm_sub_epi16(vpr[vs], diff);
    m128i overflow = _mm_cmpgt_epi16(clamped_diff, diff);
    vpr[vd] = _mm_subs_epi16(vpr[vs], clamped_diff);
    vpr[vd] = _mm_adds_epi16(vpr[vd], overflow);
    vco.lo = vco.hi = _mm_setzero_si128();
}

template<> void vsubc<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    m128i vt_op = GetVTBroadcast(vt, e);
    vco.lo = _mm_cmplt_epu16(vpr[vs], vt_op); /* check borrow */
    vpr[vd] = acc.low = _mm_sub_epi16(vpr[vs], vt_op);
    vco.hi = _mm_or_si128(vco.lo, _mm_cmpneq_epi16(vpr[vd], _mm_setzero_si128()));
}

template<> void vxor<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_xor_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vzero<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    acc.low = _mm_add_epi16(vpr[vs], GetVTBroadcast(vt, e));
    vpr[vd] = _mm_setzero_si128();
}

} // namespace n64::rsp

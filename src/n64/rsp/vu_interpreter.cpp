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
#include <immintrin.h>
#endif

#define vco ctrl_reg[0]
#define vcc ctrl_reg[1]
#define vce ctrl_reg[2]

namespace n64::rsp {

using enum CpuImpl;

static void AddToAcc(__m128i low);
static void AddToAcc(__m128i low, __m128i mid);
static void AddToAcc(__m128i low, __m128i mid, __m128i high);
static void AddToAccCond(__m128i low, __m128i cond);
static void AddToAccCond(__m128i low, __m128i mid, __m128i cond);
static void AddToAccCond(__m128i low, __m128i mid, __m128i high, __m128i cond);
static void AddToAccFromMid(__m128i mid, __m128i high);
static __m128i ClampSigned(__m128i low, __m128i high);
static __m128i ClampUnsigned(__m128i low, __m128i high);
static __m128i GetVTBroadcast(uint vt, uint element);

template<bool vmulf> static void vmulfu(u32 vs, u32 vt, u32 vd, u32 e);

void AddToAcc(__m128i low)
{
    /* for i in 0..7
            accumulator<i>(47..0) += low<i>
        endfor
    */
    __m128i prev_acc_low = acc.low;
    acc.low = _mm_add_epi16(acc.low, low);
    __m128i low_carry = _mm_cmplt_epu16(acc.low, prev_acc_low);
    acc.mid = _mm_sub_epi16(acc.mid, low_carry);
    __m128i mid_carry = _mm_and_si128(low_carry, _mm_cmpeq_epi16(acc.mid, _mm_setzero_si128()));
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAcc(__m128i low, __m128i mid)
{
    /* for i in 0..7
            accumulator<i>(47..0) += mid<i> << 16 | low<i>
       endfor
    */
    AddToAcc(low);
    __m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_add_epi16(acc.mid, mid);
    __m128i mid_carry = _mm_cmplt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

void AddToAcc(__m128i low, __m128i mid, __m128i high)
{
    /* for i in 0..7
            accumulator<i>(47..0) += high<i> << 32 | mid<i> << 16 | low<i>
       endfor
    */
    AddToAcc(low, mid);
    acc.high = _mm_add_epi16(acc.high, high);
}

void AddToAccCond(__m128i low, __m128i cond)
{
    /* Like AddToAcc(__m128i), but only perform the operation if the corresponding lane in 'cond' is 0xFFFF */
    __m128i prev_acc_low = acc.low;
    acc.low = _mm_blendv_epi8(acc.low, _mm_add_epi16(acc.low, low), cond);
    __m128i low_carry = _mm_cmplt_epu16(acc.low, prev_acc_low);
    acc.mid = _mm_blendv_epi8(acc.mid, _mm_sub_epi16(acc.mid, low_carry), cond);
    __m128i mid_carry = _mm_and_si128(low_carry, _mm_cmpeq_epi16(acc.mid, _mm_setzero_si128()));
    acc.high = _mm_blendv_epi8(acc.high, _mm_sub_epi16(acc.high, mid_carry), cond);
}

void AddToAccCond(__m128i low, __m128i mid, __m128i cond)
{
    AddToAccCond(low, cond);
    __m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_blendv_epi8(acc.mid, _mm_add_epi16(acc.mid, mid), cond);
    __m128i mid_carry = _mm_cmplt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_blendv_epi8(acc.high, _mm_sub_epi16(acc.high, mid_carry), cond);
}

void AddToAccCond(__m128i low, __m128i mid, __m128i high, __m128i cond)
{
    AddToAccCond(low, mid, cond);
    acc.high = _mm_blendv_epi8(acc.high, _mm_add_epi16(acc.high, high), cond);
}

void AddToAccFromMid(__m128i mid, __m128i high)
{
    /* for i in 0..7
            accumulator<i>(47..0) += high<i> << 32 | mid<i> << 16
       endfor
    */
    __m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_add_epi16(acc.mid, mid);
    __m128i mid_carry = _mm_cmplt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_add_epi16(acc.high, high);
    acc.high = _mm_sub_epi16(acc.high, mid_carry);
}

__m128i ClampSigned(__m128i lo, __m128i hi)
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

__m128i ClampUnsigned(__m128i lo, __m128i hi)
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
    __m128i words1 = _mm_unpacklo_epi16(lo, hi);
    __m128i words2 = _mm_unpackhi_epi16(lo, hi);
    __m128i clamp_sse = _mm_packus_epi32(words1, words2);
    return _mm_blendv_epi8(clamp_sse, _mm_set1_epi64x(s64(-1)), _mm_srai_epi16(clamp_sse, 15));
}

__m128i GetVTBroadcast(uint vt /* 0-31 */, uint element /* 0-15 */)
{
    return _mm_shuffle_epi8(vpr[vt], broadcast_mask[element]);
}

// LBV, LSV, LLV, LDV
template<std::signed_integral Int> void LoadUpToDword(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * sizeof(Int);
    if constexpr (sizeof(Int) == 1) {
        *(vpr_dst + (e ^ 1)) = dmem[addr & 0xFFF];
    } else {
        u32 num_bytes = std::min((u32)sizeof(Int), 16 - e);
        for (u32 i = 0; i < num_bytes; ++i) {
            *(vpr_dst + (e + i ^ 1)) = dmem[addr + i & 0xFFF];
        }
    }
}

// SBV, SSV, SLV, SDV
template<std::signed_integral Int> void StoreUpToDword(u32 base, u32 vt, u32 e, s32 offset)
{
    u8 const* vpr_src = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * sizeof(Int);
    for (size_t i = 0; i < sizeof(Int); ++i) {
        dmem[addr + i & 0xFFF] = *(vpr_src + ((e + i ^ 1) & 0xF));
    }
}

s32 Rcp(s32 input)
{
    auto mask = input >> 31;
    auto data = input ^ mask;
    if (input > -32768) {
        data -= mask;
    }
    if (data == 0) {
        return 0x7FFF'FFFF;
    } else if (input == -32768) {
        return 0xFFFF'0000;
    } else {
        auto shift = std::countl_zero(to_unsigned(data));
        auto index = (u64(data) << shift & 0x7FC0'0000) >> 22;
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
}

template<> void cfc2<Interpreter>(u32 rt, u32 vs)
{
    /* GPR(31..0) = sign_extend(CTRL(15..0)) */
    vs = std::min(vs & 3, 2u);
    int lo = _mm_movemask_epi8(_mm_packs_epi16(ctrl_reg[vs].low, _mm_setzero_si128()));
    int hi = _mm_movemask_epi8(_mm_packs_epi16(ctrl_reg[vs].high, _mm_setzero_si128()));
    gpr.set(rt, s16(hi << 8 | lo));
}

template<> void ctc2<Interpreter>(u32 rt, u32 vs)
{
    /* CTRL(15..0) = GPR(15..0) */
    /* Control registers (16-bit) are encoded in two __m128i. Each lane represents one bit. */
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
    ctrl_reg[vs].low = _mm_set_epi64x(lanes[r >> 4 & 0xF], lanes[r >> 0 & 0xF]);
    if (vs < 2) {
        ctrl_reg[vs].high = _mm_set_epi64x(lanes[r >> 12 & 0xF], lanes[r >> 8 & 0xF]);
    }
}

template<> void mfc2<Interpreter>(u32 rt, u32 vs, u32 e)
{
    /* GPR[rt](31..0) = sign_extend(VS<elem>(15..0)) */
    u8* v = (u8*)(&vpr[vs]);
    gpr.set(rt, s16(v[e ^ 1] | v[e + 1 & 0xF ^ 1] << 8));
}

template<> void mtc2<Interpreter>(u32 rt, u32 vs, u32 e)
{
    /* VS<elem>(15..0) = GPR[rt](15..0) */
    u8* v = (u8*)(&vpr[vs]);
    v[e ^ 1] = u8(gpr[rt]);
    if (e < 0xF) v[e + 1 ^ 1] = u8(gpr[rt] >> 8);
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
    /* TODO */
}

template<> void lhv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    /* TODO */
}

template<> void llv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s32>(base, vt, e, offset);
}

template<> void lpv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_src = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8 & 0xFFF;
    auto dword_offset = addr;
    addr &= 0xFF8;
    auto lane = e;
    for (int i = 0; i < 8; ++i) {
        *(vpr_src + (lane++ & 7)) = dmem[addr | dword_offset++ & 7] << 8;
    }
}

template<> void lqv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    u32 addr = gpr[base] + offset * 16;
    u32 num_bytes = 16 - std::max(addr & 0xF, e); // == std::min(16 - (addr & 0xF), 16 - element)
    addr &= 0xFFF;
    for (u32 i = 0; i < num_bytes; ++i) {
        *(vpr_dst + (e++ ^ 1)) = dmem[addr++];
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
            *(vpr_dst + (e++ ^ 1)) = dmem[addr++];
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
    auto const wrap_addr = addr & 0xFF8;
    auto const num_bytes_until_addr_wrap = 16 - (addr & 7);
    addr += e + (addr & 8) & 0xF;
    e &= 0xE;
    auto const reg_base = vt & 0x18;
    auto reg_off = e >> 1;
    auto CopyNextByte = [&, even_byte = true]() mutable {
        *((u8*)(&vpr[reg_base + reg_off]) + (e++ & 0xE) + even_byte) = dmem[addr++ & 0xFFF];
        even_byte ^= 1;
        reg_off += even_byte;
        reg_off &= 7;
    };
    for (int i = 0; i < num_bytes_until_addr_wrap; ++i) {
        CopyNextByte();
    }
    addr = wrap_addr;
    for (int i = 0; i < 16 - num_bytes_until_addr_wrap; ++i) {
        CopyNextByte();
    }
}

template<> void luv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_src = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8 & 0xFFF;
    auto dword_offset = addr;
    addr &= 0xFF8;
    auto lane = e;
    for (int i = 0; i < 8; ++i) {
        *(vpr_src + (lane++ & 7)) = dmem[addr | dword_offset++ & 7] << 7;
    }
}

template<> void lwv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    u8* vpr_dst = (u8*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 16;
    for (auto current_elem = 16 - e; current_elem < e + 16; ++current_elem) {
        *(vpr_dst + ((current_elem & 0xF) ^ 1)) = dmem[addr & 0xFFF];
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
    /* TODO */
}

template<> void shv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    /* TODO */
}

template<> void slv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s32>(base, vt, e, offset);
}

template<> void spv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_src = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8 & 0xFFF;
    auto dword_offset = addr;
    addr &= 0xFF8;
    auto lane = e;
    for (int i = 0; i < 8; ++i) {
        dmem[addr | dword_offset++ & 7] = *(vpr_src + (lane++ & 7)) >> 8 & 0xFF;
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
    auto const wrap_addr = addr & 0xFF8;
    auto const num_bytes_until_addr_wrap = 16 - (addr & 7);
    auto const base_reg = vt & 0x18;
    auto reg = base_reg | e >> 1;
    e = 0;
    auto CopyNextByte = [&, even_byte = true]() mutable {
        dmem[addr++ & 0xFFF] = *((u8*)(&vpr[reg]) + (e++ & 0xE) + even_byte);
        even_byte ^= 1;
        reg += even_byte;
        reg &= base_reg | reg & 7;
    };
    for (int i = 0; i < num_bytes_until_addr_wrap; ++i) {
        CopyNextByte();
    }
    addr = wrap_addr;
    for (int i = 0; i < 16 - num_bytes_until_addr_wrap; ++i) {
        CopyNextByte();
    }
}

template<> void suv<Interpreter>(u32 base, u32 vt, u32 e, s32 offset)
{
    s16* vpr_src = (s16*)(&vpr[vt]);
    auto addr = gpr[base] + offset * 8 & 0xFFF;
    auto dword_offset = addr;
    addr &= 0xFF8;
    auto lane = e;
    for (int i = 0; i < 8; ++i) {
        dmem[addr | dword_offset++ & 7] = *(vpr_src + (lane++ & 7)) >> 7 & 0xFF;
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
    __m128i eq0 = _mm_cmpeq_epi16(vpr[vs], _mm_setzero_si128());
    __m128i slt = _mm_srai_epi16(vpr[vs], 15);
    vpr[vd] = _mm_andnot_si128(eq0, GetVTBroadcast(vt, e));
    vpr[vd] = _mm_xor_si128(vpr[vd], slt);
    acc.low = _mm_sub_epi16(vpr[vd], slt);
    vpr[vd] = _mm_subs_epi16(vpr[vd], slt);
}

template<> void vadd<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_add_epi16(vpr[vs], _mm_sub_epi16(vt_op, vco.low));
    __m128i op1 = _mm_subs_epi16(_mm_min_epi16(vpr[vs], vt_op), vco.low);
    __m128i op2 = _mm_max_epi16(vpr[vs], vt_op);
    vpr[vd] = _mm_adds_epi16(op1, op2);
    vco.low = vco.high = _mm_setzero_si128();
}

template<> void vaddc<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    vpr[vd] = acc.low = _mm_add_epi16(vpr[vs], vt_op);
    vco.low = _mm_cmplt_epu16(vpr[vd], vt_op); /* check carry */
    vco.high = _mm_setzero_si128();
}

template<> void vand<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_and_si128(vpr[vs], GetVTBroadcast(vt, e));
}

template<> void vch<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i neg_vt = _mm_neg_epi16(vt_op);
    __m128i signs_different = _mm_cmplt_epi16(_mm_xor_si128(vpr[vs], vt_op), _mm_setzero_si128());
    vcc.high = _mm_blendv_epi8(vcc.high, _mm_set1_epi64x(s64(-1)), _mm_cmplt_epi16(vt_op, _mm_setzero_si128()));
    vco.low = signs_different;
    __m128i vt_abs = _mm_blendv_epi8(vt_op, neg_vt, signs_different);
    vce.low = _mm_and_si128(vco.low, _mm_cmpeq_epi16(vpr[vs], _mm_sub_epi16(neg_vt, _mm_set1_epi16(1))));
    vco.high = _mm_not_si128(_mm_or_si128(vce.low, _mm_cmpeq_epi16(vpr[vs], vt_abs)));
    vcc.low = _mm_cmple_epi16(vpr[vs], neg_vt);
    vcc.high = _mm_cmpge_epi16(vpr[vs], vt_op);
    __m128i clip = _mm_blendv_epi8(vcc.high, vcc.low, vco.low);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], vt_abs, clip);
}

template<> void vcl<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    vcc.high = _mm_blendv_epi8(_mm_cmpge_epu16(vpr[vs], vt_op), vcc.high, _mm_or_si128(vco.low, vco.high));
    __m128i neg_vt = _mm_neg_epi16(vt_op);
    __m128i le = _mm_cmple_epu16(vpr[vs], neg_vt);
    __m128i eq = _mm_cmpeq_epi16(vpr[vs], neg_vt);
    vcc.low =
      _mm_blendv_epi8(vcc.low, _mm_blendv_epi8(eq, le, vce.low), _mm_and_si128(vco.low, _mm_not_si128(vco.high)));
    __m128i clip = _mm_blendv_epi8(vcc.high, vcc.low, vco.low);
    __m128i vt_abs = _mm_blendv_epi8(vt_op, neg_vt, vco.low);
    vpr[vd] = acc.low = _mm_blendv_epi8(vpr[vs], vt_abs, clip);
}

template<> void vcr<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i sign = _mm_srai_epi16(_mm_xor_si128(vpr[vs], vt_op), 15);
    __m128i dlez = _mm_add_epi16(_mm_and_si128(vpr[vs], sign), vt_op);
    vcc.low = _mm_srai_epi16(dlez, 15);
    __m128i dgez = _mm_min_epi16(_mm_or_si128(vpr[vs], sign), vt_op);
    vcc.high = _mm_cmpeq_epi16(dgez, vt_op);
    __m128i nvt = _mm_xor_si128(vt_op, sign);
    __m128i mask = _mm_blendv_epi8(vcc.high, vcc.low, sign);
    acc.low = _mm_blendv_epi8(vpr[vs], nvt, mask);
    vpr[vd] = acc.low;
    vco.low = vco.high = vce.low = _mm_setzero_si128();
}

template<> void veq<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    vcc.low = _mm_and_si128(_mm_not_si128(vco.high), eq);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.low); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    std::memset(&vco, 0, sizeof(vco));
    std::memset(&vcc.high, 0, sizeof(vcc.high));
}

template<> void vge<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    __m128i neg = _mm_and_si128(_mm_nand_si128(vco.low, vco.high), eq);
    __m128i gt = _mm_cmpgt_epi16(vpr[vs], vt_op);
    vcc.low = _mm_or_si128(neg, gt);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.low); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    std::memset(&vco, 0, sizeof(vco));
    std::memset(&vcc.high, 0, sizeof(vcc.high));
}

template<> void vlt<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    __m128i neg = _mm_and_si128(_mm_and_si128(vco.low, vco.high), eq);
    __m128i lt = _mm_cmplt_epi16(vpr[vs], vt_op);
    vcc.low = _mm_or_si128(neg, lt);
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.low); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    std::memset(&vco, 0, sizeof(vco));
    std::memset(&vcc.high, 0, sizeof(vcc.high));
}

template<> void vmacf<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two to get a 33-bit product. Sign-extend to 48 bits, and add to the accumulator. */
    __m128i low_carry = _mm_srli_epi16(low, 15);
    __m128i high_carry = _mm_srai_epi16(high, 15);
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
    __m128i mask = _mm_set1_epi16(32);
    __m128i addend = _mm_and_si128(_mm_not_si128(acc.mid), mask); /* 0 or 32 */
    __m128i acc_high_pos = _mm_cmpgt_epi16(acc.high, _mm_setzero_si128());
    __m128i acc_high_neg = _mm_cmplt_epi16(acc.high, _mm_setzero_si128());
    /* Possibly subtract 32. */
    __m128i neg_addend =
      _mm_and_si128(addend, _mm_or_si128(acc_high_pos, _mm_cmpge_epu16(acc.mid, mask))); /* unsigned(acc.mid) >= 32 */
    __m128i prev_acc_mid = acc.mid;
    acc.mid = _mm_sub_epi16(acc.mid, neg_addend);
    __m128i borrow = _mm_cmpgt_epu16(acc.mid, prev_acc_mid);
    acc.high = _mm_add_epi16(acc.high, borrow); /* same as subtracting 0 or 1 */
    /* Possibly add 32. No carry in acc.mid, since bit 5 is clear if addend != 0. */
    __m128i pos_addend = _mm_and_si128(addend, acc_high_neg);
    acc.mid = _mm_add_epi16(acc.mid, pos_addend);
    __m128i clamp_input_low = _mm_or_si128(_mm_srli_epi16(acc.mid, 1), _mm_slli_epi16(acc.high, 15));
    __m128i clamp_input_high = _mm_srai_epi16(acc.high, 1);
    vpr[vd] = _mm_and_si128(_mm_set1_epi16(~0xF), ClampSigned(clamp_input_low, clamp_input_high));
}

template<> void vmacu<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two to get a 33-bit product. Sign-extend to 48 bits, and add to the accumulator. */
    __m128i low_carry = _mm_srli_epi16(low, 15);
    __m128i high_carry = _mm_srai_epi16(high, 15);
    low = _mm_slli_epi16(low, 1);
    high = _mm_slli_epi16(high, 1);
    high = _mm_add_epi16(high, low_carry);
    AddToAcc(low, high, high_carry);
    vpr[vd] = ClampUnsigned(acc.mid, acc.high);
}

template<> void vmadh<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    AddToAccFromMid(low, high);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmadl<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    AddToAcc(_mm_mulhi_epu16(vpr[vs], GetVTBroadcast(vt, e)));
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    __m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    __m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

template<> void vmadm<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epu16_epi16(vt_op, vpr[vs]);
    __m128i sign_ext = _mm_srai_epi16(high, 15);
    AddToAcc(low, high, sign_ext);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmadn<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epu16_epi16(vpr[vs], vt_op);
    __m128i sign_ext = _mm_srai_epi16(high, 15);
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    AddToAcc(low, high, sign_ext);
    __m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    __m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

template<> void vmov<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    _mm_setlane_epi16(&vpr[vd], vd_e, _mm_getlane_epi16(&vpr[vt], vt_e));
    acc.low = vpr[vt];
}

template<> void vmrg<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    vpr[vd] = acc.low = _mm_blendv_epi8(GetVTBroadcast(vt, e), vpr[vs], vcc.low);
    std::memset(&vco, 0, sizeof(vco));
}

template<> void vmudh<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_setzero_si128(); /* seems necessary given tests */
    acc.mid = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.high = _mm_mulhi_epi16(vpr[vs], vt_op);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmudl<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    acc.low = _mm_mulhi_epu16(vpr[vs], GetVTBroadcast(vt, e));
    /* In this case, the unsigned clamp will return ACC_LO if ACC_HI is the sign extension of ACC_MD -
    otherwise, it will return 0 for negative ACC_HI, and 65535 for positive ACC_HI */
    __m128i is_sign_ext = _mm_cmpeq_epi16(acc.high, _mm_srai_epi16(acc.mid, 15));
    __m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    vpr[vd] = _mm_blendv_epi8(_mm_blendv_epi8(_mm_set1_epi64x(s64(-1)), _mm_setzero_si128(), acc_high_neg),
      acc.low,
      is_sign_ext);
}

template<> void vmudm<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.mid = _mm_mulhi_epu16_epi16(vt_op, vpr[vs]);
    acc.high = _mm_srai_epi16(acc.mid, 15);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vmudn<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    acc.low = _mm_mullo_epi16(vpr[vs], vt_op);
    acc.mid = _mm_mulhi_epu16_epi16(vpr[vs], vt_op);
    acc.high = _mm_srai_epi16(acc.mid, 15);
    __m128i is_sign_ext = _mm_set1_epi64x(s64(-1));
    __m128i acc_high_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
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
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* multiply by two */
    __m128i low_carry_mul = _mm_srli_epi16(low, 15); /* note: either 0 or 1 */
    __m128i high_carry_mul = _mm_srai_epi16(high, 15); /* note: either 0 or 0xFFFF */
    low = _mm_slli_epi16(low, 1);
    high = _mm_slli_epi16(high, 1);
    high = _mm_add_epi16(high, low_carry_mul);
    /* add $8000 */
    low = _mm_add_epi16(low, m128i_epi16_sign_mask);
    __m128i low_carry_add = _mm_cmpgt_epi16(low, _mm_setzero_si128()); /* carry if low >= 0 */
    high = _mm_sub_epi16(high, low_carry_add);
    __m128i high_carry_add = _mm_and_si128(_mm_cmpeq_epi16(high, _mm_setzero_si128()), low_carry_add);
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
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i low = _mm_mullo_epi16(vpr[vs], vt_op);
    __m128i high = _mm_mulhi_epi16(vpr[vs], vt_op);
    /* add 31 to product if product < 0 */
    __m128i addend = _mm_and_si128(_mm_srai_epi16(high, 15), _mm_set1_epi16(0x1F));
    low = _mm_add_epi16(low, addend);
    __m128i low_carry = _mm_srli_epi16(_mm_cmplt_epu16(low, addend), 15);
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
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i eq = _mm_cmpeq_epi16(vpr[vs], vt_op);
    vcc.low = _mm_or_si128(vco.high, _mm_cmpneq_epi16(vpr[vs], vt_op));
    vpr[vd] = acc.low = _mm_blendv_epi8(vt_op, vpr[vs], vcc.low); /* Each 16-bit lane in vcc is either 0 or $FFFF */
    std::memset(&vco, 0, sizeof(vco));
    std::memset(&vcc.high, 0, sizeof(vcc.high));
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
    s32 input = div_in << 16 | _mm_getlane_epi16(&vpr[vt], vt_e);
    s32 result = Rcp(input);
    _mm_setlane_epi16(&vpr[vd], vd_e, s16(result));
    div_out = result >> 16 & 0xFFFF;
    div_in = div_dp = 0;
    acc.low = vpr[vt];
}

template<> void vrndn<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    __m128i low, mid, high;
    if (vd_e & 1) { /* sign_extend(VT << 16) */
        low = _mm_setzero_si128();
        mid = vpr[vt];
        high = _mm_srai_epi16(mid, 15);
    } else { /* sign_extend(VT) */
        low = vpr[vt];
        mid = high = _mm_srai_epi16(low, 15);
    }
    __m128i acc_neg = _mm_cmpeq_epi16(_mm_set1_epi64x(s64(-1)), _mm_srai_epi16(acc.high, 15));
    AddToAccCond(low, mid, high, acc_neg);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
}

template<> void vrndp<Interpreter>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    __m128i low, mid, high;
    if (vd_e & 1) { /* sign_extend(VT << 16) */
        low = _mm_setzero_si128();
        mid = vpr[vt];
        high = _mm_srai_epi16(mid, 15);
    } else { /* sign_extend(VT) */
        low = vpr[vt];
        mid = high = _mm_srai_epi16(low, 15);
    }
    __m128i acc_non_neg = _mm_cmpeq_epi16(_mm_setzero_si128(), _mm_srai_epi16(acc.high, 15));
    AddToAccCond(low, mid, high, acc_non_neg);
    vpr[vd] = ClampSigned(acc.mid, acc.high);
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
    s32 input = div_in << 16 | _mm_getlane_epi16(&vpr[vt], vt_e);
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
    __m128i vt_op = GetVTBroadcast(vt, e);
    __m128i diff = _mm_sub_epi16(vt_op, vco.low);
    __m128i clamped_diff = _mm_subs_epi16(vt_op, vco.low);
    acc.low = _mm_sub_epi16(vpr[vs], diff);
    __m128i overflow = _mm_cmpgt_epi16(clamped_diff, diff);
    vpr[vd] = _mm_subs_epi16(vpr[vs], clamped_diff);
    vpr[vd] = _mm_adds_epi16(vpr[vd], overflow);
    vco.low = vco.high = _mm_setzero_si128();
}

template<> void vsubc<Interpreter>(u32 vs, u32 vt, u32 vd, u32 e)
{
    __m128i vt_op = GetVTBroadcast(vt, e);
    vco.low = _mm_cmplt_epu16(vpr[vs], vt_op); /* check borrow */
    vpr[vd] = acc.low = _mm_sub_epi16(vpr[vs], vt_op);
    vco.high = _mm_or_si128(vco.low, _mm_cmpneq_epi16(vpr[vd], _mm_setzero_si128()));
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

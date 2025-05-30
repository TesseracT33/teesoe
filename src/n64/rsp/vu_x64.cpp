#include "jit_common.hpp"
#include "rsp/recompiler.hpp"
#include "rsp/register_allocator.hpp"
#include "rsp/rsp.hpp"
#include "rsp/vu.hpp"

#include <type_traits>

// todo: all leas should use 64 bit operands

#define vco ctrl_reg[0]
#define vcc ctrl_reg[1]
#define vce ctrl_reg[2].lo

using namespace asmjit;
using namespace asmjit::x86;

namespace n64::rsp::x64 {

static Xmm GetVpr(u32 idx)
{
    return reg_alloc.GetVpr(idx);
}

static Xmm GetDirtyVpr(u32 idx)
{
    return reg_alloc.GetDirtyVpr(idx);
}

static Gpd GetGpr(u32 idx)
{
    return reg_alloc.GetGpr(idx);
}

static Gpd GetDirtyGpr(u32 idx)
{
    return reg_alloc.GetDirtyGpr(idx);
}

static Xmm GetAccLow()
{
    return reg_alloc.GetAccLow();
}

static Xmm GetAccMid()
{
    return reg_alloc.GetAccMid();
}

static Xmm GetAccHigh()
{
    return reg_alloc.GetAccHigh();
}

static Xmm GetDirtyAccLow()
{
    return reg_alloc.GetDirtyAccLow();
}

static Xmm GetDirtyAccMid()
{
    return reg_alloc.GetDirtyAccMid();
}

static Xmm GetDirtyAccHigh()
{
    return reg_alloc.GetDirtyAccHigh();
}

static std::tuple<Xmm, Xmm, Xmm> GetDirtyAccs()
{
    return { GetDirtyAccLow(), GetDirtyAccMid(), GetDirtyAccHigh() };
}

static Xmm GetVte(uint vt /* 0-31 */, uint e /* 0-15 */)
{
    Xmm ht = GetVpr(vt);
    if (e < 2) {
        return ht;
    } else {
        Xmm vte = reg_alloc.GetVte();
        c.vpshufb(vte, ht, JitPtrOffset(broadcast_mask, 16 * e, 16));
        return vte;
    }
}

void cfc2(u32 rt, u32 vs)
{
    Gpd hrt = GetDirtyGpr(rt);
    vs = std::min(vs & 3, 2u);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(xmm1, JitPtr(ctrl_reg[vs].lo));
    c.vpacksswb(xmm1, xmm1, xmm0);
    c.vpmovmskb(eax, xmm1);
    c.vmovaps(xmm1, JitPtr(ctrl_reg[vs].hi));
    c.vpacksswb(xmm1, xmm1, xmm0);
    c.vpmovmskb(hrt, xmm1);
    c.shl(hrt, 8);
    c.or_(hrt, eax);
    c.movsx(hrt, hrt.r16());
}

void ctc2(u32 rt, u32 vs)
{
    Gpd ht = GetGpr(rt);
    vs = std::min(vs & 3, 2u);
    c.mov(rcx, &ctc2_table);
    c.mov(eax, ht);
    c.and_(eax, 15);
    c.vmovq(xmm0, qword_ptr(rcx, rax, 3u));
    c.mov(eax, ht);
    c.shr(eax, 4);
    c.and_(eax, 15);
    c.vpinsrq(xmm0, xmm0, qword_ptr(rcx, rax, 3u), 1);
    c.vmovaps(JitPtr(ctrl_reg[vs].lo), xmm0);
    if (vs < 2) {
        c.mov(eax, ht);
        c.mov(al, ah);
        c.and_(eax, 15);
        c.vmovq(xmm0, qword_ptr(rcx, rax, 3u));
        c.mov(eax, ht);
        c.shr(eax, 12);
        c.and_(eax, 15);
        c.vpinsrq(xmm0, xmm0, qword_ptr(rcx, rax, 3u), 1);
        c.vmovaps(JitPtr(ctrl_reg[vs].hi), xmm0);
    }
}

void mfc2(u32 rt, u32 vs, u32 e)
{
    if (!rt) return;
    Gpd hrt = GetDirtyGpr(rt);
    Xmm hvs = GetVpr(vs);
    if (e & 1) {
        c.vpextrb(ecx, hvs, e ^ 1);
        c.vpextrb(eax, hvs, e + 1 & 15 ^ 1);
        c.shl(ecx, 8);
        c.or_(ecx, eax);
        c.movsx(hrt, cx);
    } else {
        c.vpextrw(hrt, hvs, e / 2);
        c.movsx(hrt, hrt.r16());
    }
}

void mtc2(u32 rt, u32 vs, u32 e)
{
    Gpd hrt = GetGpr(rt);
    Xmm hvs = GetDirtyVpr(vs);
    if (e & 1) {
        c.mov(eax, hrt);
        c.shr(eax, 8);
        c.vpinsrb(hvs, hvs, eax, e ^ 1);
        if (e < 15) {
            c.vpinsrb(hvs, hvs, hrt, e + 1 ^ 1);
        }
    } else {
        c.vpinsrw(hvs, hvs, hrt, e / 2);
    }
}

// LSV, LLV, LDV
template<std::signed_integral Int> static void LoadUpToDword(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);
    c.lea(eax, ptr(hbase, offset * sizeof(Int)));
    c.mov(rcx, dmem);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);
    for (u32 i = 0; i < std::min((u32)sizeof(Int), 16 - e); ++i) {
        if (i) c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(dl, byte_ptr(rcx, rax));
        c.mov(byte_ptr(x86::rsp, (e + i ^ 1) - 16), dl);
    }
    c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));
}

// SSV, SLV, SDV
template<std::signed_integral Int> static void StoreUpToDword(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    c.lea(eax, ptr(hbase, offset * sizeof(Int)));
    c.mov(rcx, dmem);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);
    for (u32 i = 0; i < sizeof(Int); ++i) {
        if (i) c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(dl, byte_ptr(x86::rsp, (e + i & 15 ^ 1) - 16));
        c.mov(byte_ptr(rcx, rax), dl);
    }
}

void lbv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);
    c.lea(eax, ptr(hbase, offset));
    c.and_(eax, 0xFFF);
    c.vpinsrb(ht, ht, JitPtrOffset(dmem, rax, 1), e ^ 1);
}

void ldv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s64>(base, vt, e, offset);
}

void lfv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);

    c.lea(eax, ptr(hbase, offset * 16));
    c.mov(ecx, eax);
    c.and_(ecx, 7);
    if (e) c.sub(ecx, e); // addr_offset
    c.and_(eax, ~7);

    for (int i = 0; i < 4; ++i) {
        if (i) c.add(ecx, 4);
        c.and_(ecx, 15); // todo: only needed for i==0 if underflow from (addr & 7) - e is possible
        c.lea(edx, ptr(rax, rcx));
        c.and_(edx, 0xFFF);
        c.mov(dl, JitPtrOffset(dmem, rdx, 1));
        c.shl(edx, 7);
        c.and_(edx, 0x7FFF);
        c.mov(word_ptr(x86::rsp, 2 * lfv_table[2 * i] - 16), dx);
        c.mov(word_ptr(x86::rsp, 2 * lfv_table[2 * i + 1] - 16), dx);
    }

    if (e == 0) {
        c.vpinsrq(ht, ht, qword_ptr(x86::rsp, -16), 0);
    } else if (e == 8) {
        c.vpinsrq(ht, ht, qword_ptr(x86::rsp, -8), 1);
    } else {
        c.vmovaps(xmmword_ptr(x86::rsp, -32), ht);
        if (e & 1) {
            for (auto byte = e; byte < std::min(e + 8, 16u); ++byte) {
                c.mov(al, byte_ptr(x86::rsp, (byte ^ 1) - 16));
                c.mov(byte_ptr(x86::rsp, (byte ^ 1) - 32), al);
            }
        } else {
            for (auto byte = e; byte < std::min(e + 8, 16u); byte += 2) {
                c.mov(ax, word_ptr(x86::rsp, byte - 16));
                c.mov(word_ptr(x86::rsp, byte - 32), ax);
            }
        }
        c.vmovaps(ht, xmmword_ptr(x86::rsp, -32));
    }
}

void lhv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);

    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7);
    if (e) c.sub(ecx, e); // mem offset
    c.and_(eax, ~7);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

    for (int i = 0; i < 8; ++i) {
        if (i) c.add(ecx, 2);
        c.and_(ecx, 15); // todo: only needed for i==0 if underflow from (addr & 7) - e is possible
        c.lea(edx, ptr(rax, rcx));
        c.and_(edx, 0xFFF);
        c.mov(dl, JitPtrOffset(dmem, rdx, 1));
        c.shl(edx, 7);
        c.and_(edx, 0x7FFF);
        c.mov(word_ptr(x86::rsp, 2 * i - 16), dx);
    }

    c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));
}

void llv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s32>(base, vt, e, offset);
}

void lpv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);

    c.lea(eax, ptr(hbase, offset * 8)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7);
    if (e) c.sub(ecx, e); // mem offset
    c.and_(eax, ~7);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

    for (int i = 0; i < 8; ++i) {
        if (i) c.inc(ecx);
        c.and_(ecx, 15); // todo: only needed for i==0 if underflow from (addr & 7) - e is possible
        c.lea(edx, ptr(rax, rcx));
        c.and_(edx, 0xFFF);
        c.mov(dl, JitPtrOffset(dmem, rdx, 1));
        c.shl(edx, 8);
        c.mov(word_ptr(x86::rsp, 2 * i - 16), dx);
    }

    c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));
}

void lqv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);

    Label l_start = c.newLabel();
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(rcx, dmem);
    c.mov(edx, eax);
    c.and_(eax, 0xFFF);
    c.and_(edx, 15);

    if (e == 0) {
        Label l_simple = c.newLabel(), l_end = c.newLabel();
        c.je(l_simple);

        c.add(rcx, rax); // addr base
        c.xor_(eax, eax); // element
        c.neg(edx);
        c.add(edx, 16); // last element written to, exclusive
        c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

        c.bind(l_start);
        c.mov(r8b, byte_ptr(rcx));
        c.inc(rcx);
        c.xor_(eax, 1);
        c.mov(byte_ptr(x86::rsp, rax, 0, -16), r8b);
        c.xor_(eax, 1);
        c.inc(eax);
        c.cmp(eax, edx);
        c.jne(l_start);
        c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));
        c.jmp(l_end);

        c.bind(l_simple);
        c.vmovaps(ht, xmmword_ptr(rcx, rax));
        c.vpshufb(ht, ht, JitPtr(byteswap16_mask));

        c.bind(l_end);
    } else {
        c.add(rcx, rax); // addr base
        c.mov(eax, e); // e counter
        c.cmp(edx, eax);
        c.cmovb(edx, eax);
        c.neg(edx);
        c.add(edx, 16 + e); // last element written to, exclusive
        c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

        c.bind(l_start);
        c.mov(r8b, byte_ptr(rcx));
        c.inc(rcx);
        c.xor_(eax, 1);
        c.mov(byte_ptr(x86::rsp, rax, 0, -16), r8b);
        c.xor_(eax, 1);
        c.inc(eax);
        c.cmp(eax, edx);
        c.jne(l_start);

        c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));
    }

    reg_alloc.Free(r8);
}

void lrv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);

    Label l_start = c.newLabel(), l_end_l = c.newLabel(), l_end = c.newLabel();
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 15);
    e ? c.cmp(ecx, e) : c.test(ecx, ecx);
    c.jbe(l_end);

    c.neg(ecx);
    c.add(ecx, e + 16); // e
    c.and_(eax, 0xFF0);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

    c.bind(l_start);
    c.mov(dl, JitPtrOffset(dmem, rax, 1));
    c.inc(eax);
    c.xor_(ecx, 1);
    c.mov(byte_ptr(x86::rsp, rcx, 0, -16), dl);
    c.xor_(ecx, 1);
    c.inc(ecx);
    c.cmp(ecx, 16);
    c.jb(l_start);

    c.bind(l_end_l);
    c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));

    c.bind(l_end);
}

void lsv(u32 base, u32 vt, u32 e, s32 offset)
{
    LoadUpToDword<s16>(base, vt, e, offset);
}

void ltv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);

    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, ~7); // wrap_addr
    c.and_(eax, 8);
    if (e) c.add(eax, e);
    c.and_(eax, 15);
    c.add(eax, ecx);
    c.mov(edx, ecx);
    c.add(edx, 16); // addr wrap-around point (exclusive)

    auto const reg_base = vt & 0x18;
    auto reg_off = e >> 1;

    for (int i = 0; i < 8; ++i) {
        Xmm hreg = GetDirtyVpr(reg_base + reg_off);
        c.vmovaps(xmmword_ptr(x86::rsp, -16), hreg);
        for (int j = 1; j >= 0; --j) {
            if (i != 0 || j != 1) {
                c.inc(eax);
                c.cmp(eax, edx);
                c.cmove(eax, ecx);
            }
            c.and_(eax, 0xFFF);
            c.mov(r8b, JitPtrOffset(dmem, rax, 1));
            c.mov(byte_ptr(x86::rsp, 2 * i + j - 16), r8b);
        }
        c.vmovaps(hreg, xmmword_ptr(x86::rsp, -16));
        reg_off = reg_off + 1 & 7;
    }

    reg_alloc.Free(r8);
}

void luv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetDirtyVpr(vt);

    c.lea(eax, ptr(hbase, offset * 8)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7);
    if (e) c.sub(ecx, e); // mem offset
    c.and_(eax, ~7);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

    for (int i = 0; i < 8; ++i) {
        if (i) c.inc(ecx);
        c.and_(ecx, 15); // todo: only needed for i==0 if underflow from (addr & 7) - e is possible
        c.lea(edx, ptr(rax, rcx));
        c.and_(edx, 0xFFF);
        c.mov(dl, JitPtrOffset(dmem, rdx, 1));
        c.shl(edx, 7);
        c.and_(edx, 0x7FFF);
        c.mov(word_ptr(x86::rsp, 2 * i - 16), dx);
    }

    c.vmovaps(ht, xmmword_ptr(x86::rsp, -16));
}

void sbv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    c.lea(eax, ptr(hbase, offset));
    c.and_(eax, 0xFFF);
    c.vpextrb(JitPtrOffset(dmem, rax, 1), ht, e ^ 1);
}

void sdv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s64>(base, vt, e, offset);
}

void sfv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7); // addr_offset
    c.and_(eax, ~7);

    auto store_elems = [vt](std::array<u8, 4> elems) {
        Xmm ht = GetVpr(vt);

        c.vpsrlw(xmm0, ht, 7);
        c.vmovaps(xmmword_ptr(x86::rsp, -16), xmm0);
        for (int i = 0; i < 4; ++i) {
            if (i) {
                c.add(ecx, 4);
                c.and_(ecx, 15);
            }
            c.lea(edx, ptr(rax, rcx));
            c.and_(edx, 0xFFF);
            c.mov(r8b, byte_ptr(x86::rsp, 2 * elems[i] - 16));
            c.mov(JitPtrOffset(dmem, rdx, 1), r8b);
        }
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
    default:
        for (int i = 0; i < 4; ++i) {
            if (i) {
                c.add(ecx, 4);
                c.and_(ecx, 15);
            }
            c.lea(edx, ptr(rax, rcx));
            c.and_(edx, 0xFFF);
            c.mov(JitPtrOffset(dmem, rdx, 1), 0);
        }
        break;
    }
    reg_alloc.Free(r8);
}

void shv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7); // addr_offset
    c.and_(eax, ~7);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);
    for (int i = 0; i < 8; ++i) {
        auto byte = e + 2 * i;
        c.mov(dl, byte_ptr(x86::rsp, (byte & 15 ^ 1) - 16));
        c.mov(r8b, byte_ptr(x86::rsp, (byte + 1 & 15 ^ 1) - 16));
        c.shl(edx, 1);
        c.shr(r8b, 7);
        c.or_(edx, r8d);
        if (i > 0) {
            c.add(ecx, 2);
            if (i > 3) c.and_(ecx, 15);
        }
        c.lea(r8d, ptr(rax, rcx));
        c.and_(r8d, 0xFFF);
        c.mov(JitPtrOffset(dmem, r8d, 1), dl);
    }
    reg_alloc.Free(r8);
}

void slv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s32>(base, vt, e, offset);
}

void spv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    c.lea(eax, ptr(hbase, offset * 8));
    c.mov(rcx, dmem);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);
    for (auto elem = e; elem < e + 8; ++elem) {
        if ((elem & 15) < 8) {
            c.mov(dl, byte_ptr(x86::rsp, (elem << 1 & 0xE ^ 1) - 16));
        } else {
            c.mov(dx, word_ptr(x86::rsp, (elem << 1 & 15) - 16));
            c.shr(edx, 7);
        }
        if (elem > e) c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(byte_ptr(rcx, rax), dl);
    }
}

void sqv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    Label l_simple = c.newLabel();
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(eax, 0xFFF);
    c.and_(ecx, 15);

    if (e == 0) {
        c.je(l_simple);
    }

    c.neg(ecx);
    c.add(ecx, e + 16); // e end (exclusive)
    e ? c.mov(edx, e) : c.xor_(edx, edx); // e start, counter
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

    c.xor_(edx, 1);
    c.mov(r8b, byte_ptr(x86::rsp, rdx, 0, -16));
    c.mov(JitPtrOffset(dmem, rax, 1), r8b);
    c.xor_(edx, 1);
    c.inc(edx);

    Label l_start = c.newLabel(), l_end = c.newLabel();

    c.cmp(edx, ecx);
    c.je(l_end);

    c.bind(l_start);
    c.inc(eax);
    c.and_(eax, 0xFFF);
    c.mov(r8d, edx); // e current index. needed since edx (e counter) can go beyond 15
    c.and_(r8d, 15);
    c.xor_(r8d, 1);
    c.mov(r8b, byte_ptr(x86::rsp, r8, 0, -16));
    c.mov(JitPtrOffset(dmem, rax, 1), r8b);
    c.inc(edx);
    c.cmp(edx, ecx);
    c.jne(l_start);
    c.jmp(l_end);

    c.bind(l_simple);
    if (e == 0) {
        c.vpshufb(xmm0, ht, JitPtr(byteswap16_mask));
        c.vmovaps(JitPtrOffset(dmem, rax, 16), xmm0);
    }

    c.bind(l_end);
    reg_alloc.Free(r8);
}

void srv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8, r9);
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);

    Label l_start = c.newLabel(), l_end = c.newLabel();

    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 15); // addr_offset
    c.je(l_end);
    c.mov(edx, 16);
    c.sub(edx, ecx); // base element
    e ? c.mov(r8d, e) : c.xor_(r8d, r8d); // element
    c.and_(eax, 0xFF0);
    c.add(ecx, eax); // end address (exclusive)
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);

    c.bind(l_start);
    c.lea(r9d, ptr(rdx, r8));
    c.and_(r9d, 15);
    c.xor_(r9d, 1);
    c.mov(r9b, byte_ptr(x86::rsp, r9, 0, -16));
    c.mov(JitPtrOffset(dmem, rax, 1), r9b);
    c.inc(eax);
    c.inc(r8d);
    c.cmp(eax, ecx);
    c.jne(l_start);

    c.bind(l_end);
    reg_alloc.Free(r8, r9);
}

void ssv(u32 base, u32 vt, u32 e, s32 offset)
{
    StoreUpToDword<s16>(base, vt, e, offset);
}

void stv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7);
    if (e & ~1) c.sub(ecx, e & ~1); // offset_addr
    c.and_(eax, ~7);
    auto elem = 16 - (e & ~1);
    auto const reg_start = vt & 0x18;
    for (auto reg = reg_start; reg < reg_start + 8; ++reg) {
        Xmm hreg = GetVpr(reg);
        c.vmovaps(xmmword_ptr(x86::rsp, -16), hreg);
        for (int i = 0; i < 2; ++i) {
            c.mov(dl, byte_ptr(x86::rsp, (elem++ & 15 ^ 1) - 16));
            if (reg != reg_start || i) {
                c.inc(ecx);
            }
            c.and_(ecx, 15);
            c.lea(r8d, ptr(rax, rcx));
            c.and_(r8d, 0xFFF);
            c.mov(JitPtrOffset(dmem, r8, 1), dl);
        }
    }
    reg_alloc.Free(r8);
}

void suv(u32 base, u32 vt, u32 e, s32 offset)
{
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    c.lea(eax, ptr(hbase, offset * 8)); // addr
    c.mov(rcx, dmem);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);
    for (auto elem = e; elem < e + 8; ++elem) {
        if ((elem & 15) < 8) {
            c.mov(dx, word_ptr(x86::rsp, (elem << 1 & 15) - 16));
            c.shr(edx, 7);
        } else {
            c.mov(dl, byte_ptr(x86::rsp, (elem << 1 & 0xE ^ 1) - 16));
        }
        if (elem > e) c.inc(eax);
        c.and_(eax, 0xFFF);
        c.mov(byte_ptr(rcx, rax), dl);
    }
}

void swv(u32 base, u32 vt, u32 e, s32 offset)
{
    reg_alloc.Reserve(r8);
    Gpd hbase = GetGpr(base);
    Xmm ht = GetVpr(vt);
    c.lea(eax, ptr(hbase, offset * 16)); // addr
    c.mov(ecx, eax);
    c.and_(ecx, 7); // base
    c.and_(eax, ~7);
    c.vmovaps(xmmword_ptr(x86::rsp, -16), ht);
    for (auto elem = e; elem < e + 16; ++elem) {
        c.mov(dl, byte_ptr(x86::rsp, (elem & 15 ^ 1) - 16));
        if (elem > e) {
            c.inc(ecx);
            c.and_(ecx, 15);
        }
        c.lea(r8d, ptr(rax, rcx));
        c.and_(r8d, 0xFFF);
        c.mov(JitPtrOffset(dmem, r8, 1), dl);
    }
    reg_alloc.Free(r8);
}

void vabs(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpxor(xmm0, xmm0, xmm0);
    c.vpcmpeqw(xmm1, hs, xmm0);
    c.vpsraw(xmm0, hs, 15);
    c.vpandn(hd, xmm1, ht);
    c.vpxor(hd, hd, xmm0);
    c.vpsubw(haccl, hd, xmm0);
    c.vpsubsw(hd, hd, xmm0);
}

void vadd(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vmovaps(xmm0, JitPtr(vco.lo));
    c.vpsubw(xmm1, ht, xmm0);
    c.vpaddw(haccl, hs, xmm1);
    c.vpminsw(xmm1, hs, ht);
    c.vpsubsw(xmm0, xmm1, xmm0);
    c.vpmaxsw(xmm1, hs, ht);
    c.vpaddsw(hd, xmm0, xmm1);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void vaddc(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    if (hd == ht) {
        c.vmovaps(xmm0, ht);
        c.vpaddw(hd, hs, ht);
        c.vmovaps(haccl, hd);
        c.vpcmpeqw(xmm1, xmm1, xmm1);
        c.vpsllw(xmm1, xmm1, 15); // 0x8000
        c.vpaddw(xmm2, hd, xmm1);
        c.vpaddw(xmm0, xmm0, xmm1);
        c.vpcmpgtw(xmm0, xmm0, xmm2);
    } else {
        c.vpaddw(hd, hs, ht);
        c.vmovaps(haccl, hd);
        c.vpcmpeqw(xmm0, xmm0, xmm0);
        c.vpsllw(xmm0, xmm0, 15); // 0x8000
        c.vpaddw(xmm1, hd, xmm0);
        c.vpaddw(xmm0, ht, xmm0);
        c.vpcmpgtw(xmm0, xmm0, xmm1);
    }
    c.vmovaps(JitPtr(vco.lo), xmm0);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vco.hi), xmm0);
}

void vand(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpand(hd, hs, ht);
    c.vmovaps(haccl, hd);
}

void vch(u32 vs, u32 vt, u32 vd, u32 e)
{
    reg_alloc.Reserve(xmm3, xmm4, xmm5);
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpxor(xmm0, xmm0, xmm0);
    c.vpxor(xmm1, hs, ht); // vco.lo
    c.vpcmpgtw(xmm1, xmm0, xmm1);
    c.vpxor(xmm2, ht, xmm1); // nvt
    c.vpsubw(xmm2, xmm2, xmm1);
    c.vpsubw(xmm3, hs, xmm2); // diff
    c.vpcmpeqw(xmm4, xmm3, xmm0); // diff0
    c.vpcmpgtw(xmm5, xmm3, xmm0); // dlez
    c.vpcmpeqw(xmm3, xmm3, xmm1); // vce
    c.vpand(xmm3, xmm3, xmm1);
    c.vmovaps(JitPtr(vce), xmm3);
    c.vpor(xmm3, xmm5, xmm4); // dgez
    c.vpor(xmm4, xmm4, JitPtr(vce)); // vco.hi
    c.vpcmpeqw(xmm4, xmm4, xmm0);
    c.vmovaps(JitPtr(vco.hi), xmm4);
    c.vpcmpeqw(xmm5, xmm5, xmm0);
    c.vpcmpgtw(xmm4, xmm0, ht); // vtn
    c.vpblendvb(xmm3, xmm3, xmm4, xmm1); // vcc.hi
    c.vpblendvb(xmm4, xmm4, xmm5, xmm1); // vcc.lo
    c.vmovaps(JitPtr(vcc.hi), xmm3);
    c.vmovaps(JitPtr(vcc.lo), xmm4);
    c.vpblendvb(xmm0, xmm3, xmm4, xmm1); // mask
    c.vpblendvb(hd, hs, xmm2, xmm0);
    c.vmovaps(haccl, hd);
    c.vmovaps(JitPtr(vco.lo), xmm1);
    reg_alloc.Free(xmm3, xmm4, xmm5);
}

void vcl(u32 vs, u32 vt, u32 vd, u32 e)
{
    reg_alloc.Reserve(xmm3, xmm4, xmm5);
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(xmm1, JitPtr(vco.lo)); // vco.lo
    c.vpxor(xmm2, ht, xmm1); // nvt
    c.vpsubw(xmm2, xmm2, xmm1);
    c.vpsubw(xmm3, hs, xmm2); // diff
    c.vpaddusw(xmm4, hs, ht); // ncarry
    c.vpcmpeqw(xmm4, xmm4, xmm3);
    c.vpcmpeqw(xmm5, xmm0, JitPtr(vce)); // nvce
    c.vpcmpeqw(xmm3, xmm3, xmm0); // diff0
    c.vpand(xmm0, xmm3, xmm4); // lec1
    c.vpand(xmm0, xmm0, xmm5);
    c.vpor(xmm3, xmm3, xmm4); // lec2
    c.vpand(xmm3, xmm3, JitPtr(vce));
    c.vpor(xmm0, xmm0, xmm3); // leeq
    c.vpsubusw(xmm3, ht, hs); // geeq
    c.vpxor(xmm4, xmm4, xmm4);
    c.vpcmpeqw(xmm3, xmm3, xmm4);
    c.vmovaps(xmm4, JitPtr(vco.hi)); // vco.hi
    c.vpor(xmm5, xmm1, xmm4); // ge
    c.vpblendvb(xmm5, xmm3, JitPtr(vcc.hi), xmm5);
    c.vpandn(xmm3, xmm4, xmm1); // le
    c.vmovaps(xmm4, JitPtr(vcc.lo)); // vcc.lo
    c.vpblendvb(xmm3, xmm4, xmm0, xmm3);
    c.vpblendvb(xmm0, xmm5, xmm3, xmm1); // mask
    c.vpblendvb(hd, hs, xmm2, xmm0);
    c.vmovaps(haccl, hd);
    c.vmovaps(JitPtr(vcc.lo), xmm3);
    c.vmovaps(JitPtr(vcc.hi), xmm5);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vce), xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
    reg_alloc.Free(xmm3, xmm4, xmm5);
}

void vcr(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpxor(xmm0, hs, ht);
    c.vpsraw(xmm0, xmm0, 15); // sign
    c.vpand(xmm1, hs, xmm0);
    c.vpaddw(xmm1, xmm1, ht); // dlez
    c.vpsraw(xmm1, xmm1, 15); // vcc.lo
    c.vmovaps(JitPtr(vcc.lo), xmm1);
    c.vpor(xmm2, hs, xmm0);
    c.vpminsw(xmm2, xmm2, ht); // dgez
    c.vpcmpeqw(xmm2, xmm2, ht); // vcc.hi
    c.vmovaps(JitPtr(vcc.hi), xmm2);
    c.vpblendvb(xmm1, xmm2, xmm1, xmm0); // mask
    c.vpxor(xmm0, ht, xmm0); // nvt
    c.vpblendvb(hd, hs, xmm0, xmm1);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vce), xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void veq(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpcmpeqw(xmm0, hs, ht);
    c.vmovaps(xmm1, JitPtr(vco.hi));
    c.vpandn(xmm0, xmm1, xmm0);
    c.vmovaps(JitPtr(vcc.lo), xmm0);
    c.vpblendvb(hd, ht, hs, xmm0);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vcc.hi), xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void vge(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpcmpeqw(xmm0, hs, ht);
    c.vmovaps(xmm1, JitPtr(vco.lo));
    c.vpand(xmm1, xmm1, JitPtr(vco.hi));
    c.vpandn(xmm0, xmm1, xmm0);
    c.vpcmpgtw(xmm1, hs, ht);
    c.vpor(xmm0, xmm0, xmm1);
    c.vmovaps(JitPtr(vcc.lo), xmm0);
    c.vpblendvb(hd, ht, hs, xmm0);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vcc.hi), xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void vlt(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpcmpeqw(xmm0, hs, ht);
    c.vmovaps(xmm1, JitPtr(vco.lo));
    c.vpand(xmm1, xmm1, JitPtr(vco.hi));
    c.vpand(xmm0, xmm1, xmm0);
    c.vpcmpgtw(xmm1, ht, hs);
    c.vpor(xmm0, xmm0, xmm1);
    c.vmovaps(JitPtr(vcc.lo), xmm0);
    c.vpblendvb(hd, ht, hs, xmm0);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vcc.hi), xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

template<bool vmacf> void vmacfu(u32 vs, u32 vt, u32 vd, u32 e)
{
    reg_alloc.Reserve(xmm3, xmm4);
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmullw(xmm0, hs, ht); // low
    c.vpmulhw(xmm1, hs, ht); // high
    c.vpsraw(xmm2, xmm1, 15); // high mul sext
    c.vpaddw(hacch, hacch, xmm2);
    c.vpsrlw(xmm2, xmm0, 15); // low mul carry
    c.vpsllw(xmm0, xmm0, 1);
    c.vpsllw(xmm1, xmm1, 1);
    c.vpaddw(xmm1, xmm1, xmm2);
    c.vpaddw(haccl, haccl, xmm0);
    c.vpcmpeqw(xmm2, xmm2, xmm2); // 0xFFFF
    c.vpsllw(xmm3, xmm2, 15); // 0x8000
    c.vpaddw(xmm0, xmm0, xmm3);
    c.vpaddw(xmm4, haccl, xmm3);
    c.vpcmpgtw(xmm0, xmm0, xmm4); // low acc carry
    c.vpcmpeqw(xmm4, xmm1, xmm2);
    c.vpand(xmm4, xmm4, xmm0); // low acc carry && mid == 0xFFFF => overflow
    c.vpsubw(xmm1, xmm1, xmm0);
    c.vpaddw(haccm, haccm, xmm1);
    c.vpaddw(xmm0, xmm1, xmm3);
    c.vpaddw(xmm1, haccm, xmm3);
    c.vpcmpgtw(xmm0, xmm0, xmm1); // mid acc carry
    c.vpor(xmm0, xmm0, xmm4);
    c.vpsubw(hacch, hacch, xmm0);
    c.vpunpcklwd(xmm0, haccm, hacch);
    c.vpunpckhwd(xmm1, haccm, hacch);
    if constexpr (vmacf) {
        c.vpackssdw(hd, xmm0, xmm1);
    } else { // vmacu
        c.vpackusdw(xmm0, xmm0, xmm1);
        c.vpsraw(xmm1, xmm0, 15);
        c.vpblendvb(hd, xmm0, xmm2, xmm1);
    }
    reg_alloc.Free(xmm3, xmm4);
}

void vmacf(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmacfu<true>(vs, vt, vd, e);
}

void vmacq(u32 vd)
{
    reg_alloc.Reserve(xmm3);
    Xmm hd = GetDirtyVpr(vd);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpandn(xmm0, haccm, JitPtr(mask32x8)); // 0 or 32
    c.vpxor(xmm1, xmm1, xmm1);
    c.vpcmpeqw(xmm2, hacch, xmm1); // acch eqz
    c.vpsrlw(xmm3, haccm, 5);
    c.vpcmpgtw(xmm3, xmm3, xmm1); // accm geu32
    c.vpand(xmm2, xmm2, xmm3);
    c.vpcmpgtw(xmm3, hacch, xmm1); // acch gtz
    c.vpor(xmm1, xmm2, xmm3);
    c.vpand(xmm1, xmm0, xmm1); // neg addend
    c.vmovaps(xmm2, haccm); // prev accm
    c.vpsubw(haccm, haccm, xmm1);
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpsllw(xmm1, xmm1, 15); // 0x8000
    c.vpaddw(xmm3, haccm, xmm1);
    c.vpaddw(xmm1, xmm2, xmm1);
    c.vpcmpgtw(xmm1, xmm3, xmm1); // borrow
    c.vpxor(xmm2, xmm2, xmm2);
    c.vpcmpgtw(xmm2, xmm2, hacch); // acch ltz
    c.vpaddw(hacch, hacch, xmm1);
    c.vpand(xmm0, xmm0, xmm2); // pos addend
    c.vpaddw(haccm, haccm, xmm0);
    c.vpsrlw(xmm0, haccm, 1);
    c.vpsllw(xmm1, hacch, 15);
    c.vpor(xmm0, xmm0, xmm1); // clamp input low
    c.vpsraw(xmm1, hacch, 1); // clamp input high
    c.vpunpcklwd(xmm2, xmm0, xmm1);
    c.vpunpckhwd(xmm0, xmm0, xmm1);
    c.vpackssdw(xmm0, xmm2, xmm0); // clamp result
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpsllw(xmm1, xmm1, 4); // ~0xf mask
    c.vpand(hd, xmm0, xmm1);
    reg_alloc.Free(xmm3);
}

void vmacu(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmacfu<false>(vs, vt, vd, e);
}

void vmadh(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    Xmm haccm = GetDirtyAccMid(), hacch = GetDirtyAccHigh();
    c.vpmullw(xmm0, hs, ht); // low prod
    c.vpaddw(haccm, haccm, xmm0);
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpsllw(xmm1, xmm1, 15); // 0x8000
    c.vpaddw(xmm2, haccm, xmm1);
    c.vpaddw(xmm0, xmm0, xmm1);
    c.vpcmpgtw(xmm0, xmm0, xmm2); // mid carry
    c.vpsubw(hacch, hacch, xmm0);
    c.vpmulhw(xmm0, hs, ht);
    c.vpaddw(hacch, hacch, xmm0);
    c.vpunpcklwd(xmm0, haccm, hacch);
    c.vpunpckhwd(xmm1, haccm, hacch);
    c.vpackssdw(hd, xmm0, xmm1);
}

void vmadl(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmulhuw(xmm0, hs, ht);
    c.vpaddw(haccl, haccl, xmm0);
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpsllw(xmm1, xmm1, 15); // 0x8000
    c.vpaddw(xmm2, haccl, xmm1);
    c.vpaddw(xmm0, xmm0, xmm1);
    c.vpcmpgtw(xmm0, xmm0, xmm2); // low carry
    c.vpsubw(haccm, haccm, xmm0);
    c.vpxor(xmm1, xmm1, xmm1);
    c.vpcmpeqw(xmm2, xmm1, haccm);
    c.vpand(xmm0, xmm0, xmm2); // mid carry
    c.vpsubw(hacch, hacch, xmm0);
    c.vpsraw(xmm0, hacch, 15); // acc_high_neg
    c.vpcmpeqw(xmm2, xmm2, xmm2);
    c.vpblendvb(xmm0, xmm2, xmm1, xmm0);
    c.vpsraw(xmm1, haccm, 15);
    c.vpcmpeqw(xmm1, xmm1, hacch); // is_sign_ext
    c.vpblendvb(hd, xmm0, haccl, xmm1);
}

template<bool vmadm> void vmadmn(u32 vs, u32 vt, u32 vd, u32 e)
{
    reg_alloc.Reserve(xmm3);
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmullw(xmm0, hs, ht); // low prod
    c.vpaddw(haccl, haccl, xmm0);
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpsllw(xmm1, xmm1, 15); // 0x8000
    c.vpaddw(xmm2, haccl, xmm1);
    c.vpaddw(xmm0, xmm0, xmm1);
    c.vpcmpgtw(xmm0, xmm0, xmm2); // low acc carry
    c.vpxor(xmm2, xmm2, xmm2);
    if constexpr (vmadm) {
        c.vpcmpgtw(xmm2, xmm2, hs);
        c.vpsrlw(xmm2, xmm2, 15);
        c.vpmullw(xmm2, xmm2, ht);
    } else { // vmadn
        c.vpcmpgtw(xmm2, xmm2, ht);
        c.vpsrlw(xmm2, xmm2, 15);
        c.vpmullw(xmm2, xmm2, hs);
    }
    c.vpmulhuw(xmm3, hs, ht);
    c.vpsubw(xmm2, xmm3, xmm2); // add acc mid
    c.vpsraw(xmm3, xmm2, 15); // add acc high
    c.vpaddw(hacch, hacch, xmm3);
    c.vpcmpeqw(xmm3, xmm3, xmm3);
    c.vpcmpeqw(xmm3, xmm3, xmm2);
    c.vpand(xmm3, xmm0, xmm3); // add acc mid + low acc carry overflow
    c.vpsubw(xmm2, xmm2, xmm0);
    c.vpaddw(haccm, haccm, xmm2);
    c.vpaddw(xmm0, haccm, xmm1);
    c.vpaddw(xmm1, xmm2, xmm1);
    c.vpcmpgtw(xmm0, xmm1, xmm0); // mid acc carry
    c.vpor(xmm0, xmm0, xmm3);
    c.vpsubw(hacch, hacch, xmm0);
    if constexpr (vmadm) {
        c.vpunpcklwd(xmm0, haccm, hacch);
        c.vpunpckhwd(xmm1, haccm, hacch);
        c.vpackssdw(hd, xmm0, xmm1);
    } else { // vmadn
        c.vpsraw(xmm0, hacch, 15); // acc_high_neg
        c.vpxor(xmm1, xmm1, xmm1);
        c.vpcmpeqw(xmm2, xmm2, xmm2);
        c.vpblendvb(xmm0, xmm2, xmm1, xmm0);
        c.vpsraw(xmm1, haccm, 15);
        c.vpcmpeqw(xmm1, xmm1, hacch); // is_sign_ext
        c.vpblendvb(hd, xmm0, haccl, xmm1);
    }
    reg_alloc.Free(xmm3);
}

void vmadm(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmadmn<true>(vs, vt, vd, e);
}

void vmadn(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmadmn<false>(vs, vt, vd, e);
}

void vmov(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    Xmm hd = GetDirtyVpr(vd), ht = GetVte(vt, vt_e), haccl = GetDirtyAccLow();
    c.vpextrw(eax, ht, vd_e);
    c.vpinsrw(hd, hd, eax, vd_e);
    c.vmovaps(haccl, ht);
}

void vmrg(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vmovaps(xmm0, JitPtr(vcc.lo));
    c.vpblendvb(hd, ht, hs, xmm0);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void vmudh(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpxor(haccl, haccl, haccl);
    c.vpmullw(haccm, hs, ht);
    c.vpmulhw(hacch, hs, ht);
    c.vpunpcklwd(xmm0, haccm, hacch);
    c.vpunpckhwd(xmm1, haccm, hacch);
    c.vpackssdw(hd, xmm0, xmm1);
}

void vmudl(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmulhuw(hd, hs, ht);
    c.vmovaps(haccl, hd);
    c.vpxor(haccm, haccm, haccm);
    c.vpxor(hacch, hacch, hacch);
}

void vmudm(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmullw(haccl, hs, ht);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vpcmpgtw(xmm0, xmm0, hs);
    c.vpsrlw(xmm0, xmm0, 15);
    c.vpmullw(xmm0, xmm0, ht);
    c.vpmulhuw(xmm1, ht, hs);
    c.vpsubw(haccm, xmm1, xmm0);
    c.vpsraw(hacch, haccm, 15);
    c.vpunpcklwd(xmm0, haccm, hacch);
    c.vpunpckhwd(xmm1, haccm, hacch);
    c.vpackssdw(hd, xmm0, xmm1);
}

void vmudn(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmullw(haccl, hs, ht);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vpcmpgtw(xmm0, xmm0, ht);
    c.vpsrlw(xmm0, xmm0, 15);
    c.vpmullw(xmm0, xmm0, hs);
    c.vpmulhuw(xmm1, hs, ht);
    c.vpsubw(haccm, xmm1, xmm0);
    c.vpsraw(hacch, haccm, 15);
    c.vmovaps(hd, haccl);
}

template<bool vmulf> void vmulfu(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpmullw(haccl, hs, ht);
    c.vpmulhw(haccm, hs, ht);
    c.vpsrlw(xmm0, haccl, 15); // low carry mul; 0 or 1
    c.vpsraw(xmm1, haccm, 15); // high carry mul; 0 or 0xffff
    c.vpaddw(haccl, haccl, haccl);
    c.vpaddw(haccm, haccm, haccm);
    c.vpaddw(haccm, haccm, xmm0);
    c.vpcmpeqw(xmm0, xmm0, xmm0);
    c.vpsllw(xmm0, xmm0, 15); // 0x8000
    c.vpaddw(haccl, haccl, xmm0);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vpcmpgtw(xmm2, haccl, xmm0); // TODO: should be ge, not gt?
    c.vpsubw(haccm, haccm, xmm2);
    c.vpcmpeqw(xmm0, haccm, xmm0);
    c.vpand(xmm0, xmm0, xmm2); // high carry add
    c.vpxor(hacch, xmm0, xmm1);
    c.vpunpcklwd(xmm0, haccm, hacch);
    c.vpunpckhwd(xmm1, haccm, hacch);
    if constexpr (vmulf) {
        c.vpackssdw(hd, xmm0, xmm1);
    } else { // vmulu
        c.vpackusdw(xmm0, xmm0, xmm1);
        c.vpcmpeqw(xmm1, xmm1, xmm1);
        c.vpsraw(xmm2, xmm0, 15);
        c.vpblendvb(hd, xmm0, xmm1, xmm2);
    }
}

void vmulf(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmulfu<true>(vs, vt, vd, e);
}

void vmulq(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e);
    auto [haccl, haccm, hacch] = GetDirtyAccs();
    c.vpxor(haccl, haccl, haccl);
    c.vpmullw(haccm, hs, ht);
    c.vpmulhw(hacch, hs, ht);
    c.vpcmpeqw(xmm0, xmm0, xmm0);
    c.vpsrlw(xmm1, xmm0, 11); // 0x1f mask
    c.vpsraw(xmm2, hacch, 15);
    c.vpand(xmm1, xmm1, xmm2); // addend
    c.vpaddw(haccm, haccm, xmm1);
    c.vpsllw(xmm0, xmm0, 15);
    c.vpaddw(xmm2, haccm, xmm0);
    c.vpaddw(xmm0, xmm1, xmm0);
    c.vpcmpgtw(xmm0, xmm0, xmm2); // mid carry
    c.vpsubw(hacch, hacch, xmm0);
    c.vpsraw(xmm0, haccm, 1);
    c.vpsraw(xmm1, hacch, 1);
    c.vpunpcklwd(xmm2, xmm0, xmm1);
    c.vpunpckhwd(xmm0, xmm0, xmm1);
    c.vpackssdw(xmm0, xmm2, xmm0);
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpsllw(xmm1, xmm1, 4); // ~0xf mask
    c.vpand(hd, xmm0, xmm1);
    // todo: could keep result from first vpcmpeqw in xmm0. Then we'd have to use xmm3, and I guess we should let the
    // reg alloc have it? But we would not need the last vpcmpeqw. Probably go with first option
}

void vmulu(u32 vs, u32 vt, u32 vd, u32 e)
{
    vmulfu<false>(vs, vt, vd, e);
}

void vnand(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpand(hd, hs, ht);
    c.vpcmpeqd(xmm0, xmm0, xmm0);
    c.vpxor(hd, hd, xmm0);
    c.vmovaps(haccl, hd);
}

void vne(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpcmpeqw(xmm0, hs, ht); // eq
    c.vpcmpeqw(xmm1, xmm1, xmm1);
    c.vpxor(xmm0, xmm0, xmm1);
    c.vpor(xmm0, xmm0, JitPtr(vco.hi)); // vcc.lo
    c.vmovaps(JitPtr(vcc.lo), xmm0);
    c.vpblendvb(hd, ht, hs, xmm0);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vcc.hi), xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void vnop()
{
}

void vnor(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpor(hd, hs, ht);
    c.vpcmpeqd(xmm0, xmm0, xmm0);
    c.vpxor(hd, hd, xmm0);
    c.vmovaps(haccl, hd);
}

void vnxor(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpxor(hd, hs, ht);
    c.vpcmpeqd(xmm0, xmm0, xmm0);
    c.vpxor(hd, hd, xmm0);
    c.vmovaps(haccl, hd);
}

void vor(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpor(hd, hs, ht);
    c.vmovaps(haccl, hd);
}

void vrcpq(u32 vt, u32 vt_e, u32 vd, u32 vd_e, s32 (*impl)(s32))
{
    reg_alloc.DestroyVolatile(host_gpr_arg[0]);
    Xmm ht = GetVpr(vt), hte = GetVte(vt, vt_e), haccl = GetDirtyAccLow();
    c.vmovaps(haccl, hte);
    c.vpextrw(host_gpr_arg[0].r32(), ht, vt_e & 7);
    c.movsx(host_gpr_arg[0].r32(), host_gpr_arg[0].r16());
    reg_alloc.Call((void*)impl);
    Xmm hd = GetDirtyVpr(vd);
    c.vpinsrw(hd, hd, eax, vd_e & 7);
    c.shr(eax, 16);
    c.mov(JitPtr(div.out), ax);
    c.mov(JitPtr(div.dp), 0);
}

void vrcpql(u32 vt, u32 vt_e, u32 vd, u32 vd_e, s32 (*impl)(s32))
{
    reg_alloc.DestroyVolatile(host_gpr_arg[0]);
    Xmm ht = GetVpr(vt), hte = GetVte(vt, vt_e), haccl = GetDirtyAccLow();
    c.vmovaps(haccl, hte);
    c.vpextrw(host_gpr_arg[0].r32(), ht, vt_e & 7);
    c.mov(ax, JitPtr(div.in));
    c.shl(eax, 16);
    c.or_(eax, host_gpr_arg[0].r32());
    c.movsx(host_gpr_arg[0].r32(), host_gpr_arg[0].r16());
    c.cmp(JitPtr(div.dp), 1);
    c.cmove(host_gpr_arg[0].r32(), eax);
    reg_alloc.Call((void*)impl);
    Xmm hd = GetDirtyVpr(vd);
    c.vpinsrw(hd, hd, eax, vd_e & 7);
    c.shr(eax, 16);
    static_assert(sizeof(div) == 8);
    static_assert(offsetof(Div, out) == 0);
    static_assert(offsetof(Div, in) == 2);
    static_assert(offsetof(Div, dp) == 4);
    // out := ax, in := 0, dp := 0
    c.mov(JitPtr(div), rax);
}

void vrcp(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrcpq(vt, vt_e, vd, vd_e, Rcp);
}

void vrcph(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    Xmm hd = GetDirtyVpr(vd), ht = GetVpr(vt), hte = GetVte(vt, vt_e), haccl = GetDirtyAccLow();
    c.vmovaps(haccl, hte);
    c.vpinsrw(hd, hd, JitPtr(div.out), vd_e & 7);
    c.vpextrw(JitPtr(div.in), ht, vt_e & 7);
    c.mov(JitPtr(div.dp), 1);
}

void vrcpl(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrcpql(vt, vt_e, vd, vd_e, Rcp);
}

template<bool p> void vrnd(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    reg_alloc.Reserve(xmm3, xmm4);
    Xmm hd = GetDirtyVpr(vd), ht = GetVte(vt, vt_e), haccm = GetDirtyAccMid(), hacch = GetDirtyAccHigh();
    c.vpsraw(xmm0, hacch, 16); // cond
    if constexpr (p) {
        c.vpxor(xmm1, xmm1, xmm1);
        c.vpcmpeqw(xmm0, xmm0, xmm1);
    }
    if (vd_e & 1) { // add_low == 0, add_mid = vt, add_high = vt >> 16
        c.vpaddw(xmm1, haccm, ht); // new mid
        c.vpblendvb(xmm1, haccm, xmm1, xmm0);
        c.vpcmpeqw(xmm2, xmm2, xmm2);
        c.vpsllw(xmm2, xmm2, 15); // 0x8000
        c.vpaddw(xmm3, xmm1, xmm2);
        c.vpaddw(xmm2, haccm, xmm2);
        c.vpcmpgtw(xmm2, xmm2, xmm3); // mid carry
        c.vmovaps(haccm, xmm1);
        c.vpsubw(hacch, hacch, xmm2);
    } else { // add_low == ht, add_mid = vt >> 16, add_high = vt >> 16
        Xmm haccl = GetDirtyAccLow();
        c.vpaddw(xmm1, haccl, ht); // new low
        c.vpblendvb(xmm1, haccl, xmm1, xmm0);
        c.vpcmpeqw(xmm2, xmm2, xmm2); // 0xFFFF
        c.vpsllw(xmm3, xmm2, 15); // 0x8000
        c.vpaddw(xmm4, xmm1, xmm3);
        c.vpaddw(xmm3, haccl, xmm3);
        c.vpcmpgtw(xmm3, xmm3, xmm4); // low carry
        c.vmovaps(haccl, xmm1);
        c.vpsraw(xmm1, ht, 16); // add_mid
        c.vpxor(xmm4, xmm4, xmm4);
        c.vpblendvb(xmm1, xmm4, xmm1, xmm0);
        c.vpcmpeqw(xmm2, xmm1, xmm2);
        c.vpand(xmm2, xmm2, xmm3); // add acc mid + low acc carry overflow
        c.vpsubw(xmm1, xmm1, xmm3);
        c.vpaddw(xmm1, xmm1, haccm); // new mid
        c.vpcmpeqw(xmm3, xmm3, xmm3);
        c.vpsllw(xmm3, xmm3, 15); // 0x8000
        c.vpaddw(xmm4, xmm1, xmm3);
        c.vpaddw(xmm3, haccm, xmm3);
        c.vpcmpgtw(xmm3, xmm3, xmm4); // mid carry
        c.vpor(xmm3, xmm3, xmm2);
        c.vmovaps(haccm, xmm1);
        c.vpsubw(hacch, hacch, xmm3);
    }

    c.vpsraw(xmm1, ht, 16); // add_high
    c.vpaddw(xmm1, xmm1, hacch); // new high
    c.vpblendvb(hacch, hacch, xmm1, xmm0);
    c.vpunpcklwd(xmm0, haccm, hacch);
    c.vpunpckhwd(xmm1, haccm, hacch);
    c.vpackssdw(hd, xmm0, xmm1);

    reg_alloc.Free(xmm3, xmm4);
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
    vrcpq(vt, vt_e, vd, vd_e, Rsq);
}

void vrsqh(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrcph(vt, vt_e, vd, vd_e);
}

void vrsql(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
    vrcpql(vt, vt_e, vd, vd_e, Rsq);
}

void vsar(u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd);
    switch (e) {
    case 8: c.vmovaps(hd, GetAccHigh()); break;
    case 9: c.vmovaps(hd, GetAccMid()); break;
    case 10: c.vmovaps(hd, GetAccLow()); break;
    default: c.vpxor(hd, hd, hd); break;
    }
}

void vsub(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vmovaps(xmm0, JitPtr(vco.lo));
    c.vpsubw(xmm1, ht, xmm0); // diff
    c.vpsubw(haccl, hs, xmm1);
    c.vpsubsw(xmm0, ht, xmm0); // clamped diff
    c.vpsubsw(hd, hs, xmm0);
    c.vpcmpgtw(xmm0, xmm0, xmm1); // overflow
    c.vpaddsw(hd, hd, xmm0);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovaps(JitPtr(vco), ymm0);
}

void vsubc(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpcmpeqw(xmm0, xmm0, xmm0);
    c.vpsllw(xmm0, xmm0, 15); // 0x8000
    c.vpaddw(xmm1, hs, xmm0);
    c.vpaddw(xmm2, ht, xmm0);
    c.vpcmpgtw(xmm0, xmm2, xmm1); // vco.lo
    c.vmovaps(JitPtr(vco.lo), xmm0);
    c.vpsubw(hd, hs, ht);
    c.vmovaps(haccl, hd);
    c.vpxor(xmm1, xmm1, xmm1);
    c.vpcmpeqw(xmm1, hd, xmm1);
    c.vpcmpeqw(xmm2, xmm2, xmm2);
    c.vpxor(xmm1, xmm1, xmm2);
    c.vpor(xmm0, xmm0, xmm1);
    c.vmovaps(JitPtr(vco.hi), xmm0);
}

void vxor(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpxor(hd, hs, ht);
    c.vmovaps(haccl, hd);
}

void vzero(u32 vs, u32 vt, u32 vd, u32 e)
{
    Xmm hd = GetDirtyVpr(vd), hs = GetVpr(vs), ht = GetVte(vt, e), haccl = GetDirtyAccLow();
    c.vpaddw(haccl, hs, ht);
    c.vpxor(hd, hd, hd);
}

} // namespace n64::rsp::x64

#include "jit_util.hpp"
#include "recompiler.hpp"
#include "rsp.hpp"
#include "vu.hpp"

using namespace asmjit;
using namespace asmjit::x86;

namespace n64::rsp {

auto& c = compiler;

using enum CpuImpl;

static Mem acc_lo_ptr();
static Mem acc_mi_ptr();
static Mem acc_hi_ptr();
static Xmm get_vpr(u32 idx);
static void set_vpr(u32 idx, Xmm r);
static Mem vpr_ptr(u32 idx);

Mem acc_lo_ptr()
{
    return ptr(acc.low);
}

Mem acc_mi_ptr()
{
    return ptr(acc.mid);
}

Mem acc_hi_ptr()
{
    return ptr(acc.high);
}
Xmm get_vpr(u32 idx)
{
    Xmm v = c.newXmm();
    c.vmovdqa(v, vpr_ptr(idx));
    return v;
}

void set_vpr(u32 idx, Xmm r)
{
    c.vmovdqa(vpr_ptr(idx), r);
}

Mem vpr_ptr(u32 idx)
{
    return ptr(vpr[idx]);
}

// template<Cpu cpu, auto impl, typename Arg, typename... Args>
// void call_interpreter_impl(Arg first_arg, Args... remaining_args)
//{
//     static int r_idx{};
//     c.mov(gp[r_idx], first_arg);
//     if (sizeof...(remaining_args)) {
//         r_idx++;
//         jit_call_interpreter_impl<cpu, impl>(remaining_args...);
//     } else {
//         r_idx = 0;
//         compiler->call(impl);
//     }
// }

template<> void cfc2<Recompiler>(u32 rt, u32 vs)
{
    vs = std::min(vs & 3, 2u);
    c.vpxor(xmm0, xmm0, xmm0);
    c.vmovdqa(xmm1, ptr(&ctrl_reg[vs].lo));
    c.vpacksswb(xmm1, xmm1, xmm0);
    c.vpmovmskb(eax, xmm1);
    c.vmovdqa(xmm1, ptr(&ctrl_reg[vs].hi));
    c.vpacksswb(xmm1, xmm1, xmm0);
    c.vpmovmskb(ecx, xmm1);
}

template<> void ctc2<Recompiler>(u32 rt, u32 vs)
{
}

template<> void mfc2<Recompiler>(u32 rt, u32 vs, u32 e)
{
}

template<> void mtc2<Recompiler>(u32 rt, u32 vs, u32 e)
{
}

template<> void lbv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
    c.mov(gp[0], base);
    c.mov(gp[1], vt);
    c.mov(gp[2], e);
    c.mov(gp[3], offset);
    call(c, lbv<Interpreter>);
}

template<> void ldv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void lfv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void lhv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void llv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void lpv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void lqv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void lrv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void lsv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void ltv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void luv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void sbv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void sdv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void sfv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void shv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void slv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void spv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void sqv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void srv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void ssv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void stv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void suv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void swv<Recompiler>(u32 base, u32 vt, u32 e, s32 offset)
{
}

template<> void vabs<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vadd<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vaddc<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vand<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vch<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vcl<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vcr<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void veq<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vge<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vlt<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmacf<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmacq<Recompiler>(u32 vd)
{
}

template<> void vmacu<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmadh<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmadl<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmadm<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmadn<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmov<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vmrg<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmudh<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmudl<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmudm<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmudn<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmulf<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmulq<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vmulu<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vnand<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vne<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vnop<Recompiler>()
{
}

template<> void vnor<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vnxor<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vor<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vrcp<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrcph<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrcpl<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrndn<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrndp<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrsq<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrsqh<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vrsql<Recompiler>(u32 vt, u32 vt_e, u32 vd, u32 vd_e)
{
}

template<> void vsar<Recompiler>(u32 vd, u32 e)
{
    // switch (e) {
    // case 8: c.movdqa(xmm0, acc_lo_ptr()); break;
    // case 9: c.movdqa(xmm0, acc_mi_ptr()); break;
    // case 10: c.movdqa(xmm0, acc_hi_ptr()); break;
    // default: c.pxor(xmm0, xmm0); break;
    // }
    // set_vpr(vd, xmm0);
}

template<> void vsub<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vsubc<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vxor<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

template<> void vzero<Recompiler>(u32 vs, u32 vt, u32 vd, u32 e)
{
}

} // namespace n64::rsp

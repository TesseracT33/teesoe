#pragma once

#include "mips/disassembler.hpp"

namespace n64::rsp {

class Disassembler : public mips::Disassembler {
public:
    auto cfc2(u32 rt, u32 vs) { return std::format("cfc2 {}, v{}", mips::GprIdxToName(rt), vs); }
    auto ctc2(u32 rt, u32 vs) { return std::format("ctc2 v{}, {}", vs, mips::GprIdxToName(rt)); }
    auto mfc2(u32 rt, u32 vs, u32 e) { return std::format("mfc2 {}, v{}({})", mips::GprIdxToName(rt), vs, e); }
    auto mtc2(u32 rt, u32 vs, u32 e) { return std::format("mtc2 v{}({}), {}", vs, e, mips::GprIdxToName(rt)); }

    auto lbv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lbv", vt, e, base, offset); }
    auto ldv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("ldv", vt, e, base, offset); }
    auto lfv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lfv", vt, e, base, offset); }
    auto lhv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lhv", vt, e, base, offset); }
    auto llv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("llv", vt, e, base, offset); }
    auto lpv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lpv", vt, e, base, offset); }
    auto lqv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lqv", vt, e, base, offset); }
    auto lrv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lrv", vt, e, base, offset); }
    auto lsv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("lsv", vt, e, base, offset); }
    auto ltv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("ltv", vt, e, base, offset); }
    auto luv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("luv", vt, e, base, offset); }
    auto sbv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("sbv", vt, e, base, offset); }
    auto sdv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("sdv", vt, e, base, offset); }
    auto sfv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("sfv", vt, e, base, offset); }
    auto shv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("shv", vt, e, base, offset); }
    auto slv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("slv", vt, e, base, offset); }
    auto spv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("spv", vt, e, base, offset); }
    auto sqv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("sqv", vt, e, base, offset); }
    auto srv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("srv", vt, e, base, offset); }
    auto ssv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("ssv", vt, e, base, offset); }
    auto stv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("stv", vt, e, base, offset); }
    auto suv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("suv", vt, e, base, offset); }
    auto swv(u32 base, u32 vt, u32 e, s32 offset) { return VteBaseOffset("swv", vt, e, base, offset); }

    auto vabs(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vabs", vd, vs, vt, e); }
    auto vadd(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vadd", vd, vs, vt, e); }
    auto vaddc(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vaddc", vd, vs, vt, e); }
    auto vand(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vand", vd, vs, vt, e); }
    auto vch(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vch", vd, vs, vt, e); }
    auto vcl(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vcl", vd, vs, vt, e); }
    auto vcr(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vcr", vd, vs, vt, e); }
    auto veq(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("veq", vd, vs, vt, e); }
    auto vge(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vge", vd, vs, vt, e); }
    auto vlt(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vlt", vd, vs, vt, e); }
    auto vmacf(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmacf", vd, vs, vt, e); }
    auto vmacq(u32 vd) { return std::format("vmacq v{}", vd); }
    auto vmacu(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmacu", vd, vs, vt, e); }
    auto vmadh(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmadh", vd, vs, vt, e); }
    auto vmadl(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmadl", vd, vs, vt, e); }
    auto vmadm(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmadm", vd, vs, vt, e); }
    auto vmadn(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmadn", vd, vs, vt, e); }
    auto vmov(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vmov", vd, vd_e, vt, vt_e); }
    auto vmrg(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmrg", vd, vs, vt, e); }
    auto vmudh(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmudh", vd, vs, vt, e); }
    auto vmudl(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmudl", vd, vs, vt, e); }
    auto vmudm(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vudm", vd, vs, vt, e); }
    auto vmudn(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmudn", vd, vs, vt, e); }
    auto vmulf(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmulf", vd, vs, vt, e); }
    auto vmulq(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmulq", vd, vs, vt, e); }
    auto vmulu(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vmulu", vd, vs, vt, e); }
    auto vnand(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vnand", vd, vs, vt, e); }
    auto vne(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vne", vd, vs, vt, e); }
    auto vnop() { return "vnop"; }
    auto vnor(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vnor", vd, vs, vt, e); }
    auto vnxor(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vnxor", vd, vs, vt, e); }
    auto vor(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vor", vd, vs, vt, e); }
    auto vrcp(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrcp", vd, vd_e, vt, vt_e); }
    auto vrcph(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrcph", vd, vd_e, vt, vt_e); }
    auto vrcpl(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrcpl", vd, vd_e, vt, vt_e); }
    auto vrndn(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrndn", vd, vd_e, vt, vt_e); }
    auto vrndp(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrndp", vd, vd_e, vt, vt_e); }
    auto vrsq(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrsq", vd, vd_e, vt, vt_e); }
    auto vrsqh(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrsqh", vd, vd_e, vt, vt_e); }
    auto vrsql(u32 vt, u32 vt_e, u32 vd, u32 vd_e) { return VdeVte("vrsql", vd, vd_e, vt, vt_e); }
    auto vsar(u32 vd, u32 e) { return std::format("vsar v{}()", vd, e); }
    auto vsub(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vsub", vd, vs, vt, e); }
    auto vsubc(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vsubc", vd, vs, vt, e); }
    auto vxor(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vxor", vd, vs, vt, e); }
    auto vzero(u32 vs, u32 vt, u32 vd, u32 e) { return VdVsVte("vzero", vd, vs, vt, e); }

protected:
    std::string VdVsVte(std::string_view instr, u32 vd, u32 vs, u32 vt, u32 e)
    {
        return std::format("{} v{}, v{}, v{}({})", instr, vd, vs, vt, e);
    }
    std::string VdeVte(std::string_view instr, u32 vd, u32 vd_e, u32 vt, u32 vt_e)
    {
        return std::format("{} v{}({}), v{}({})", instr, vd, vd_e, vt, vt_e);
    }
    std::string VteBaseOffset(std::string_view instr, u32 vt, u32 e, u32 base, u32 offset)
    {
        return std::format("{} v{}({}), {} + ${:08X}", instr, vt, e, mips::GprIdxToName(base), offset);
    }
} inline disassembler;

} // namespace n64::rsp

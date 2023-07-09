#pragma once

#include "mips/disassembler.hpp"

namespace n64::vr4300 {

class Disassembler : public mips::Disassembler {
public:
    using mips::Disassembler::add;
    using mips::Disassembler::div;
    using mips::Disassembler::sub;

    auto dmfc0(u32 rt, u32 rd) { return std::format("dmfc0 {}, {}", mips::GprIdxToName(rt), cop0_reg_to_str[rd]); }
    auto dmtc0(u32 rt, u32 rd) { return std::format("dmtc0 {}, {}", cop0_reg_to_str[rd], mips::GprIdxToName(rt)); }
    auto mfc0(u32 rt, u32 rd) { return std::format("mfc0 {}, {}", mips::GprIdxToName(rt), cop0_reg_to_str[rd]); }
    auto mtc0(u32 rt, u32 rd) { return std::format("mtc0 {}, {}", cop0_reg_to_str[rd], mips::GprIdxToName(rt)); }

    auto bc1f(u16 imm) { return std::format("bc1f ${:04X}", imm); }
    auto bc1fl(u16 imm) { return std::format("bc1fl ${:04X}", imm); }
    auto bc1t(u16 imm) { return std::format("bc1t ${:04X}", imm); }
    auto bc1tl(u16 imm) { return std::format("bc1tl ${:04X}", imm); }

    auto cfc1(u32 fs, u32 rt) { return RtFs("cfc1", rt, fs); }
    auto ctc1(u32 fs, u32 rt) { return FsRt("ctc1", fs, rt); }
    auto dcfc1() { return "dcfc1"; }
    auto dctc1() { return "dctc1"; }
    auto dmfc1(u32 fs, u32 rt) { return RtFs("dmfc1", rt, fs); }
    auto dmtc1(u32 fs, u32 rt) { return FsRt("dmtc1", fs, rt); }
    auto ldc1(u32 base, u32 ft, u16 imm) { return BaseFtImm("ldc1", base, ft, imm); }
    auto lwc1(u32 base, u32 ft, u16 imm) { return BaseFtImm("lwc1", base, ft, imm); }
    auto mfc1(u32 fs, u32 rt) { return RtFs("mfc1", rt, fs); }
    auto mtc1(u32 fs, u32 rt) { return FsRt("mtc1", fs, rt); }
    auto sdc1(u32 base, u32 ft, u16 imm) { return BaseFtImm("sdc1", base, ft, imm); }
    auto swc1(u32 base, u32 ft, u16 imm) { return BaseFtImm("swc1", base, ft, imm); }

    template<Fmt fmt> auto compare(u32 fs, u32 ft, u8 cond)
    {
        return std::format("c.{}. f{}, f{}", compare_cond_to_str[cond], FmtToStr(fmt), fs, ft);
    }

    template<Fmt fmt> auto ceil_l(u32 fs, u32 fd) { return FmtInstr("ceil", Fmt::Int64, fmt, fs, fd); }
    template<Fmt fmt> auto ceil_w(u32 fs, u32 fd) { return FmtInstr("ceil", Fmt::Int32, fmt, fs, fd); }
    template<Fmt fmt> auto cvt_d(u32 fs, u32 fd) { return FmtInstr("cvt", Fmt::Float64, fmt, fs, fd); }
    template<Fmt fmt> auto cvt_l(u32 fs, u32 fd) { return FmtInstr("cvt", Fmt::Int64, fmt, fs, fd); }
    template<Fmt fmt> auto cvt_s(u32 fs, u32 fd) { return FmtInstr("cvt", Fmt::Float32, fmt, fs, fd); }
    template<Fmt fmt> auto cvt_w(u32 fs, u32 fd) { return FmtInstr("cvt", Fmt::Int32, fmt, fs, fd); }
    template<Fmt fmt> auto floor_l(u32 fs, u32 fd) { return FmtInstr("floor", Fmt::Int64, fmt, fs, fd); }
    template<Fmt fmt> auto floor_w(u32 fs, u32 fd) { return FmtInstr("floor", Fmt::Int32, fmt, fs, fd); }
    template<Fmt fmt> auto round_l(u32 fs, u32 fd) { return FmtInstr("round", Fmt::Int64, fmt, fs, fd); }
    template<Fmt fmt> auto round_w(u32 fs, u32 fd) { return FmtInstr("round", Fmt::Int32, fmt, fs, fd); }
    template<Fmt fmt> auto trunc_l(u32 fs, u32 fd) { return FmtInstr("trunc", Fmt::Int64, fmt, fs, fd); }
    template<Fmt fmt> auto trunc_w(u32 fs, u32 fd) { return FmtInstr("trunc", Fmt::Int32, fmt, fs, fd); }

    template<Fmt fmt> auto abs(u32 fs, u32 fd) { return FmtInstr("abs", fmt, fs, fd); }
    template<Fmt fmt> auto add(u32 fs, u32 ft, u32 fd) { return FmtInstr("add", fmt, fs, ft, fd); }
    template<Fmt fmt> auto div(u32 fs, u32 ft, u32 fd) { return FmtInstr("div", fmt, fs, ft, fd); }
    template<Fmt fmt> auto mov(u32 fs, u32 fd) { return FmtInstr("mov", fmt, fs, fd); }
    template<Fmt fmt> auto mul(u32 fs, u32 ft, u32 fd) { return FmtInstr("mul", fmt, fs, ft, fd); }
    template<Fmt fmt> auto neg(u32 fs, u32 fd) { return FmtInstr("neg", fmt, fs, fd); }
    template<Fmt fmt> auto sqrt(u32 fs, u32 fd) { return FmtInstr("sqrt", fmt, fs, fd); }
    template<Fmt fmt> auto sub(u32 fs, u32 ft, u32 fd) { return FmtInstr("sub", fmt, fs, ft, fd); }

    auto cfc2(u32 rt) { return std::format("cfc2 {}", mips::GprIdxToName(rt)); }
    auto ctc2(u32 rt) { return std::format("cfc2 {}", mips::GprIdxToName(rt)); }
    auto dcfc2() { return "dcfc2"; }
    auto dctc2() { return "dctc2"; }
    auto dmfc2(u32 rt) { return std::format("dmfc2 {}", mips::GprIdxToName(rt)); }
    auto dmtc2(u32 rt) { return std::format("dmtc2 {}", mips::GprIdxToName(rt)); }
    auto mfc2(u32 rt) { return std::format("mfc2 {}", mips::GprIdxToName(rt)); }
    auto mtc2(u32 rt) { return std::format("mtc2 {}", mips::GprIdxToName(rt)); }

    auto cop2_reserved() { return "COP2 RESERVED"; }

protected:
    constexpr std::string_view FmtToStr(Fmt fmt)
    {
        switch (fmt) {
        case Fmt::Float32: return "S";
        case Fmt::Float64: return "D";
        case Fmt::Int32: return "W";
        case Fmt::Int64: return "L";
        case Fmt::Invalid: return "INVALID";
        }
    }

    std::string FmtInstr(std::string_view instr, Fmt fmt_to, Fmt fmt_from, u32 fs, u32 fd)
    {
        return std::format("{}.{}.{} f{}, f{}", instr, FmtToStr(fmt_to), FmtToStr(fmt_from), fd, fs);
    }

    std::string FmtInstr(std::string_view instr, Fmt fmt, u32 fs, u32 fd)
    {
        return std::format("{}.{} f{}, f{}", instr, FmtToStr(fmt), fd, fs);
    }

    std::string FmtInstr(std::string_view instr, Fmt fmt, u32 fs, u32 ft, u32 fd)
    {
        return std::format("{}.{} f{}, f{}, f{}", instr, FmtToStr(fmt), fd, fs, ft);
    }

    std::string FsRt(std::string_view instr, u32 fs, u32 rt)
    {
        return std::format("{} f{}, {}", instr, fs, mips::GprIdxToName(rt));
    }

    std::string RtFs(std::string_view instr, u32 rt, u32 fs)
    {
        return std::format("{} {}, f{}", instr, mips::GprIdxToName(rt), fs);
    }

    std::string BaseFtImm(std::string_view instr, u32 base, u32 ft, u16 imm)
    {
        return std::format("{} f{}, {} + ${:04X}", instr, ft, mips::GprIdxToName(base), imm);
    }

    static constexpr std::array compare_cond_to_str = {
        "F",
        "UN",
        "EQ",
        "UEQ",
        "OLT",
        "ULT",
        "OLE",
        "ULE",
        "SF",
        "NGLE",
        "SEQ",
        "NGL",
        "LT",
        "NGE",
        "LE",
        "NGT",
    };

} inline disassembler;

} // namespace n64::vr4300

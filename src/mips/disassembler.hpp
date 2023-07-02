#pragma once

#include "types.hpp"

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace mips {

constexpr std::string_view GprIdxToName(u32 idx)
{
    switch (idx) {
    case 0: return "$zero";
    case 1: return "$at";
    case 2: return "$v0";
    case 3: return "$v1";
    case 4: return "$a0";
    case 5: return "$a1";
    case 6: return "$a2";
    case 7: return "$a3";
    case 8: return "$t0";
    case 9: return "$t1";
    case 10: return "$t2";
    case 11: return "$t3";
    case 12: return "$t4";
    case 13: return "$t5";
    case 14: return "$t6";
    case 15: return "$t7";
    case 16: return "$s0";
    case 17: return "$s1";
    case 18: return "$s2";
    case 19: return "$s3";
    case 20: return "$s4";
    case 21: return "$s5";
    case 22: return "$s6";
    case 23: return "$s7";
    case 24: return "$t8";
    case 25: return "$t9";
    case 26: return "$k0";
    case 27: return "$k1";
    case 28: return "$gp";
    case 29: return "$sp";
    case 30: return "$fp";
    case 31: return "$ra";
    default: std::unreachable();
    }
}

class Disassembler {
public:
    auto add(u32 rs, u32 rt, u32 rd) { return RdRsRt("add", rd, rs, rt); }
    auto addi(u32 rs, u32 rt, s16 imm) { return RtRsImm("addi", rt, rs, imm); }
    auto addiu(u32 rs, u32 rt, s16 imm) { return RtRsImm("addiu", rt, rs, imm); }
    auto addu(u32 rs, u32 rt, u32 rd) { return RdRsRt("addu", rd, rs, rt); }
    auto and_(u32 rs, u32 rt, u32 rd) { return RdRsRt("and", rd, rs, rt); }
    auto andi(u32 rs, u32 rt, u16 imm) { return RtRsImm("andi", rt, rs, imm); }
    auto beq(u32 rs, u32 rt, s16 imm) { return RsRtImm("beq", rs, rt, imm); }
    auto beql(u32 rs, u32 rt, s16 imm) { return RsRtImm("beql", rs, rt, imm); }
    auto bgez(u32 rs, s16 imm) { return RsImm("bgez", rs, imm); }
    auto bgezal(u32 rs, s16 imm) { return RsImm("bgezal", rs, imm); }
    auto bgezall(u32 rs, s16 imm) { return RsImm("bgezall", rs, imm); }
    auto bgezl(u32 rs, s16 imm) { return RsImm("bgezl", rs, imm); }
    auto bgtz(u32 rs, s16 imm) { return RsImm("bgtz", rs, imm); }
    auto bgtzl(u32 rs, s16 imm) { return RsImm("bgtzl", rs, imm); }
    auto blez(u32 rs, s16 imm) { return RsImm("blez", rs, imm); }
    auto blezl(u32 rs, s16 imm) { return RsImm("blezl", rs, imm); }
    auto bltz(u32 rs, s16 imm) { return RsImm("bltz", rs, imm); }
    auto bltzl(u32 rs, s16 imm) { return RsImm("bltzl", rs, imm); }
    auto bltzal(u32 rs, s16 imm) { return RsImm("bltzal", rs, imm); }
    auto bltzall(u32 rs, s16 imm) { return RsImm("bltzall", rs, imm); }
    auto bne(u32 rs, u32 rt, s16 imm) { return RsRtImm("bne", rs, rt, imm); }
    auto bnel(u32 rs, u32 rt, s16 imm) { return RsRtImm("bnel", rs, rt, imm); }
    auto break_() { return "break"; }
    auto cache(u32 rs, u32 rt, s16 imm) { return RsRtImm("cache", rs, rt, imm); }
    auto dadd(u32 rs, u32 rt, u32 rd) { return RdRsRt("dadd", rd, rs, rt); }
    auto daddi(u32 rs, u32 rt, s16 imm) { return RtRsImm("daddi", rt, rs, imm); }
    auto daddiu(u32 rs, u32 rt, s16 imm) { return RtRsImm("daddiu", rt, rs, imm); }
    auto daddu(u32 rs, u32 rt, u32 rd) { return RdRsRt("daddu", rd, rs, rt); }
    auto ddiv(u32 rs, u32 rt) { return RsRt("ddiv", rs, rt); }
    auto ddivu(u32 rs, u32 rt) { return RsRt("ddivu", rs, rt); }
    auto div(u32 rs, u32 rt) { return RsRt("div", rs, rt); }
    auto divu(u32 rs, u32 rt) { return RsRt("ddivu", rs, rt); }
    auto dmult(u32 rs, u32 rt) { return RsRt("dmult", rs, rt); }
    auto dmultu(u32 rs, u32 rt) { return RsRt("dmultu", rs, rt); }
    auto dsll(u32 rt, u32 rd, u32 sa) { return RdRtSa("dsll", rd, rt, sa); }
    auto dsll32(u32 rt, u32 rd, u32 sa) { return RdRtSa("dsll", rd, rt, sa); }
    auto dsllv(u32 rs, u32 rt, u32 rd) { return RdRtRs("dsllv", rd, rt, rs); }
    auto dsra(u32 rt, u32 rd, u32 sa) { return RdRtSa("dsra", rd, rt, sa); }
    auto dsra32(u32 rt, u32 rd, u32 sa) { return RdRtSa("dsra32", rd, rt, sa); }
    auto dsrav(u32 rs, u32 rt, u32 rd) { return RdRtRs("dsrav", rd, rt, rs); }
    auto dsrl(u32 rt, u32 rd, u32 sa) { return RdRtSa("dsrl", rd, rt, sa); }
    auto dsrl32(u32 rt, u32 rd, u32 sa) { return RdRtSa("dsrl32", rd, rt, sa); }
    auto dsrlv(u32 rs, u32 rt, u32 rd) { return RdRtRs("dsrlv", rd, rt, rs); }
    auto dsub(u32 rs, u32 rt, u32 rd) { return RdRsRt("dsub", rd, rs, rt); }
    auto dsubu(u32 rs, u32 rt, u32 rd) { return RdRsRt("dsubu", rd, rs, rt); }
    auto j(u32 instr) { return std::format("j ${:08X}", instr << 2 & 0xFFF'FFFF); }
    auto jal(u32 instr) { return std::format("jal ${:08X}", instr << 2 & 0xFFF'FFFF); }
    auto jalr(u32 rs, u32 rd) { return std::format("jalr {} {}", GprIdxToName(rs), GprIdxToName(rd)); }
    auto jr(u32 rs) { return std::format("jr {}", GprIdxToName(rs)); }
    auto lb(u32 rs, u32 rt, s16 imm) { return RtRsImm("lb", rt, rs, imm); }
    auto lbu(u32 rs, u32 rt, s16 imm) { return RtRsImm("lbu", rt, rs, imm); }
    auto ld(u32 rs, u32 rt, s16 imm) { return RtRsImm("ld", rt, rs, imm); }
    auto ldl(u32 rs, u32 rt, s16 imm) { return RtRsImm("ld", rt, rs, imm); }
    auto ldr(u32 rs, u32 rt, s16 imm) { return RtRsImm("ldr", rt, rs, imm); }
    auto lh(u32 rs, u32 rt, s16 imm) { return RtRsImm("lh", rt, rs, imm); }
    auto lhu(u32 rs, u32 rt, s16 imm) { return RtRsImm("lhu", rt, rs, imm); }
    auto ll(u32 rs, u32 rt, s16 imm) { return RtRsImm("ll", rt, rs, imm); }
    auto lld(u32 rs, u32 rt, s16 imm) { return RtRsImm("lld", rt, rs, imm); }
    auto lui(u32 rt, s16 imm) { return RtImm("lui", rt, imm); }
    auto lw(u32 rs, u32 rt, s16 imm) { return RtRsImm("lw", rt, rs, imm); }
    auto lwl(u32 rs, u32 rt, s16 imm) { return RtRsImm("lwl", rt, rs, imm); }
    auto lwr(u32 rs, u32 rt, s16 imm) { return RtRsImm("lwr", rt, rs, imm); }
    auto lwu(u32 rs, u32 rt, s16 imm) { return RtRsImm("lwu", rt, rs, imm); }
    auto mfhi(u32 rd) { return std::format("mfhi {}", GprIdxToName(rd)); }
    auto mflo(u32 rd) { return std::format("mflo {}", GprIdxToName(rd)); }
    auto movn(u32 rs, u32 rt, u32 rd) { return RdRsRt("movn", rd, rs, rt); }
    auto movz(u32 rs, u32 rt, u32 rd) { return RdRsRt("movz", rd, rs, rt); }
    auto mthi(u32 rs) { return std::format("mthi {}", GprIdxToName(rs)); }
    auto mtlo(u32 rs) { return std::format("mtlo {}", GprIdxToName(rs)); }
    auto mult(u32 rs, u32 rt) { return RsRt("mult", rs, rt); }
    auto multu(u32 rs, u32 rt) { return RsRt("multu", rs, rt); }
    auto nor(u32 rs, u32 rt, u32 rd) { return RdRsRt("nor", rd, rs, rt); }
    auto or_(u32 rs, u32 rt, u32 rd) { return RdRsRt("or", rd, rs, rt); }
    auto ori(u32 rs, u32 rt, u16 imm) { return RtRsImm("ori", rt, rs, imm); }
    auto sb(u32 rs, u32 rt, s16 imm) { return RtRsImm("sb", rt, rs, imm); }
    auto sc(u32 rs, u32 rt, s16 imm) { return RtRsImm("sc", rt, rs, imm); }
    auto scd(u32 rs, u32 rt, s16 imm) { return RtRsImm("scd", rt, rs, imm); }
    auto sd(u32 rs, u32 rt, s16 imm) { return RtRsImm("sd", rt, rs, imm); }
    auto sdl(u32 rs, u32 rt, s16 imm) { return RtRsImm("sdl", rt, rs, imm); }
    auto sdr(u32 rs, u32 rt, s16 imm) { return RtRsImm("sdr", rt, rs, imm); }
    auto sh(u32 rs, u32 rt, s16 imm) { return RtRsImm("sh", rt, rs, imm); }
    auto sll(u32 rt, u32 rd, u32 sa) { return !rt && !rd && !sa ? "nop" : RdRtSa("sll", rd, rt, sa); }
    auto sllv(u32 rs, u32 rt, u32 rd) { return RdRtRs("sllv", rd, rt, rs); }
    auto slt(u32 rs, u32 rt, u32 rd) { return RdRsRt("slt", rd, rs, rt); }
    auto slti(u32 rs, u32 rt, s16 imm) { return RtRsImm("slti", rt, rs, imm); }
    auto sltiu(u32 rs, u32 rt, s16 imm) { return RtRsImm("sltiu", rt, rs, imm); }
    auto sltu(u32 rs, u32 rt, u32 rd) { return RdRsRt("sltu", rd, rs, rt); }
    auto sra(u32 rt, u32 rd, u32 sa) { return RdRtSa("sra", rd, rt, sa); }
    auto srav(u32 rs, u32 rt, u32 rd) { return RdRtRs("srav", rd, rt, rs); }
    auto srl(u32 rt, u32 rd, u32 sa) { return RdRtSa("srl", rd, rt, sa); }
    auto srlv(u32 rs, u32 rt, u32 rd) { return RdRtRs("srlv", rd, rt, rs); }
    auto sub(u32 rs, u32 rt, u32 rd) { return RdRsRt("sub", rd, rs, rt); }
    auto subu(u32 rs, u32 rt, u32 rd) { return RdRsRt("subu", rd, rs, rt); }
    auto sync() { return "sync"; }
    auto syscall() { return "syscall"; }
    auto sw(u32 rs, u32 rt, s16 imm) { return RtRsImm("sw", rt, rs, imm); }
    auto swl(u32 rs, u32 rt, s16 imm) { return RtRsImm("swl", rt, rs, imm); }
    auto swr(u32 rs, u32 rt, s16 imm) { return RtRsImm("swr", rt, rs, imm); }
    auto teq(u32 rs, u32 rt) { return RsRt("teq", rs, rt); }
    auto teqi(u32 rs, s16 imm) { return RsImm("teqi", rs, imm); }
    auto tge(u32 rs, u32 rt) { return RsRt("tge", rs, rt); }
    auto tgei(u32 rs, s16 imm) { return RsImm("tgei", rs, imm); }
    auto tgeu(u32 rs, u32 rt) { return RsRt("tgeu", rs, rt); }
    auto tgeiu(u32 rs, s16 imm) { return RsImm("tgeiu", rs, imm); }
    auto tlt(u32 rs, u32 rt) { return RsRt("tlt", rs, rt); }
    auto tlti(u32 rs, s16 imm) { return RsImm("tlti", rs, imm); }
    auto tltu(u32 rs, u32 rt) { return RsRt("tltu", rs, rt); }
    auto tltiu(u32 rs, s16 imm) { return RsImm("tltiu", rs, imm); }
    auto tne(u32 rs, u32 rt) { return RsRt("tne", rs, rt); }
    auto tnei(u32 rs, s16 imm) { return RsImm("tnei", rs, imm); }
    auto xor_(u32 rs, u32 rt, u32 rd) { return RdRsRt("xor", rd, rs, rt); }
    auto xori(u32 rs, u32 rt, u16 imm) { return RtRsImm("xori", rt, rs, imm); }

    auto dmfc0(u32 rt, u32 rd) { return std::format("dmfc0 {}, {}", GprIdxToName(rd), rt); }
    auto dmtc0(u32 rt, u32 rd) { return std::format("dmtc0 {}, {}", rd, GprIdxToName(rt)); }
    auto eret() { return "eret"; }
    auto mfc0(u32 rt, u32 rd) { return std::format("mfc0 {}, {}", GprIdxToName(rd), rt); }
    auto mtc0(u32 rt, u32 rd) { return std::format("mtc0 {}, {}", rd, GprIdxToName(rt)); }
    auto tlbr() { return "tlbr"; }
    auto tlbwi() { return "tlbwi"; }
    auto tlbwr() { return "tlbwr"; }
    auto tlbp() { return "tlbp"; }

protected:
    std::string RdRsRt(std::string_view instr, u32 rd, u32 rs, u32 rt)
    {
        return std::format("{} {}, {}, {}", instr, GprIdxToName(rd), GprIdxToName(rs), GprIdxToName(rt));
    }

    std::string RdRtRs(std::string_view instr, u32 rd, u32 rt, u32 rs)
    {
        return std::format("{} {}, {}, {}", instr, GprIdxToName(rd), GprIdxToName(rt), GprIdxToName(rs));
    }

    std::string RsRtImm(std::string_view instr, u32 rs, u32 rt, u16 imm)
    {
        return std::format("{} {}, {}, ${:04X}", instr, GprIdxToName(rs), GprIdxToName(rt), imm);
    }

    std::string RsRt(std::string_view instr, u32 rs, u32 rt)
    {
        return std::format("{} {}, {}", instr, GprIdxToName(rs), GprIdxToName(rt));
    }

    std::string RtRd(std::string_view instr, u32 rt, u32 rd) { return RsRt(instr, rt, rd); }

    std::string RdRt(std::string_view instr, u32 rd, u32 rt) { return RsRt(instr, rd, rt); }

    std::string RsImm(std::string_view instr, u32 rs, u16 imm)
    {
        return std::format("{} {}, ${:04X}", instr, GprIdxToName(rs), imm);
    }
    std::string RtImm(std::string_view instr, u32 rt, u16 imm) { return RsImm(instr, rt, imm); }

    std::string RtRsImm(std::string_view instr, u32 rt, u32 rs, u16 imm)
    {
        return std::format("{} {}, {}, ${:04X}", instr, GprIdxToName(rt), GprIdxToName(rs), imm);
    }

    std::string RdRtSa(std::string_view instr, u32 rd, u32 rt, u32 sa)
    {
        return std::format("{} {}, {}, {}", instr, GprIdxToName(rd), GprIdxToName(rt), GprIdxToName(sa));
    }
};

} // namespace mips

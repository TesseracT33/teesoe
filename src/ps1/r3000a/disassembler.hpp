#pragma once

#include "mips/disassembler.hpp"

namespace ps1::r3000a {

class Disassembler : public mips::Disassembler {
public:
    auto mfc0(u32 rt, u32 rd) { return std::format("mfc0 {}, {}", mips::GprIdxToName(rt), cop0_reg_to_str[rd]); }
    auto mtc0(u32 rt, u32 rd) { return std::format("mtc0 {}, {}", cop0_reg_to_str[rd], mips::GprIdxToName(rt)); }
    auto rfe() { return "rfe"; }

protected:
    static constexpr std::array cop0_reg_to_str = {
        "UNUSED_0",
        "UNUSED_1",
        "UNUSED_2",
        "BPC",
        "UNUSED_4",
        "BDA",
        "JUMPDEST",
        "DCIC",
        "BadVaddr",
        "BDAM",
        "UNUSED_10",
        "BPCM",
        "STATUS",
        "CAUSE",
        "EPC",
        "PRID",
    };
    static_assert(cop0_reg_to_str.size() == 16);

} inline disassembler;

} // namespace ps1::r3000a

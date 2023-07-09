#pragma once

#include "common/types.hpp"
#include "common/util.hpp"
#include "disassembler.hpp"

#include <array>
#include <concepts>
#include <format>
#include <span>
#include <string>

namespace mips {

enum class BranchState {
    DelaySlotNotTaken,
    DelaySlotTaken,
    NoBranch,
    Perform,
};

enum class Cond {
    Eq,
    Ge,
    Geu,
    Gt,
    Le,
    Lt,
    Ltu,
    Ne
};

enum class OperatingMode {
    User,
    Supervisor,
    Kernel
};

template<std::signed_integral Int> struct Gpr {
    Int get(u32 idx) const { return gpr[idx]; }
    Int* ptr(u32 idx) { return &gpr[idx]; }
    void set(u32 idx, auto data)
    {
        gpr[idx] = data;
        gpr[0] = 0;
    }

    Int operator[](u32 idx) const { return gpr[idx]; } // return by value so that writes have to be made through 'set'

    std::span<const Int, 32> view() const { return gpr; }

    std::string Format() const
    {
        std::string str;
        str.reserve(32 * 24);
        for (int i = 0; i < 32; ++i) {
            str += std::format("{}\t{:#x}\n", GprIdxToName(i), to_unsigned(gpr[i]));
        }
        return str;
    }

private:
    std::array<Int, 32> gpr{};
};

} // namespace mips

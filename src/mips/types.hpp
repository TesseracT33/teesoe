#pragma once

#include "common/types.hpp"

#include <array>
#include <concepts>
#include <span>

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
    std::span<const Int, 32> view() const { return gpr; }

    Int operator[](u32 idx) const { return gpr[idx]; } // return by value so that writes have to be made through 'set'

private:
    std::array<Int, 32> gpr{};
};

} // namespace mips

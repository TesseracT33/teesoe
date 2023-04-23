#pragma once

#include "types.hpp"

#include <array>
#include <concepts>

namespace mips {

template<std::signed_integral Int> struct Gpr {
    Int get(u32 idx) const { return gpr[idx]; }
    Int* ptr(u32 idx) { return &gpr[idx]; }
    void set(u32 idx, auto data)
    {
        gpr[idx] = data;
        gpr[0] = 0;
    }

    Int operator[](u32 idx) const { return gpr[idx]; } // return by value so that writes have to be made through 'set'

private:
    std::array<Int, 32> gpr{};
};

} // namespace mips

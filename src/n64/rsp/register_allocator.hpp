#pragma once

#include "mips/register_allocator.hpp"

namespace n64::rsp {

inline constexpr std::array reg_alloc_volatile_gprs = [] {
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        return std::array<Gpq, 8>{ r11, r10, r9, r8, rcx, rdx, rsi, rdi };
    } else {
        return std::array<Gpq, 6>{ r11, r10, r9, r8, rdx, rcx };
    }
}();

inline constexpr std::array reg_alloc_nonvolatile_gprs = [] {
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        return std::array<Gpq, 4>{ r12, r13, r14, r15 };
    } else {
        return std::array<Gpq, 6>{ r12, r13, r14, r15, rdi, rsi };
    }
}();

inline constexpr HostGpr reg_alloc_base_gpr_ptr_reg = [] {
    if constexpr (arch.a64) return asmjit::a64::x0;
    if constexpr (arch.x64) return asmjit::x86::rbp;
}();

inline constexpr HostGpr reg_alloc_base_vpr_ptr_reg = [] {
    if constexpr (arch.a64) return asmjit::a64::x0;
    if constexpr (arch.x64) return asmjit::x86::rbx;
}();

using GprRegisterAllocator =
  mips::RegisterAllocator<s32, reg_alloc_volatile_gprs.size(), reg_alloc_nonvolatile_gprs.size()>;

using RegisterAllocator = GprRegisterAllocator;

} // namespace n64::rsp

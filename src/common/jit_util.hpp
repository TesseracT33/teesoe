#pragma once

#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "build_options.hpp"
#include "host.hpp"
#include "log.hpp"
#include "types.hpp"
#include "util.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <expected>
#include <format>
#include <limits>
#include <string>
#include <type_traits>

using AsmjitCompiler = std::conditional_t<arch.x64, asmjit::x86::Compiler, asmjit::a64::Compiler>;
using HostGpr = std::conditional_t<arch.x64, asmjit::x86::Gpq, asmjit::a64::GpX>;
using HostVpr128 = std::conditional_t<arch.x64, asmjit::x86::Xmm, asmjit::a64::VecV>;

struct AsmjitLogErrorHandler : public asmjit::ErrorHandler {
    void handleError(asmjit::Error err, char const* message, asmjit::BaseEmitter* origin) override
    {
        log_error(std::format("AsmJit error: {}", message));
    }
};

inline constexpr std::array host_gpr_arg = [] {
    if constexpr (arch.a64) {
    } else {
        using namespace asmjit::x86;
        if constexpr (os.windows) {
            return std::array{ rcx, rdx, r8, r9 };
        } else {
            return std::array{ rdi, rsi, rdx, rcx, r8, r9 };
        }
    }
}();

inline asmjit::x86::Mem jit_mem_global_var(asmjit::x86::Gpq base_ptr_reg, auto const* base_ptr, auto const* obj_ptr)
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(obj_ptr) - reinterpret_cast<u8 const*>(base_ptr);
    assert(diff >= std::numeric_limits<s32>::min() && diff <= std::numeric_limits<s32>::max());
    return asmjit::x86::ptr(base_ptr_reg, diff, sizeof(decltype(*obj_ptr)));
}

// inline asmjit::x86::Mem jit_mem_global_arr_with_imm_index(asmjit::x86::Gpq base_ptr_reg,
//   s32 index,
//   auto const* base_ptr,
//   auto const* arr_ptr,
//   size_t ptr_size)
//{
//     std::ptrdiff_t diff = reinterpret_cast<u8 const*>(arr_ptr) + index - reinterpret_cast<u8 const*>(base_ptr);
//     assert(diff >= std::numeric_limits<s32>::min() && diff <= std::numeric_limits<s32>::max());
//     return asmjit::x86::ptr(base_ptr_reg, diff, ptr_size);
// }

inline asmjit::x86::Mem jit_mem_global_arr_with_reg_index(asmjit::x86::Gpq base_ptr_reg,
  asmjit::x86::Gpq index,
  auto const* base_ptr,
  auto const* arr_ptr,
  size_t ptr_size)
{
    std::ptrdiff_t diff = reinterpret_cast<u8 const*>(arr_ptr) - reinterpret_cast<u8 const*>(base_ptr);
    assert(diff >= std::numeric_limits<s32>::min() && diff <= std::numeric_limits<s32>::max());
    return asmjit::x86::ptr(base_ptr_reg, index, 0u, diff, ptr_size);
}

inline void jit_x64_call(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        c.push(rax);
        c.call(func);
        c.pop(rcx);
    } else {
        c.sub(rsp, 40);
        c.call(func);
        c.add(rsp, 40);
    }
}

inline void jit_x86_call_no_stack_alignment(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        c.call(func);
    } else {
        c.sub(rsp, 32);
        c.call(func);
        c.add(rsp, 32);
    }
}

constexpr std::string_view host_reg_to_string(asmjit::x86::Gp gp)
{
    using namespace asmjit::x86;
    switch (gp.id()) {
    case Gp::Id::kIdAx: return "rax";
    case Gp::Id::kIdCx: return "rcx";
    case Gp::Id::kIdDx: return "rdx";
    case Gp::Id::kIdBx: return "rbx";
    case Gp::Id::kIdSp: return "rsp";
    case Gp::Id::kIdBp: return "rbp";
    case Gp::Id::kIdSi: return "rsi";
    case Gp::Id::kIdDi: return "rdi";
    case Gp::Id::kIdR8: return "r8";
    case Gp::Id::kIdR9: return "r9";
    case Gp::Id::kIdR10: return "r10";
    case Gp::Id::kIdR11: return "r11";
    case Gp::Id::kIdR12: return "r12";
    case Gp::Id::kIdR13: return "r13";
    case Gp::Id::kIdR14: return "r14";
    case Gp::Id::kIdR15: return "r15";
    default: return "UNKNOWN";
    }
}

inline std::string host_reg_to_string(asmjit::x86::Xmm xmm)
{
    using namespace asmjit::x86;
    return std::format("xmm{}", xmm.id());
}

constexpr bool is_volatile(HostGpr gpr)
{
    if constexpr (arch.a64) {
    } else {
        using namespace asmjit::x86;
        if constexpr (os.windows) {
            return one_of(gpr, rax, rcx, rdx, r8, r9, r10, r11);
        } else {
            return one_of(gpr, rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11);
        }
    }
}

constexpr bool is_volatile(HostVpr128 vpr)
{
    if constexpr (arch.a64) {
    } else {
        using namespace asmjit::x86;
        if constexpr (os.windows) {
            return one_of(vpr, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5);
        } else {
            return one_of(vpr, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
        }
    }
}

constexpr bool is_nonvolatile(auto gpr)
{
    return !is_volatile(gpr);
}

constexpr asmjit::x86::Mem ptr(auto const& obj)
{
    return asmjit::x86::ptr(std::bit_cast<u64>(&obj), sizeof(obj));
}

constexpr asmjit::x86::Mem byte_ptr(auto const& obj)
    requires(sizeof(obj) == 1)
{
    return asmjit::x86::byte_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem word_ptr(auto const& obj)
    requires(sizeof(obj) == 2)
{
    return asmjit::x86::word_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem dword_ptr(auto const& obj)
    requires(sizeof(obj) == 4)
{
    return asmjit::x86::dword_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem qword_ptr(auto const& obj)
    requires(sizeof(obj) == 8)
{
    return asmjit::x86::qword_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem xmm_ptr(auto const& obj)
    requires(sizeof(obj) == 16)
{
    return asmjit::x86::xmmword_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem ymm_ptr(auto const& obj)
    requires(sizeof(obj) == 32)
{
    return asmjit::x86::ymmword_ptr(std::bit_cast<u64>(&obj));
}

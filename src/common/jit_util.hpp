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
#include <expected>
#include <format>
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

inline void call(asmjit::x86::Compiler& c, auto func)
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

inline void call_no_stack_alignment(asmjit::x86::Compiler& c, auto func)
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

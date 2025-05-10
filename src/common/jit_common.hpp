#pragma once

#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "numtypes.hpp"
#include "platform.hpp"

#include <array>
#include <string>
#include <type_traits>

using HostGpr32 = std::conditional_t<platform.x64, asmjit::x86::Gpd, asmjit::a64::GpW>;
using HostGpr64 = std::conditional_t<platform.x64, asmjit::x86::Gpq, asmjit::a64::GpX>;
using HostGpr128 = std::conditional_t<platform.x64, asmjit::x86::Xmm, asmjit::a64::VecV>;
using HostVpr128 = std::conditional_t<platform.x64, asmjit::x86::Xmm, asmjit::a64::VecV>;
using JitCompiler = std::conditional_t<platform.x64, asmjit::x86::Compiler, asmjit::a64::Compiler>;

struct AsmjitLogErrorHandler : public asmjit::ErrorHandler {
    void handleError(asmjit::Error err, char const* message, asmjit::BaseEmitter* /*origin*/) override;
};

inline constexpr s32 host_num_gprs = [] {
    if constexpr (platform.a64) return 31;
    if constexpr (platform.x64) return 16;
}();

inline constexpr s32 host_num_vprs = [] {
    if constexpr (platform.a64) return 16;
    if constexpr (platform.x64) return 16 + (platform.avx512 ? 16 : 0);
}();

inline constexpr std::array host_gpr_arg = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{ x0, x1, x2, x3, x4, x5, x6, x7 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array{ rdi, rsi, rdx, rcx, r8, r9 };
        }
        if constexpr (platform.abi.win64) {
            return std::array{ rcx, rdx, r8, r9 };
        }
    }
}();

void jit_call_no_stack_alignment(asmjit::x86::Compiler& c, void* func);
void jit_call_with_stack_alignment(asmjit::x86::Compiler& c, void* func);
[[gnu::const]] std::string HostRegToStr(HostGpr32 reg);
[[gnu::const]] std::string HostRegToStr(HostGpr64 reg);
[[gnu::const]] std::string HostRegToStr(HostGpr128 reg);
[[gnu::const]] constexpr bool IsVolatile(asmjit::a64::Gp reg);
[[gnu::const]] constexpr bool IsVolatile(asmjit::x86::Gp reg);
[[gnu::const]] constexpr bool IsVolatile(asmjit::a64::Vec reg);
[[gnu::const]] constexpr bool IsVolatile(asmjit::x86::Vec reg);

inline void jit_call_no_stack_alignment(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
    if constexpr (platform.abi.systemv) {
        c.call(func);
    }
    if constexpr (platform.abi.win64) {
        c.sub(rsp, 32);
        c.call(func);
        c.add(rsp, 32);
    }
}

inline void jit_call_with_stack_alignment(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
    if constexpr (platform.abi.systemv) {
        c.push(rax);
        c.call(func);
        c.pop(rcx);
    }
    if constexpr (platform.abi.win64) {
        c.sub(rsp, 40);
        c.call(func);
        c.add(rsp, 40);
    }
}

constexpr bool IsVolatile(asmjit::a64::Gp reg)
{
    return reg.id() < 18;
}

constexpr bool IsVolatile(asmjit::x86::Gp reg)
{
    using namespace asmjit::x86;
    static constexpr u32 table = [] {
        static constexpr std::array volatile_regs = [] {
            if constexpr (platform.abi.systemv) {
                return std::array{ rax, rcx, rdx, rdi, rsi, r8, r9, r10, r11 };
            }
            if constexpr (platform.abi.win64) {
                return std::array{ rax, rcx, rdx, r8, r9, r10, r11 };
            }
        }();
        u32 table{};
        for (Gpq reg : volatile_regs) {
            table |= 1_u32 << reg.id();
        }
        return table;
    }();
    return table >> reg.id() & 1;
}

constexpr bool IsVolatile(asmjit::a64::Vec reg)
{
    u32 id = reg.id();
    return id < 8 || id > 15;
}

constexpr bool IsVolatile(asmjit::x86::Vec reg)
{
    if constexpr (platform.abi.systemv) {
        return true;
    }
    if constexpr (platform.abi.win64) {
        u32 id = reg.id();
        return id < 6 || id > 15;
    }
}

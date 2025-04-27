#pragma once

#include "algorithm.hpp"
#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "build_options.hpp"
#include "log.hpp"
#include "numtypes.hpp"
#include "platform.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <expected>
#include <limits>
#include <string>
#include <type_traits>

using AsmjitCompiler = std::conditional_t<arch.x64, asmjit::x86::Compiler, asmjit::a64::Compiler>;
using HostGpr32 = std::conditional_t<arch.x64, asmjit::x86::Gpd, asmjit::a64::GpW>;
using HostGpr64 = std::conditional_t<arch.x64, asmjit::x86::Gpq, asmjit::a64::GpX>;
using HostGpr = HostGpr64;
using HostVpr128 = std::conditional_t<arch.x64, asmjit::x86::Xmm, asmjit::a64::VecV>;

template<size_t size> struct SizeToHostReg {};

template<> struct SizeToHostReg<4> {
    using type = HostGpr32;
};

template<> struct SizeToHostReg<8> {
    using type = HostGpr64;
};

struct AsmjitLogErrorHandler : public asmjit::ErrorHandler {
    void handleError(asmjit::Error err, char const* message, asmjit::BaseEmitter* /*origin*/) override
    {
        LogError("AsmJit error {}: {}", err, message);
    }
};

inline constexpr std::array host_gpr_arg = [] {
    if constexpr (arch.a64) {
        using namespace asmjit::a64;
        return std::array{ x0, x1, x2, x3, x4, x5, x6, x7 };
    } else {
        using namespace asmjit::x86;
        if constexpr (os.windows) {
            return std::array{ rcx, rdx, r8, r9 };
        } else {
            return std::array{ rdi, rsi, rdx, rcx, r8, r9 };
        }
    }
}();

inline void jit_x64_call_with_stack_alignment(asmjit::x86::Compiler& c, auto func)
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

constexpr std::string_view JitRegToStr(asmjit::x86::Gp gp)
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

inline std::string JitRegToStr(asmjit::x86::Xmm xmm)
{
    return std::format("xmm{}", xmm.id());
}

inline std::string JitRegToStr(asmjit::x86::Ymm ymm)
{
    return std::format("ymm{}", ymm.id());
}

inline std::string JitRegToStr(asmjit::x86::Zmm zmm)
{
    return std::format("zmm{}", zmm.id());
}

inline std::string JitRegToStr(asmjit::arm::Vec vec)
{
    return std::format("v{}", vec.id());
}

constexpr bool IsVolatile(HostGpr gpr)
{
    if constexpr (arch.a64) {
        return gpr.id() < 16;
    } else {
        using namespace asmjit::x86;
        if constexpr (os.windows) {
            return OneOf(gpr, rax, rcx, rdx, r8, r9, r10, r11);
        } else {
            return OneOf(gpr, rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11);
        }
    }
}

constexpr bool IsVolatile(HostVpr128 vpr)
{
    u32 id = vpr.id();
    if constexpr (arch.a64) {
        return id < 16;
    } else {
        if constexpr (os.windows) {
            return id < 6 || id > 15;
        } else {
            return true;
        }
    }
}

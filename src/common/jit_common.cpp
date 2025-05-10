#include "jit_common.hpp"
#include "log.hpp"

#include <format>

void AsmjitLogErrorHandler::handleError(asmjit::Error err, char const* message, asmjit::BaseEmitter* /*origin*/)
{
    LogError("AsmJit error {}: {}", err, message);
}

std::string HostRegToStr(asmjit::a64::Vec reg)
{
    return std::format("v{}", reg.id());
}

std::string HostRegToStr(HostGpr32 reg)
{
    if constexpr (platform.a64) {
        return std::format("w{}", reg.id());
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        switch (reg.id()) {
        case Gp::Id::kIdAx: return "eax";
        case Gp::Id::kIdCx: return "ecx";
        case Gp::Id::kIdDx: return "edx";
        case Gp::Id::kIdBx: return "ebx";
        case Gp::Id::kIdSp: return "esp";
        case Gp::Id::kIdBp: return "ebp";
        case Gp::Id::kIdSi: return "esi";
        case Gp::Id::kIdDi: return "edi";
        case Gp::Id::kIdR8: return "r8d";
        case Gp::Id::kIdR9: return "r9d";
        case Gp::Id::kIdR10: return "r10d";
        case Gp::Id::kIdR11: return "r11d";
        case Gp::Id::kIdR12: return "r12d";
        case Gp::Id::kIdR13: return "r13d";
        case Gp::Id::kIdR14: return "r14d";
        case Gp::Id::kIdR15: return "r15d";
        default: return "UNKNOWN";
        }
    }
}

std::string HostRegToStr(HostGpr64 reg)
{
    if constexpr (platform.a64) {
        return std::format("x{}", reg.id());
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        switch (reg.id()) {
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
}

std::string HostRegToStr(asmjit::x86::Xmm reg)
{
    return std::format("xmm{}", reg.id());
}

void jit_call_no_stack_alignment(asmjit::x86::Compiler& c, void* func)
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

void jit_call_with_stack_alignment(asmjit::x86::Compiler& c, void* func)
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

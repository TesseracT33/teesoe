#include "jit_common.hpp"
#include "log.hpp"

#include <format>

void AsmjitLogErrorHandler::handleError(asmjit::Error err, char const* message, asmjit::BaseEmitter* /*origin*/)
{
    LogError(std::format("AsmJit error {}: {}", err, message));
}

std::string HostRegToStr(asmjit::a64::Vec reg)
{
    return std::format("v{}", reg.id());
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

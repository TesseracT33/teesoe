#pragma once

#include "gpr.hpp"
#include "types.hpp"

#include <concepts>

namespace mips {

template<std::signed_integral GprInt, std::signed_integral LoHiInt, std::integral PcInt> struct Cpu {
    using ExceptionHandler = void (*)();
    template<std::integral Int> using JumpHandler = void (*)(PcInt target);

    consteval Cpu(Gpr<GprInt>& gpr,
      LoHiInt& lo,
      LoHiInt& hi,
      PcInt& pc,
      bool const& dword_op_cond,
      JumpHandler<PcInt> jump_handler,
      ExceptionHandler integer_overflow_exception = nullptr,
      ExceptionHandler reserved_instruction_exception = nullptr,
      ExceptionHandler trap_exception = nullptr)
      : gpr(gpr),
        lo(lo),
        hi(hi),
        pc(pc),
        dword_op_cond(dword_op_cond),
        jump(jump_handler),
        integer_overflow_exception(integer_overflow_exception),
        reserved_instruction_exception(reserved_instruction_exception),
        trap_exception(trap_exception)
    {
    }

    static constexpr bool mips32 = sizeof(GprInt) == 4;
    static constexpr bool mips64 = sizeof(GprInt) == 8;

    Gpr<GprInt>& gpr;
    LoHiInt& lo;
    LoHiInt& hi;
    PcInt& pc;
    bool const& dword_op_cond;
    JumpHandler<PcInt> const jump;
    ExceptionHandler const integer_overflow_exception, reserved_instruction_exception, trap_exception;
};

} // namespace mips

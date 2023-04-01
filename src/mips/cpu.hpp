#pragma once

#include "gpr.hpp"
#include "types.hpp"

#include <concepts>

namespace mips {

template<typename GprInt, typename LoHiInt, std::integral PcInt, typename GprBaseInt = GprInt> struct Cpu {
    using ExceptionHandler = void (*)();
    using LinkHandler = void (*)(u32 reg);
    template<std::integral Int> using JumpHandler = void (*)(PcInt target);

    consteval Cpu(Gpr<GprInt>& gpr,
      LoHiInt& lo,
      LoHiInt& hi,
      PcInt& pc,
      bool& in_branch_delay_slot,
      JumpHandler<PcInt> jump_handler,
      LinkHandler link_handler,
      ExceptionHandler integer_overflow_exception = nullptr,
      ExceptionHandler trap_exception = nullptr)
      : gpr(gpr),
        lo(lo),
        hi(hi),
        pc(pc),
        in_branch_delay_slot(in_branch_delay_slot),
        jump(jump_handler),
        link(link_handler),
        integer_overflow_exception(integer_overflow_exception),
        trap_exception(trap_exception)
    {
    }

    static constexpr bool mips32 = sizeof(GprBaseInt) == 4;
    static constexpr bool mips64 = sizeof(GprBaseInt) == 8;

    Gpr<GprInt>& gpr;
    LoHiInt& lo;
    LoHiInt& hi;
    PcInt& pc;
    bool& in_branch_delay_slot;
    JumpHandler<PcInt> const jump;
    LinkHandler link;
    ExceptionHandler const integer_overflow_exception, trap_exception;
};

} // namespace mips

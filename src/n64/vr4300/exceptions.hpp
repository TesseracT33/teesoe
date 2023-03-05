#pragma once

#include "mmu.hpp"
#include "types.hpp"

namespace n64::vr4300 {

enum class Exception {
    AddressError,
    Breakpoint,
    BusError,
    ColdReset,
    CoprocessorUnusable,
    FloatingPoint,
    IntegerOverflow,
    Interrupt,
    Nmi,
    ReservedInstruction,
    ReservedInstructionCop2,
    SoftReset,
    Syscall,
    TlbInvalid,
    TlbMiss,
    TlbModification,
    Trap,
    Watch,
    XtlbMiss
};

void HandleException();
template<Exception exception, MemOp mem_op = MemOp::Read> void SignalException();
template<MemOp mem_op> void SignalAddressErrorException(u64 bad_virt_addr);
void SignalCoprocessorUnusableException(int co);

inline bool exception_has_occurred;
inline u64 exception_bad_virt_addr;

} // namespace n64::vr4300

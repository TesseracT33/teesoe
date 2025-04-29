#pragma once

#include "mmu.hpp"
#include "numtypes.hpp"

namespace n64::vr4300 {

void AddressErrorException(u64 bad_vaddr, MemOp mem_op);
void BreakpointException();
void BusErrorException(MemOp mem_op);
void ColdResetException();
void CoprocessorUnusableException(int cop);
void FloatingPointException();
void IntegerOverflowException();
void InterruptException();
void NmiException();
void ReservedInstructionException();
void ReservedInstructionCop2Exception();
void SoftResetException();
void SyscallException();
void TlbInvalidException(u64 bad_vaddr, MemOp mem_op);
void TlbMissException(u64 bad_vaddr, MemOp mem_op);
void TlbModificationException(u64 bad_vaddr);
void TrapException();
void WatchException();
void XtlbMissException(u64 bad_vaddr, MemOp mem_op);

inline bool exception_occurred;

} // namespace n64::vr4300

#pragma once

#include "mmu.hpp"
#include "types.hpp"

namespace n64::vr4300 {

template<MemOp> void AddressErrorException(u64 bad_vaddr);
void BreakpointException();
template<MemOp> void BusErrorException();
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
template<MemOp> void TlbInvalidException(u64 bad_vaddr);
template<MemOp> void TlbMissException(u64 bad_vaddr);
void TlbModificationException(u64 bad_vaddr);
void TrapException();
void WatchException();
template<MemOp> void XtlbMissException(u64 bad_vaddr);

inline bool exception_occurred;

} // namespace n64::vr4300

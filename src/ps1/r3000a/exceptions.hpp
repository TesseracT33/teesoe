#pragma once

#include "numtypes.hpp"

namespace ps1::r3000a {

enum class MemOp;

inline bool exception_occurred;

void address_error_exception(u32 vaddr, MemOp mem_op);
void breakpoint_exception();
void bus_error_exception(MemOp mem_op);
void coprocessor_unusable_exception(u32 cop);
void integer_overflow_exception();
void interrupt_exception();
void nmi_exception();
void reserved_instruction_exception();
void reset_exception();
void syscall_exception();

} // namespace ps1::r3000a

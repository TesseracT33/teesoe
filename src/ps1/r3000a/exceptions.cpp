#include "exceptions.hpp"
#include "cop0.hpp"
#include "memory.hpp"
#include "r3000a.hpp"

namespace ps1::r3000a {

static u32 get_common_vector();
static void handle_exception();

void address_error_exception(u32 vaddr, MemOp mem_op)
{
    cop0.cause.exccode = mem_op == MemOp::DataStore ? 5 : 4;
    cop0.bad_v_addr = vaddr;
    pc = get_common_vector();
}

void breakpoint_exception()
{
    cop0.cause.exccode = 9;
    pc = get_common_vector();
}

void bus_error_exception(MemOp mem_op)
{
    cop0.cause.exccode = mem_op == MemOp::InstrFetch ? 6 : 7;
    pc = get_common_vector();
}

void coprocessor_unusable_exception(u32 cop)
{
    cop0.cause.exccode = 11;
    cop0.cause.ce = cop;
    pc = get_common_vector();
}

u32 get_common_vector()
{
    return cop0.status.bev ? 0xbfc0'0180 : 0x8000'0000;
}

void integer_overflow_exception()
{
    cop0.cause.exccode = 12;
    pc = get_common_vector();
}

void interrupt_exception()
{
    cop0.cause.exccode = 0;
    pc = get_common_vector();
}

void handle_exception()
{
    exception_occurred = true;
    // if (!cop0.status.exl) {
    //     cop0.cause.bd = in_branch_delay_slot;
    //     cop0.epc = pc;
    //     if (in_branch_delay_slot) cop0.epc -= 4;
    //     cop0.status.exl = 1;
    //     // SignalInterruptFalse();
    // }
    reset_branch();
}

void nmi_exception()
{
}

void reserved_instruction_exception()
{
    cop0.cause.exccode = 10;
    pc = get_common_vector();
}

void reset_exception()
{
    pc = 0xbfc0'0000;
}

void syscall_exception()
{
    cop0.cause.exccode = 8;
    pc = get_common_vector();
}

} // namespace ps1::r3000a

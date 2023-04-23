#pragma once

#include "types.hpp"

#include <concepts>

namespace n64::vr4300 {

using VirtualToPhysicalAddressFun = u32 (*)(u64 /* in: v_addr */, bool& /* out: cached area? */);

enum class AddressingMode {
    _32bit,
    _64bit
} inline addressing_mode;

enum class Alignment {
    Aligned,
    UnalignedLeft, /* Load/Store (Double)Word Left instructions */
    UnalignedRight /* Load/Store (Double)Word Right instructions */
};

enum class MemOp {
    Read,
    InstrFetch,
    Write
};

u32 FetchInstruction(u64 virtual_address);
u32 GetPhysicalPC();
void InitializeMMU();
template<std::signed_integral Int, Alignment alignment = Alignment::Aligned, MemOp mem_op = MemOp::Read>
Int ReadVirtual(u64 virtual_address);
void SetActiveVirtualToPhysicalFunctions();
template<size_t access_size, Alignment alignment = Alignment::Aligned> void WriteVirtual(u64 virtual_address, s64 data);

void tlbr();
void tlbwi();
void tlbwr();
void tlbp();

inline VirtualToPhysicalAddressFun active_virtual_to_physical_fun_read;
inline VirtualToPhysicalAddressFun active_virtual_to_physical_fun_write;

inline bool can_execute_dword_instrs;

inline u32 last_physical_address_on_load;
/* Used for logging. Set when memory is read during an instruction fetch. */
inline u32 last_instr_fetch_phys_addr;
} // namespace n64::vr4300

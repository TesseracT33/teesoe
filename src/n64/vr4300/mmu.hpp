#pragma once

#include "numtypes.hpp"

#include <concepts>

namespace n64::vr4300 {

using VaddrToPaddrFunc = u32 (*)(u64 /* in: v_addr */, bool& /* out: cached area? */);

enum class AddressingMode {
    _32bit,
    _64bit
} inline addressing_mode;

enum class Alignment {
    Aligned,
    UnalignedLeft, // LWL/LDL/SWL/SDL
    UnalignedRight // LWR/LDR/SWR/SDR
};

enum class MemOp {
    Read,
    InstrFetch,
    Write
};

u32 FetchInstruction(u64 vaddr);
u32 GetPhysicalPC();
void InitializeMMU();
template<std::signed_integral Int, Alignment alignment = Alignment::Aligned, MemOp mem_op = MemOp::Read>
Int ReadVirtual(u64 vaddr);
void SetVaddrToPaddrFuncs();
template<size_t access_size, Alignment alignment = Alignment::Aligned> void WriteVirtual(u64 vaddr, s64 data);

void tlbr();
void tlbwi();
void tlbwr();
void tlbp();

inline VaddrToPaddrFunc vaddr_to_paddr_read_func;
inline VaddrToPaddrFunc vaddr_to_paddr_write_func;

inline bool can_execute_dword_instrs, can_exec_cop0_instrs;

inline u32 last_paddr_on_instr_fetch;
inline u32 last_paddr_on_load;

} // namespace n64::vr4300

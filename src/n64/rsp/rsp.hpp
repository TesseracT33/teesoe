#pragma once

#include "jit/jit.hpp"
#include "mips/gpr.hpp"
#include "types.hpp"

#include <array>
#include <concepts>
#include <string>
#include <string_view>

namespace n64::rsp {

void AdvancePipeline(u64 cycles);
void FetchDecodeExecuteInstruction();
u8* GetPointerToMemory(u32 addr);
void Jump(u32 target_address);
void Link(u32 reg);
void NotifyIllegalInstr(std::string_view instr);
void NotifyIllegalInstrCode(u32 instr_code);
template<std::signed_integral Int> Int ReadDMEM(u32 addr);
void PowerOn();
u64 RdpReadCommand(u32 addr);
template<std::signed_integral Int> Int ReadMemoryCpu(u32 addr);
u64 Run(u64 rsp_cycles_to_run);
template<std::signed_integral Int> void WriteDMEM(u32 addr, Int data);
template<size_t access_size> void WriteMemoryCpu(u32 addr, s64 data);

inline bool in_branch_delay_slot;
inline bool jump_is_pending;
inline bool ll_bit;
inline uint pc;
inline uint p_cycle_counter;
inline uint instructions_until_jump;
inline uint jump_addr;

inline ::mips::Gpr<s32> gpr;

inline constinit std::array<u8, 0x2000> mem{}; /* 0 - $FFF: data memory; $1000 - $1FFF: instruction memory */

inline constinit u8* const dmem = mem.data();
inline constinit u8* const imem = mem.data() + 0x1000;

// recompiler
inline Jit jit;

/* Debugging */
inline u32 current_instr_pc;
inline std::string_view current_instr_name;
inline std::string current_instr_log_output;

} // namespace n64::rsp

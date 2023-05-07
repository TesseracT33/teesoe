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
u8* GetPointerToMemory(u32 addr);
void Link(u32 reg);
void NotifyIllegalInstr(std::string_view instr);
void NotifyIllegalInstrCode(u32 instr_code);
template<std::signed_integral Int> Int ReadDMEM(u32 addr);
void PowerOn();
u64 RdpReadCommand(u32 addr);
u64 RunInterpreter(u64 rsp_cycles);
u64 RunRecompiler(u64 rsp_cycles);
void TakeBranch(u32 target_address);
template<std::signed_integral Int> void WriteDMEM(u32 addr, Int data);

inline bool in_branch_delay_slot;
inline bool jump_is_pending;
inline u32 pc;
inline s32 lo_dummy, hi_dummy;
constexpr bool can_execute_dword_instrs_dummy{};

inline ::mips::Gpr<s32> gpr;

inline constinit std::array<u8, 0x2000> mem{}; /* 0 - $FFF: data memory; $1000 - $1FFF: instruction memory */

inline constinit u8* const dmem = mem.data();
inline constinit u8* const imem = mem.data() + 0x1000;

// recompiler
inline Jit jit;

} // namespace n64::rsp

#pragma once

#include "mips/types.hpp"
#include "n64.hpp"
#include "numtypes.hpp"

#include <array>
#include <concepts>
#include <string_view>

namespace n64::rsp {

struct Sp {
    u32 dma_spaddr, dma_ramaddr, dma_rdlen, dma_wrlen;
    struct {
        u32 halted   : 1;
        u32 broke    : 1;
        u32 dma_busy : 1;
        u32 dma_full : 1;
        u32 io_busy  : 1;
        u32 sstep    : 1;
        u32 intbreak : 1;
        u32 sig      : 8;
        u32          : 17;
    } status;
    u32 dma_full, dma_busy, semaphore;
} inline sp;

void AdvancePipeline(u32 cycles);
u32 FetchInstruction(u32 addr);
u8* GetPointerToMemory(u32 addr);
void Link(u32 reg);
void NotifyIllegalInstr(std::string_view instr);
void NotifyIllegalInstrCode(u32 instr_code);
void PerformBranch();
void PowerOn();
u64 RdpReadCommand(u32 addr);
template<std::signed_integral Int> Int ReadDMEM(u32 addr);
template<std::signed_integral Int> Int ReadMemoryCpu(u32 addr);
u32 ReadReg(u32 addr);
void SetActiveCpuImpl(CpuImpl cpu_impl);
void TakeBranch(u32 target_address);
template<std::signed_integral Int> void WriteDMEM(u32 addr, Int data);
template<size_t access_size> void WriteMemoryCpu(u32 addr, s64 data);
void WriteReg(u32 addr, u32 data);

inline constexpr bool can_execute_dword_instrs_dummy{};

inline mips::Gpr<s32> gpr;
inline u32 pc;
inline u32 jump_addr;
inline u32 cycle_counter;
inline s32 lo_dummy, hi_dummy;
inline bool in_branch_delay_slot;
inline bool jump_is_pending;

inline CpuImpl cpu_impl;

/* 0 - $FFF: data memory; $1000 - $1FFF: instruction memory */
alignas(16) inline constinit std::array<u8, 0x2000> mem{};

inline constinit u8* const dmem = mem.data();
inline constinit u8* const imem = mem.data() + 0x1000;

} // namespace n64::rsp

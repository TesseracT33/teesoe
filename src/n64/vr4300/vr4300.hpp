#pragma once

#include "mips/types.hpp"
#include "n64.hpp"
#include "numtypes.hpp"

namespace n64::vr4300 {

enum class BranchState {
    DelaySlotNotTaken,
    DelaySlotTaken,
    NoBranch,
    Perform,
};

enum class ExternalInterruptSource {
    MI = 1 << 2, /* ip2; MIPS Interface interrupt. Set to 1 when (MI_INTR_REG & MI_INTR_MASK_REG) != 0  */
    Cartridge = 1 << 3, /* ip3; This is connected to the cartridge slot. Cartridges with special hardware can
                           trigger this interrupt. */
    Reset = 1 << 4, /* ip4; Becomes 1 when the console's reset button is pressed. */
    IndyRead = 1 << 5, /* ip5; Connected to the Indy dev kit’s RDB port. Set to 1 when a value is read. */
    IndyWrite = 1 << 6 /* ip6; Connected to the Indy dev kit’s RDB port. Set to 1 when a value is written. */
};

enum class OperatingMode {
    User,
    Supervisor,
    Kernel
};

void AddInitialEvents();
void AdvancePipeline(u32 cycles);
void CheckInterrupts();
void ClearInterruptPending(ExternalInterruptSource);
u64 GetElapsedCycles();
void InitRun(bool hle_pif);
void NotifyIllegalInstrCode(u32 instr_code);
void PerformBranch();
void PowerOn();
void Reset();
void SetActiveCpuImpl(CpuImpl cpu_impl);
void SetInterruptPending(ExternalInterruptSource);
void SignalInterruptFalse();

inline mips::Gpr<s64> gpr;
inline u64 pc;
inline u64 jump_addr;
inline s64 lo;
inline s64 hi;
inline u32 cycle_counter;
inline BranchState branch_state{ BranchState::NoBranch };
inline OperatingMode operating_mode;
inline bool ll_bit;
inline bool last_instr_was_load;
inline bool last_instr_was_branch;

inline CpuImpl cpu_impl;

} // namespace n64::vr4300

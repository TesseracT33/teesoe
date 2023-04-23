#pragma once

#include "jit/jit.hpp"
#include "mips/gpr.hpp"
#include "n64.hpp"
#include "types.hpp"

#include <string>
#include <string_view>

namespace n64::vr4300 {

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
} inline operating_mode;

void AddInitialEvents();
void AdvancePipeline(u64 cycles);
void CheckInterrupts();
void ClearInterruptPending(ExternalInterruptSource);
u64 GetElapsedCycles();
void InitRun(bool hle_pif);
void Jump(u64 target_address);
void Link(u32 reg);
void LinkRecompiler(u32 reg);
void NotifyIllegalInstrCode(u32 instr_code);
void PowerOn();
void Reset();
u64 RunInterpreter(u64 cpu_cycles);
u64 RunRecompiler(u64 cpu_cycles);
void SetInterruptPending(ExternalInterruptSource);
void SignalInterruptFalse();

inline bool in_branch_delay_slot;
inline bool ll_bit; /* Read from / written to by load linked and store conditional instructions. */
inline bool jump_is_pending = false;
inline bool last_instr_was_load = false;
inline uint instructions_until_jump = 0;
inline u64 jump_addr;
inline u64 pc;
inline s64 lo, hi; /* Contain the result of a double-word multiplication or division. */
inline u64 p_cycle_counter;
inline ::mips::Gpr<s64> gpr;

// recompiler
inline Jit jit;

/* Debugging */
inline std::string_view current_instr_name;
inline std::string current_instr_log_output;

} // namespace n64::vr4300

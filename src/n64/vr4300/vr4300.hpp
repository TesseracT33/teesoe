#pragma once

#include "types.hpp"

#include <string>
#include <string_view>

namespace n64::vr4300 {

enum class CpuInstruction;
enum class Cop0Instruction;
enum class Cop1Instruction;
enum class Cop2Instruction;

enum class OperatingMode {
    User,
    Supervisor,
    Kernel
} inline operating_mode;

enum class ExternalInterruptSource {
    MI = 1 << 2, /* ip2; MIPS Interface interrupt. Set to 1 when (MI_INTR_REG & MI_INTR_MASK_REG) != 0  */
    Cartridge = 1 << 3, /* ip3; This is connected to the cartridge slot. Cartridges with special hardware can
                           trigger this interrupt. */
    Reset = 1 << 4, /* ip4; Becomes 1 when the console's reset button is pressed. */
    IndyRead = 1 << 5, /* ip5; Connected to the Indy dev kit’s RDB port. Set to 1 when a value is read. */
    IndyWrite = 1 << 6 /* ip6; Connected to the Indy dev kit’s RDB port. Set to 1 when a value is written. */
};

void AddInitialEvents();
void CheckInterrupts();
void ClearInterruptPending(ExternalInterruptSource);
u64 GetElapsedCycles();
void InitRun(bool hle_pif);
void Reset();
u64 Run(u64 cpu_cycles_to_run);
void PowerOn();
void SetInterruptPending(ExternalInterruptSource);

void AdvancePipeline(u64 cycles);
void DecodeExecuteCop0Instruction();
void DecodeExecuteCop1Instruction();
void DecodeExecuteCop2Instruction();
void DecodeExecuteCop3Instruction();
void DecodeExecuteInstruction(u32 instr_code);
void DecodeExecuteRegimmInstruction();
void DecodeExecuteSpecialInstruction();
template<CpuInstruction> void ExecuteCpuInstruction();
template<Cop0Instruction> void ExecuteCop0Instruction();
template<Cop1Instruction> void ExecuteCop1Instruction();
template<Cop2Instruction> void ExecuteCop2Instruction();
void FetchDecodeExecuteInstruction();
void InitializeRegisters();
void NotifyIllegalInstrCode(u32 instr_code);
void PrepareJump(u64 target_address);

inline bool in_branch_delay_slot;
inline bool ll_bit; /* Read from / written to by load linked and store conditional instructions. */
inline bool jump_is_pending = false;
inline bool last_instr_was_load = false;
inline uint instructions_until_jump = 0;
inline u64 addr_to_jump_to;
inline u64 pc;
inline u64 hi_reg, lo_reg; /* Contain the result of a double-word multiplication or division. */
inline u64 p_cycle_counter;
inline u8* rdram_ptr;

/* Debugging */
inline std::string_view current_instr_name;
inline std::string current_instr_log_output;

} // namespace n64::vr4300

#pragma once

#include "serializer.hpp"
#include "numtypes.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace gba::arm7tdmi {

using ExceptionHandler = void (*)();

enum class Exception {
    DataAbort,
    Fiq,
    Irq,
    PrefetchAbort,
    Reset,
    SoftwareInterrupt,
    UndefinedInstruction
};

enum class ExecutionState {
    ARM = 0,
    THUMB = 1
} inline execution_state;

enum class Mode {
    User,
    Fiq,
    Irq,
    Supervisor,
    Abort,
    Undefined,
    System
};

enum class OffsetType {
    Register,
    Immediate
};

void AddCycles(u64 cycles);
u64 GetElapsedCycles();
void Initialize();
u64 Run(u64 cycles);
void SetIRQ(bool new_irq);
void StreamState(Serializer& stream);
void SuspendRun();

bool CheckCondition(u32 cond);
void DecodeExecute(u32 opcode);
void DecodeExecuteARM(u32 opcode);
void DecodeExecuteTHUMB(u16 opcode);
constexpr std::string_view ExceptionToStr(Exception exc);
u32 Fetch();
void FlushPipeline();
void SetExecutionState(ExecutionState state);
template<Mode> void SetMode();
template<Exception> void SignalException();
void StallPipeline(uint cycles);
void StepPipeline();
void SoftwareInterrupt();

struct CPSR {
    u32 mode        : 5; /* 16=User, 17=FIQ, 18=IRQ, 19=Supervisor, 23=Abort, 27=Undefined, 31=System */
    u32 state       : 1; /* 0=ARM, 1=THUMB */
    u32 fiq_disable : 1;
    u32 irq_disable : 1;
    u32             : 20;
    u32 overflow    : 1;
    u32 carry       : 1;
    u32 zero        : 1;
    u32 negative    : 1;
} inline cpsr;

constexpr u32 cpsr_mode_bits_user = 16;
constexpr u32 cpsr_mode_bits_fiq = 17;
constexpr u32 cpsr_mode_bits_irq = 18;
constexpr u32 cpsr_mode_bits_supervisor = 19;
constexpr u32 cpsr_mode_bits_abort = 23;
constexpr u32 cpsr_mode_bits_undefined = 27;
constexpr u32 cpsr_mode_bits_system = 31;
constexpr u32 exception_vector_reset = 0;
constexpr u32 exception_vector_undefined_instr = 4;
constexpr u32 exception_vector_software_int = 8;
constexpr u32 exception_vector_prefetch_abort = 0xC;
constexpr u32 exception_vector_data_abort = 0x10;
constexpr u32 exception_vector_irq = 0x18;
constexpr u32 exception_vector_fiq = 0x1C;

inline std::array<u32, 5> r8_r12_non_fiq; /* R8-R12 */
inline std::array<u32, 5> r8_r12_fiq; /* R8-R12 */
inline u32 r13_usr, r14_usr; /* also system */
inline u32 r13_fiq, r14_fiq;
inline u32 r13_svc, r14_svc;
inline u32 r13_abt, r14_abt;
inline u32 r13_irq, r14_irq;
inline u32 r13_und, r14_und;
inline u32 spsr_fiq, spsr_svc, spsr_abt, spsr_irq, spsr_und;
inline u32 spsr;
inline std::array<u32, 16> r; /* currently active registers */

inline bool exception_has_occurred;
inline ExceptionHandler exception_handler;

inline u64 cycle;

/// debugging
inline u32 pc_when_current_instr_fetched;
} // namespace gba::arm7tdmi

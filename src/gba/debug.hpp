#pragma once

#include "bus.hpp"
#include "numtypes.hpp"

#include <concepts>
#include <span>
#include <string>
#include <string_view>

namespace gba {

inline constexpr bool log_instrs = false;
inline constexpr bool log_io_reads = false;
inline constexpr bool log_io_writes = false;

void LogInstruction(u32 pc, u32 opcode, std::string_view cond_str, bool cond, std::span<u32 const, 16> reg, u32 cpsr);
void LogInstruction(u32 pc, u16 opcode, std::span<u32 const, 16> reg, u32 cpsr);
void LogInstruction(u32 pc, std::string instr_output, std::span<u32 const, 16> reg, u32 cpsr);
template<bus::IoOperation> void LogIoAccess(u32 addr, std::integral auto data);
void SetLogPath(std::string const& log_path);

inline constexpr bool LoggingIsEnabled()
{
    return log_instrs || log_io_reads || log_io_writes;
}

} // namespace gba

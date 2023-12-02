#pragma once

namespace ps1 {

inline constexpr bool enable_logging = 0;
inline constexpr bool enable_cpu_jit_error_handler = 1;
inline constexpr bool log_cpu_branches = enable_logging && 0;
inline constexpr bool log_cpu_instructions = enable_logging && 0;
inline constexpr bool log_cpu_jit_blocks = enable_logging && 0;
inline constexpr bool log_cpu_jit_register_status = enable_logging && 0;
inline constexpr bool log_cpu_reads = enable_logging && 0;
inline constexpr bool log_cpu_writes = enable_logging && 0;
inline constexpr bool log_dma = enable_logging && 0;
inline constexpr bool log_exceptions = enable_logging && 0;
inline constexpr bool log_interrupts = enable_logging && 0;
inline constexpr bool log_io_all = enable_logging && 0;

} // namespace ps1

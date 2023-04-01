#pragma once

namespace n64 {

inline constexpr bool enable_logging = 0;

inline constexpr bool log_cpu_instructions = enable_logging && 0;
inline constexpr bool log_exceptions = enable_logging && 0;
inline constexpr bool log_interrupts = enable_logging && 0;
inline constexpr bool log_dma = enable_logging && 0;
inline constexpr bool log_io_all = enable_logging && 0;
inline constexpr bool log_io_ai = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_mi = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_pi = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_rdram = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_ri = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_si = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_vi = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_rdp = enable_logging && (log_io_all || 0);
inline constexpr bool log_io_rsp = enable_logging && (log_io_all || 0);
inline constexpr bool log_rsp_instructions = enable_logging && 0;

inline constexpr bool skip_boot_rom = 1;

} // namespace n64

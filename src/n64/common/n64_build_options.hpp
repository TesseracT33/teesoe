#pragma once

#include <string_view>

namespace n64 {

inline constexpr bool enable_logging = false;

inline constexpr bool log_cpu_instructions = enable_logging && false;
inline constexpr bool log_cpu_exceptions = enable_logging && false;
inline constexpr bool log_dma = enable_logging && false;
inline constexpr bool log_io_all = enable_logging && false;
inline constexpr bool log_io_ai = enable_logging && (log_io_all || false);
inline constexpr bool log_io_mi = enable_logging && (log_io_all || false);
inline constexpr bool log_io_pi = enable_logging && (log_io_all || false);
inline constexpr bool log_io_rdram = enable_logging && (log_io_all || false);
inline constexpr bool log_io_ri = enable_logging && (log_io_all || false);
inline constexpr bool log_io_si = enable_logging && (log_io_all || false);
inline constexpr bool log_io_vi = enable_logging && (log_io_all || false);
inline constexpr bool log_io_rdp = enable_logging && (log_io_all || false);
inline constexpr bool log_io_rsp = enable_logging && (log_io_all || false);
inline constexpr bool log_rsp_instructions = enable_logging && false;

inline constexpr std::string_view log_path = "C:\\n64.log";

inline constexpr bool skip_boot_rom = true;

inline constexpr bool interpret_cpu = true;
inline constexpr bool recompile_cpu = !interpret_cpu;

} // namespace n64

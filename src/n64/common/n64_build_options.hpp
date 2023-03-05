#pragma once

#include <string_view>

inline constexpr bool enable_file_logging = false;

inline constexpr bool log_cpu_instructions = enable_file_logging && false;
inline constexpr bool log_cpu_exceptions = enable_file_logging && true;
inline constexpr bool log_dma = enable_file_logging && true;
inline constexpr bool log_io_all = enable_file_logging && true;
inline constexpr bool log_io_ai = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_mi = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_pi = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_rdram = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_ri = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_si = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_vi = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_rdp = enable_file_logging && (log_io_all || true);
inline constexpr bool log_io_rsp = enable_file_logging && (log_io_all || true);
inline constexpr bool log_rsp_instructions = enable_file_logging && false;

inline constexpr std::string_view log_path = "F:\\n64.log";

inline constexpr bool skip_boot_rom = true;

inline constexpr bool interpret_cpu = false;
inline constexpr bool recompile_cpu = !interpret_cpu;
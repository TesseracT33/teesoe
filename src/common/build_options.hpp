#pragma once

#include <string_view>

inline constexpr bool enable_console_logging = 1;
inline constexpr bool enable_file_logging = 0;
inline constexpr bool enable_jit_block_logging = 0;
inline constexpr bool enable_jit_error_logging = 0;

inline constexpr std::string_view log_path = "F:\\teesoe.log";

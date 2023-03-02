#pragma once

#include <bit>

#if defined __x86_64__ || defined _M_X64
struct Arch {
    static constexpr bool x64 = 1;
    static constexpr bool arm64 = 0;
} constexpr arch;
#define X64 1
#elif defined __aarch64__ || defined _M_ARM64
struct Arch {
    static constexpr bool x64 = 0;
    static constexpr bool arm64 = 1;
} constexpr arch;
#define ARM64 1
#else
#error Unsupported architecture; only x86-64 and arm64 are supported.
#endif

#ifdef __has_builtin
#if __has_builtin(__builtin_add_overflow)
#define HAS_BUILTIN_ADD_OVERFLOW 1
#endif
#if __has_builtin(__builtin_sub_overflow)
#define HAS_BUILTIN_SUB_OVERFLOW 1
#endif
#endif

static_assert(std::endian::native == std::endian::little, "Only little-endian hosts are supported.");

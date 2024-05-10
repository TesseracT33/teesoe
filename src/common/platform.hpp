#pragma once

#include <bit>

#if defined __x86_64__ || defined _M_X64
struct Arch {
    static constexpr bool x64 = 1;
    static constexpr bool a64 = 0;
} constexpr arch;
#define X64 1
#elif defined __aarch64__ || defined _M_ARM64
struct Arch {
    static constexpr bool x64 = 0;
    static constexpr bool a64 = 1;
} constexpr arch;
#define A64 1
#else
#error Unsupported architecture; only x86-64 and arm64 are supported.
#endif

#ifdef _WIN32
struct Os {
    static constexpr bool windows = 1;
    static constexpr bool linux = 0;
} constexpr os;
#else
struct Os {
    static constexpr bool windows = 0;
    static constexpr bool linux = 1;
} constexpr os;
#endif

struct Abi {
    static constexpr bool arm64 = arch.a64;
    static constexpr bool system_v = arch.x64 && os.linux;
    static constexpr bool win_x64 = arch.x64 && os.windows;
} constexpr abi;

// TODO: do this properly
#ifdef __AVX512F__
constexpr bool avx512 = 1;
#else
constexpr bool avx512 = 0;
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

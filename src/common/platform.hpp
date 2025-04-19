#pragma once

#include <bit>

struct Platform {
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
#define PLATFORM_X64 1
    static constexpr bool a64 = 0;
    static constexpr bool x64 = 1;
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define PLATFORM_A64 1
    static constexpr bool a64 = 1;
    static constexpr bool x64 = 0;
#else
#error Unsupported host architecture; only x86-64 and arm64 are supported.
#endif

    struct Abi {
#if PLATFORM_A64
        static constexpr bool a64 = 1;
        static constexpr bool systemv = 0;
        static constexpr bool win64 = 0;
#elifdef __linux__
        static constexpr bool a64 = 0;
        static constexpr bool systemv = 1;
        static constexpr bool win64 = 0;
#elifdef _WIN64
        static constexpr bool a64 = 0;
        static constexpr bool systemv = 0;
        static constexpr bool win64 = 1;
#else
#error Unsupported host platform.
#endif
    } static constexpr abi{};

    static constexpr bool avx512 = 0; // TODO

} constexpr platform;

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

static_assert(std::endian::native == std::endian::little, "Only little-endian host platforms are supported.");

#pragma once

#include <bit>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
#    define PLATFORM_X64 1
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#    define PLATFORM_A64 1
#else
#    error Unsupported host architecture; only x86-64 and arm64 are supported.
#endif

#if defined(__linux__)
#    define PLATFORM_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
#    define PLATFORM_WINDOWS 1
#else
#    error "Unsupported host OS; only Linux and Windows are supported."
#endif

#if PLATFORM_LINUX
#    if __has_include(<cpuid.h>)
#        include <cpuid.h>
#    else
#        error "cpuid.h not found; please install the required headers."
#    endif
#endif

struct Platform {
#if PLATFORM_A64
    static constexpr bool a64 = 1;
    static constexpr bool x64 = 0;
#elif PLATFORM_X64
    static constexpr bool a64 = 0;
    static constexpr bool x64 = 1;
#endif

    struct Abi {
#if PLATFORM_A64
        static constexpr bool a64 = 1;
        static constexpr bool systemv = 0;
        static constexpr bool win64 = 0;
#elif PLATFORM_LINUX
        static constexpr bool a64 = 0;
        static constexpr bool systemv = 1;
        static constexpr bool win64 = 0;
#elif PLATFORM_WINDOWS
        static constexpr bool a64 = 0;
        static constexpr bool systemv = 0;
        static constexpr bool win64 = 1;
#endif
    } static constexpr abi{};

    static constexpr bool avx512 = 0; // TODO

} constexpr platform;

static_assert(std::endian::native == std::endian::little, "Only little-endian host platforms are supported.");

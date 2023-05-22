#pragma once

#include <cstddef>
#include <cstdint>

using s8 = std::int8_t;
using u8 = std::uint8_t;
using s16 = std::int16_t;
using u16 = std::uint16_t;
using s32 = std::int32_t;
using u32 = std::uint32_t;
using s64 = std::int64_t;
using u64 = std::uint64_t;

#ifdef __SIZEOF_INT128__
#define INT128_AVAILABLE 1
using s128 = __int128_t;
using u128 = __uint128_t;
#else
struct s128 {
    s64 lo, hi;
};
struct u128 {
    u64 lo, hi;
};
#endif

using uint = unsigned;

using f32 = float;
using f64 = double;

using std::size_t;

constexpr s8 operator""_s8(unsigned long long x)
{
    return static_cast<s8>(x);
}

constexpr u8 operator""_u8(unsigned long long x)
{
    return static_cast<u8>(x);
}

constexpr s16 operator""_s16(unsigned long long x)
{
    return static_cast<s16>(x);
}

constexpr u16 operator""_u16(unsigned long long x)
{
    return static_cast<u16>(x);
}

constexpr s32 operator""_s32(unsigned long long x)
{
    return static_cast<s32>(x);
}

constexpr u32 operator""_u32(unsigned long long x)
{
    return static_cast<u32>(x);
}

constexpr s64 operator""_s64(unsigned long long x)
{
    return static_cast<s64>(x);
}

constexpr u64 operator""_u64(unsigned long long x)
{
    return static_cast<u64>(x);
}

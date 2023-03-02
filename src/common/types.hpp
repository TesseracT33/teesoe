#pragma once

#include <cstddef>
#include <cstdint>

using s8 = int8_t;
using u8 = uint8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using s64 = int64_t;
using u64 = uint64_t;

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

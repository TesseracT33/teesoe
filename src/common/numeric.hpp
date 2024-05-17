#pragma once

#include <concepts>
#include <type_traits>

template<std::integral Int> Int AddWithOverflow(Int a, Int b, bool* overflow)
{
    using sInt = std::make_signed_t<Int>;
    sInt sum;
#if __has_builtin(__builtin_add_overflow)
    *overflow = __builtin_add_overflow(sInt(a), sInt(b), &sum);
#else
    sum = a + b;
    static constexpr auto shift = sizeof(Int) * 8 - 1;
    *overflow = ((a ^ sum) & (b ^ sum)) >> shift & 1;
#endif
    return sum;
}

template<std::integral Int> [[nodiscard]] constexpr auto MakeSigned(Int value)
{
    return static_cast<std::make_signed_t<Int>>(value);
}

template<std::integral Int> [[nodiscard]] constexpr auto MakeUnsigned(Int value)
{
    return static_cast<std::make_unsigned_t<Int>>(value);
}

template<std::integral Int> Int SubWithOverflow(Int a, Int b, bool* overflow)
{
    /* Note: __builtin_sub_overflow with signed args does not give us what we want */
    Int diff = a - b;
    static constexpr auto shift = sizeof(Int) * 8 - 1;
    *overflow = ((a ^ diff) & (b ^ diff)) >> shift & 1;
    return diff;
}

template<std::integral Int> Int AddWithCarry(Int a, Int b, bool* carry_out)
{
    static_assert(sizeof(Int) <= 8, "Integers larger than 8 bytes not supported.");
    using uInt = std::make_unsigned_t<Int>;
    uInt sum;
#if __has_builtin(__builtin_add_overflow)
    *carry_out = __builtin_add_overflow(uInt(a), uInt(b), &sum);
#elif defined _MSC_VER && defined _M_X64
    if constexpr (sizeof(Int) == 1) *carry_out = _addcarry_u8(0, uInt(a), uInt(b), &sum);
    if constexpr (sizeof(Int) == 2) *carry_out = _addcarry_u16(0, uInt(a), uInt(b), &sum);
    if constexpr (sizeof(Int) == 4) *carry_out = _addcarry_u32(0, uInt(a), uInt(b), &sum);
    if constexpr (sizeof(Int) == 8) *carry_out = _addcarry_u64(0, uInt(a), uInt(b), &sum);
#else
    sum = a + b;
    *carry_out = std::numeric_limits<uInt>::max() - uInt(a) < uInt(b);
#endif
    return sum;
}

/* Sign extends 'value' consisting of 'num_bits' bits to the width given by 'Int' */
template<std::integral Int, uint num_bits>
[[nodiscard]] constexpr Int SignExtend(std::integral auto value)
    requires(num_bits > 0 && sizeof(Int) * 8 >= num_bits && sizeof(Int) <= 8)
{
    if constexpr (num_bits == 8) {
        return Int(s8(value));
    } else if constexpr (num_bits == 16) {
        return Int(s16(value));
    } else if constexpr (num_bits == 32) {
        return Int(s32(value));
    } else if constexpr (num_bits == 64) {
        return Int(value);
    } else {
        static constexpr auto shift_amount = 8 * sizeof(Int) - num_bits;
        using sInt = std::make_signed_t<Int>;
        return Int(sInt(sInt(value) << shift_amount) >> shift_amount);
    }
}

template<std::integral Int> Int SubWithBorrow(Int a, Int b, bool* borrow_out)
{
    static_assert(sizeof(Int) <= 8);
    using uInt = std::make_unsigned_t<Int>;
    uInt diff;
#if __has_builtin(__builtin_sub_overflow)
    *borrow_out = __builtin_sub_overflow(uInt(a), uInt(b), &diff);
#elif defined _MSC_VER && defined _M_X64
    if constexpr (sizeof(Int) == 1) *borrow_out = _subborrow_u8(0, uInt(a), uInt(b), &diff);
    if constexpr (sizeof(Int) == 2) *borrow_out = _subborrow_u16(0, uInt(a), uInt(b), &diff);
    if constexpr (sizeof(Int) == 4) *borrow_out = _subborrow_u32(0, uInt(a), uInt(b), &diff);
    if constexpr (sizeof(Int) == 8) *borrow_out = _subborrow_u64(0, uInt(a), uInt(b), &diff);
#else
    diff = a + b;
    *borrow_out = uInt(a) < uInt(b);
#endif
    return diff;
}

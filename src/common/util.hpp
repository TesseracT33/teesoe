#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <expected>
#include <format>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

template<auto... Args> constexpr bool always_false{};

template<std::integral Int> constexpr void clear_bit(Int& num, std::integral auto pos)
{
    assert(pos >= 0 && pos < sizeof(Int) * 8);
    num &= ~(Int(1) << pos);
}

template<std::integral Int> [[nodiscard]] constexpr bool get_bit(Int num, std::integral auto pos)
{
    assert(pos >= 0 && pos < sizeof(Int) * 8);
    return num & Int(1) << pos;
}

[[nodiscard]] inline u8 get_byte(auto& obj, std::integral auto byte_index)
{
    assert(byte_index >= 0 && byte_index < sizeof(obj));
    return reinterpret_cast<u8*>(&obj)[byte_index];
}

template<typename T, size_t size> [[nodiscard]] constexpr std::array<T, size> make_array_of(T const& value)
{
    std::array<T, size> arr;
    arr.fill(value);
    return arr;
}

template<typename K, typename V1, typename... V2> [[nodiscard]] constexpr bool one_of(K key, V1 first, V2... rest)
{
    if constexpr (sizeof...(rest) == 0) {
        return key == first;
    } else {
        return key == first || one_of(key, rest...);
    }
}

[[nodiscard]] inline std::expected<std::vector<u8>, std::string> read_file(auto const& path,
  size_t expected_file_size = 0)
{
    std::ifstream ifs{ path, std::ifstream::in | std::ifstream::binary };
    if (!ifs) {
        return std::unexpected("Could not open the file.");
    }
    ifs.seekg(0, ifs.end);
    size_t file_size = ifs.tellg();
    bool test_size = expected_file_size > 0;
    if (test_size && file_size != expected_file_size) {
        return std::unexpected(
          std::format("The file was of the wrong size; expected {}, got {}.", expected_file_size, file_size));
    }
    std::vector<u8> vec(file_size);
    ifs.seekg(0, ifs.beg);
    ifs.read(reinterpret_cast<char*>(vec.data()), file_size);
    return vec;
}

template<std::integral Int> constexpr void set_bit(Int& num, std::integral auto pos)
{
    assert(pos >= 0 && pos < sizeof(Int) * 8);
    num |= Int(1) << pos;
}

inline void set_byte(auto& obj, std::integral auto byte_index, u8 value)
{
    assert(byte_index >= 0 && byte_index < sizeof(obj));
    reinterpret_cast<u8*>(&obj)[byte_index] = value;
}

/* Sign extends 'value' consisting of 'num_bits' bits to the width given by 'Int' */
template<std::integral Int, uint num_bits> [[nodiscard]] constexpr Int sign_extend(auto value)
{
    static_assert(num_bits > 0);
    static_assert(sizeof(Int) * 8 >= num_bits);
    static_assert(sizeof(Int) <= 8);

    if constexpr (num_bits == 8) {
        return Int(s8(value));
    } else if constexpr (num_bits == 16) {
        return Int(s16(value));
    } else if constexpr (num_bits == 32) {
        return Int(s32(value));
    } else if constexpr (num_bits == 64) {
        return value;
    } else {
        using sInt = std::make_signed_t<Int>;
        static constexpr auto shift_amount = 8 * sizeof(Int) - num_bits;
        auto signed_int = static_cast<sInt>(value);
        return static_cast<Int>(static_cast<sInt>(signed_int << shift_amount) >> shift_amount);
    }
}

template<std::integral Int> [[nodiscard]] constexpr auto to_signed(Int val)
{
    return static_cast<std::make_signed_t<Int>>(val);
}

template<std::integral Int> [[nodiscard]] constexpr auto to_unsigned(Int val)
{
    return static_cast<std::make_unsigned_t<Int>>(val);
}

template<std::integral Int> constexpr void toggle_bit(Int& num, uint pos)
{
    assert(pos >= 0 && pos < sizeof(Int) * 8);
    num ^= ~(Int(1) << pos);
}

template<std::integral IntOut, std::integral IntIn> [[nodiscard]] constexpr IntOut zero_extend(IntIn value)
{
    static_assert(sizeof(IntOut) >= sizeof(IntIn));
    using uIntIn = std::make_unsigned_t<IntIn>;
    return static_cast<IntOut>(static_cast<uIntIn>(value));
}

template<size_t> struct SizeToUInt {};

template<> struct SizeToUInt<1> {
    using type = u8;
};

template<> struct SizeToUInt<2> {
    using type = u16;
};

template<> struct SizeToUInt<4> {
    using type = u32;
};

template<> struct SizeToUInt<8> {
    using type = u64;
};

template<> struct SizeToUInt<16> {
    using type = u128;
};

/////////// Tests /////////////
static_assert(one_of(0, 0));
static_assert(!one_of(0, 1));
static_assert(one_of(0, 1, 2, 0, 3));
static_assert(one_of(0, 1, 2, 3, 0));
static_assert(!one_of(0, 1, 2, 3, 4));

#pragma once

#include "numtypes.hpp"

#include <concepts>
#include <type_traits>

constexpr void ClearBit(std::integral auto& num, uint pos)
{
    num &= ~(1ull << pos);
}

[[nodiscard]] constexpr bool GetBit(std::integral auto num, uint pos)
{
    return num & 1ull << pos;
}

constexpr void SetBit(std::integral auto& num, uint pos)
{
    num |= 1ull << pos;
}

constexpr void ToggleBit(std::integral auto& num, uint pos)
{
    num ^= ~(1ull << pos);
}

[[nodiscard]] constexpr u8 GetByte(auto& obj, uint byte_index)
{
    return *(reinterpret_cast<u8*>(&obj) + byte_index);
}

constexpr void SetByte(auto& obj, uint byte_index, u8 value)
{
    *(reinterpret_cast<u8*>(&obj) + byte_index) = value;
}

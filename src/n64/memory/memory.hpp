#pragma once

#include "numtypes.hpp"

#include <concepts>

namespace n64::memory {

template<std::signed_integral Int> Int Read(u32 addr);
template<size_t access_size, typename... MaskT> void Write(u32 addr, s64 data, MaskT... mask);

} // namespace n64::memory

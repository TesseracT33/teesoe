#include "controller_pak.hpp"

#include <algorithm>
#include <array>

namespace n64::controller_pak {

static std::array<u8, 0x8000> mem;

Status LoadFromFile(std::filesystem::path const& path)
{
    return UnimplementedStatus();
}

std::span<u8 const, 32> Read(u16 addr)
{
    addr &= 0x7FE0; // TODO: what happens when addr.15 is set?
    return std::span<u8, 32>{ mem.begin() + addr, 32 };
}

Status StoreToFile(std::filesystem::path const& path)
{
    return UnimplementedStatus();
}

void Write(u16 addr, std::span<u8 const, 32> data)
{
    addr &= 0x7FE0; // TODO: what happens when addr.15 is set?
    std::copy(data.begin(), data.end(), mem.begin() + addr);
}

} // namespace n64::controller_pak

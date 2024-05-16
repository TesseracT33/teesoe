#include "bios.hpp"
#include "files.hpp"

#include <cstring>
#include <expected>
#include <utility>
#include <vector>

namespace gba::bios {

static std::vector<u8> bios;

void Initialize()
{
}

Status Load(std::filesystem::path const& path)
{
    constexpr size_t bios_size = 0x4000;
    std::expected<std::vector<u8>, std::string> expected_bios = OpenFile(path, bios_size);
    if (expected_bios) {
        bios = std::move(expected_bios.value());
        return OkStatus();
    } else {
        return FailureStatus(expected_bios.error());
    }
}

template<std::integral Int> Int Read(u32 addr)
{
    Int ret;
    std::memcpy(&ret, &bios[addr], sizeof(Int));
    return ret;
}

template s8 Read<s8>(u32);
template u8 Read<u8>(u32);
template s16 Read<s16>(u32);
template u16 Read<u16>(u32);
template s32 Read<s32>(u32);
template u32 Read<u32>(u32);

} // namespace gba::bios

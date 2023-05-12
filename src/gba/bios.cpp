#include "bios.hpp"
#include "util.hpp"

#include <cstring>
#include <expected>
#include <format>
#include <vector>

namespace gba::bios {

static std::vector<u8> bios;

void Initialize()
{
}

Status Load(std::filesystem::path const& path)
{
    std::expected<std::vector<u8>, std::string> expected_bios = read_file(path);
    if (expected_bios) {
        if (expected_bios.value().size() == 0x4000) {
            bios = expected_bios.value();
            return status_ok();
        } else {
            return status_failure(
              std::format("BIOS must be 16 KiB large, but was {} bytes", expected_bios.value().size()));
        }
    } else {
        return status_failure(expected_bios.error());
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

#include "cart.hpp"
#include "util.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <filesystem>
#include <vector>

namespace gba::cart {

static void ResizeRomToPowerOfTwo(std::vector<u8>& rom);

static u32 rom_size_mask;
static u32 sram_size_mask;

static std::vector<u8> rom;
static std::vector<u8> sram;

void Initialize()
{
    sram.resize(0x10000, 0xFF); /* TODO: for now, SRAM is assumed to always exist and be 64 KiB */
    sram_size_mask = 0xFFFF;
}

Status LoadRom(std::filesystem::path const& path)
{
    std::expected<std::vector<u8>, std::string> expected_rom = read_file(path);
    if (expected_rom) {
        rom = expected_rom.value();
        ResizeRomToPowerOfTwo(rom);
        rom_size_mask = uint(rom.size() - 1);
        return OkStatus();
    } else {
        return FailureStatus(expected_rom.error());
    }
}

u8 ReadSram(u32 addr)
{
    u32 offset = addr & sram_size_mask; /* todo: detect sram size */
    return sram[offset];
}

template<std::integral Int> Int ReadRom(u32 addr)
{
    u32 offset = addr & 0x1FF'FFFF & rom_size_mask;
    Int ret;
    std::memcpy(&ret, rom.data() + offset, sizeof(Int));
    return ret;
}

void ResizeRomToPowerOfTwo(std::vector<u8>& rom)
{
    size_t actual_size = rom.size();
    size_t pow_two_size = std::bit_ceil(actual_size);
    size_t diff = pow_two_size - actual_size;
    if (diff > 0) {
        rom.resize(pow_two_size);
        std::copy(rom.begin(), rom.begin() + diff, rom.begin() + actual_size);
    }
}

void WriteSram(u32 addr, u8 data)
{
    u32 offset = addr & sram_size_mask;
    sram[offset] = data;
}

template u8 ReadRom<u8>(u32);
template s8 ReadRom<s8>(u32);
template u16 ReadRom<u16>(u32);
template s16 ReadRom<s16>(u32);
template u32 ReadRom<u32>(u32);
template s32 ReadRom<s32>(u32);

} // namespace gba::cart

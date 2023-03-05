#include "pif.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace n64::pif {

constexpr size_t command_byte_index = 0x7FF;
constexpr size_t ram_size = 0x40;
constexpr size_t rom_size = 0x7C0;
constexpr size_t ram_start = rom_size;
constexpr size_t memory_size = ram_size + rom_size;

struct JoypadStatus {
    u32 a      : 1;
    u32 b      : 1;
    u32 z      : 1;
    u32 s      : 1;
    u32 dU     : 1;
    u32 dD     : 1;
    u32 dL     : 1;
    u32 dR     : 1;
    u32 rst    : 1;
    u32        : 1;
    u32 l      : 1;
    u32 r      : 1;
    u32 cU     : 1;
    u32 cD     : 1;
    u32 cL     : 1;
    u32 cR     : 1;
    u32 x_axis : 8;
    u32 y_axis : 8;
} static joypad_status;

static std::array<u8, memory_size> memory; /* $0-$7BF: rom; $7C0-$7FF: ram */

static void ChallengeProtection();
static void ChecksumVerification();
static void ClearRam();
template<bool press> static void OnButtonAction(Control control);
static void RomLockout();
static void RunJoybusProtocol();
static void TerminateBootProcess();

void ChallengeProtection()
{
}

void ChecksumVerification()
{
    memory[command_byte_index] |= 0x80;
}

void ClearRam()
{
    std::fill(memory.begin() + ram_start, memory.end(), 0);
}

size_t GetNumberOfBytesUntilMemoryEnd(u32 offset)
{
    return memory_size - (offset & (memory_size - 1)); /* Size is 0x800 bytes */
}

size_t GetNumberOfBytesUntilRamStart(u32 offset)
{
    offset &= memory_size - 1;
    return offset < ram_start ? ram_start - offset : 0;
}

u8* GetPointerToMemory(u32 address)
{
    return memory.data() + (address & (memory_size - 1));
}

Status LoadIPL12(std::filesystem::path const& path)
{
    std::expected<std::vector<u8>, std::string> expected_rom = read_file(path, memory_size);
    if (!expected_rom) {
        return status_failure(std::format("Failed to open boot rom (IPL) file; {}", expected_rom.error()));
    }
    std::copy(expected_rom.value().begin(), expected_rom.value().end(), memory.begin());
    return status_ok();
}

void OnButtonAction(Control control, bool pressed)
{
    auto OnShoulderOrStartChanged = [pressed] {
        if (pressed) {
            if (joypad_status.l && joypad_status.r && joypad_status.s) {
                joypad_status.rst = 1;
                joypad_status.s = joypad_status.x_axis = joypad_status.y_axis = 0;
            }
        } else {
            joypad_status.rst = 0;
        }
    };
    switch (control) {
    case Control::A: joypad_status.a = pressed; break;
    case Control::B: joypad_status.b = pressed; break;
    case Control::CUp: joypad_status.cU = pressed; break;
    case Control::CDown: joypad_status.cD = pressed; break;
    case Control::CLeft: joypad_status.cL = pressed; break;
    case Control::CRight: joypad_status.cR = pressed; break;
    case Control::DUp: joypad_status.dU = pressed; break;
    case Control::DDown: joypad_status.dD = pressed; break;
    case Control::DLeft: joypad_status.dL = pressed; break;
    case Control::DRight: joypad_status.dR = pressed; break;
    case Control::ShoulderL:
        joypad_status.l = pressed;
        OnShoulderOrStartChanged();
        break;
    case Control::ShoulderR:
        joypad_status.r = pressed;
        OnShoulderOrStartChanged();
        break;
    case Control::Start:
        joypad_status.s = pressed;
        OnShoulderOrStartChanged();
        break;
    case Control::Z: joypad_status.z = pressed; break;
    default: std::unreachable();
    }
}

void OnJoystickMovement(Control control, s16 value)
{
    u8 adjusted_value = u8(value >> 8);
    if (control == Control::JX) {
        joypad_status.x_axis = adjusted_value;
    } else if (control == Control::JY) {
        joypad_status.y_axis = adjusted_value;
    } else {
        std::unreachable();
    }
}

template<std::signed_integral Int> Int ReadMemory(u32 addr)
{ /* CPU precondition: addr is aligned */
    Int ret;
    std::memcpy(&ret, memory.data() + (addr & 0x7FF), sizeof(Int));
    return std::byteswap(ret);
}

void RomLockout()
{
}

void RunJoybusProtocol()
{
    switch (memory[ram_start]) { /* joybus command */
    case 0x00: /* Info */
    case 0xFF: /* Reset & Info */
        /* Device: controller */
        memory[ram_start] = 0x05;
        memory[ram_start + 1] = 0x00;
        /* Pak installed */
        memory[ram_start + 2] = 0x01;
        break;

    case 0x01: /* Controller State */ std::memcpy(&memory[ram_start], &joypad_status, sizeof(joypad_status)); break;

    case 0x02: /* Read Controller Accessory */ break;

    case 0x03: /* Write Controller Accessory */ break;

    case 0x04: /* Read EEPROM */ break;

    case 0x05: /* Write EEPROM */ break;

    case 0x06: /* Real-Time Clock Info */
        std::memset(&memory[ram_start], 0, 3); /* clock does not exist */
        break;
    }
}

void TerminateBootProcess()
{
}

template<size_t access_size> void WriteMemory(u32 addr, s64 data)
{ /* CPU precondition: write does not go to the next boundary */
    addr &= 0x7FF;
    if (addr < ram_start) return;
    s32 to_write = [&] {
        if constexpr (access_size == 1) return data << (8 * (3 - (addr & 3)));
        if constexpr (access_size == 2) return data << (8 * (2 - (addr & 2)));
        if constexpr (access_size == 4) return data;
        if constexpr (access_size == 8) return data >> 32; /* TODO: not confirmed; could cause console lock-up? */
    }();
    to_write = std::byteswap(to_write);
    addr &= ~3;
    std::memcpy(&memory[addr], &to_write, 4);

    if (addr == command_byte_index - 3) {
        if (memory[command_byte_index] & 1) {
            RunJoybusProtocol();
            memory[command_byte_index] &= ~1;
        }
        if (memory[command_byte_index] & 2) {
            ChallengeProtection();
            memory[command_byte_index] &= ~2;
        }
        if (memory[command_byte_index] & 8) {
            TerminateBootProcess();
            memory[command_byte_index] &= ~8;
        }
        if (memory[command_byte_index] & 0x10) {
            RomLockout();
            memory[command_byte_index] &= ~0x10;
        }
        if (memory[command_byte_index] & 0x20) {
            ChecksumVerification();
            memory[command_byte_index] &= ~0x20;
        }
        if (memory[command_byte_index] & 0x40) {
            ClearRam();
            memory[command_byte_index] &= ~0x40;
        }
    }
}

template s8 ReadMemory<s8>(u32);
template s16 ReadMemory<s16>(u32);
template s32 ReadMemory<s32>(u32);
template s64 ReadMemory<s64>(u32);
template void WriteMemory<1>(u32, s64);
template void WriteMemory<2>(u32, s64);
template void WriteMemory<4>(u32, s64);
template void WriteMemory<8>(u32, s64);
} // namespace n64::pif

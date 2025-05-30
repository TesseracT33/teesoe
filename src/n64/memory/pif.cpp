#include "pif.hpp"
#include "files.hpp"
#include "log.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace n64::pif {

constexpr size_t command_byte_index = 0x3F;

struct {
    std::array<u8, 0x7C0> rom;
    std::array<u8, 0x40> ram;
} static mem;

static_assert(sizeof(mem) == 0x800);

struct JoypadStatus {
    u32 dR     : 1;
    u32 dL     : 1;
    u32 dD     : 1;
    u32 dU     : 1;
    u32 s      : 1;
    u32 z      : 1;
    u32 b      : 1;
    u32 a      : 1;
    u32 cR     : 1;
    u32 cL     : 1;
    u32 cD     : 1;
    u32 cU     : 1;
    u32 r      : 1;
    u32 l      : 1;
    u32        : 1;
    u32 rst    : 1;
    u32 x_axis : 8;
    u32 y_axis : 8;
} static joypad_status;

static void ChallengeProtection();
static void ChecksumVerification();
static void ControllerId(u8* cmd_result);
static void ControllerReadState();
static void ControllerReset();
static u8 AddrCrc(u16 addr);
static u8 DataCrc(std::string_view data);
static void RomLockout();
static void RunJoybusProtocol();
static void TerminateBootProcess();

void ChallengeProtection()
{
}

void ChecksumVerification()
{
    mem.ram[command_byte_index] |= 0x80;
}

void ControllerId(u8* cmd_result)
{
    /* Device: controller */
    cmd_result[0] = 0x05;
    cmd_result[1] = 0x00;
    /* Pak installed */
    cmd_result[2] = 0x01;
}

void ControllerReadState()
{
    // std::memcpy(&memory[ram_start], &joypad_status, sizeof(joypad_status));
}

void ControllerReset()
{
}

u8 AddrCrc(u16 addr)
{
    u8 crc{};
    for (int i = 0; i < 16; ++i) {
        u8 xor_ = crc & 0x10 ? 0x15 : 0;
        crc <<= 1;
        if (addr & 0x8000) crc |= 1;
        addr <<= 1;
        crc ^= xor_;
    }
    return crc & 31;
}

u8 DataCrc(std::string_view data)
{
    u8 crc{};
    for (int i = 0; i < 33; ++i) {
        for (int j = 7; j >= 0; --j) {
            u8 xor_ = crc & 0x80 ? 0x85 : 0;
            crc <<= 1;
            if (i < 32 && data[i] & 1 << j) crc |= 1;
            crc ^= xor_;
        }
    }
    return crc;
}

Status LoadIPL12(std::filesystem::path const& path)
{
    std::expected<std::vector<u8>, std::string> expected_rom = OpenFile(path, sizeof(mem));
    if (!expected_rom) {
        return FailureStatus(std::format("Failed to open boot rom (IPL) file; {}", expected_rom.error()));
    }
    std::copy(expected_rom.value().begin(), expected_rom.value().end(), mem.rom.begin());
    return OkStatus();
}

void OnButtonAction(Control control, bool pressed)
{
    auto OnShoulderOrStartChanged = [pressed] {
        if (pressed) {
            if (joypad_status.l && joypad_status.r && joypad_status.s) {
                joypad_status.rst = 1;
                joypad_status.s = 0;
                joypad_status.x_axis = 0;
                joypad_status.y_axis = 0;
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
    case Control::L:
        joypad_status.l = pressed;
        OnShoulderOrStartChanged();
        break;
    case Control::R:
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
    std::memcpy(&ret, reinterpret_cast<u8*>(&mem) + (addr & 0x7FF), sizeof(Int));
    return std::byteswap(ret);
}

void RomLockout()
{
}

void RunCommands()
{
    u8* cmd_byte = &mem.ram[command_byte_index];
    if (*cmd_byte & 1) {
        RunJoybusProtocol();
    }
    if (*cmd_byte & 2) {
        ChallengeProtection();
        *cmd_byte &= ~2;
    }
    if (*cmd_byte & 8) {
        TerminateBootProcess();
        *cmd_byte &= ~8;
    }
    if (*cmd_byte & 16) {
        RomLockout();
        *cmd_byte &= ~16;
    }
    if (*cmd_byte & 32) {
        ChecksumVerification();
        *cmd_byte &= ~32;
    }
    if (*cmd_byte & 64) {
        mem.ram = {};
        *cmd_byte &= ~64;
    }
}

void RunJoybusProtocol()
{
    int channel{}, offset{};
    while (offset < 64 && channel < 5) {
        u8* cmd_base = &mem.ram[offset++];
        u8 cmd_len = cmd_base[0];
        if (cmd_len == 0 || cmd_len == 253) {
            ++channel;
            continue;
        }
        if (cmd_len == 254) break; // end of commands
        if (cmd_len == 255) continue;
        cmd_len &= 63;
        u8 result_len = mem.ram[offset++];
        if (result_len == 254) break; // end of commands
        result_len &= 63;
        assert(offset + cmd_len < (int)mem.ram.size());
        u8* result = &mem.ram[offset + cmd_len];

        switch (u8 cmd = mem.ram[offset++]; cmd) {
        case 0:
            ControllerId(result);
            ++channel;
            break; /* Info */
        case 1: /* Controller State */
            std::memcpy(result, &joypad_status, 4);
            ++channel;
            break;
        case 2: /* Read Controller Accessory */ break;
        case 3: /* Write Controller Accessory */ break;
        case 4: /* Read EEPROM */ break;
        case 5: /* Write EEPROM */ break;
        case 6: /* Real-Time Clock Info */ break;
        case 255: /* Reset & Info */
            ControllerReset();
            ControllerId(result);
            ++channel;
            break;
        default: LogWarn("Unexpected joybus command {} encountered.", cmd);
        }
    }
}

void TerminateBootProcess()
{
}

template<size_t access_size> void WriteMemory(u32 addr, s64 data)
{ /* CPU precondition: write does not go to the next boundary */
    addr &= 0x7FF;
    if (addr < 0x7C0) return;
    s32 to_write = [&] {
        if constexpr (access_size == 1) return s32(data << (8 * (3 - (addr & 3))));
        if constexpr (access_size == 2) return s32(data << (8 * (2 - (addr & 2))));
        if constexpr (access_size == 4) return s32(data);
        if constexpr (access_size == 8) return s32(data >> 32); /* TODO: could cause console lock-up? */
    }();
    to_write = std::byteswap(to_write);
    addr &= 0x3C;
    std::memcpy(&mem.ram[addr], &to_write, 4);

    if (addr == command_byte_index - 3) {
        RunCommands();
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

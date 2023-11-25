#include "frontend/audio.hpp"
#include "frontend/message.hpp"
#include "interface/mi.hpp"
#include "log.hpp"
#include "memory/memory.hpp"
#include "n64.hpp"
#include "n64_build_options.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <string_view>

namespace n64::ai {

enum Register {
    DramAddr,
    Len,
    Control,
    Status,
    Dacrate,
    Bitrate
};

static u64 cycles;
static u32 dac_freq;
static u32 dac_period;
static u16 dacrate;
static u8 dma_count;
static bool dma_enable;

static std::array<u32, 2> dma_addr, dma_len; // 0: current addr/len; 1 : buffered addr/len

constexpr std::string_view RegOffsetToStr(u32 reg_offset);
static void Sample();

void Initialize()
{
    dma_addr[0] = dma_addr[1] = dma_len[0] = dma_len[1] = dacrate = dma_count = cycles = 0;
    dma_enable = false;
    dac_freq = 44100;
    dac_period = cpu_cycles_per_second / dac_freq;
}

u32 ReadReg(u32 addr)
{
    u32 ret;
    if (addr == 0x0450'000C) { // AI_STATUS
        ret = (dma_count > 1) | 1 << 20 | 1 << 24 | dma_enable << 25 | (dma_count > 0) << 30 | (dma_count > 1) << 31;
    } else { // AI_LENGTH (mirrored)
        ret = dma_len[0];
    }
    if constexpr (log_io_ai) {
        log(std::format("AI: {} => ${:08X}", RegOffsetToStr(addr >> 2 & 7), ret));
    }
    return ret;
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case DramAddr: return "AI_DRAM_ADDR";
    case Len: return "AI_LEN";
    case Control: return "AI_CONTROL";
    case Status: return "AI_STATUS";
    case Dacrate: return "AI_DACRATE";
    case Bitrate: return "AI_BITRATE";
    default: return "UNKNOWN";
    }
}

void Sample()
{
    if (dma_count == 0) {
        frontend::audio::PushSample(0, 0);
    } else {
        if (dma_enable && dma_len[0] > 0) {
            s32 data = memory::Read<s32>(dma_addr[0]);
            s16 left = data >> 16;
            s16 right = data & 0xFFFF;
            frontend::audio::PushSample(left, right);
            dma_addr[0] += 4;
            dma_len[0] -= 4;
        }
        if (dma_len[0] == 0 && --dma_count > 0) {
            mi::RaiseInterrupt(mi::InterruptType::AI);
            bool addr_carry_bug = !(dma_addr[0] & 0x1FFF);
            dma_addr[0] = dma_addr[1];
            dma_len[0] = dma_len[1];
            if (addr_carry_bug) {
                dma_addr[0] += 0x2000;
            }
        }
    }
}

void Step(u64 cpu_cycles)
{
    cycles += cpu_cycles;
    while (cycles >= dac_period) {
        Sample();
        cycles -= dac_period;
    }
}

void WriteReg(u32 addr, u32 data)
{
    u32 offset = addr >> 2 & 7;
    if constexpr (log_io_ai) {
        log(std::format("AI: {} <= ${:08X}", RegOffsetToStr(offset), data));
    }

    switch (offset) {
    case Register::DramAddr:
        if (dma_count < 2) {
            dma_addr[dma_count] = data & 0xFF'FFF8;
        }
        break;

    case Register::Len:
        if (dma_count < 2) {
            if (dma_count == 0) {
                mi::RaiseInterrupt(mi::InterruptType::AI);
            }
            dma_len[dma_count++] = data & 0x3'FFF8;
        }
        break;

    case Register::Control: dma_enable = data & 1; break;

    case Register::Status: mi::ClearInterrupt(mi::InterruptType::AI); break;

    case Register::Dacrate: {
        dacrate = data & 0x3FFF;
        auto prev_freq = dac_freq;
        dac_freq = std::max(1u, u32(cpu_cycles_per_second / 2 / (dacrate + 1) * 1.037));
        dac_period = cpu_cycles_per_second / dac_freq;
        if (dac_freq != prev_freq) {
            frontend::audio::SetSampleRate(dac_freq);
        }
    } break;

    case Register::Bitrate: break;

    default: log_warn(std::format("Unexpected write made to AI register at address ${:08X}", addr));
    }
}

} // namespace n64::ai

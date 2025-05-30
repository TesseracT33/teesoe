#include "vi.hpp"
#include "log.hpp"
#include "mi.hpp"
#include "n64.hpp"
#include "n64_build_options.hpp"
#include "rdp/rdp.hpp"
#include "scheduler.hpp"

#include <bit>
#include <cstring>
#include <string_view>
#include <utility>

namespace n64::vi {

static Registers vi;
static u32 cpu_cycles_per_halfline;
constexpr u32 default_vsync_ntsc = 0x20D;

static void CheckVideoInterrupt();
static bool Interlaced();
static void OnNewHalflineEvent();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

void AddInitialEvents()
{
    scheduler::AddEvent(scheduler::EventType::VINewHalfline, cpu_cycles_per_halfline, OnNewHalflineEvent);
}

void CheckVideoInterrupt()
{
    bool int_enable = (vi.ctrl & 3) != 0;
    if (int_enable) {
        u32 mask = 0x3fe | ~vi.v_sync & 1;
        if ((vi.v_current & mask) == (vi.v_intr & mask)) {
            mi::RaiseInterrupt(mi::InterruptType::VI);
        }
    }
}

void Initialize()
{
    vi = {};
    vi.ctrl = 3;
    vi.width = 320;
    vi.v_intr = 0x3FF;
    vi.burst = 1;
    vi.v_sync = default_vsync_ntsc; /* todo: pal */
    vi.h_sync = 0x15'07FF;
    cpu_cycles_per_halfline = cpu_cycles_per_frame / (vi.v_sync >> 1);
    scheduler::ChangeEventTime(scheduler::EventType::VINewHalfline, cpu_cycles_per_halfline);
}

bool Interlaced()
{
    return vi.ctrl & 0x40;
}

void OnNewHalflineEvent()
{
    vi.v_current += 2;
    if (vi.v_current >= vi.v_sync) {
        u32 field = vi.v_current & 1;
        vi.v_current = (field ^ 1) & u32(Interlaced());
        rdp::implementation->UpdateScreen();
    }
    CheckVideoInterrupt();
    scheduler::AddEvent(scheduler::EventType::VINewHalfline, cpu_cycles_per_halfline, OnNewHalflineEvent);
}

Registers const& ReadAllRegisters()
{
    return vi;
}

u32 ReadReg(u32 addr)
{
    static_assert(sizeof(vi) >> 2 == 0x10);
    u32 offset = addr >> 2 & 0xF;
    u32 ret = std::bit_cast<std::array<u32, 16>>(vi)[offset];
    if constexpr (log_io_vi) {
        LogInfo("VI: {} => ${:08X}", RegOffsetToStr(offset), ret);
    }
    return ret;
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case Ctrl: return "VI_CTRL";
    case Origin: return "VI_ORIGIN";
    case Width: return "VI_WIDTH";
    case VIntr: return "VI_V_INTR";
    case VCurrent: return "VI_V_CURRENT";
    case Burst: return "VI_BURST";
    case VSync: return "VI_V_SYNC";
    case HSync: return "VI_H_SYNC";
    case HSyncLeap: return "VI_HYNC_LEAP";
    case HVideo: return "VI_H_VIDEO";
    case VVideo: return "VI_V_VIDEO";
    case VBurst: return "VI_V_BURST";
    case XScale: return "VI_X_SCALE";
    case YScale: return "VI_Y_SCALE";
    case TestAddr: return "VI_TEST_ADDR";
    case StagedData: return "VI_STAGED_DATA";
    default: std::unreachable();
    }
}

void WriteReg(u32 addr, u32 data)
{
    static_assert(sizeof(vi) >> 2 == 0x10);
    u32 offset = addr >> 2 & 0xF;
    if constexpr (log_io_vi) {
        LogInfo("VI: {} <= ${:08X}", RegOffsetToStr(offset), data);
    }

    switch (offset) {
    case Register::Ctrl: vi.ctrl = data; break;

    case Register::Origin: vi.origin = data & 0xFF'FFFF; break;

    case Register::Width: vi.width = data & 0xFFF; break;

    case Register::VIntr: vi.v_intr = data & 0x3FF; break;

    case Register::VCurrent: mi::ClearInterrupt(mi::InterruptType::VI); break;

    case Register::Burst: vi.burst = data & 0x3FFF'FFFF; break;

    case Register::VSync:
        vi.v_sync = data & 0x3FF ? data & 0x3FF : default_vsync_ntsc; /* todo: pal */
        cpu_cycles_per_halfline = cpu_cycles_per_frame / (vi.v_sync >> 1);
        scheduler::ChangeEventTime(scheduler::EventType::VINewHalfline, cpu_cycles_per_halfline);
        break;

    case Register::HSync: vi.h_sync = data & 0x1F'0FFF; break;

    case Register::HSyncLeap: vi.h_sync_leap = data & 0xFFF'0FFF; break;

    case Register::HVideo: vi.h_video = data & 0x3FF'03FF; break;

    case Register::VVideo: vi.v_video = data & 0x3FF'03FF; break;

    case Register::VBurst: vi.v_burst = data & 0x3FF'03FF; break;

    case Register::XScale: vi.x_scale = data & 0xFFF'0FFF; break;

    case Register::YScale: vi.y_scale = data & 0xFFF'0FFF; break;

    case Register::TestAddr: vi.test_addr = data & 0x7F; break;

    case Register::StagedData: vi.staged_data = data; break;

    default: std::unreachable();
    }
}
} // namespace n64::vi

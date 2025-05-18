#include "mi.hpp"
#include "log.hpp"
#include "n64_build_options.hpp"
#include "platform.hpp"
#include "vr4300/vr4300.hpp"

#include <cstring>
#include <string_view>
#include <utility>

#if PLATFORM_X64
#    include <immintrin.h>
#endif

namespace n64::mi {

enum Register {
    Mode,
    Version,
    Interrupt,
    Mask
};

static void CheckInterrupts();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

struct {
    s32 mode, version, interrupt, mask;
} static mi;

void CheckInterrupts()
{
    if (mi.interrupt & mi.mask) {
        vr4300::SetInterruptPending(vr4300::ExternalInterruptSource::MI);
    } else {
        vr4300::ClearInterruptPending(vr4300::ExternalInterruptSource::MI);
    }
}

void ClearInterrupt(InterruptType interrupt_type)
{
    mi.interrupt &= ~std::to_underlying(interrupt_type);
    CheckInterrupts();
}

void Initialize()
{
    mi.mode = mi.interrupt = mi.mask = 0;
    mi.version = 0x0202'0102;
}

void RaiseInterrupt(InterruptType interrupt_type)
{
    mi.interrupt |= std::to_underlying(interrupt_type);
    CheckInterrupts();
}

u32 ReadReg(u32 addr)
{
    static_assert(sizeof(mi) >> 2 == 4);
    u32 offset = addr >> 2 & 3;
    u32 ret;
    std::memcpy(&ret, (u32*)(&mi) + offset, 4);
    if constexpr (log_io_mi) {
        LogInfo("MI: {} => ${:08X}", RegOffsetToStr(offset), ret);
    }
    return ret;
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case Mode: return "MI_MODE";
    case Version: return "MI_VERSION";
    case Interrupt: return "MI_INTERRUPT";
    case Mask: return "MI_MASK";
    default: std::unreachable();
    }
}

void WriteReg(u32 addr, u32 data)
{
    u32 offset = addr >> 2 & 3;
    if constexpr (log_io_mi) {
        LogInfo("MI: {} <= ${:08X}", RegOffsetToStr(offset), data);
    }

    if (offset == Register::Mode) {
        mi.mode = data;
        if (mi.mode & 0x800) {
            ClearInterrupt(InterruptType::DP);
        }
    } else if (offset == Register::Mask) {
        enum {
            clear_sp_mask = 1 << 0,
            set_sp_mask = 1 << 1,
            clear_si_mask = 1 << 2,
            set_si_mask = 1 << 3,
            clear_ai_mask = 1 << 4,
            set_ai_mask = 1 << 5,
            clear_vi_mask = 1 << 6,
            set_vi_mask = 1 << 7,
            clear_pi_mask = 1 << 8,
            set_pi_mask = 1 << 9,
            clear_dp_mask = 1 << 10,
            set_dp_mask = 1 << 11,
        };

#if PLATFORM_X64
        static constexpr u32 clear_mask =
          clear_sp_mask | clear_si_mask | clear_ai_mask | clear_vi_mask | clear_pi_mask | clear_dp_mask;
        static constexpr u32 set_mask =
          set_sp_mask | set_si_mask | set_ai_mask | set_vi_mask | set_pi_mask | set_dp_mask;
        mi.mask &= ~_pext_u32(data, clear_mask);
        mi.mask |= _pext_u32(data, set_mask);
#else
        auto ClearMask = [](InterruptType interrupt_type) { mi.mask &= ~std::to_underlying(interrupt_type); };
        auto SetMask = [](InterruptType interrupt_type) { mi.mask |= std::to_underlying(interrupt_type); };

        if (data & set_sp_mask) {
            SetMask(InterruptType::SP);
        } else if (data & clear_sp_mask) {
            ClearMask(InterruptType::SP);
        }
        if (data & set_si_mask) {
            SetMask(InterruptType::SI);
        } else if (data & clear_si_mask) {
            ClearMask(InterruptType::SI);
        }
        if (data & set_ai_mask) {
            SetMask(InterruptType::AI);
        } else if (data & clear_ai_mask) {
            ClearMask(InterruptType::AI);
        }
        if (data & set_vi_mask) {
            SetMask(InterruptType::VI);
        } else if (data & clear_vi_mask) {
            ClearMask(InterruptType::VI);
        }
        if (data & set_pi_mask) {
            SetMask(InterruptType::PI);
        } else if (data & clear_pi_mask) {
            ClearMask(InterruptType::PI);
        }
        if (data & set_dp_mask) {
            SetMask(InterruptType::DP);
        } else if (data & clear_dp_mask) {
            ClearMask(InterruptType::DP);
        }
#endif
        CheckInterrupts();
    }
}

} // namespace n64::mi

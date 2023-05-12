#include "dma.hpp"
#include "bus.hpp"
#include "irq.hpp"
#include "scheduler.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <utility>

namespace gba::dma {

struct DmaChannel {
    void OnDmaDisable();
    void OnDmaEnable();
    void NotifyDmaActive() const;
    void NotifyDmaInactive() const;
    void ReloadCount();
    void ReloadDstAddr();
    void ReloadSrcAddr();
    void WriteControlLo(u8 data);
    void WriteControlHi(u8 data);
    void WriteControl(u16 data);
    bool next_copy_is_repeat;
    bool suspended;
    uint index;
    u32 current_count;
    u32 current_dst_addr;
    u32 current_src_addr;
    u32 count;
    u32 dst_addr;
    u32 dst_addr_incr;
    u32 src_addr;
    u32 src_addr_incr;
    u64 cycle;
    struct Control {
        u16               : 5;
        u16 dst_addr_ctrl : 2;
        u16 src_addr_ctrl : 2;
        u16 repeat        : 1;
        u16 transfer_type : 1;
        u16 game_pak_drq  : 1;
        u16 start_timing  : 2;
        u16 irq_enable    : 1;
        u16 enable        : 1;
    } control;
    irq::Source irq_source;
    scheduler::DriverType driver_type;
    scheduler::DriverRunFunc perform_dma_func;
    scheduler::DriverSuspendFunc suspend_dma_func;
};

template<uint dma_index> static u64 PerformDma(u64 max_cycles_to_run);
template<uint dma_index> static void SuspendDma();

static std::array<DmaChannel, 4> dma_ch;

void AddCycles(u64 cycles, uint ch)
{
    dma_ch[ch].cycle += cycles;
}

void Initialize()
{
    for (int i = 0; i < 4; ++i) {
        std::memset(&dma_ch[i], 0, sizeof(DmaChannel));
        dma_ch[i].index = i;
    }
    dma_ch[0].driver_type = scheduler::DriverType::Dma0;
    dma_ch[1].driver_type = scheduler::DriverType::Dma1;
    dma_ch[2].driver_type = scheduler::DriverType::Dma2;
    dma_ch[3].driver_type = scheduler::DriverType::Dma3;
    dma_ch[0].perform_dma_func = PerformDma<0>;
    dma_ch[1].perform_dma_func = PerformDma<1>;
    dma_ch[2].perform_dma_func = PerformDma<2>;
    dma_ch[3].perform_dma_func = PerformDma<3>;
    dma_ch[0].suspend_dma_func = SuspendDma<0>;
    dma_ch[1].suspend_dma_func = SuspendDma<1>;
    dma_ch[2].suspend_dma_func = SuspendDma<2>;
    dma_ch[3].suspend_dma_func = SuspendDma<3>;
    dma_ch[0].irq_source = irq::Source::Dma0;
    dma_ch[1].irq_source = irq::Source::Dma1;
    dma_ch[2].irq_source = irq::Source::Dma2;
    dma_ch[3].irq_source = irq::Source::Dma3;
}

void OnHBlank()
{
    static constexpr u16 hblank_start_timing = 2;
    std::ranges::for_each(dma_ch, [](DmaChannel const& dma) {
        if (dma.control.enable && dma.control.start_timing == hblank_start_timing) {
            dma.NotifyDmaActive();
        }
    });
}

void OnVBlank()
{
    static constexpr u16 vblank_start_timing = 1;
    std::ranges::for_each(dma_ch, [](DmaChannel const& dma) {
        if (dma.control.enable && dma.control.start_timing == vblank_start_timing) {
            dma.NotifyDmaActive();
        }
    });
}

/* A free function (not part of DmaChannel struct), so that it works correctly with the scheduler
   (it stores a pointer to this function, and cannot store member function pointers).
   Templated, because the scheduler expects a function of the form u64 f(u64).
   TODO: maybe address? Could use std::function and bind 'this', but it has some overhead */
template<uint dma_index> u64 PerformDma(u64 max_cycles_to_run)
{
    static_assert(dma_index <= 3);
    DmaChannel& dma = dma_ch[dma_index];
    static constexpr scheduler::DriverType driver = [&] {
        if constexpr (dma_index == 0) return scheduler::DriverType::Dma0;
        if constexpr (dma_index == 1) return scheduler::DriverType::Dma1;
        if constexpr (dma_index == 2) return scheduler::DriverType::Dma2;
        if constexpr (dma_index == 3) return scheduler::DriverType::Dma3;
    }();

    if (dma.suspended) {
        dma.suspended = false;
    } else {
        if (dma.next_copy_is_repeat) {
            dma.ReloadCount();
            if (dma.control.dst_addr_ctrl == 3) {
                dma.ReloadDstAddr();
            }
        }
        auto GetAddrIncr = [&](auto addr_control) {
            switch (addr_control) {
            case 0:
            case 3: /* increment */ return 2 + 2 * dma.control.transfer_type;
            case 1: /* decrement */ return -2 - 2 * dma.control.transfer_type;
            case 2: /* fixed */ return 0;
            default: std::unreachable();
            }
        };
        dma.dst_addr_incr = GetAddrIncr(dma.control.dst_addr_ctrl);
        dma.src_addr_incr = GetAddrIncr(dma.control.src_addr_ctrl);
    }

    dma.cycle = 0;
    auto DoDma = [&]<std::integral Int> {
        while (dma.current_count > 0 && dma.cycle < max_cycles_to_run) {
            bus::Write<Int, driver>(dma.current_dst_addr, bus::Read<Int, driver>(dma.current_src_addr));
            --dma.current_count;
            dma.current_dst_addr += dma.dst_addr_incr;
            dma.current_src_addr += dma.src_addr_incr;
        }
    };
    if (dma.control.transfer_type == 0) {
        DoDma.template operator()<u16>();
    } else {
        DoDma.template operator()<u32>();
    }
    if (dma.current_count == 0) {
        dma.NotifyDmaInactive();
        if (dma.control.irq_enable) {
            irq::Raise(dma.irq_source);
        }
        if (dma.control.repeat) {
            dma.next_copy_is_repeat = true;
            assert(dma.control.transfer_type != 0);
        } else {
            dma.control.enable = false;
        }
    }
    return dma.cycle;
}

template<std::integral Int> Int ReadReg(u32 addr)
{
    auto ReadByte = [](u32 addr) {
        switch (addr) {
        case bus::ADDR_DMA0CNT_L:
        case bus::ADDR_DMA0CNT_L + 1: return u8(0);
        case bus::ADDR_DMA0CNT_H: return get_byte(dma_ch[0].control, 0);
        case bus::ADDR_DMA0CNT_H + 1: return get_byte(dma_ch[0].control, 1);
        case bus::ADDR_DMA1CNT_L:
        case bus::ADDR_DMA1CNT_L + 1: return u8(0);
        case bus::ADDR_DMA1CNT_H: return get_byte(dma_ch[1].control, 0);
        case bus::ADDR_DMA1CNT_H + 1: return get_byte(dma_ch[1].control, 1);
        case bus::ADDR_DMA2CNT_L:
        case bus::ADDR_DMA2CNT_L + 1: return u8(0);
        case bus::ADDR_DMA2CNT_H: return get_byte(dma_ch[2].control, 0);
        case bus::ADDR_DMA2CNT_H + 1: return get_byte(dma_ch[2].control, 1);
        case bus::ADDR_DMA3CNT_L:
        case bus::ADDR_DMA3CNT_L + 1: return u8(0);
        case bus::ADDR_DMA3CNT_H: return get_byte(dma_ch[3].control, 0);
        case bus::ADDR_DMA3CNT_H + 1: return get_byte(dma_ch[3].control, 1);
        default: return bus::ReadOpenBus<u8>(addr);
        }
    };

    auto ReadHalf = [](u32 addr) {
        switch (addr) {
        case bus::ADDR_DMA0CNT_L: return u16(0);
        case bus::ADDR_DMA0CNT_H: return std::bit_cast<u16>(dma_ch[0].control);
        case bus::ADDR_DMA1CNT_L: return u16(0);
        case bus::ADDR_DMA1CNT_H: return std::bit_cast<u16>(dma_ch[1].control);
        case bus::ADDR_DMA2CNT_L: return u16(0);
        case bus::ADDR_DMA2CNT_H: return std::bit_cast<u16>(dma_ch[2].control);
        case bus::ADDR_DMA3CNT_L: return u16(0);
        case bus::ADDR_DMA3CNT_H: return std::bit_cast<u16>(dma_ch[3].control);
        default: return bus::ReadOpenBus<u16>(addr);
        }
    };

    auto ReadWord = [&](u32 addr) {
        u16 lo = ReadHalf(addr);
        u16 hi = ReadHalf(addr + 2);
        return lo | hi << 16;
    };

    if constexpr (sizeof(Int) == 1) {
        return ReadByte(addr);
    }
    if constexpr (sizeof(Int) == 2) {
        return ReadHalf(addr);
    }
    if constexpr (sizeof(Int) == 4) {
        return ReadWord(addr);
    }
}

template<uint dma_index> void SuspendDma()
{
    dma_ch[dma_index].suspended = true;
}

template<std::integral Int> void WriteReg(u32 addr, Int data)
{
    auto WriteByte = [](u32 addr, u8 data) {
        switch (addr) {
        case bus::ADDR_DMA0SAD: set_byte(dma_ch[0].src_addr, 0, data); break;
        case bus::ADDR_DMA0SAD + 1: set_byte(dma_ch[0].src_addr, 1, data); break;
        case bus::ADDR_DMA0SAD + 2: set_byte(dma_ch[0].src_addr, 2, data); break;
        case bus::ADDR_DMA0SAD + 3: set_byte(dma_ch[0].src_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA0DAD: set_byte(dma_ch[0].dst_addr, 0, data); break;
        case bus::ADDR_DMA0DAD + 1: set_byte(dma_ch[0].dst_addr, 1, data); break;
        case bus::ADDR_DMA0DAD + 2: set_byte(dma_ch[0].dst_addr, 2, data); break;
        case bus::ADDR_DMA0DAD + 3: set_byte(dma_ch[0].dst_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA0CNT_L: set_byte(dma_ch[0].count, 0, data); break;
        case bus::ADDR_DMA0CNT_L + 1: set_byte(dma_ch[0].count, 1, data & 0x3F); break;
        case bus::ADDR_DMA0CNT_H: dma_ch[0].WriteControlLo(data); break;
        case bus::ADDR_DMA0CNT_H + 1: dma_ch[0].WriteControlHi(data); break;
        case bus::ADDR_DMA1SAD: set_byte(dma_ch[1].src_addr, 0, data); break;
        case bus::ADDR_DMA1SAD + 1: set_byte(dma_ch[1].src_addr, 1, data); break;
        case bus::ADDR_DMA1SAD + 2: set_byte(dma_ch[1].src_addr, 2, data); break;
        case bus::ADDR_DMA1SAD + 3: set_byte(dma_ch[1].src_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA1DAD: set_byte(dma_ch[1].dst_addr, 0, data); break;
        case bus::ADDR_DMA1DAD + 1: set_byte(dma_ch[1].dst_addr, 1, data); break;
        case bus::ADDR_DMA1DAD + 2: set_byte(dma_ch[1].dst_addr, 2, data); break;
        case bus::ADDR_DMA1DAD + 3: set_byte(dma_ch[1].dst_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA1CNT_L: set_byte(dma_ch[1].count, 0, data); break;
        case bus::ADDR_DMA1CNT_L + 1: set_byte(dma_ch[1].count, 1, data & 0x3F); break;
        case bus::ADDR_DMA1CNT_H: dma_ch[1].WriteControlLo(data); break;
        case bus::ADDR_DMA1CNT_H + 1: dma_ch[1].WriteControlHi(data); break;
        case bus::ADDR_DMA2SAD: set_byte(dma_ch[2].src_addr, 0, data); break;
        case bus::ADDR_DMA2SAD + 1: set_byte(dma_ch[2].src_addr, 1, data); break;
        case bus::ADDR_DMA2SAD + 2: set_byte(dma_ch[2].src_addr, 2, data); break;
        case bus::ADDR_DMA2SAD + 3: set_byte(dma_ch[2].src_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA2DAD: set_byte(dma_ch[2].dst_addr, 0, data); break;
        case bus::ADDR_DMA2DAD + 1: set_byte(dma_ch[2].dst_addr, 1, data); break;
        case bus::ADDR_DMA2DAD + 2: set_byte(dma_ch[2].dst_addr, 2, data); break;
        case bus::ADDR_DMA2DAD + 3: set_byte(dma_ch[2].dst_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA2CNT_L: set_byte(dma_ch[2].count, 0, data); break;
        case bus::ADDR_DMA2CNT_L + 1: set_byte(dma_ch[2].count, 1, data & 0x3F); break;
        case bus::ADDR_DMA2CNT_H: dma_ch[2].WriteControlLo(data); break;
        case bus::ADDR_DMA2CNT_H + 1: dma_ch[2].WriteControlHi(data); break;
        case bus::ADDR_DMA3SAD: set_byte(dma_ch[3].src_addr, 0, data); break;
        case bus::ADDR_DMA3SAD + 1: set_byte(dma_ch[3].src_addr, 1, data); break;
        case bus::ADDR_DMA3SAD + 2: set_byte(dma_ch[3].src_addr, 2, data); break;
        case bus::ADDR_DMA3SAD + 3: set_byte(dma_ch[3].src_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA3DAD: set_byte(dma_ch[3].dst_addr, 0, data); break;
        case bus::ADDR_DMA3DAD + 1: set_byte(dma_ch[3].dst_addr, 1, data); break;
        case bus::ADDR_DMA3DAD + 2: set_byte(dma_ch[3].dst_addr, 2, data); break;
        case bus::ADDR_DMA3DAD + 3: set_byte(dma_ch[3].dst_addr, 3, data & 0xF); break;
        case bus::ADDR_DMA3CNT_L: set_byte(dma_ch[3].count, 0, data); break;
        case bus::ADDR_DMA3CNT_L + 1: set_byte(dma_ch[3].count, 1, data); break;
        case bus::ADDR_DMA3CNT_H: dma_ch[3].WriteControlLo(data); break;
        case bus::ADDR_DMA3CNT_H + 1: dma_ch[3].WriteControlHi(data); break;
        }
    };

    auto WriteHalf = [](u32 addr, u16 data) {
        switch (addr) {
        case bus::ADDR_DMA0SAD: dma_ch[0].src_addr = dma_ch[0].src_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA0SAD + 2: dma_ch[0].src_addr = dma_ch[0].src_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA0DAD: dma_ch[0].dst_addr = dma_ch[0].dst_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA0DAD + 2: dma_ch[0].dst_addr = dma_ch[0].dst_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA0CNT_L: dma_ch[0].count = data; break;
        case bus::ADDR_DMA0CNT_H: dma_ch[0].WriteControl(data); break;
        case bus::ADDR_DMA1SAD: dma_ch[1].src_addr = dma_ch[1].src_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA1SAD + 2: dma_ch[1].src_addr = dma_ch[1].src_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA1DAD: dma_ch[1].dst_addr = dma_ch[1].dst_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA1DAD + 2: dma_ch[1].dst_addr = dma_ch[1].dst_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA1CNT_L: dma_ch[1].count = data; break;
        case bus::ADDR_DMA1CNT_H: dma_ch[1].WriteControl(data); break;
        case bus::ADDR_DMA2SAD: dma_ch[2].src_addr = dma_ch[2].src_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA2SAD + 2: dma_ch[2].src_addr = dma_ch[2].src_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA2DAD: dma_ch[2].dst_addr = dma_ch[2].dst_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA2DAD + 2: dma_ch[2].dst_addr = dma_ch[2].dst_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA2CNT_L: dma_ch[2].count = data; break;
        case bus::ADDR_DMA2CNT_H: dma_ch[2].WriteControl(data); break;
        case bus::ADDR_DMA3SAD: dma_ch[3].src_addr = dma_ch[3].src_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA3SAD + 2: dma_ch[3].src_addr = dma_ch[3].src_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA3DAD: dma_ch[3].dst_addr = dma_ch[3].dst_addr & 0xFFFF'0000 | data; break;
        case bus::ADDR_DMA3DAD + 2: dma_ch[3].dst_addr = dma_ch[3].dst_addr & 0xFFFF | data << 16; break;
        case bus::ADDR_DMA3CNT_L: dma_ch[3].count = data; break;
        case bus::ADDR_DMA3CNT_H: dma_ch[3].WriteControl(data); break;
        }
    };

    auto WriteWord = [&](u32 addr, u32 data) {
        switch (addr) {
        case bus::ADDR_DMA0SAD: dma_ch[0].src_addr = data; break;
        case bus::ADDR_DMA0DAD: dma_ch[0].dst_addr = data; break;
        case bus::ADDR_DMA1SAD: dma_ch[1].src_addr = data; break;
        case bus::ADDR_DMA1DAD: dma_ch[1].dst_addr = data; break;
        case bus::ADDR_DMA2SAD: dma_ch[2].src_addr = data; break;
        case bus::ADDR_DMA2DAD: dma_ch[2].dst_addr = data; break;
        case bus::ADDR_DMA3SAD: dma_ch[3].src_addr = data; break;
        case bus::ADDR_DMA3DAD: dma_ch[3].dst_addr = data; break;
        default: {
            WriteHalf(addr, data & 0xFFFF);
            WriteHalf(addr + 2, data >> 16 & 0xFFFF);
        }
        }
    };

    if constexpr (sizeof(Int) == 1) {
        WriteByte(addr, data);
    }
    if constexpr (sizeof(Int) == 2) {
        WriteHalf(addr, data);
    }
    if constexpr (sizeof(Int) == 4) {
        WriteWord(addr, data);
    }
}

void DmaChannel::OnDmaDisable()
{
    NotifyDmaInactive();
}

void DmaChannel::OnDmaEnable()
{
    next_copy_is_repeat = suspended = false;
    ReloadCount();
    ReloadDstAddr();
    ReloadSrcAddr();
    if (control.start_timing == 0) {
        NotifyDmaActive();
    }
}

void DmaChannel::NotifyDmaActive() const
{
    scheduler::EngageDriver(driver_type, perform_dma_func, suspend_dma_func);
}

void DmaChannel::NotifyDmaInactive() const
{
    scheduler::DisengageDriver(driver_type);
}

void DmaChannel::ReloadCount()
{
    if (count == 0) {
        current_count = index == 3 ? 0x10000 : 0x4000;
    } else {
        current_count = count;
    }
}

void DmaChannel::ReloadDstAddr()
{
    current_dst_addr = dst_addr;
}

void DmaChannel::ReloadSrcAddr()
{
    current_src_addr = src_addr;
}

void DmaChannel::WriteControlLo(u8 data)
{
    set_byte(control, 0, data);
}

void DmaChannel::WriteControlHi(u8 data)
{
    Control prev_control = control;
    set_byte(control, 1, data);
    if (control.enable ^ prev_control.enable) {
        control.enable ? OnDmaEnable() : OnDmaDisable();
    }
}

void DmaChannel::WriteControl(u16 data)
{
    Control prev_control = control;
    control = std::bit_cast<Control>(data);
    if (control.enable ^ prev_control.enable) {
        control.enable ? OnDmaEnable() : OnDmaDisable();
    }
}

template u8 ReadReg<u8>(u32);
template s8 ReadReg<s8>(u32);
template u16 ReadReg<u16>(u32);
template s16 ReadReg<s16>(u32);
template u32 ReadReg<u32>(u32);
template s32 ReadReg<s32>(u32);
template void WriteReg<u8>(u32, u8);
template void WriteReg<s8>(u32, s8);
template void WriteReg<u16>(u32, u16);
template void WriteReg<s16>(u32, s16);
template void WriteReg<u32>(u32, u32);
template void WriteReg<s32>(u32, s32);

} // namespace gba::dma

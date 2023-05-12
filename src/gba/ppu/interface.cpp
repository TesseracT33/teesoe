#include "../bus.hpp"
#include "ppu.hpp"
#include "util.hpp"

namespace gba::ppu {

template<std::integral Int> Int ReadOam(u32 addr)
{
    if (dispcnt.forced_blank || in_vblank || in_hblank && dispcnt.hblank_interval_free) {
        Int ret;
        std::memcpy(&ret, oam.data() + (addr & 0x3FF), sizeof(Int));
        return ret;
    } else {
        return Int(-1);
    }
}

template<std::integral Int> Int ReadPaletteRam(u32 addr)
{
    if (dispcnt.forced_blank || in_vblank || in_hblank) {
        Int ret;
        std::memcpy(&ret, palette_ram.data() + (addr & 0x3FF), sizeof(Int));
        return ret;
    } else {
        return Int(-1);
    }
}

template<std::integral Int> Int ReadReg(u32 addr)
{
    auto ReadByte = [](u32 addr) {
        switch (addr) {
        case bus::ADDR_DISPCNT: return get_byte(dispcnt, 0);
        case bus::ADDR_DISPCNT + 1: return get_byte(dispcnt, 1);
        case bus::ADDR_GREEN_SWAP: return u8(green_swap);
        case bus::ADDR_GREEN_SWAP + 1: return u8(0);
        case bus::ADDR_DISPSTAT: return get_byte(dispstat, 0);
        case bus::ADDR_DISPSTAT + 1: return get_byte(dispstat, 1);
        case bus::ADDR_VCOUNT: return v_counter;
        case bus::ADDR_VCOUNT + 1: return u8(0);
        case bus::ADDR_BG0CNT: return get_byte(bgcnt[0], 0);
        case bus::ADDR_BG0CNT + 1: return get_byte(bgcnt[0], 1);
        case bus::ADDR_BG1CNT: return get_byte(bgcnt[1], 0);
        case bus::ADDR_BG1CNT + 1: return get_byte(bgcnt[1], 1);
        case bus::ADDR_BG2CNT: return get_byte(bgcnt[2], 0);
        case bus::ADDR_BG2CNT + 1: return get_byte(bgcnt[2], 1);
        case bus::ADDR_BG3CNT: return get_byte(bgcnt[3], 0);
        case bus::ADDR_BG3CNT + 1: return get_byte(bgcnt[3], 1);
        case bus::ADDR_WININ: return get_byte(winin, 0);
        case bus::ADDR_WININ + 1: return get_byte(winin, 1);
        case bus::ADDR_WINOUT: return get_byte(winout, 0);
        case bus::ADDR_WINOUT + 1: return get_byte(winout, 1);
        case bus::ADDR_BLDCNT: return get_byte(bldcnt, 0);
        case bus::ADDR_BLDCNT + 1: return get_byte(bldcnt, 1);
        case bus::ADDR_BLDALPHA: return eva;
        case bus::ADDR_BLDALPHA + 1: return evb;
        default: return bus::ReadOpenBus<u8>(addr);
        }
    };

    auto ReadHalf = [](u32 addr) {
        switch (addr) {
        case bus::ADDR_DISPCNT: return std::bit_cast<u16>(dispcnt);
        case bus::ADDR_GREEN_SWAP: return u16(green_swap);
        case bus::ADDR_DISPSTAT: return std::bit_cast<u16>(dispstat);
        case bus::ADDR_VCOUNT: return u16(v_counter);
        case bus::ADDR_BG0CNT: return std::bit_cast<u16>(bgcnt[0]);
        case bus::ADDR_BG1CNT: return std::bit_cast<u16>(bgcnt[1]);
        case bus::ADDR_BG2CNT: return std::bit_cast<u16>(bgcnt[2]);
        case bus::ADDR_BG3CNT: return std::bit_cast<u16>(bgcnt[3]);
        case bus::ADDR_WININ: return std::bit_cast<u16>(winin);
        case bus::ADDR_WINOUT: return std::bit_cast<u16>(winout);
        case bus::ADDR_BLDCNT: return std::bit_cast<u16>(bldcnt);
        case bus::ADDR_BLDALPHA: return u16(eva | evb << 8);
        default: return bus::ReadOpenBus<u16>(addr);
        }
    };

    if constexpr (sizeof(Int) == 1) {
        return ReadByte(addr);
    }
    if constexpr (sizeof(Int) == 2) {
        return ReadHalf(addr);
    }
    if constexpr (sizeof(Int) == 4) {
        u16 lo = ReadHalf(addr);
        u16 hi = ReadHalf(addr + 2);
        return lo | hi << 16;
    }
}

template<std::integral Int> Int ReadVram(u32 addr)
{
    if (dispcnt.forced_blank || in_vblank || in_hblank) {
        Int ret;
        std::memcpy(&ret, vram.data() + (addr % 0x18000), sizeof(Int));
        return ret;
    } else {
        return Int(-1);
    }
}

template<std::integral Int> void WriteOam(u32 addr, Int data)
{
    std::memcpy(oam.data() + (addr & 0x3FF), &data, sizeof(Int));
    // if (dispcnt.forced_blank || in_vblank || in_hblank && dispcnt.hblank_interval_free) {
    //	std::memcpy(oam.data() + (addr & 0x3FF), &data, sizeof(Int));
    // }
}

template<std::integral Int> void WritePaletteRam(u32 addr, Int data)
{
    std::memcpy(palette_ram.data() + (addr & 0x3FF), &data, sizeof(Int));
    // if (dispcnt.forced_blank || in_vblank || in_hblank) {
    //	std::memcpy(palette_ram.data() + (addr & 0x3FF), &data, sizeof(Int));
    // }
}

template<std::integral Int> void WriteReg(u32 addr, Int data)
{
    auto WriteByte = [](u32 addr, u8 data) {
        switch (addr) {
        case bus::ADDR_DISPCNT: set_byte(dispcnt, 0, data); break;
        case bus::ADDR_DISPCNT + 1: set_byte(dispcnt, 1, data); break;
        case bus::ADDR_GREEN_SWAP: green_swap = data & 1; break;
        case bus::ADDR_DISPSTAT: set_byte(dispstat, 0, data); break;
        case bus::ADDR_DISPSTAT + 1: set_byte(dispstat, 1, data); break;
        case bus::ADDR_BG0CNT:
            set_byte(bgcnt[0], 0, data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG0CNT + 1: set_byte(bgcnt[0], 1, data); break;
        case bus::ADDR_BG1CNT:
            set_byte(bgcnt[1], 0, data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG1CNT + 1: set_byte(bgcnt[1], 1, data); break;
        case bus::ADDR_BG2CNT:
            set_byte(bgcnt[2], 0, data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG2CNT + 1: set_byte(bgcnt[2], 1, data); break;
        case bus::ADDR_BG3CNT:
            set_byte(bgcnt[3], 0, data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG3CNT + 1: set_byte(bgcnt[3], 1, data); break;
        case bus::ADDR_BG0HOFS: set_byte(bghofs[0], 0, data); break;
        case bus::ADDR_BG0HOFS + 1: set_byte(bghofs[0], 1, data & 1); break;
        case bus::ADDR_BG0VOFS: set_byte(bgvofs[0], 0, data); break;
        case bus::ADDR_BG0VOFS + 1: set_byte(bgvofs[0], 1, data & 1); break;
        case bus::ADDR_BG1HOFS: set_byte(bghofs[1], 0, data); break;
        case bus::ADDR_BG1HOFS + 1: set_byte(bghofs[1], 1, data & 1); break;
        case bus::ADDR_BG1VOFS: set_byte(bgvofs[1], 0, data); break;
        case bus::ADDR_BG1VOFS + 1: set_byte(bgvofs[1], 1, data & 1); break;
        case bus::ADDR_BG2HOFS: set_byte(bghofs[2], 0, data); break;
        case bus::ADDR_BG2HOFS + 1: set_byte(bghofs[2], 1, data & 1); break;
        case bus::ADDR_BG2VOFS: set_byte(bgvofs[2], 0, data); break;
        case bus::ADDR_BG2VOFS + 1: set_byte(bgvofs[2], 1, data & 1); break;
        case bus::ADDR_BG3HOFS: set_byte(bghofs[3], 0, data); break;
        case bus::ADDR_BG3HOFS + 1: set_byte(bghofs[3], 1, data & 1); break;
        case bus::ADDR_BG3VOFS: set_byte(bgvofs[3], 0, data); break;
        case bus::ADDR_BG3VOFS + 1: set_byte(bgvofs[3], 1, data & 1); break;
        case bus::ADDR_BG2PA: set_byte(bgpa[0], 0, data); break;
        case bus::ADDR_BG2PA + 1: set_byte(bgpa[0], 1, data); break;
        case bus::ADDR_BG2PB: set_byte(bgpb[0], 0, data); break;
        case bus::ADDR_BG2PB + 1: set_byte(bgpb[0], 1, data); break;
        case bus::ADDR_BG2PC: set_byte(bgpc[0], 0, data); break;
        case bus::ADDR_BG2PC + 1: set_byte(bgpc[0], 1, data); break;
        case bus::ADDR_BG2PD: set_byte(bgpd[0], 0, data); break;
        case bus::ADDR_BG2PD + 1: set_byte(bgpd[0], 1, data); break;
        case bus::ADDR_BG2X: set_byte(bgx[0], 0, data); break;
        case bus::ADDR_BG2X + 1: set_byte(bgx[0], 1, data); break;
        case bus::ADDR_BG2X + 2: set_byte(bgx[0], 2, data); break;
        case bus::ADDR_BG2X + 3: set_byte(bgx[0], 3, data); break;
        case bus::ADDR_BG2Y: set_byte(bgy[0], 0, data); break;
        case bus::ADDR_BG2Y + 1: set_byte(bgy[0], 1, data); break;
        case bus::ADDR_BG2Y + 2: set_byte(bgy[0], 2, data); break;
        case bus::ADDR_BG2Y + 3: set_byte(bgy[0], 3, data); break;
        case bus::ADDR_BG3PA: set_byte(bgpa[1], 0, data); break;
        case bus::ADDR_BG3PA + 1: set_byte(bgpa[1], 1, data); break;
        case bus::ADDR_BG3PB: set_byte(bgpb[1], 0, data); break;
        case bus::ADDR_BG3PB + 1: set_byte(bgpb[1], 1, data); break;
        case bus::ADDR_BG3PC: set_byte(bgpc[1], 0, data); break;
        case bus::ADDR_BG3PC + 1: set_byte(bgpc[1], 1, data); break;
        case bus::ADDR_BG3PD: set_byte(bgpd[1], 0, data); break;
        case bus::ADDR_BG3PD + 1: set_byte(bgpd[1], 1, data); break;
        case bus::ADDR_BG3X: set_byte(bgx[1], 0, data); break;
        case bus::ADDR_BG3X + 1: set_byte(bgx[1], 1, data); break;
        case bus::ADDR_BG3X + 2: set_byte(bgx[1], 2, data); break;
        case bus::ADDR_BG3X + 3: set_byte(bgx[1], 3, data); break;
        case bus::ADDR_BG3Y: set_byte(bgy[1], 0, data); break;
        case bus::ADDR_BG3Y + 1: set_byte(bgy[1], 1, data); break;
        case bus::ADDR_BG3Y + 2: set_byte(bgy[1], 2, data); break;
        case bus::ADDR_BG3Y + 3: set_byte(bgy[1], 3, data); break;
        case bus::ADDR_WIN0H: winh_x2[0] = data; break;
        case bus::ADDR_WIN0H + 1: winh_x1[0] = data; break;
        case bus::ADDR_WIN1H: winh_x2[1] = data; break;
        case bus::ADDR_WIN1H + 1: winh_x1[1] = data; break;
        case bus::ADDR_WIN0V: winv_y2[0] = data; break;
        case bus::ADDR_WIN0V + 1: winv_y1[0] = data; break;
        case bus::ADDR_WIN1V: winv_y2[1] = data; break;
        case bus::ADDR_WIN1V + 1: winv_y1[1] = data; break;
        case bus::ADDR_WININ: set_byte(winin, 0, data); break;
        case bus::ADDR_WININ + 1: set_byte(winin, 1, data); break;
        case bus::ADDR_WINOUT: set_byte(winout, 0, data); break;
        case bus::ADDR_WINOUT + 1: set_byte(winout, 1, data); break;
        case bus::ADDR_MOSAIC: set_byte(mosaic, 0, data); break;
        case bus::ADDR_MOSAIC + 1: set_byte(mosaic, 1, data); break;
        case bus::ADDR_BLDCNT: set_byte(bldcnt, 0, data); break;
        case bus::ADDR_BLDCNT + 1: set_byte(bldcnt, 1, data); break;
        case bus::ADDR_BLDALPHA: eva = data & 0x1F; break;
        case bus::ADDR_BLDALPHA + 1: evb = data & 0x1F; break;
        case bus::ADDR_BLDY: evy = data & 0x1F; break;
        }
    };

    auto WriteHalf = [](u32 addr, u16 data) {
        switch (addr) {
        case bus::ADDR_DISPCNT: dispcnt = std::bit_cast<DISPCNT>(data); break;
        case bus::ADDR_GREEN_SWAP: green_swap = data & 1; break;
        case bus::ADDR_DISPSTAT: dispstat = std::bit_cast<DISPSTAT>(data); break;
        case bus::ADDR_BG0CNT:
            bgcnt[0] = std::bit_cast<BGCNT>(data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG1CNT:
            bgcnt[1] = std::bit_cast<BGCNT>(data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG2CNT:
            bgcnt[2] = std::bit_cast<BGCNT>(data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG3CNT:
            bgcnt[3] = std::bit_cast<BGCNT>(data);
            SortBackgroundsAfterPriority();
            break;
        case bus::ADDR_BG0HOFS: bghofs[0] = data & 0x1FF; break;
        case bus::ADDR_BG0VOFS: bgvofs[0] = data & 0x1FF; break;
        case bus::ADDR_BG1HOFS: bghofs[1] = data & 0x1FF; break;
        case bus::ADDR_BG1VOFS: bgvofs[1] = data & 0x1FF; break;
        case bus::ADDR_BG2HOFS: bghofs[2] = data & 0x1FF; break;
        case bus::ADDR_BG2VOFS: bgvofs[2] = data & 0x1FF; break;
        case bus::ADDR_BG3HOFS: bghofs[3] = data & 0x1FF; break;
        case bus::ADDR_BG3VOFS: bgvofs[3] = data & 0x1FF; break;
        case bus::ADDR_BG2PA: bgpa[0] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG2PB: bgpb[0] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG2PC: bgpc[0] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG2PD: bgpd[0] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG2X:
            set_byte(bgx[0], 0, data & 0xFF);
            set_byte(bgx[0], 1, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG2X + 2:
            set_byte(bgx[0], 2, data & 0xFF);
            set_byte(bgx[0], 3, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG2Y:
            set_byte(bgy[0], 0, data & 0xFF);
            set_byte(bgy[0], 1, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG2Y + 2:
            set_byte(bgy[0], 2, data & 0xFF);
            set_byte(bgy[0], 3, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG3PA: bgpa[1] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG3PB: bgpb[1] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG3PC: bgpc[1] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG3PD: bgpd[1] = std::bit_cast<BGP>(data); break;
        case bus::ADDR_BG3X:
            set_byte(bgx[1], 0, data & 0xFF);
            set_byte(bgx[1], 1, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG3X + 2:
            set_byte(bgx[1], 2, data & 0xFF);
            set_byte(bgx[1], 3, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG3Y:
            set_byte(bgy[1], 0, data & 0xFF);
            set_byte(bgy[1], 1, data >> 8 & 0xFF);
            break;
        case bus::ADDR_BG3Y + 2:
            set_byte(bgy[1], 2, data & 0xFF);
            set_byte(bgy[1], 3, data >> 8 & 0xFF);
            break;
        case bus::ADDR_WIN0H:
            winh_x2[0] = data & 0xFF;
            winh_x1[0] = data >> 8 & 0xFF;
            break;
        case bus::ADDR_WIN1H:
            winh_x2[1] = data & 0xFF;
            winh_x1[1] = data >> 8 & 0xFF;
            break;
        case bus::ADDR_WIN0V:
            winv_y2[0] = data & 0xFF;
            winv_y1[0] = data >> 8 & 0xFF;
            break;
        case bus::ADDR_WIN1V:
            winv_y2[1] = data & 0xFF;
            winv_y1[1] = data >> 8 & 0xFF;
            break;
        case bus::ADDR_WININ: winin = std::bit_cast<WININ>(data); break;
        case bus::ADDR_WINOUT: winout = std::bit_cast<WINOUT>(data); break;
        case bus::ADDR_MOSAIC: mosaic = std::bit_cast<MOSAIC>(data); break;
        case bus::ADDR_BLDCNT: bldcnt = std::bit_cast<BLDCNT>(data); break;
        case bus::ADDR_BLDALPHA:
            eva = data & 0x1F;
            evb = data >> 8 & 0x1F;
            break;
        case bus::ADDR_BLDY: evy = data & 0x1F; break;
        }
    };

    if constexpr (sizeof(Int) == 1) {
        WriteByte(addr, data);
    }
    if constexpr (sizeof(Int) == 2) {
        WriteHalf(addr, data);
    }
    if constexpr (sizeof(Int) == 4) {
        WriteHalf(addr, data & 0xFFFF);
        WriteHalf(addr + 2, data >> 16 & 0xFFFF);
    }
}

template<std::integral Int> void WriteVram(u32 addr, Int data)
{
    std::memcpy(vram.data() + (addr % 0x18000), &data, sizeof(Int));
    // if (dispcnt.forced_blank || in_vblank || in_hblank) {
    //	std::memcpy(vram.data() + (addr % 0x18000), &data, sizeof(Int));
    // }
}

template u8 ReadOam<u8>(u32);
template s8 ReadOam<s8>(u32);
template u16 ReadOam<u16>(u32);
template s16 ReadOam<s16>(u32);
template u32 ReadOam<u32>(u32);
template s32 ReadOam<s32>(u32);
template void WriteOam<u8>(u32, u8);
template void WriteOam<s8>(u32, s8);
template void WriteOam<u16>(u32, u16);
template void WriteOam<s16>(u32, s16);
template void WriteOam<u32>(u32, u32);
template void WriteOam<s32>(u32, s32);

template u8 ReadPaletteRam<u8>(u32);
template s8 ReadPaletteRam<s8>(u32);
template u16 ReadPaletteRam<u16>(u32);
template s16 ReadPaletteRam<s16>(u32);
template u32 ReadPaletteRam<u32>(u32);
template s32 ReadPaletteRam<s32>(u32);
template void WritePaletteRam<u8>(u32, u8);
template void WritePaletteRam<s8>(u32, s8);
template void WritePaletteRam<u16>(u32, u16);
template void WritePaletteRam<s16>(u32, s16);
template void WritePaletteRam<u32>(u32, u32);
template void WritePaletteRam<s32>(u32, s32);

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

template u8 ReadVram<u8>(u32);
template s8 ReadVram<s8>(u32);
template u16 ReadVram<u16>(u32);
template s16 ReadVram<s16>(u32);
template u32 ReadVram<u32>(u32);
template s32 ReadVram<s32>(u32);
template void WriteVram<u8>(u32, u8);
template void WriteVram<s8>(u32, s8);
template void WriteVram<u16>(u32, u16);
template void WriteVram<s16>(u32, s16);
template void WriteVram<u32>(u32, u32);
template void WriteVram<s32>(u32, s32);

} // namespace gba::ppu

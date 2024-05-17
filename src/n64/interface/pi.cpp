#include "pi.hpp"
#include "log.hpp"
#include "memory/cart.hpp"
#include "memory/rdram.hpp"
#include "mi.hpp"
#include "n64_build_options.hpp"
#include "scheduler.hpp"
#include "vr4300/recompiler.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

namespace n64::pi {

enum class DmaType {
    CartToRdram,
    RdramToCart
};

enum Register {
    DramAddr,
    CartAddr,
    RdLen,
    WrLen,
    Status,
    BsdDom1Lat,
    BsdDom1Pwd,
    BsdDom1Pgs,
    BsdDom1Rls,
    BsdDom2Lat,
    BsdDom2Pwd,
    BsdDom2Pgs,
    BsdDom2Rls
};

static u32 latch;
static u8* write_dst;

struct {
    u32 dram_addr, cart_addr, rd_len, wr_len;
    struct {
        u32 dma_busy      : 1;
        u32 io_busy       : 1;
        u32 dma_error     : 1;
        u32 dma_completed : 1;
        u32               : 28;
    } status;
    u32 bsd_dom1_lat, bsd_dom1_pwd, bsd_dom1_pgs, bsd_dom1_rls;
    u32 bsd_dom2_lat, bsd_dom2_pwd, bsd_dom2_pgs, bsd_dom2_rls;
    u32 dummy0, dummy1, dummy2;
} static pi;

static u32 cart_addr_end, dram_addr_end;

template<DmaType> static void InitDma(DmaType type);
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

template<DmaType type> void InitDma()
{
    pi.status.dma_busy = 1;
    pi.status.dma_completed = 0;
    u32 dma_len = pi.wr_len + 1;
    if constexpr (type == DmaType::CartToRdram) {
        if constexpr (log_dma) {
            Log(
              std::format("DMA: from cart ROM ${:X} to RDRAM ${:X}: ${:X} bytes", pi.cart_addr, pi.dram_addr, dma_len));
        }

        u32 cart_addr = pi.cart_addr;
        u32 dram_addr = pi.dram_addr;

        if (!(cart_addr & 7) && !(dram_addr & 7) && !(dma_len & 3)) { // simple case
            for (u32 i = 0; i < dma_len; i += 4) {
                rdram::Write<4>(dram_addr, cart::ReadRom<s32>(cart_addr));
                cart_addr += 4;
                dram_addr += 4;
            }
        } else {
            cart_addr &= ~1;
            dram_addr &= ~1;
            u32 const offset = pi.dram_addr & 7;
            static constexpr u32 block_size = 128;
            assert(dma_len >= offset); // TODO: what if dma_len < offset?
            u32 num_bytes_first_block = std::min(dma_len, block_size) - offset;
            for (u32 i = 0; i < num_bytes_first_block; ++i) {
                rdram::Write<1>(dram_addr++, cart::ReadDma(cart_addr++));
            }
            dma_len -= std::min(dma_len, block_size);
            if (dma_len) {
                dram_addr += offset;
                for (u32 i = 0; i < dma_len; ++i) {
                    rdram::Write<1>(dram_addr++, cart::ReadDma(cart_addr++));
                }
            }
        }
        cart_addr_end = cart_addr;
        dram_addr_end = dram_addr & 0xFF'FFFF;
        vr4300::InvalidateRange(pi.dram_addr, dram_addr_end);
    } else { /* RDRAM to cart */
        /* TODO: it seems we can write to SRAM/FLASH. */
        LogWarn("Attempted DMA from RDRAM to Cart, but this is unimplemented.");
        OnDmaFinish();
        return;
    }

    static constexpr auto cycles_per_byte_dma = 9;
    auto cycles_until_finish = dma_len * cycles_per_byte_dma;
    scheduler::AddEvent(scheduler::EventType::PiDmaFinish, cycles_until_finish, OnDmaFinish);
}

void Initialize()
{
    latch = {};
    pi = {};
}

std::optional<u32> IoBusy()
{
    // TODO: this is probably not how it works? Or will the read block if io_busy?
    if (pi.status.io_busy) {
        pi.status.io_busy = 0;
        return latch;
    }
    return {};
}

void OnDmaFinish()
{
    pi.status.dma_busy = 0;
    pi.status.dma_completed = 1;
    mi::RaiseInterrupt(mi::InterruptType::PI);
    pi.cart_addr = cart_addr_end;
    pi.dram_addr = dram_addr_end;
}

u32 ReadReg(u32 addr)
{
    static_assert(sizeof(pi) >> 2 == 0x10);
    u32 offset = addr >> 2 & 0xF;
    u32 ret;
    if ((offset & 0xE) == 2) { // PI_RD_LEN, PI_WR_LEN
        ret = 0x7F;
    } else {
        std::memcpy(&ret, (u32*)(&pi) + offset, 4);
    }
    if constexpr (log_io_pi) {
        Log(std::format("PI: {} => ${:08X}", RegOffsetToStr(offset), ret));
    }
    return ret;
}

constexpr std::string_view RegOffsetToStr(u32 reg_offset)
{
    switch (reg_offset) {
    case DramAddr: return "PI_DRAM_ADDR";
    case CartAddr: return "PI_CART_ADDR";
    case RdLen: return "PI_RDLEN";
    case WrLen: return "PI_WRLEN";
    case Status: return "PI_STATUS";
    case BsdDom1Lat: return "PI_BSDDOM1LAT";
    case BsdDom1Pwd: return "PI_BSDDOM1PWD";
    case BsdDom1Pgs: return "PI_BSDDOM1PGS";
    case BsdDom1Rls: return "PI_BSDDOM1RLS";
    case BsdDom2Lat: return "PI_BSDDOM2LAT";
    case BsdDom2Pwd: return "PI_BSDDOM2PWD";
    case BsdDom2Pgs: return "PI_BSDDOM2PGS";
    case BsdDom2Rls: return "PI_BSDDOM2RLS";
    default: return "PI_UNKNOWN";
    }
}

template<size_t size> void Write(u32 addr, s64 value, u8* dst)
{
    if (pi.status.io_busy) return;

    if constexpr (size == 1) latch = u32(value << 8 * (3 - (addr & 3)));
    if constexpr (size == 2) latch = u32(value << 8 * (2 - (addr & 2)));
    if constexpr (size == 4) latch = u32(value);
    if constexpr (size == 8) latch = u32(value >> 32);
    addr &= ~1;
    pi.status.io_busy = 1;
    write_dst = dst;
    scheduler::AddEvent(scheduler::EventType::PiWriteFinish, 50, [] { // TODO: how many cycles?
        pi.status.io_busy = 0;
        latch = std::byteswap(latch); // TODO: assuming only SRAM is the only effectful write and that it is in BE
        if (write_dst) std::memcpy(write_dst, &latch, 4);
    });
}

void WriteReg(u32 addr, u32 data)
{
    static_assert(sizeof(pi) >> 2 == 0x10);
    u32 offset = addr >> 2 & 0xF;
    if constexpr (log_io_pi) {
        Log(std::format("PI: {} <= ${:08X}", RegOffsetToStr(offset), data));
    }

    switch (offset) {
    case Register::DramAddr: pi.dram_addr = data & 0xFF'FFFE; break;

    case Register::CartAddr: pi.cart_addr = data; break;

    case Register::RdLen:
        pi.rd_len = data;
        InitDma<DmaType::RdramToCart>();
        break;

    case Register::WrLen:
        pi.wr_len = data;
        InitDma<DmaType::CartToRdram>();
        break;

    case Register::Status: {
        static constexpr s32 reset_dma_mask = 0x01;
        static constexpr s32 clear_interrupt_mask = 0x02;
        if (data & reset_dma_mask) {
            /* Reset the DMA controller and stop any transfer being done */
            pi.status = {};
            mi::ClearInterrupt(mi::InterruptType::PI); /* TODO: correct? */
            scheduler::RemoveEvent(scheduler::EventType::PiDmaFinish);
        }
        if (data & clear_interrupt_mask) {
            /* Clear Interrupt */
            pi.status.dma_completed = 0;
            mi::ClearInterrupt(mi::InterruptType::PI);
        }
    } break;

    case Register::BsdDom1Lat: pi.bsd_dom1_lat = data; break;
    case Register::BsdDom1Pwd: pi.bsd_dom1_pwd = data; break;
    case Register::BsdDom1Pgs: pi.bsd_dom1_pgs = data; break;
    case Register::BsdDom1Rls: pi.bsd_dom1_rls = data; break;
    case Register::BsdDom2Lat: pi.bsd_dom2_lat = data; break;
    case Register::BsdDom2Pwd: pi.bsd_dom2_pwd = data; break;
    case Register::BsdDom2Pgs: pi.bsd_dom2_pgs = data; break;
    case Register::BsdDom2Rls: pi.bsd_dom2_rls = data; break;

    default: LogWarn(std::format("Unexpected write made to PI register at address ${:08X}", addr));
    }
}

template void Write<1>(u32, s64, u8*);
template void Write<2>(u32, s64, u8*);
template void Write<4>(u32, s64, u8*);
template void Write<8>(u32, s64, u8*);

} // namespace n64::pi

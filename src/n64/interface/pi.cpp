#include "pi.hpp"
#include "log.hpp"
#include "memory/cart.hpp"
#include "memory/rdram.hpp"
#include "mi.hpp"
#include "n64_build_options.hpp"
#include "scheduler.hpp"

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

static size_t dma_len;

template<DmaType> static void InitDma(DmaType type);
static void OnDmaFinish();
constexpr std::string_view RegOffsetToStr(u32 reg_offset);

template<DmaType type> void InitDma()
{
    pi.status.dma_busy = 1;
    pi.status.dma_completed = 0;

    u8* rdram_ptr = rdram::GetPointerToMemory(pi.dram_addr);
    u8* cart_ptr = cart::GetPointerToRom(pi.cart_addr);
    size_t bytes_until_rdram_end = rdram::GetNumberOfBytesUntilMemoryEnd(pi.dram_addr);
    size_t bytes_until_cart_end = cart::GetNumberOfBytesUntilRomEnd(pi.cart_addr);
    dma_len = std::min(bytes_until_rdram_end, bytes_until_cart_end);
    if constexpr (type == DmaType::CartToRdram) {
        dma_len = std::min(dma_len, size_t(pi.wr_len + 1));
        // TODO: temporary workaround to make nm64 work. If cart and rdram are both in LE, making a simple dma to rdram
        // does not work if either addr is not work-aligned. E.g.: given cart addr 0x0032e6a6 and aligned rdram:
        // - cart BE, rdram BE
        //     cart 0x0032e6a4 : ffff0000 00120000 00100028
        //     rdram                 0000 00120000 00100028
        // - cart LE, rdram LE
        //     cart 0x0032e6a4 : 0000ffff 00001200 28001000
        //     rdram           :     ffff 00001200 28001000
        // The two first two bytes copied are incorrect. Temporarily, make cart BE and use the below..
        assert(!(dma_len & 3));
        assert(!(pi.dram_addr & 3));
        for (size_t i = 0; i < dma_len; i += 4) {
            u32 val;
            std::memcpy(&val, cart_ptr + i, 4);
            val = std::byteswap(val);
            std::memcpy(rdram_ptr + i, &val, 4);
        }
        //// See https://n64brew.dev/wiki/Peripheral_Interface#Unaligned_DMA_transfer for behaviour when addr is
        /// unaligned
        // static constexpr size_t block_size = 128;
        // size_t num_bytes_first_block = block_size - (pi.dram_addr & (block_size - 1));
        // if (num_bytes_first_block > (pi.dram_addr & 7)) {
        //     std::memcpy(rdram_ptr, cart_ptr, std::min(dma_len, num_bytes_first_block - (pi.dram_addr & 7)));
        // }
        // if (dma_len > num_bytes_first_block) {
        //     std::memcpy(rdram_ptr + num_bytes_first_block,
        //       cart_ptr + num_bytes_first_block,
        //       dma_len - num_bytes_first_block);
        // }
        if constexpr (log_dma) {
            log(
              std::format("DMA: from cart ROM ${:X} to RDRAM ${:X}: ${:X} bytes", pi.cart_addr, pi.dram_addr, dma_len));
        }
    } else { /* RDRAM to cart */
        /* TODO: when I wrote this code, I forgot we can't write to ROM. But it seems we can write to SRAM/FLASH.
            I do not yet know the behavior */
        // dma_len = std::min(dma_len, size_t(pi.rd_len + 1));
        // std::memcpy(cart_ptr, rdram_ptr, dma_len);
        // if constexpr (log_dma) {
        //	LogDMA(std::format("From RDRAM ${:X} to cart ROM ${:X}: ${:X} bytes",
        //		pi.dram_addr, pi.cart_addr, dma_len));
        // }
        log_warn("Attempted DMA from RDRAM to Cart, but this is unimplemented.");
        OnDmaFinish();
        return;
    }

    static constexpr auto cycles_per_byte_dma = 18;
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
    pi.dram_addr = (pi.dram_addr + dma_len) & 0xFF'FFFF;
    pi.cart_addr = (pi.cart_addr + dma_len) & 0xFF'FFFF;
}

u32 ReadReg(u32 addr)
{
    static_assert(sizeof(pi) >> 2 == 0x10);
    u32 offset = addr >> 2 & 0xF;
    u32 ret;
    std::memcpy(&ret, (u32*)(&pi) + offset, 4);
    if constexpr (log_io_pi) {
        log(std::format("PI: {} => ${:08X}", RegOffsetToStr(offset), ret));
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
    if constexpr (size == 4) latch = value;
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
        log(std::format("PI: {} <= ${:08X}", RegOffsetToStr(offset), data));
    }

    switch (offset) {
    case Register::DramAddr: pi.dram_addr = data & 0xFF'FFFF; break;

    case Register::CartAddr: pi.cart_addr = data & 0xFF'FFFF; break;

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

    default: log_warn(std::format("Unexpected write made to PI register at address ${:08X}", addr));
    }
}

template void Write<1>(u32, s64, u8*);
template void Write<2>(u32, s64, u8*);
template void Write<4>(u32, s64, u8*);
template void Write<8>(u32, s64, u8*);

} // namespace n64::pi

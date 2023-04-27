#include "cache.hpp"
#include "cop0.hpp"
#include "exceptions.hpp"
#include "log.hpp"
#include "memory/memory.hpp"
#include "memory/rdram.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
#include "util.hpp"
#include "vr4300.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstring>
#include <format>

namespace n64::vr4300 {

struct DCacheLine {
    u8 data[16];
    u32 ptag;
    bool valid;
    bool dirty;
};

struct ICacheLine {
    u8 data[32];
    u32 ptag;
    bool valid;
};

static std::array<DCacheLine, 512> d_cache; /* 8 KB */
static std::array<ICacheLine, 512> i_cache; /* 16 KB */

static u8* rdram_ptr;

static void FillCacheLine(auto& cache_line, u32 paddr);
static void WritebackCacheLine(auto& cache_line, u32 new_paddr);

void cache(u32 rs, u32 rt, s16 imm)
{
    /* Cache op;
       Sign-extends the 16-bit offset to 32 bits and adds it to register base to
       generate a virtual address. The virtual address is converted into a physical
       address by using the TLB, and a cache operation indicated by a 5-bit sub op
       code is executed to that address. */
    uint cycles = 1;
    /* The below makes everything crash and burn */
#if 0
		if (!cop0.status.cu0 || operating_mode != OperatingMode::Kernel) {
			SignalException<Exception::CoprocessorUnusable>();
			AdvancePipeline(cycles);
			return;
		}
#endif
    auto cache = rt & 3;
    auto op = rt >> 2;
    auto virt_addr = gpr[rs] + imm;
    bool cacheable_area;
    auto paddr = active_virtual_to_physical_fun_read(virt_addr,
      cacheable_area); /* may go unused below, but could also cause a TLB exception */
    if (exception_occurred) {
        AdvancePipeline(cycles);
        return;
    }

    auto HandleOp = [&](auto& cache_line) {
        static_assert(sizeof(DCacheLine) != sizeof(ICacheLine));
        static constexpr bool is_d_cache = sizeof(cache_line) == sizeof(DCacheLine);

        auto WriteBack = [&] {
            WritebackCacheLine(cache_line, paddr);
            cycles = 40;
        };
        /* TODO: in which situations do we abort if the cache line is tagged as invalid? */
        switch (op) { /* illegal 'op' and 'cache' combinations give UB */
        case 0: /* Index_Invalidate */
            if constexpr (is_d_cache) {
                if (cache_line.valid && cache_line.dirty) {
                    WriteBack();
                }
            }
            cache_line.valid = false;
            break;

        case 1: /* Index_Load_Tag */
            cop0.tag_lo.ptag = cache_line.ptag >> 12;
            cop0.tag_lo.pstate = cache_line.valid << 1;
            if constexpr (is_d_cache) {
                cop0.tag_lo.pstate |= u32(cache_line.dirty);
            }
            break;

        case 2: /* Index_Store_Tag */
            cache_line.ptag = cop0.tag_lo.ptag << 12;
            cache_line.valid = cop0.tag_lo.pstate >> 1;
            if constexpr (is_d_cache) {
                cache_line.dirty = cop0.tag_lo.pstate & 1;
            }
            break;

        case 3: /* Create_Dirty_Exclusive */
            if constexpr (is_d_cache) {
                if (cache_line.dirty && (paddr & ~0xFFF) != cache_line.ptag) {
                    WriteBack();
                }
                cache_line.dirty = cache_line.valid = true;
                cache_line.ptag = paddr & ~0xFFF;
            }
            break;

        case 4: /* Hit_Invalidate */
            if ((paddr & ~0xFFF) == cache_line.ptag) {
                cache_line.valid = false;
                if constexpr (is_d_cache) {
                    cache_line.dirty = false;
                }
            }
            break;

        case 5: /* Hit_Write_Back_Invalidate (D-Cache), Fill (I-Cache) */
            if constexpr (is_d_cache) {
                if ((paddr & ~0xFFF) == cache_line.ptag) {
                    if (cache_line.dirty) {
                        WriteBack();
                    }
                    cache_line.valid = false;
                }
            } else {
                FillCacheLine(cache_line, paddr);
                cycles = 40;
            }
            break;

        case 6: /* Hit_Write_Back */
            if constexpr (is_d_cache) {
                if (!cache_line.dirty) {
                    break;
                }
            }
            if ((paddr & ~0xFFF) == cache_line.ptag) {
                WriteBack();
            }
            break;

        case 7: break;

        default: std::unreachable();
        }
    };

    if (cache == 0) { /* I-Cache */
        ICacheLine& cache_line = i_cache[virt_addr >> 5 & 0x1FF];
        HandleOp(cache_line);
    } else if (cache == 1) { /* D-Cache */
        DCacheLine& cache_line = d_cache[virt_addr >> 4 & 0x1FF];
        HandleOp(cache_line);
    }

    AdvancePipeline(cycles);
}

void FillCacheLine(auto& cache_line, u32 paddr)
{
    /* TODO: For now, we are lazy and assume that only RDRAM is cached. Other regions are too;
    https://discord.com/channels/465585922579103744/600463718924681232/1034605516900544582 */
    auto rdram_offset = paddr & ~(sizeof(cache_line.data) - 1);
    if (rdram_offset >= rdram::GetSize()) {
        log_warn(std::format("Attempted to fill cache line from physical addr ${:08X} (beyond RDRAM)", rdram_offset));
    }
    std::memcpy(cache_line.data, rdram_ptr + rdram_offset, sizeof(cache_line.data));
    cache_line.ptag = paddr & ~0xFFF;
    cache_line.valid = true;
    if constexpr (sizeof(cache_line) == sizeof(DCacheLine)) {
        cache_line.dirty = false;
    }
}

void InitCache()
{
    rdram_ptr = rdram::GetPointerToMemory(0);
}

template<std::signed_integral Int, MemOp mem_op> Int ReadCacheableArea(u32 paddr)
{ /* Precondition: paddr is aligned to sizeof(Int) */
    static_assert(one_of(mem_op, MemOp::InstrFetch, MemOp::Read));
    if constexpr (log_cpu_instructions && mem_op == MemOp::InstrFetch) {
        last_instr_fetch_phys_addr = paddr;
    }
    auto ReadFromCacheLine = [paddr](auto const& cache_line) mutable {
        // RDRAM is stored in LE, word-wise
        if constexpr (sizeof(Int) == 1) paddr ^= 3;
        if constexpr (sizeof(Int) == 2) paddr ^= 2;
        u8 const* cache_src = cache_line.data + (paddr & (sizeof(cache_line.data) - 1));
        Int ret;
        if constexpr (sizeof(Int) <= 4) {
            std::memcpy(&ret, cache_src, sizeof(Int));
        } else {
            std::memcpy(&ret, cache_src + 4, 4);
            std::memcpy(reinterpret_cast<u8*>(&ret) + 4, cache_src, 4);
        }
        return ret;
    };
    if constexpr (mem_op == MemOp::InstrFetch) {
        ICacheLine& cache_line = i_cache[paddr >> 5 & 0x1FF];
        if (cache_line.valid && (paddr & ~0xFFF) == cache_line.ptag) { /* cache hit */
            p_cycle_counter += cache_hit_read_cycle_delay;
        } else { /* cache miss */
            FillCacheLine(cache_line, paddr);
            p_cycle_counter += cache_miss_cycle_delay;
        }
        return ReadFromCacheLine(cache_line);
    } else { /* MemOp::Read */
        DCacheLine& cache_line = d_cache[paddr >> 4 & 0x1FF];
        if (cache_line.valid && (paddr & ~0xFFF) == cache_line.ptag) { /* cache hit */
            p_cycle_counter += cache_hit_read_cycle_delay;
        } else { /* cache miss */
            if (cache_line.valid && cache_line.dirty) {
                WritebackCacheLine(cache_line, paddr);
            }
            FillCacheLine(cache_line, paddr);
            p_cycle_counter += cache_miss_cycle_delay;
        }
        return ReadFromCacheLine(cache_line);
    }
}

template<size_t access_size, typename... MaskT> void WriteCacheableArea(u32 paddr, s64 data, MaskT... mask)
{ /* Precondition: paddr is aligned to access_size if sizeof...(mask) == 0 */
    static_assert(std::has_single_bit(access_size) && access_size <= 8);
    static_assert(sizeof...(mask) <= 1);
    DCacheLine& cache_line = d_cache[paddr >> 4 & 0x1FF];
    if (cache_line.valid && (paddr & ~0xFFF) == cache_line.ptag) { /* cache hit */
        cache_line.dirty = true;
        p_cycle_counter += cache_hit_write_cycle_delay;
    } else { /* cache miss */
        if (cache_line.valid && cache_line.dirty) {
            WritebackCacheLine(cache_line, paddr);
        }
        FillCacheLine(cache_line, paddr);
        p_cycle_counter += cache_miss_cycle_delay;
    }
    static constexpr bool apply_mask = sizeof...(mask) == 1;
    if constexpr (apply_mask) {
        paddr &= ~(access_size - 1);
    }
    // RDRAM is stored in LE, word-wise
    if constexpr (access_size == 1) paddr ^= 3;
    if constexpr (access_size == 2) paddr ^= 2;
    u8* cache_dst = cache_line.data + (paddr & (sizeof(cache_line.data) - 1));
    auto to_write = [&] {
        if constexpr (access_size == 1) return u8(data);
        if constexpr (access_size == 2) return u16(data);
        if constexpr (access_size == 4) return u32(data);
        if constexpr (access_size == 8) return data;
    }();
    if constexpr (apply_mask) {
        u64 existing;
        if constexpr (access_size <= 4) {
            std::memcpy(&existing, cache_dst, access_size);
        } else {
            std::memcpy(&existing, cache_dst + 4, 4);
            std::memcpy(reinterpret_cast<u8*>(&existing) + 4, cache_dst, 4);
        }
        to_write &= (..., mask);
        to_write |= existing & (..., ~mask);
    }
    if constexpr (access_size <= 4) {
        std::memcpy(cache_dst, &to_write, access_size);
    } else {
        std::memcpy(cache_dst, reinterpret_cast<u8 const*>(&to_write) + 4, 4);
        std::memcpy(cache_dst + 4, &to_write, 4);
    }
    cache_line.dirty = true;
}

void WritebackCacheLine(auto& cache_line, u32 new_paddr)
{
    /* The address in the main memory to be written is the address in the cache tag
        and not the physical address translated by using TLB */
    auto rdram_offset = cache_line.ptag | new_paddr & 0xFFF & ~(sizeof(cache_line.data) - 1);
    std::memcpy(rdram_ptr + rdram_offset, cache_line.data, sizeof(cache_line.data));
    if constexpr (sizeof(cache_line) == sizeof(DCacheLine)) {
        cache_line.dirty = false;
    }
}

template s32 ReadCacheableArea<s32, MemOp::InstrFetch>(u32);
template s8 ReadCacheableArea<s8, MemOp::Read>(u32);
template s16 ReadCacheableArea<s16, MemOp::Read>(u32);
template s32 ReadCacheableArea<s32, MemOp::Read>(u32);
template s64 ReadCacheableArea<s64, MemOp::Read>(u32);
template void WriteCacheableArea<1>(u32, s64);
template void WriteCacheableArea<2>(u32, s64);
template void WriteCacheableArea<4>(u32, s64);
template void WriteCacheableArea<8>(u32, s64);
template void WriteCacheableArea<4, u32>(u32, s64, u32);
template void WriteCacheableArea<8, u64>(u32, s64, u64);
} // namespace n64::vr4300

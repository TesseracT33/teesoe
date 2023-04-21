#include "mmu.hpp"
#include "cache.hpp"
#include "cop0.hpp"
#include "exceptions.hpp"
#include "log.hpp"
#include "memory/memory.hpp"
#include "n64_build_options.hpp"
#include "util.hpp"
#include "vr4300.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <limits>
#include <type_traits>

namespace n64::vr4300 {

struct TlbEntry {
    void Read() const;
    void Write();

    Cop0Registers::EntryLo entry_lo[2];
    Cop0Registers::EntryHi entry_hi;
    u64 vpn2_addr_mask; /* Used to extract the VPN2 from a virtual address, given page_mask. */
    u64 vpn2_compare; /* entry_hi.vpn2, but shifted left according to page_mask. */
    u32 page_mask;
    u32 offset_addr_mask; /* Used to extract the offset from a virtual address, given page_mask, i.e., the bits lower
                         than those part of the VPN. */
    bool global;
};

static std::array<TlbEntry, 32> tlb_entries;

template<MemOp> static u32 VirtualToPhysicalAddressUserMode32(u64 vaddr, bool& cacheable_area);
template<MemOp> static u32 VirtualToPhysicalAddressUserMode64(u64 vaddr, bool& cacheable_area);
template<MemOp> static u32 VirtualToPhysicalAddressSupervisorMode32(u64 vaddr, bool& cacheable_area);
template<MemOp> static u32 VirtualToPhysicalAddressSupervisorMode64(u64 vaddr, bool& cacheable_area);
template<MemOp> static u32 VirtualToPhysicalAddressKernelMode32(u64 vaddr, bool& cacheable_area);
template<MemOp> static u32 VirtualToPhysicalAddressKernelMode64(u64 vaddr, bool& cacheable_area);
template<MemOp> static u32 VirtualToPhysicalAddressTlb(u64 vaddr);

void TlbEntry::Read() const
{
    cop0.entry_lo[0] = entry_lo[0];
    cop0.entry_lo[1] = entry_lo[1];
    cop0.entry_lo[0].g = cop0.entry_lo[1].g = global;
    // cop0.entry_hi = std::bit_cast<Cop0Registers::EntryHi>(std::bit_cast<u64>(entry_hi) & ~u64(page_mask));
    cop0.entry_hi = entry_hi;
    cop0.page_mask = page_mask;
}

void TlbEntry::Write()
{
    // Each pair of bits in PageMask should be either 00 or 11
    page_mask = cop0.page_mask & 0xAAA << 13;
    page_mask |= page_mask >> 1;
    for (int i = 0; i < 2; ++i) {
        entry_lo[i] = cop0.entry_lo[i];
        entry_lo[i].pfn &= 0xFFFFF;
    }
    entry_hi = std::bit_cast<Cop0Registers::EntryHi>(std::bit_cast<u64>(cop0.entry_hi) & ~u64(page_mask));
    global = cop0.entry_lo[0].g & cop0.entry_lo[1].g;
    // Compute things that speed up virtual-to-physical-address translation
    vpn2_addr_mask = 0xFF'FFFF'E000 & ~u64(page_mask);
    vpn2_compare = std::bit_cast<u64>(entry_hi) & vpn2_addr_mask;
    offset_addr_mask = page_mask >> 1 | 0xFFF;
}

u32 FetchInstruction(u64 virtual_address)
{
    return ReadVirtual<s32, Alignment::Aligned, MemOp::InstrFetch>(virtual_address);
}

u32 GetPhysicalPC()
{
    bool cacheable_area;
    return active_virtual_to_physical_fun_read(pc, cacheable_area);
}

void InitializeMMU()
{
    for (TlbEntry& entry : tlb_entries) {
        entry.entry_hi.asid = 0xFF;
        entry.entry_hi.vpn2 = 0x07FF'FFFF;
        entry.global = 1;
        /* TODO: vpn2_addr_mask, vpn2_compare, offset_addr_mask? */
    }
}

template<std::signed_integral Int, Alignment alignment, MemOp mem_op> Int ReadVirtual(u64 vaddr)
{
    /* For aligned accesses, check if the address is misaligned. No need to do it for instruction fetches.
       The PC can be misaligned after an ERET instruction, but we manually check there if the PC read from the EPC
       register is misaligned. */
    if constexpr (sizeof(Int) > 1) {
        if constexpr (mem_op == MemOp::Read) {
            if constexpr (alignment == Alignment::Aligned) {
                if (vaddr & (sizeof(Int) - 1)) {
                    SignalAddressErrorException<mem_op>(vaddr);
                    return {};
                }
            } else {
                /* For unaligned accesses, always read from the last boundary, with the number of bytes being sizeof
                   Int. The rest is taken care of by the function which handles the load instruction. */
                vaddr &= ~(sizeof(Int) - 1);
            }
        }
        if (addressing_mode == AddressingMode::_32bit && s32(vaddr) != vaddr) {
            SignalAddressErrorException<mem_op>(vaddr);
            return {};
        }
    }
    bool cacheable_area;
    u32 physical_address = active_virtual_to_physical_fun_read(vaddr, cacheable_area);
    if (exception_occurred) {
        return {};
    }
    if constexpr (mem_op == MemOp::InstrFetch && log_cpu_instructions) {
        last_instr_fetch_phys_addr = physical_address;
    }
    last_physical_address_on_load = physical_address;
    if (cacheable_area) { /* TODO: figure out some way to avoid this branch, if possible */
        /* cycle counter incremented in the function, depending on if cache hit/miss */
        return ReadCacheableArea<Int, mem_op>(physical_address);
    } else {
        p_cycle_counter += cache_miss_cycle_delay; /* using the same number here */
        return memory::Read<Int>(physical_address);
    }
}

void SetActiveVirtualToPhysicalFunctions()
{
    using enum AddressingMode;
    using enum OperatingMode;

    if (cop0.status.ksu == 0 || cop0.status.erl == 1 || cop0.status.exl == 1) { /* Kernel mode */
        operating_mode = Kernel;
        if (cop0.status.kx == 0) {
            active_virtual_to_physical_fun_read = VirtualToPhysicalAddressKernelMode32<MemOp::Read>;
            active_virtual_to_physical_fun_write = VirtualToPhysicalAddressKernelMode32<MemOp::Write>;
            addressing_mode = _32bit;
        } else {
            active_virtual_to_physical_fun_read = VirtualToPhysicalAddressKernelMode64<MemOp::Read>;
            active_virtual_to_physical_fun_write = VirtualToPhysicalAddressKernelMode64<MemOp::Write>;
            addressing_mode = _64bit;
        }
    } else if (cop0.status.ksu == 1) { /* Supervisor mode */
        operating_mode = Supervisor;
        if (cop0.status.sx == 0) {
            active_virtual_to_physical_fun_read = VirtualToPhysicalAddressSupervisorMode32<MemOp::Read>;
            active_virtual_to_physical_fun_write = VirtualToPhysicalAddressSupervisorMode32<MemOp::Write>;
            addressing_mode = _32bit;
        } else {
            active_virtual_to_physical_fun_read = VirtualToPhysicalAddressSupervisorMode64<MemOp::Read>;
            active_virtual_to_physical_fun_write = VirtualToPhysicalAddressSupervisorMode64<MemOp::Write>;
            addressing_mode = _64bit;
        }
    } else if (cop0.status.ksu == 2) { /* User mode */
        operating_mode = User;
        if (cop0.status.ux == 0) {
            active_virtual_to_physical_fun_read = VirtualToPhysicalAddressUserMode32<MemOp::Read>;
            active_virtual_to_physical_fun_write = VirtualToPhysicalAddressUserMode32<MemOp::Write>;
            addressing_mode = _32bit;
        } else {
            active_virtual_to_physical_fun_read = VirtualToPhysicalAddressUserMode64<MemOp::Read>;
            active_virtual_to_physical_fun_write = VirtualToPhysicalAddressUserMode64<MemOp::Write>;
            addressing_mode = _64bit;
        }
    } else { /* Unknown?! */
        log_error("cop0.status.ksu was set to 3.");
        assert(false);
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressUserMode32(u64 vaddr, bool& cacheable_area)
{
    if (vaddr & 0x8000'0000) {
        SignalAddressErrorException<mem_op>(vaddr);
        return 0;
    } else {
        cacheable_area = false;
        return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressUserMode64(u64 vaddr, bool& cacheable_area)
{
    if (vaddr < 0x100'0000'0000) {
        cacheable_area = false;
        return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
    } else {
        SignalAddressErrorException<mem_op>(vaddr);
        return 0;
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressSupervisorMode32(u64 vaddr, bool& cacheable_area)
{
    /* $8000'0000-$BFFF'FFFF; $E000'0000-$FFFF'FFFF */
    if ((vaddr & 1 << 31) && (vaddr & 0b11 << 29) != 0b10 << 29) {
        SignalAddressErrorException<mem_op>(vaddr);
        return 0;
    }
    /* 0-$7FFF'FFFF; $C000'0000-$DFFF'FFFF */
    else {
        cacheable_area = false;
        return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressSupervisorMode64(u64 vaddr, bool& cacheable_area)
{
    switch (vaddr >> 60) {
    case 0x0:
        if (vaddr <= 0x0000'00FF'FFFF'FFFF) {
            cacheable_area = false;
            return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    case 0x4:
        if (vaddr <= 0x4000'00FF'FFFF'FFFF) {
            cacheable_area = false;
            return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    case 0xF:
        /* $FFFF'FFFF'C0000'0000 -- $FFFF'FFFF'DFFF'FFFF */
        if ((vaddr & 0xFFFF'FFFF'E000'0000) == 0xFFFF'FFFF'C000'0000) {
            cacheable_area = false;
            return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
        }
        /* $F000'0000'0000'0000 -- $FFFF'FFFF'BFFF'FFFF; $FFFF'FFFF'E000'0000 -- $FFFF'FFFF'FFFF'FFFF */
        else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    default: SignalAddressErrorException<mem_op>(vaddr); return 0;
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressKernelMode32(u64 vaddr, bool& cacheable_area)
{
    if ((vaddr & 0xE000'0000) == 0x8000'0000) {
        /* $8000'0000-$9FFF'FFFF; TLB unmapped; cacheable */
        cacheable_area = true;
        return vaddr & 0x1FFF'FFFF;
    } else if ((vaddr & 0xE000'0000) == 0xA000'0000) {
        /* $A000'0000-$BFFF'FFFF; TLB unmapped; uncacheable */
        cacheable_area = false;
        return vaddr & 0x1FFF'FFFF;
    } else {
        cacheable_area = false;
        return VirtualToPhysicalAddressTlb<mem_op>(s32(vaddr)); /* TLB mapped */
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressKernelMode64(u64 vaddr, bool& cacheable_area)
{
    switch (vaddr >> 60 & 0xF) {
    case 0x0:
        if (vaddr <= 0x0000'00FF'FFFF'FFFF) {
            cacheable_area = false;
            return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    case 0x4:
        if (vaddr <= 0x4000'00FF'FFFF'FFFF) {
            cacheable_area = false;
            return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    case 0x8:
    case 0x9:
    case 0xA:
    case 0xB:
        if ((vaddr & 0x07FF'FFFF'0000'0000) == 0) { /* tlb unmapped */
            cacheable_area = (vaddr & 0x9800'0000'0000'0000) != 0x9000'0000'0000'0000;
            u64 phys_addr = vaddr & 0xFFFF'FFFF;
            return phys_addr;
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    case 0xC:
        if (vaddr <= 0xC000'00FF'7FFF'FFFF) {
            cacheable_area = false;
            return VirtualToPhysicalAddressTlb<mem_op>(vaddr);
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    case 0xF:
        if (vaddr >= 0xFFFF'FFFF'0000'0000) {
            return VirtualToPhysicalAddressKernelMode32<mem_op>(vaddr, cacheable_area);
        } else {
            SignalAddressErrorException<mem_op>(vaddr);
            return 0;
        }

    default: SignalAddressErrorException<mem_op>(vaddr); return 0;
    }
}

template<MemOp mem_op> u32 VirtualToPhysicalAddressTlb(u64 vaddr)
{
    for (TlbEntry const& entry : tlb_entries) {
        /* Compare the virtual page number (divided by two; VPN2) of the entry with the VPN2 of the virtual address */
        if ((vaddr & entry.vpn2_addr_mask) != entry.vpn2_compare) continue;
        /* If the global bit is clear, the entry's ASID (Address space ID field) must coincide with the one in the
         * EntryHi register. */
        if (!entry.global && entry.entry_hi.asid != cop0.entry_hi.asid) continue;
        /* Bits 62-63 of vaddr must match the entry's region */ /* TODO: also checked in 32-bit mode? */
        if (vaddr >> 62 != entry.entry_hi.r) continue;
        /* The VPN maps to two (consecutive) pages; EntryLo0 for even virtual pages and EntryLo1 for odd virtual pages.
         */
        bool vpn_odd = (vaddr & (entry.offset_addr_mask + 1)) != 0;
        auto entry_lo = entry.entry_lo[vpn_odd];
        if (!entry_lo.v) { /* If the "Valid" bit is clear, it indicates that the TLB entry is invalid. */
            SignalException<Exception::TlbInvalid, mem_op>();
            exception_bad_vaddr = vaddr;
            return 0;
        }
        if constexpr (mem_op == MemOp::Write) {
            if (!entry_lo.d) { /* If the "Dirty" bit is clear, writing is disallowed. */
                SignalException<Exception::TlbModification, mem_op>();
                exception_bad_vaddr = vaddr;
                return 0;
            }
        }
        /* TLB hit */
        return vaddr & entry.offset_addr_mask | entry_lo.pfn << 12 & ~entry.offset_addr_mask;
    }
    /* TLB miss */
    if (addressing_mode == AddressingMode::_32bit) SignalException<Exception::TlbMiss, mem_op>();
    else SignalException<Exception::XtlbMiss, mem_op>();
    exception_bad_vaddr = vaddr;
    return 0;
}

template<size_t access_size, Alignment alignment> void WriteVirtual(u64 vaddr, s64 data)
{
    static_assert(std::has_single_bit(access_size) && access_size <= 8);
    u32 offset = vaddr & (access_size - 1);
    if constexpr (access_size > 1) {
        if constexpr (alignment == Alignment::Aligned) {
            if (offset) {
                SignalAddressErrorException<MemOp::Write>(vaddr);
                return;
            }
        }
        if (addressing_mode == AddressingMode::_32bit && s32(vaddr) != vaddr) {
            SignalAddressErrorException<MemOp::Write>(vaddr);
            return;
        }
    }
    bool cacheable_area;
    u32 physical_address = active_virtual_to_physical_fun_write(vaddr, cacheable_area);
    if (exception_occurred) {
        return;
    }
    static constexpr bool use_mask = alignment != Alignment::Aligned;
    auto Mask = [offset] {
        // TODO: the below probably only works when reading from rdram (it's in LE)
        if constexpr (alignment == Alignment::UnalignedLeft) { /* Store (Double)Word Left */
            // (SWL) offset => mask; 0 => FFFF'FFFF; 1 => 00FF'FFFF; 2 => 0000'FFFF; 3 => 0000'00FF
            // (SDL) offset => mask; 0 => FFFF'FFFF'FFFF'FFFF; 1 => 00FF'FFFF'FFFF'FFFF; ...
            return std::numeric_limits<typename SizeToUInt<access_size>::type>::max() >> (8 * offset);
        } else { /* UnalignedRight; Store (Double)Word Right */
            // (SWR) offset => mask; 0 => FF00'0000; 1 => FFFF'0000; 2 => FFFF'FF00; 3 => FFFF'FFFF
            // (SDR) offset => mask; 0 => FF00'0000'0000'0000; 1 => FFFF'0000'0000'0000; ...
            return std::numeric_limits<typename SizeToUInt<access_size>::type>::max()
                << (8 * (access_size - offset - 1));
        }
    };
    if (cacheable_area) {
        if constexpr (use_mask) WriteCacheableArea<access_size>(physical_address, data, Mask());
        else WriteCacheableArea<access_size>(physical_address, data);
    } else {
        p_cycle_counter += cache_miss_cycle_delay;
        if constexpr (use_mask) memory::Write<access_size>(physical_address, data, Mask());
        else memory::Write<access_size>(physical_address, data);
    }
}

void tlbp()
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
        return;
    }
    auto index = std::ranges::find_if(tlb_entries, [](TlbEntry const& entry) {
        u64 vpn2_mask = 0xFF'FFFF'E000 & ~u64(entry.page_mask);
        if ((std::bit_cast<u64>(entry.entry_hi) & vpn2_mask) != (std::bit_cast<u64>(cop0.entry_hi) & vpn2_mask))
            return false;
        if (!entry.global && entry.entry_hi.asid != cop0.entry_hi.asid) return false;
        if (entry.entry_hi.r != cop0.entry_hi.r) return false;
        return true;
    });
    if (index == tlb_entries.end()) {
        cop0.index.p = 1;
        cop0.index.value = 0; /* technically undefined, but n64-systemtest tlb::TLBPMatch seems to prefer this */
    } else {
        cop0.index.p = 0;
        cop0.index.value = std::distance(tlb_entries.begin(), index);
    }
}

void tlbr()
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
    } else {
        auto index = cop0.index.value;
        if (index < 32) tlb_entries[index].Read();
    }
}

void tlbwi()
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
    } else {
        auto index = cop0.index.value;
        if (index < 32) tlb_entries[index].Write();
    }
}

void tlbwr()
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
    } else {
        auto index = random_generator.Generate();
        if (index < 32) tlb_entries[index].Write();
    }
}

template s8 ReadVirtual<s8, Alignment::Aligned>(u64);
template s16 ReadVirtual<s16, Alignment::Aligned>(u64);
template s32 ReadVirtual<s32, Alignment::Aligned>(u64);
template s64 ReadVirtual<s64, Alignment::Aligned>(u64);
template s32 ReadVirtual<s32, Alignment::UnalignedLeft>(u64);
template s64 ReadVirtual<s64, Alignment::UnalignedLeft>(u64);
template s32 ReadVirtual<s32, Alignment::UnalignedRight>(u64);
template s64 ReadVirtual<s64, Alignment::UnalignedRight>(u64);

template void WriteVirtual<1, Alignment::Aligned>(u64, s64);
template void WriteVirtual<2, Alignment::Aligned>(u64, s64);
template void WriteVirtual<4, Alignment::Aligned>(u64, s64);
template void WriteVirtual<8, Alignment::Aligned>(u64, s64);
template void WriteVirtual<4, Alignment::UnalignedLeft>(u64, s64);
template void WriteVirtual<8, Alignment::UnalignedLeft>(u64, s64);
template void WriteVirtual<4, Alignment::UnalignedRight>(u64, s64);
template void WriteVirtual<8, Alignment::UnalignedRight>(u64, s64);

template u32 VirtualToPhysicalAddressUserMode32<MemOp::Read>(u64, bool&);
template u32 VirtualToPhysicalAddressUserMode32<MemOp::Write>(u64, bool&);
template u32 VirtualToPhysicalAddressUserMode64<MemOp::Read>(u64, bool&);
template u32 VirtualToPhysicalAddressUserMode64<MemOp::Write>(u64, bool&);
template u32 VirtualToPhysicalAddressSupervisorMode32<MemOp::Read>(u64, bool&);
template u32 VirtualToPhysicalAddressSupervisorMode32<MemOp::Write>(u64, bool&);
template u32 VirtualToPhysicalAddressSupervisorMode64<MemOp::Read>(u64, bool&);
template u32 VirtualToPhysicalAddressSupervisorMode64<MemOp::Write>(u64, bool&);
template u32 VirtualToPhysicalAddressKernelMode32<MemOp::Read>(u64, bool&);
template u32 VirtualToPhysicalAddressKernelMode32<MemOp::Write>(u64, bool&);
template u32 VirtualToPhysicalAddressKernelMode64<MemOp::Read>(u64, bool&);
template u32 VirtualToPhysicalAddressKernelMode64<MemOp::Write>(u64, bool&);

template u32 VirtualToPhysicalAddressTlb<MemOp::Read>(u64);
template u32 VirtualToPhysicalAddressTlb<MemOp::Write>(u64);
} // namespace n64::vr4300

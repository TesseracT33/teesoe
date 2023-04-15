#include "cop0.hpp"
#include "cop1.hpp"
#include "exceptions.hpp"
#include "memory/memory.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
#include "scheduler.hpp"
#include "util.hpp"
#include "vr4300.hpp"

#include <bit>
#include <cassert>
#include <cstring>

namespace n64::vr4300 {

void Cop0Registers::OnWriteToCause()
{
    CheckInterrupts();
}

void Cop0Registers::OnWriteToCompare()
{
    cause.ip &= ~0x80; /* TODO: not sure if anything else needs to be done? */
    ReloadCountCompareEvent();
}

void Cop0Registers::OnWriteToCount()
{
    ReloadCountCompareEvent();
}

void Cop0Registers::OnWriteToStatus()
{
    SetActiveVirtualToPhysicalFunctions();
    CheckInterrupts();
}

void Cop0Registers::OnWriteToWired()
{
    random_generator.SetRange(wired);
}

u64 Cop0Registers::Get(size_t reg_index) const
{
    auto StructToInt = [](auto struct_)
        requires(sizeof(struct_) == 4 || sizeof(struct_) == 8)
    {
        if constexpr (sizeof(struct_) == 4) return std::bit_cast<u32>(struct_);
        else if constexpr (sizeof(struct_) == 8) return std::bit_cast<u64>(struct_);
        else static_assert(always_false<sizeof(struct_)>, "Struct must be either 4 or 8 bytes.");
    };

    switch (reg_index & 31) {
    case Cop0Reg::index: return StructToInt(index);
    case Cop0Reg::random: return random_generator.Generate(); /* Generate a random number in the interval [wired, 31] */
    case Cop0Reg::entry_lo_0: return StructToInt(entry_lo[0]);
    case Cop0Reg::entry_lo_1: return StructToInt(entry_lo[1]);
    case Cop0Reg::context: return StructToInt(context);
    case Cop0Reg::page_mask: return page_mask;
    case Cop0Reg::wired: return wired;
    case Cop0Reg::bad_v_addr: return bad_v_addr;
    case Cop0Reg::count: return u32(count >> 1); /* See the declaration of 'count' */
    case Cop0Reg::entry_hi: return StructToInt(entry_hi);
    case Cop0Reg::compare: return u32(compare >> 1); /* See the declaration of 'compare' */
    case Cop0Reg::status: return StructToInt(status);
    case Cop0Reg::cause: return StructToInt(cause);
    case Cop0Reg::epc: return epc;
    case Cop0Reg::pr_id: return StructToInt(pr_id);
    case Cop0Reg::config: return StructToInt(config);
    case Cop0Reg::ll_addr: return ll_addr;
    case Cop0Reg::watch_lo: return StructToInt(watch_lo);
    case Cop0Reg::watch_hi: return StructToInt(watch_hi);
    case Cop0Reg::x_context: return StructToInt(x_context);
    case Cop0Reg::parity_error: return StructToInt(parity_error);
    case Cop0Reg::cache_error: return cache_error;
    case Cop0Reg::tag_lo: return StructToInt(tag_lo);
    case Cop0Reg::tag_hi: return tag_hi;
    case Cop0Reg::error_epc: return error_epc;
    case 7: return cop0_unused_7;
    case 21: return cop0_unused_21;
    case 22: return cop0_unused_22;
    case 23: return cop0_unused_23;
    case 24: return cop0_unused_24;
    case 25: return cop0_unused_25;
    case 31: return cop0_unused_31;
    default: return 0;
    }
}

template<bool raw> void Cop0Registers::Set(size_t reg_index, std::integral auto value)
{
    auto IntToStruct = [](auto& struct_, auto value) {
        /* The operation of DMFC0 instruction on a 32-bit register of the CP0 is undefined.
            Here: simply write to the lower 32 bits. */
        static_assert(sizeof(struct_) == 4 || sizeof(struct_) == 8);
        static_assert(sizeof(value) == 4 || sizeof(value) == 8);
        static constexpr auto num_bytes_to_write = std::min(sizeof(struct_), sizeof(value));
        std::memcpy(&struct_, &value, num_bytes_to_write);
    };

    auto IntToStructMasked = [](auto& struct_, auto value, auto mask) {
        using StructT = std::remove_reference_t<decltype(struct_)>;
        static_assert(sizeof(struct_) == 4 || sizeof(struct_) == 8);
        static_assert(sizeof(value) == 4 || sizeof(value) == 8);
        value &= mask;
        if constexpr (sizeof(struct_) == 4) {
            u32 prev_struct = std::bit_cast<u32>(struct_);
            u32 new_struct = u32(value | prev_struct & ~mask);
            struct_ = std::bit_cast<StructT>(new_struct);
        } else {
            u64 prev_struct = std::bit_cast<u64>(struct_);
            u64 new_struct = value | prev_struct & ~mask;
            struct_ = std::bit_cast<StructT>(new_struct);
        }
    };

    switch (reg_index) { /* Masks are used for bits that are non-writeable. */
    case Cop0Reg::index:
        if constexpr (raw) IntToStruct(index, value);
        else IntToStructMasked(index, value, 0x8000'003F);
        break;

    case Cop0Reg::random:
        if constexpr (raw) random = value;
        else random = value & 0x20;
        break;

    case Cop0Reg::entry_lo_0:
        if constexpr (raw) IntToStruct(entry_lo[0], value);
        else IntToStructMasked(entry_lo[0], value, 0x3FFF'FFFF);
        break;

    case Cop0Reg::entry_lo_1:
        if constexpr (raw) IntToStruct(entry_lo[1], value);
        else IntToStructMasked(entry_lo[1], value, 0x3FFF'FFFF);
        break;

    case Cop0Reg::context:
        if constexpr (raw) IntToStruct(context, value);
        else IntToStructMasked(context, value, ~0xFull);
        break;

    case Cop0Reg::page_mask:
        if constexpr (raw) page_mask = value;
        else page_mask = value & 0x01FF'E000;
        break;

    case Cop0Reg::wired:
        if constexpr (raw) wired = value;
        else wired = value & 0x3F;
        OnWriteToWired();
        break;

    case Cop0Reg::bad_v_addr:
        if constexpr (raw) bad_v_addr = value;
        break;

    case Cop0Reg::count:
        count = u64(value) << 1; /* See the declaration of 'count' */
        OnWriteToCount();
        break;

    case Cop0Reg::entry_hi:
        if constexpr (raw) IntToStruct(entry_hi, value);
        else IntToStructMasked(entry_hi, value, 0xC000'00FF'FFFF'E0FF);
        break;

    case Cop0Reg::compare:
        compare = u64(value) << 1; /* See the declaration of 'compare' */
        OnWriteToCompare();
        break;

    case Cop0Reg::status:
        if constexpr (raw) IntToStruct(status, value);
        else IntToStructMasked(status, value, 0xFF57'FFFF);
        OnWriteToStatus();
        break;

    case Cop0Reg::cause:
        if constexpr (raw) IntToStruct(cause, value);
        else IntToStructMasked(cause, value, 0x300);
        OnWriteToCause();
        break;

    case Cop0Reg::epc: epc = value; break;

    case Cop0Reg::config:
        if constexpr (raw) IntToStruct(config, value);
        else IntToStructMasked(config, value, 0xF00'800F);
        break;

    case Cop0Reg::ll_addr: ll_addr = value; break;

    case Cop0Reg::watch_lo:
        if constexpr (raw) IntToStruct(watch_lo, value);
        else IntToStructMasked(watch_lo, value, 0xFFFF'FFFB);
        break;

    case Cop0Reg::watch_hi: IntToStruct(watch_hi, value); break;

    case Cop0Reg::x_context:
        if constexpr (raw) IntToStruct(x_context, value);
        else IntToStructMasked(x_context, value, ~0xFull);
        break;

    case Cop0Reg::parity_error:
        if constexpr (raw) IntToStruct(parity_error, value);
        else IntToStructMasked(parity_error, value, 0xFF);
        break;

    case Cop0Reg::tag_lo:
        if constexpr (raw) IntToStruct(tag_lo, value);
        else IntToStructMasked(tag_lo, value, 0x0FFF'FFC0);
        break;

    case Cop0Reg::error_epc: error_epc = value; break;

    case 7: cop0_unused_7 = u32(value); break;
    case 21: cop0_unused_21 = u32(value); break;
    case 22: cop0_unused_22 = u32(value); break;
    case 23: cop0_unused_23 = u32(value); break;
    case 24: cop0_unused_24 = u32(value); break;
    case 25: cop0_unused_25 = u32(value); break;
    case 31: cop0_unused_31 = u32(value); break;
    }
}

void OnCountCompareMatchEvent()
{
    cop0.cause.ip |= 0x80;
    CheckInterrupts();
    ReloadCountCompareEvent();
}

template<bool initial_add> void ReloadCountCompareEvent()
{
    u64 cycles_until_count_compare_match = (cop0.compare - cop0.count) & 0x1'FFFF'FFFF;
    if ((cop0.count & 0x1'FFFF'FFFF) >= (cop0.compare & 0x1'FFFF'FFFF)) {
        cycles_until_count_compare_match += 0x2'0000'0000;
    }
    if constexpr (initial_add) {
        scheduler::AddEvent(scheduler::EventType::CountCompareMatch,
          cycles_until_count_compare_match,
          OnCountCompareMatchEvent);
    } else {
        scheduler::ChangeEventTime(scheduler::EventType::CountCompareMatch, cycles_until_count_compare_match);
    }
}

void dmfc0(u32 rt, u32 rd)
{
    if (operating_mode != OperatingMode::Kernel) {
        if (!cop0.status.cu0) {
            SignalCoprocessorUnusableException(0);
            return;
        }
        if (addressing_mode == AddressingMode::_32bit) {
            SignalException<Exception::ReservedInstruction>();
            return;
        }
    }
    gpr.set(rt, cop0.Get(rd));
}

void dmtc0(u32 rt, u32 rd)
{
    if (operating_mode != OperatingMode::Kernel) {
        if (!cop0.status.cu0) {
            SignalCoprocessorUnusableException(0);
            return;
        }
        if (addressing_mode == AddressingMode::_32bit) {
            SignalException<Exception::ReservedInstruction>();
            return;
        }
    }
    cop0.Set(rd, gpr[rt]);
}

void eret()
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
        return;
    }
    if (cop0.status.erl == 0) {
        pc = cop0.epc;
        cop0.status.exl = 0;
    } else {
        pc = cop0.error_epc;
        cop0.status.erl = 0;
    }
    CheckInterrupts();
    ll_bit = 0;
    /* Check if the pc is misaligned, and if so, signal an exception right away.
       Then, there is no need to check if the pc is misaligned every time an instruction is fetched
       (this is one of the few places where the pc can be set to a misaligned value). */
    if (pc & 3) {
        SignalAddressErrorException<MemOp::InstrFetch>(pc);
    }
    SetActiveVirtualToPhysicalFunctions();
}

void mfc0(u32 rt, u32 rd)
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
    } else {
        gpr.set(rt, s32(cop0.Get(rd)));
    }
}

void mtc0(u32 rt, u32 rd)
{
    if (operating_mode != OperatingMode::Kernel && !cop0.status.cu0) {
        SignalCoprocessorUnusableException(0);
    } else {
        cop0.Set(rd, s32(gpr[rt]));
    }
}

template void Cop0Registers::Set<false>(size_t, s32);
template void Cop0Registers::Set<false>(size_t, u32);
template void Cop0Registers::Set<false>(size_t, u64);
template void Cop0Registers::Set<true>(size_t, s32);
template void Cop0Registers::Set<true>(size_t, u32);
template void Cop0Registers::Set<true>(size_t, u64);

template void ReloadCountCompareEvent<false>();
template void ReloadCountCompareEvent<true>();

} // namespace n64::vr4300

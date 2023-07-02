#include "cop0.hpp"
#include "cop1.hpp"
#include "exceptions.hpp"
#include "memory/memory.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
#include "scheduler.hpp"
#include "util.hpp"
#include "vr4300.hpp"
#include "vr4300/interpreter.hpp"

#include <bit>
#include <cassert>
#include <cstring>

using mips::OperatingMode;

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
    can_exec_cop0_instrs = operating_mode == mips::OperatingMode::Kernel || cop0.status.cu0;
    SetVaddrToPaddrFuncs();
    CheckInterrupts();
}

void Cop0Registers::OnWriteToWired()
{
    random_generator.SetRange(wired);
}

u64 Cop0Registers::Get(size_t reg_index) const
{
    auto Read = [](auto reg) {
        if constexpr (sizeof(reg) == 4) return std::bit_cast<u32>(reg);
        else if constexpr (sizeof(reg) == 8) return std::bit_cast<u64>(reg);
        else static_assert(always_false<sizeof(reg)>, "Register must be either 4 or 8 bytes.");
    };

    switch (reg_index & 31) {
    case Cop0Reg::index: return Read(index);
    case Cop0Reg::random: return random_generator.Generate(); /* Generate a random number in the interval [wired, 31] */
    case Cop0Reg::entry_lo_0: return Read(entry_lo[0]);
    case Cop0Reg::entry_lo_1: return Read(entry_lo[1]);
    case Cop0Reg::context: return Read(context);
    case Cop0Reg::page_mask: return page_mask;
    case Cop0Reg::wired: return wired;
    case Cop0Reg::bad_v_addr: return bad_v_addr;
    case Cop0Reg::count: return u32(count >> 1); /* See the declaration of 'count' */
    case Cop0Reg::entry_hi: return Read(entry_hi);
    case Cop0Reg::compare: return u32(compare >> 1); /* See the declaration of 'compare' */
    case Cop0Reg::status: return Read(status);
    case Cop0Reg::cause: return Read(cause);
    case Cop0Reg::epc: return epc;
    case Cop0Reg::pr_id: return Read(pr_id);
    case Cop0Reg::config: return Read(config);
    case Cop0Reg::ll_addr: return ll_addr;
    case Cop0Reg::watch_lo: return Read(watch_lo);
    case Cop0Reg::watch_hi: return Read(watch_hi);
    case Cop0Reg::x_context: return Read(x_context);
    case Cop0Reg::parity_error: return Read(parity_error);
    case Cop0Reg::cache_error: return cache_error;
    case Cop0Reg::tag_lo: return Read(tag_lo);
    case Cop0Reg::tag_hi: return tag_hi;
    case Cop0Reg::error_epc: return error_epc;
    case 7:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 31: return last_cop0_write;
    default: return 0;
    }
}

template<bool raw> void Cop0Registers::Set(size_t reg_index, std::signed_integral auto value)
{
    auto Write = [](auto& reg, std::signed_integral auto value, auto... mask) {
        static_assert((sizeof(reg) == 4 || sizeof(reg) == 8) && (sizeof(value) == 4 || sizeof(value) == 8));
        static_assert(sizeof...(mask) < 2);
        auto to_write = [=] {
            if constexpr (sizeof(reg) == 4 && sizeof(value) == 8) return s32(value);
            else if constexpr (sizeof(reg) == 8 && sizeof(value) == 4) return s64(value);
            else return value;
        }();
        if constexpr (sizeof...(mask) == 1) {
            to_write &= (..., mask);
            auto prev = std::bit_cast<decltype(to_write)>(reg);
            prev &= ~(..., mask);
            to_write |= prev;
        }
        reg = std::bit_cast<std::remove_reference_t<decltype(reg)>>(to_write);
    };

    switch (reg_index & 31) {
    case Cop0Reg::index:
        if constexpr (raw) Write(index, value);
        else Write(index, value, 0x8000'003F);
        break;

    case Cop0Reg::random:
        if constexpr (raw) random = value;
        else random = value & 0x20;
        break;

    case Cop0Reg::entry_lo_0:
        if constexpr (raw) Write(entry_lo[0], value);
        else Write(entry_lo[0], value, 0x3FFF'FFFF);
        break;

    case Cop0Reg::entry_lo_1:
        if constexpr (raw) Write(entry_lo[1], value);
        else Write(entry_lo[1], value, 0x3FFF'FFFF);
        break;

    case Cop0Reg::context:
        if constexpr (raw) Write(context, value);
        else Write(context, value, 0xFFFF'FFFF'FF80'0000);
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
        if constexpr (raw) Write(entry_hi, value);
        else Write(entry_hi, value, 0xC000'00FF'FFFF'E0FF);
        break;

    case Cop0Reg::compare:
        compare = u64(value) << 1; /* See the declaration of 'compare' */
        OnWriteToCompare();
        break;

    case Cop0Reg::status:
        if constexpr (raw) Write(status, value);
        else Write(status, value, 0xFF57'FFFF);
        OnWriteToStatus();
        break;

    case Cop0Reg::cause:
        if constexpr (raw) Write(cause, value);
        else Write(cause, value, 0x300);
        OnWriteToCause();
        break;

    case Cop0Reg::epc: epc = value; break;

    case Cop0Reg::config:
        if constexpr (raw) Write(config, value);
        else Write(config, value, 0xF00'800F);
        break;

    case Cop0Reg::ll_addr: ll_addr = value; break;

    case Cop0Reg::watch_lo:
        if constexpr (raw) Write(watch_lo, value);
        else Write(watch_lo, value, 0xFFFF'FFFB);
        break;

    case Cop0Reg::watch_hi: Write(watch_hi, value); break;

    case Cop0Reg::x_context:
        if constexpr (raw) Write(x_context, value);
        else Write(x_context, value, 0xFFFF'FFFE'0000'0000);
        break;

    case Cop0Reg::parity_error:
        if constexpr (raw) Write(parity_error, value);
        else Write(parity_error, value, 0xFF);
        break;

    case Cop0Reg::tag_lo:
        if constexpr (raw) Write(tag_lo, value);
        else Write(tag_lo, value, 0x0FFF'FFC0);
        break;

    case Cop0Reg::error_epc: error_epc = value; break;
    }

    last_cop0_write = u32(value);
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
            return CoprocessorUnusableException(0);
        }
        if (addressing_mode == AddressingMode::_32bit) {
            return ReservedInstructionException();
        }
    }
    gpr.set(rt, cop0.Get(rd));
}

void dmtc0(u32 rt, u32 rd)
{
    if (operating_mode != OperatingMode::Kernel) {
        if (!cop0.status.cu0) {
            return CoprocessorUnusableException(0);
        }
        if (addressing_mode == AddressingMode::_32bit) {
            return ReservedInstructionException();
        }
    }
    cop0.Set(rd, gpr[rt]);
}

void eret()
{
    if (!can_exec_cop0_instrs) {
        return CoprocessorUnusableException(0);
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
        AddressErrorException<MemOp::InstrFetch>(pc);
    } else {
        ResetBranch();
        exception_occurred = true; // stop pc from being incremented by 4 directly after. TODO: add a new BranchState?
    }
    SetVaddrToPaddrFuncs();
}

void mfc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        gpr.set(rt, s32(cop0.Get(rd)));
    } else {
        CoprocessorUnusableException(0);
    }
}

void mtc0(u32 rt, u32 rd)
{
    if (can_exec_cop0_instrs) {
        cop0.Set(rd, s32(gpr[rt]));
    } else {
        CoprocessorUnusableException(0);
    }
}

template void Cop0Registers::Set<false>(size_t, s32);
template void Cop0Registers::Set<false>(size_t, s64);
template void Cop0Registers::Set<true>(size_t, s32);
template void Cop0Registers::Set<true>(size_t, s64);

template void ReloadCountCompareEvent<false>();
template void ReloadCountCompareEvent<true>();

} // namespace n64::vr4300

#include "r3000a.hpp"
#include "cop0.hpp"
#include "decoder.hpp"
#include "exceptions.hpp"
#include "frontend/message.hpp"
#include "memory.hpp"
#include "util.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>

namespace ps1::r3000a {

static u32 i_ctrl, i_mask, i_stat;
static u64 time_last_step_begin;

static void fetch_decode_exec();

void add_initial_events()
{
}

void advance_pipeline(u32 cycles)
{
    cycle_counter += cycles;
}

void check_interrupts()
{
    auto int2 = [] { return cop0.cause.ip2 & cop0.status.im2; };
    bool prev_int2 = int2();
    cop0.cause.ip2 = i_ctrl & 1 & bool(i_stat & i_mask);
    if (cop0.status.iec && !prev_int2 && int2()) {
        interrupt_exception();
    }
}

u64 get_time()
{
    return time_last_step_begin + cycle_counter;
}

void fetch_decode_exec()
{
    advance_pipeline(1);
    s32 instr = read<s32, Alignment::Aligned, MemOp::InstrFetch>(pc);
    pc += 4;
    decode<CpuImpl::Interpreter>(instr);
}

bool init()
{
    gpr = {};
    lo = hi = jump_addr = pc = 0;
    in_branch_delay_slot = false;
    i_ctrl = i_mask = i_stat = 0;

    return true;
}

void lower_interrupt(Interrupt interrupt)
{
    i_stat &= ~std::to_underlying(interrupt);
    cop0.cause.ip2 = i_ctrl & 1 & bool(i_stat & i_mask & 0x3F'FFFF);
}

void jump(u32 target)
{
    assert(!in_branch_delay_slot); // TODO: going beyond this can result in stack overflow. First need to know what the
                                   // behaviour is
    in_branch_delay_slot = true;
    fetch_decode_exec();
    in_branch_delay_slot = false;
    if (!exception_occurred) pc = target;
}

bool LoadBios(std::filesystem::path const& path)
{
    std::expected<std::vector<u8>, std::string> expected_bios = read_file(path, bios_size);
    if (expected_bios) {
        std::vector<u8> const& bios_val = expected_bios.value();
        std::copy(bios_val.cbegin(), bios_val.cend(), bios.begin());
        return true;
    } else {
        message::error(std::string("Failed to load bios; ") + expected_bios.error());
        return false;
    }
}

void raise_interrupt(Interrupt interrupt)
{
    i_stat |= std::to_underlying(interrupt);
    check_interrupts();
}

u32 read_i_ctrl()
{
    cop0.cause.ip2 = 0;
    return std::exchange(i_ctrl, i_ctrl & ~1);
}

u32 read_i_mask()
{
    return i_mask;
}

u32 read_i_stat()
{
    return i_stat;
}

void reset_branch()
{
}

void write_i_ctrl(u32 value)
{
    i_ctrl = value;
    check_interrupts();
}

void write_i_mask(u32 value)
{
    i_mask = value;
    check_interrupts();
}

void write_i_stat(u32 value)
{
    i_stat &= value;
    cop0.cause.ip2 = i_ctrl & 1 & bool(i_stat & i_mask & 0x3F'FFFF);
}

} // namespace ps1::r3000a

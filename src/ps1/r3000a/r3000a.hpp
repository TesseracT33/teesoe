#pragma once

#include "mips/types.hpp"
#include "types.hpp"

#include <array>
#include <concepts>
#include <filesystem>

namespace ps1::r3000a {

enum class Interrupt : u32 {
    VBlank = 1 << 0,
    GPU = 1 << 1,
    CDROM = 1 << 2,
    DMA = 1 << 3,
    Timer0 = 1 << 4,
    Timer1 = 1 << 5,
    Timer2 = 1 << 6,
    CtrlMemCard = 1 << 7,
    SIO = 1 << 8,
    SPU = 1 << 9,
    Controller = 1 << 10,
};

struct LoadDelay {
    void insert(u32 index, u32 value) { slot[slot_index] = { index, value }; }

private:
    struct Slot {
        u32 index, value;
    } slot[2];
    int slot_index;
} load_delay;

inline mips::Gpr<s32> gpr;
inline bool in_branch_delay_slot;
inline s32 lo, hi, jump_addr, pc, cycle_counter;

void add_initial_events();
void advance_pipeline(u32 cycles);
void check_interrupts();
u64 get_time();
bool init();
void lower_interrupt(Interrupt interrupt);
void jump(u32 target);
bool LoadBios(std::filesystem::path const& path);
void raise_interrupt(Interrupt interrupt);
u32 read_i_ctrl();
u32 read_i_mask();
u32 read_i_stat();
void reset_branch();
void write_i_ctrl(u32 value);
void write_i_mask(u32 value);
void write_i_stat(u32 value);

} // namespace ps1::r3000a

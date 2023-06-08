#include "recompiler.hpp"
#include "asmjit/arm/a64compiler.h"
#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86compiler.h"
#include "bump_allocator.hpp"
#include "cop0.hpp"
#include "disassembler.hpp"
#include "exceptions.hpp"
#include "frontend/message.hpp"
#include "jit_util.hpp"
#include "mmu.hpp"
#include "util.hpp"
#include "vr4300.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <type_traits>
#include <utility>
#include <vector>

using namespace asmjit;
using namespace asmjit::x86;

namespace n64::vr4300 {

struct Block {
    void (*func)();
    void operator()() const { func(); }
};

struct Pool {
    std::array<Block*, 64> blocks;
};

static void BlockEpilogWithPcFlush(int pc_offset);
static void ExecuteBlock(Block* block);
static std::pair<Block*, bool> GetBlock(u32 pc);
static void JumpJit();

constexpr size_t num_pools = 0x80'0000;

static BumpAllocator allocator;
static asmjit::CodeHolder code_holder;
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;

static auto& c = compiler;

void BlockEpilog()
{
    BlockRecordCycles();
    reg_alloc.BlockEpilog();
    c.ret();
}

void BlockEpilogWithJmp(void* func)
{
    BlockRecordCycles();
    reg_alloc.BlockEpilogWithJmp(func);
}

void BlockEpilogWithPcFlush(int pc_offset)
{
    c.mov(ptr(pc), jit_pc + pc_offset);
    BlockEpilog();
}

void BlockProlog()
{
    reg_alloc.BlockProlog();
}

void BlockRecordCycles()
{
    c.add(ptr(cycle_counter), block_cycles);
    c.add(ptr(cop0.count), block_cycles);
}

bool CheckDwordOpCondJit()
{
    if (can_execute_dword_instrs) {
        return true;
    } else {
        BlockEpilogWithJmp(ReservedInstructionException);
        return false;
    }
}

void DiscardBranchJit()
{
    c.mov(ptr(in_branch_delay_slot_taken), 0);
    c.mov(ptr(in_branch_delay_slot_not_taken), 0);
    c.mov(ptr(branch_state), std::to_underlying(BranchState::NoBranch));
    BlockEpilogWithPcFlush(8);
}

void ExecuteBlock(Block* block)
{
    (*block)();
    if (branch_state == BranchState::Perform) {
        in_branch_delay_slot_taken = false;
        branch_state = BranchState::NoBranch;
        pc = jump_addr;
        if (pc & 3) AddressErrorException<MemOp::InstrFetch>(pc);
    } else {
        in_branch_delay_slot_not_taken &= branch_state != BranchState::NoBranch;
        branch_state = branch_state == BranchState::DelaySlotTaken ? BranchState::Perform : BranchState::NoBranch;
    }
}

std::pair<Block*, bool> GetBlock(u32 pc)
{
    static_assert(std::has_single_bit(num_pools));
    pc &= num_pools - 1;
    Pool*& pool = pools[pc >> 8]; // each pool 6 bits, each instruction 2 bits
    if (!pool) pool = reinterpret_cast<Pool*>(allocator.acquire(sizeof(Pool)));
    Block*& block = pool->blocks[pc >> 2 & 63];
    bool compiled = block != nullptr;
    if (!compiled) {
        block = reinterpret_cast<Block*>(allocator.acquire(sizeof(Block)));
    }
    return { block, compiled };
}

Status InitRecompiler()
{
    allocator.allocate(32 * 1024 * 1024);
    pools.resize(num_pools, nullptr);
    return status_ok();
}

void Invalidate(u32 addr)
{
    pools[addr >> 8 & (num_pools - 1)] = nullptr; // each pool 6 bits, each instruction 2 bits
}

void InvalidateRange(u32 addr_lo, u32 addr_hi)
{
    ASSUME(addr_lo <= addr_hi);
    addr_lo = addr_lo >> 8 & (num_pools - 1);
    addr_hi = addr_hi >> 8 & (num_pools - 1);
    std::fill(pools.begin() + addr_lo, pools.begin() + addr_hi, nullptr);
}

void JumpJit()
{
    Label l_end = c.newLabel();
    c.mov(rax, ptr(jump_addr));
    c.mov(ptr(pc), rax);
    c.mov(ptr(branch_state), std::to_underlying(BranchState::NoBranch));
    c.mov(ptr(in_branch_delay_slot_taken), 0);
    c.and_(eax, 3);
    c.test(eax, eax);
    c.je(l_end);
    BlockEpilogWithJmp(AddressErrorException<MemOp::InstrFetch>);
    c.bind(l_end);
}

void LinkJit(u32 reg)
{
    c.mov(reg_alloc.GetHostMarkDirty(reg), jit_pc + 8);
}

void OnBranchNotTakenJit()
{
    c.mov(ptr(in_branch_delay_slot_taken), 0);
    c.mov(ptr(in_branch_delay_slot_not_taken), 1);
    c.mov(ptr(branch_state), std::to_underlying(BranchState::DelaySlotNotTaken));
}

u64 RunRecompiler(u64 cpu_cycles)
{
    cycle_counter = 0;
    while (cycle_counter < cpu_cycles) {
        exception_occurred = false;

        auto [block, compiled] = GetBlock(GetPhysicalPC());
        assert(block);
        if (compiled) {
            ExecuteBlock(block);
        } else {
            branch_hit = branched = false;
            block_cycles = 0;
            jit_pc = pc;

            BlockProlog();

            auto Instr = [] {
                u32 instr = FetchInstruction(jit_pc);
                if (exception_occurred) {
                    // TODO
                }
                disassembler::exec_cpu<CpuImpl::Recompiler>(instr);
                jit_pc += 4;
                block_cycles++;
            };
            Instr();

            // If the previously executed block ended with a branch instruction, meaning that the branch delay
            // slot did not fit, execute only the first instruction in this block, before jumping.
            // The jump can be cancelled if the first instruction is also a branch.
            Label l_end = c.newLabel();
            c.cmp(ptr(branch_state), std::to_underlying(BranchState::Perform));
            c.jne(l_end);
            BlockEpilog();
            c.bind(l_end);

            while (!branched && (jit_pc & 255)) {
                branched = branch_hit; // If the branch delay slot instruction fits within the block boundary, include
                                       // it before stopping
                Instr();
            }

            BlockEpilogWithPcFlush(0);
            c.endFunc();
            c.finalize();
            asmjit::Error err = jit_runtime.add(&block->func, &code_holder);
            if (err) {
                message::error(std::format("Failed to add code to asmjit runtime! Returned error code {}.", err));
            }

            ExecuteBlock(block);
        }
    }
    return cycle_counter - cpu_cycles;
}

void TakeBranchJit(Gp target)
{
    c.mov(ptr(in_branch_delay_slot_taken), 1);
    c.mov(ptr(in_branch_delay_slot_not_taken), 0);
    c.mov(ptr(branch_state), std::to_underlying(BranchState::DelaySlotTaken));
    c.mov(ptr(jump_addr), target);
}

} // namespace n64::vr4300

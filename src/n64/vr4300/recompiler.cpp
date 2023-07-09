#include "recompiler.hpp"
#include "asmjit/arm/a64compiler.h"
#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86compiler.h"
#include "bump_allocator.hpp"
#include "cop0.hpp"
#include "decoder.hpp"
#include "exceptions.hpp"
#include "frontend/message.hpp"
#include "jit_util.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
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

using mips::BranchState;

struct Block {
    void (*func)();
    void operator()() const { func(); }
    bool ends_with_branch_and_delay_slot;
};

// Permute on (dword ops enabled) x (cop0 enabled) x (cop1 enabled) x (cop1 fr bit set)
// 2 x 2 x 3 (cop1 disabled, cop1 enabled + fr = 0, cop1 enabled + fr = 1)
using BlockPack = std::array<Block*, 16>;

struct Pool {
    std::array<BlockPack*, 64> blocks;
};

static void BlockEpilogWithPcFlush(int pc_offset);
static void ExecuteBlock(Block* block);
static std::pair<Block*, bool> GetBlock(u32 pc);

constexpr size_t num_pools = 0x80'0000;

static BumpAllocator allocator;
static asmjit::CodeHolder code_holder;
static asmjit::FileLogger jit_logger(stdout);
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;

static AsmjitCompiler& c = compiler;

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

void BlockEpilogWithJmpAndPcFlush(void* func, int pc_offset)
{
    c.mov(rax, jit_pc + pc_offset);
    c.mov(GlobalVarPtr(pc), rax);
    BlockEpilogWithJmp(func);
}

void BlockEpilogWithPcFlush(int pc_offset)
{
    c.mov(rax, jit_pc + pc_offset);
    c.mov(GlobalVarPtr(pc), rax);
    BlockEpilog();
}

void BlockProlog()
{
    code_holder.reset();
    asmjit::Error err = code_holder.init(jit_runtime.environment(), jit_runtime.cpuFeatures());
    if (err) {
        log_error(
          std::format("Failed to init asmjit code holder; returned {}", asmjit::DebugUtils::errorAsString(err)));
    }
    err = code_holder.attach(&compiler);
    if (err) {
        log_error(std::format("Failed to attach asmjit compiler to code holder; returned {}",
          asmjit::DebugUtils::errorAsString(err)));
    }
    if constexpr (enable_cpu_jit_error_handler) {
        static AsmjitLogErrorHandler asmjit_log_error_handler{};
        code_holder.setErrorHandler(&asmjit_log_error_handler);
    }
    if constexpr (log_cpu_jit_blocks) {
        jit_logger.addFlags(FormatFlags::kMachineCode);
        code_holder.setLogger(&jit_logger);
        jit_logger.log("======== CPU BLOCK BEGIN ========\n");
    }
    c.addFunc(FuncSignatureT<void>());
    reg_alloc.BlockProlog();
}

void BlockRecordCycles()
{
    assert(block_cycles > 0);
    if (block_cycles == 1) {
        c.inc(GlobalVarPtr(cycle_counter));
    } else {
        c.add(GlobalVarPtr(cycle_counter), block_cycles);
    }
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
    c.mov(GlobalVarPtr(in_branch_delay_slot_taken), 0);
    c.mov(GlobalVarPtr(in_branch_delay_slot_not_taken), 0);
    c.mov(GlobalVarPtr(branch_state), std::to_underlying(BranchState::NoBranch));
    BlockEpilogWithPcFlush(8);
}

void ExecuteBlock(Block* block)
{
    (*block)();
    if (branch_state == BranchState::Perform
        || branch_state == BranchState::DelaySlotTaken && block->ends_with_branch_and_delay_slot) {
        PerformBranch();
    } else {
        in_branch_delay_slot_not_taken &= branch_state != BranchState::NoBranch;
        branch_state = branch_state == BranchState::DelaySlotTaken ? BranchState::Perform : BranchState::NoBranch;
    }
}

void FinalizeAndExecuteBlock(Block*& block)
{
    c.endFunc();
    c.finalize();
    asmjit::Error err = jit_runtime.add(&block->func, &code_holder);
    if (err) {
        message::error(std::format("Failed to add code to asmjit runtime! Returned error code {}.", err));
    }

    ExecuteBlock(block);
}

std::pair<Block*, bool> GetBlock(u32 pc)
{
    static_assert(std::has_single_bit(num_pools));
acquire:
    Pool*& pool = pools[pc >> 8 & (num_pools - 1)]; // each pool 6 bits, each instruction 2 bits
    if (!pool) {
        pool = reinterpret_cast<Pool*>(allocator.acquire(sizeof(Pool)));
    }
    BlockPack*& block_pack = pool->blocks[pc >> 2 & 63];
    if (!block_pack) {
        block_pack = reinterpret_cast<BlockPack*>(allocator.acquire(sizeof(BlockPack)));
        if (allocator.ran_out_of_memory_on_last_acquire()) {
            goto acquire;
        }
    }
    int block_perm_idx = can_execute_dword_instrs + 2 * can_exec_cop0_instrs + 4 * cop0.status.cu1 + 8 * cop0.status.fr;
    Block*& block = (*block_pack)[block_perm_idx];
    bool compiled = block != nullptr;
    if (!compiled) {
        block = reinterpret_cast<Block*>(allocator.acquire(sizeof(Block)));
        if (allocator.ran_out_of_memory_on_last_acquire()) {
            goto acquire;
        }
    }
    return { block, compiled };
}

Status InitRecompiler()
{
    allocator.allocate(64_MiB);
    pools.resize(num_pools, nullptr);
    return status_ok();
}

void Invalidate(u32 addr)
{
    if (cpu_impl == CpuImpl::Recompiler) {
        // assert(addr <= 0x1000);
        Pool*& pool = pools[addr >> 8]; // each pool 6 bits, each instruction 2 bits
        if (!pool) return;
        for (BlockPack*& block_pack : pool->blocks) {
            if (!block_pack) continue;
            for (Block*& block : *block_pack) {
                if (block) {
                    jit_runtime.release(block->func);
                    block = nullptr;
                }
            }
            block_pack = nullptr;
        }
        pool = nullptr;
    }
}

void InvalidateRange(u32 addr_lo, u32 addr_hi)
{
    if (cpu_impl == CpuImpl::Recompiler) {
        ASSUME(addr_lo <= addr_hi);
        addr_lo = addr_lo >> 8 & (num_pools - 1);
        addr_hi = addr_hi >> 8 & (num_pools - 1);
        std::fill(pools.begin() + addr_lo, pools.begin() + addr_hi, nullptr);
    }
}

void LinkJit(u32 reg)
{
    c.mov(reg_alloc.GetHostGprMarkDirty(reg), jit_pc + 8);
}

void OnBranchNotTakenJit()
{
    c.mov(GlobalVarPtr(in_branch_delay_slot_taken), 0);
    c.mov(GlobalVarPtr(in_branch_delay_slot_not_taken), 1);
    c.mov(GlobalVarPtr(branch_state), std::to_underlying(BranchState::DelaySlotNotTaken));
}

u32 RunRecompiler(u32 cpu_cycles)
{
    cycle_counter = 0;
    while (cycle_counter < cpu_cycles) {
        exception_occurred = false;

        auto [block, compiled] = GetBlock(GetPhysicalPC());
        assert(block);
        if (compiled) {
            ExecuteBlock(block);
        } else {
            branch_hit = branched = compiler_exception_occurred = false;
            block_cycles = 0;
            jit_pc = pc;

            BlockProlog();

            auto Instr = [] {
                block_cycles++;
                u32 instr = FetchInstruction(jit_pc);
                if (compiler_exception_occurred) return; // todo: handle this. need to compile exception handling
                decoder::exec_cpu<CpuImpl::Recompiler>(instr);
                if (compiler_exception_occurred) return;
                jit_pc += 4;
                if constexpr (log_cpu_jit_register_status) {
                    jit_logger.log(reg_alloc.GetStatusString().c_str());
                }
            };

            Instr();

            if (compiler_exception_occurred) {
                BlockEpilog();
                FinalizeAndExecuteBlock(block);
                continue;
            }

            // If the previously executed block ended with a branch instruction, meaning that the branch delay
            // slot did not fit, execute only the first instruction in this block, before jumping.
            // The jump can be cancelled if the first instruction is also a branch.
            Label l_nobranch = c.newLabel();
            c.cmp(GlobalVarPtr(branch_state), std::to_underlying(BranchState::Perform));
            c.jne(l_nobranch);
            BlockEpilog();
            c.bind(l_nobranch);

            while (!branched && !compiler_exception_occurred && (jit_pc & 255)) {
                branched = branch_hit; // If the branch delay slot instruction fits within the block boundary,
                                       // include it before stopping
                Instr();
            }

            // If the block ends with a branch delay slot instruction, need to signal that we should evaluate
            // jumping immeditely after the execution of this block. Only branches/jumps can set branch_hit.
            block->ends_with_branch_and_delay_slot = branched && branch_hit;

            BlockEpilogWithPcFlush(0);
            FinalizeAndExecuteBlock(block);
        }
    }
    return cycle_counter - cpu_cycles;
}

void TearDownRecompiler()
{
    allocator.deallocate();
    pools.clear();
}

} // namespace n64::vr4300

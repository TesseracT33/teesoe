#include "recompiler.hpp"
#include "asmjit/arm/a64compiler.h"
#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86compiler.h"
#include "bump_allocator.hpp"
#include "cop0.hpp"
#include "decoder.hpp"
#include "exceptions.hpp"
#include "fatal_error.hpp"
#include "frontend/message.hpp"
#include "jit_common.hpp"
#include "mmu.hpp"
#include "n64_build_options.hpp"
#include "vr4300.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <utility>
#include <vector>

using namespace asmjit;
using namespace asmjit::x86;

namespace n64::vr4300 {

constexpr u32 bytes_per_pool = 0x100;
constexpr u32 instructions_per_pool = bytes_per_pool / 4;
constexpr u32 num_pools = 0x80'0000; // 32 bits (address range) - 8 (bits per pool)
constexpr u32 pool_max_addr_excl = (num_pools * bytes_per_pool);
static_assert(std::has_single_bit(pool_max_addr_excl));

using Block = void (*)();

struct Pool {
    std::array<Block, instructions_per_pool> blocks;
};

static BumpAllocator allocator;
static asmjit::CodeHolder code_holder;
static asmjit::FileLogger jit_logger(stdout);
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;
static bool block_has_branch_instr;

static void Compile(Block& block);
static void EmitBranchCheck();
static bool EmitInstruction();
static void FinalizeBlock(Block& block);
static Block& GetBlock(u32 paddr);
static void RecordBlockCycles();
static void ResetPool(Pool*& pool);

void BlockEpilog()
{
    RecordBlockCycles();
    reg_alloc.BlockEpilog();
    c.ret();
}

void BlockEpilogWithJmp(void* func)
{
    RecordBlockCycles();
    reg_alloc.BlockEpilogWithJmp(func);
}

void BlockEpilogWithPcFlushAndJmp(void* func, int pc_offset)
{
    FlushPc(pc_offset);
    BlockEpilogWithJmp(func);
}

void BlockEpilogWithPcFlush(int pc_offset)
{
    FlushPc(pc_offset);
    BlockEpilog();
}

void BlockProlog()
{
    code_holder.reset();
    asmjit::Error err = code_holder.init(jit_runtime.environment(), jit_runtime.cpuFeatures());
    if (err) {
        FATAL("Failed to init asmjit code holder; returned {}", asmjit::DebugUtils::errorAsString(err));
    }
    err = code_holder.attach(&c);
    if (err) {
        FATAL("Failed to attach asmjit compiler to code holder; returned {}", asmjit::DebugUtils::errorAsString(err));
    }
    if constexpr (enable_cpu_jit_error_handler) {
        static AsmjitLogErrorHandler asmjit_log_error_handler;
        code_holder.setErrorHandler(&asmjit_log_error_handler);
    }
    if constexpr (log_cpu_jit_blocks) {
        jit_logger.addFlags(FormatFlags::kMachineCode);
        code_holder.setLogger(&jit_logger);
        jit_logger.log("======== CPU BLOCK BEGIN ========\n");
    }
    FuncNode* func_node = c.addFunc(FuncSignatureT<void>());
    func_node->frame().setAvxEnabled();
    func_node->frame().setAvxCleanup();
    reg_alloc.BlockProlog();
}

bool CheckDwordOpCondJit()
{
    if (can_execute_dword_instrs) {
        return true;
    } else {
        BlockEpilogWithPcFlushAndJmp((void*)ReservedInstructionException);
        branched = true;
        return false;
    }
}

void Compile(Block& block)
{
    branched = block_has_branch_instr = false;
    block_cycles = 0;
    jit_pc = pc;

    BlockProlog();

    bool got_exception = EmitInstruction();

    if (got_exception) {
        BlockEpilog();
        goto compile_end;
    }

    // If the previously executed block ended with a branch instruction, meaning that the branch delay
    // slot did not fit, execute only the first instruction in this block, before jumping.
    // The jump can be cancelled if the first instruction is also a branch.
    if (!last_instr_was_branch) {
        EmitBranchCheck();
    }

    while (!branched && !got_exception && (jit_pc & 255)) {
        branched |= last_instr_was_branch; // If the branch delay slot instruction fits within the block boundary,
                                           // include it before stopping
        got_exception = EmitInstruction();
    }

    if (got_exception) {
        BlockEpilog();
    } else {
        if (!last_instr_was_branch && block_has_branch_instr) {
            EmitBranchCheck();
        }
        BlockEpilogWithPcFlush(0);
    }

compile_end:
    FinalizeBlock(block);
}

void EmitBranchCheck()
{
    Label l_nobranch = c.newLabel();
    c.cmp(JitPtr(branch_state), std::to_underlying(mips::BranchState::DelaySlotTaken));
    c.jne(l_nobranch);
    BlockEpilogWithJmp((void*)PerformBranch);
    c.bind(l_nobranch);
    c.mov(JitPtr(branch_state), std::to_underlying(mips::BranchState::NoBranch));
}

void EmitBranchDiscarded()
{
    c.mov(JitPtr(branch_state), std::to_underlying(mips::BranchState::NoBranch));
    BlockEpilogWithPcFlush(8);
}

void EmitBranchNotTaken()
{
    c.mov(JitPtr(branch_state), std::to_underlying(mips::BranchState::DelaySlotNotTaken));
}

void EmitBranchTaken(u64 target)
{
    c.mov(JitPtr(branch_state), std::to_underlying(mips::BranchState::DelaySlotTaken));
    c.mov(rax, target);
    c.mov(JitPtr(jump_addr), rax);
}

void EmitBranchTaken(HostGpr64 target)
{
    c.mov(JitPtr(branch_state), std::to_underlying(mips::BranchState::DelaySlotTaken));
    c.mov(JitPtr(jump_addr), target);
}

bool EmitInstruction()
{
    block_cycles++;
    last_instr_was_branch = false;
    bool got_exception = false; // TODO
    u32 instr = FetchInstruction(jit_pc);
    if (got_exception) {
        return got_exception; // todo: handle this. need to compile exception handling
    }
    decoder::exec_cpu<CpuImpl::Recompiler>(instr);
    if (got_exception) {
        return got_exception;
    }
    jit_pc += 4;
    block_has_branch_instr |= last_instr_was_branch;
    if constexpr (log_cpu_jit_register_status) {
        jit_logger.log(reg_alloc.GetStatusString().c_str());
    }
    return got_exception;
}

void EmitLink(u32 reg)
{
    Gpq gp = reg_alloc.GetHostGprMarkDirty(reg);
    c.mov(gp, jit_pc + 8);
}

void FinalizeBlock(Block& block)
{
    c.endFunc();
    asmjit::Error err = c.finalize();
    if (err) {
        FATAL("Failed to finalize code block; returned {}", asmjit::DebugUtils::errorAsString(err));
    }
    err = jit_runtime.add(&block, &code_holder);
    if (err) {
        FATAL("Failed to add code to asmjit runtime! Returned {}", err);
    }
}

void FlushPc(int pc_offset)
{
    // Todo:no need to flush pc if jumping to exception handler. check uses
    u64 new_pc = jit_pc + pc_offset;
    s64 new_pc_diff = new_pc - pc;
    if (std::in_range<s32>(new_pc_diff)) {
        c.add(JitPtr(pc), new_pc_diff);
    } else {
        c.mov(rax, new_pc);
        c.mov(JitPtr(pc), rax);
    }
}

Block& GetBlock(u32 paddr)
{
    static_assert(std::has_single_bit(num_pools));
    Pool*& pool = pools[paddr >> 8 & (num_pools - 1)]; // each pool 6 bits, each instruction 2 bits
    if (!pool) {
        pool = allocator.acquire<Pool>(); // TODO: check if OOM
    }
    assert(pool);
    return pool->blocks[paddr >> 2 & 63];
}

Status InitRecompiler()
{
    allocator.allocate(64_MiB);
    pools.resize(num_pools, nullptr);
    return OkStatus();
}

void Invalidate(u32 paddr)
{
    assert(paddr < pool_max_addr_excl);
    ResetPool(pools[paddr >> 8 & (num_pools - 1)]); // each pool 6 bits, each instruction 2 bits
}

void InvalidateRange(u32 paddr_lo, u32 paddr_hi)
{
    assert(paddr_lo <= paddr_hi);
    assert(paddr_hi < pool_max_addr_excl);
    u32 pool_lo = paddr_lo >> 8;
    u32 pool_hi = paddr_hi >> 8;
    for (u32 i = pool_lo; i <= pool_hi; ++i) {
        ResetPool(pools[i]);
    }
}

void RecordBlockCycles()
{
    assert(block_cycles > 0);
    c.add(JitPtr(cycle_counter), block_cycles);
    c.add(JitPtr(cop0.count), block_cycles);
}

void ResetPool(Pool*& pool)
{
    if (pool) {
        for (Block block : pool->blocks) {
            if (block) {
                jit_runtime.release(block);
            }
        }
        pool = nullptr;
    }
}

u32 RunRecompiler(u32 cycles)
{
    cycle_counter = 0;
    while (cycle_counter < cycles) {
        exception_occurred = false;
        Block& block = GetBlock(Devirtualize(pc));
        if (!block) {
            Compile(block);
        }
        block();
    }
    return cycle_counter - cycles;
}

void TearDownRecompiler()
{
    allocator.deallocate();
    pools.clear();
}

} // namespace n64::vr4300

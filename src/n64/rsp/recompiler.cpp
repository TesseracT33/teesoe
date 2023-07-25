#include "recompiler.hpp"
#include "build_options.hpp"
#include "bump_allocator.hpp"
#include "decoder.hpp"
#include "frontend/message.hpp"
#include "interface/mi.hpp"
#include "interpreter.hpp"
#include "n64_build_options.hpp"
#include "rdp/rdp.hpp"
#include "register_allocator.hpp"
#include "rsp.hpp"

#include <algorithm>

using namespace asmjit;
using namespace asmjit::x86;

using mips::BranchState;

namespace n64::rsp {

struct Block {
    void (*func)();
    void operator()() const { func(); }
};

struct Pool {
    std::array<Block*, 64> blocks;
};

static void BlockEpilogWithJmp(void* func);
static void BlockRecordCycles();
static void EmitInstruction();
static void ExecuteBlock(Block* block);
static void FinalizeAndExecuteBlock(Block*& block);
static std::pair<Block*, bool> GetBlock(u32 pc);
static void ResetPool(Pool*& pool);
static void UpdateBranchStateJit();

constexpr u32 pool_size = 0x100;
constexpr size_t num_pools = 0x1000 / pool_size;

static BumpAllocator allocator;
static asmjit::CodeHolder code_holder;
static asmjit::FileLogger jit_logger(stdout);
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;
static bool block_has_branch_instr;

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

void BlockEpilogWithPcFlushAndJmp(void* func, int pc_offset)
{
    c.mov(GlobalVarPtr(pc), jit_pc + pc_offset);
    BlockEpilogWithJmp(func);
}

void BlockEpilogWithPcFlush(int pc_offset)
{
    c.mov(GlobalVarPtr(pc), jit_pc + pc_offset);
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
    if constexpr (enable_rsp_jit_error_handler) {
        static AsmjitLogErrorHandler asmjit_log_error_handler{};
        code_holder.setErrorHandler(&asmjit_log_error_handler);
    }
    if constexpr (log_rsp_jit_blocks) {
        jit_logger.addFlags(FormatFlags::kMachineCode);
        code_holder.setLogger(&jit_logger);
    }
    FuncNode* func_node = c.addFunc(FuncSignatureT<void>());
    func_node->frame().setAvxEnabled();
    func_node->frame().setAvxCleanup();
    if constexpr (avx512) {
        func_node->frame().setAvx512Enabled();
    }
    if constexpr (log_rsp_jit_blocks) {
        jit_logger.log("======== RSP BLOCK BEGIN ========\n");
    }
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

void EmitInstruction()
{
    block_cycles++;
    branch_hit = false;
    u32 instr = FetchInstruction(jit_pc);
    decoder::exec_rsp<CpuImpl::Recompiler>(instr);
    jit_pc = (jit_pc + 4) & 0xFFC;
    block_has_branch_instr |= branch_hit;
    if constexpr (log_rsp_jit_register_status) {
        jit_logger.log(reg_alloc.GetStatusString().c_str());
    }
}

void ExecuteBlock(Block* block)
{
    (*block)();
}

void FinalizeAndExecuteBlock(Block*& block)
{
    c.endFunc();
    asmjit::Error err = c.finalize();
    if (err) {
        message::error(
          std::format("Failed to finalize code block; returned {}", asmjit::DebugUtils::errorAsString(err)));
    }
    err = jit_runtime.add(&block->func, &code_holder);
    if (err) {
        message::error(std::format("Failed to add code to asmjit runtime! Returned error code {}.", err));
    }

    ExecuteBlock(block);
}

std::pair<Block*, bool> GetBlock(u32 pc)
{
acquire:
    Pool*& pool = pools[pc >> 8]; // each pool 6 bits, each instruction 2 bits
    if (!pool) {
        pool = reinterpret_cast<Pool*>(allocator.acquire(sizeof(Pool)));
    }
    Block*& block = pool->blocks[pc >> 2 & 63];
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
    allocator.allocate(16_MiB);
    pools.resize(num_pools, nullptr);
    return status_ok();
}

void Invalidate(u32 addr)
{
    if (cpu_impl == CpuImpl::Recompiler) {
        assert(addr < 0x1000);
        Pool*& pool = pools[addr >> 8]; // each pool 6 bits, each instruction 2 bits
        ResetPool(pool);
    }
}

void InvalidateRange(u32 addr_lo, u32 addr_hi)
{
    if (cpu_impl == CpuImpl::Recompiler) {
        ASSUME(addr_lo <= addr_hi);
        assert(addr_hi <= 0x1000);
        addr_lo = std::min(addr_lo, 0xFFF_u32);
        addr_hi = std::min(addr_hi, 0xFFF_u32);
        addr_lo >>= 8;
        addr_hi >>= 8;
        std::for_each(pools.begin() + addr_lo, pools.begin() + addr_hi + 1, [](Pool*& pool) { ResetPool(pool); });
    }
}

void LinkJit(u32 reg)
{
    compiler.mov(reg_alloc.GetHostGprMarkDirty(reg), (jit_pc + 8) & 0xFFF);
}

void ResetPool(Pool*& pool)
{
    if (!pool) return;
    for (Block*& block : pool->blocks) {
        if (block) {
            jit_runtime.release(block->func);
            block = nullptr;
        }
    }
    pool = nullptr;
}

u32 RunRecompiler(u32 rsp_cycles)
{
    if (sp.status.halted) return 0;
    cycle_counter = 0;
    if (sp.status.sstep) {
        OnSingleStep();
    } else {
        while (cycle_counter < rsp_cycles && !sp.status.halted && !sp.status.sstep) {
            auto [block, compiled] = GetBlock(pc);
            assert(block);
            if (compiled) {
                ExecuteBlock(block);
            } else {
                branched = block_has_branch_instr = false;
                block_cycles = 0;
                jit_pc = pc;

                BlockProlog();

                EmitInstruction();

                // If the previously executed block ended with a branch instruction, meaning that the branch delay
                // slot did not fit, execute only the first instruction in this block, before jumping.
                // The jump can be cancelled if the first instruction is also a branch.
                if (!branch_hit) {
                    UpdateBranchStateJit();
                }

                while (!branched && (jit_pc & 255)) {
                    branched |= branch_hit; // If the branch delay slot instruction fits within the block boundary,
                                            // include it before stopping
                    EmitInstruction();
                }

                if (!branch_hit && block_has_branch_instr) {
                    UpdateBranchStateJit();
                }
                BlockEpilogWithPcFlush(0);
                FinalizeAndExecuteBlock(block);
            }
        }
        if (sp.status.halted) {
            if (jump_is_pending) { // note for future refactors: this makes rsp::op_break::BREAKWithinDelay pass
                PerformBranch();
            }
        } else if (sp.status.sstep) {
            OnSingleStep();
        }
    }
    return cycle_counter <= rsp_cycles ? 0 : cycle_counter - rsp_cycles;
}

void TearDownRecompiler()
{
    allocator.deallocate();
    pools.clear();
}

void UpdateBranchStateJit()
{
    Label l_nobranch = c.newLabel();
    c.cmp(GlobalVarPtr(jump_is_pending), 0);
    c.je(l_nobranch);
    BlockEpilogWithJmp(PerformBranch);
    c.bind(l_nobranch);
}

} // namespace n64::rsp

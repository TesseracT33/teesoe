#include "recompiler.hpp"
#include "build_options.hpp"
#include "bump_allocator.hpp"
#include "decoder.hpp"
#include "fatal_error.hpp"
#include "interpreter.hpp"
#include "n64_build_options.hpp"
#include "register_allocator.hpp"
#include "rsp.hpp"

using namespace asmjit;
using namespace asmjit::x86;

namespace n64::rsp {

constexpr u32 pool_size = 0x100;
constexpr size_t num_pools = 0x1000 / pool_size;

using Block = void (*)();

struct Pool {
    std::array<Block, 64> blocks;
};

static BumpAllocator allocator;
static asmjit::CodeHolder code_holder;
static asmjit::FileLogger jit_logger(stdout);
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;
static bool block_has_branch_instr;

static void Compile(Block& block);
static void EmitBranchCheck();
static void EmitInstruction();
static void FinalizeBlock(Block& block);
static void FlushPc(int pc_offset);
static Block& GetBlock(u32 addr);
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
    // Todo:no need to flush pc if jumping to exception handler
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
    if constexpr (enable_rsp_jit_error_handler) {
        static AsmjitLogErrorHandler asmjit_log_error_handler;
        code_holder.setErrorHandler(&asmjit_log_error_handler);
    }
    if constexpr (log_rsp_jit_blocks) {
        jit_logger.addFlags(FormatFlags::kMachineCode);
        code_holder.setLogger(&jit_logger);
        jit_logger.log("======== RSP BLOCK BEGIN ========\n");
    }
    FuncNode* func_node = c.addFunc(FuncSignatureT<void>());
    func_node->frame().setAvxEnabled();
    func_node->frame().setAvxCleanup();
    if constexpr (platform.avx512) {
        func_node->frame().setAvx512Enabled();
    }
    reg_alloc.BlockProlog();
}

void Compile(Block& block)
{
    branched = block_has_branch_instr = false;
    block_cycles = 0;
    jit_pc = pc;

    BlockProlog();

    EmitInstruction();

    // If the previously executed block ended with a branch instruction, meaning that the branch delay
    // slot did not fit, execute only the first instruction in this block, before jumping.
    // The jump can be cancelled if the first instruction is also a branch.
    if (!last_instr_was_branch) {
        EmitBranchCheck();
    }

    while (!branched && (jit_pc & 255)) {
        branched |= last_instr_was_branch; // If the branch delay slot instruction fits within the block boundary,
                                           // include it before stopping
        EmitInstruction();
    }

    if (!last_instr_was_branch && block_has_branch_instr) {
        EmitBranchCheck();
    }

    BlockEpilogWithPcFlush(0);
    FinalizeBlock(block);
}

void EmitBranchCheck()
{
    Label l_nobranch = c.newLabel();
    c.cmp(JitPtr(jump_is_pending), 0);
    c.je(l_nobranch);
    BlockEpilogWithJmp((void*)PerformBranch);
    c.bind(l_nobranch);
}

void EmitBranchTaken(u32 target)
{
    c.mov(JitPtr(jump_is_pending), 1);
    c.mov(JitPtr(jump_addr), s32(target & 0xFFC));
}

void EmitBranchTaken(HostGpr32 target)
{
    c.mov(JitPtr(jump_is_pending), 1);
    if (target != eax) c.mov(eax, target.r32()); // todo: check if this is needed
    c.and_(eax, 0xFFC);
    c.mov(JitPtr(jump_addr), eax);
}

void EmitInstruction()
{
    block_cycles++;
    last_instr_was_branch = false;
    u32 instr = FetchInstruction(jit_pc);
    decoder::exec_rsp<CpuImpl::Recompiler>(instr);
    jit_pc = (jit_pc + 4) & 0xFFC;
    block_has_branch_instr |= last_instr_was_branch;
    if constexpr (log_rsp_jit_register_status) {
        jit_logger.log(reg_alloc.GetStatusString().c_str());
    }
}

void EmitLink(u32 reg)
{
    Gpq gp = reg_alloc.GetHostGprMarkDirty(reg);
    c.mov(gp, (jit_pc + 8) & 0xFFF);
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
    u32 new_pc = jit_pc + pc_offset;
    s32 new_pc_diff = new_pc - pc;
    if (std::in_range<s8>(new_pc_diff)) {
        c.add(JitPtr(pc), new_pc_diff);
    } else {
        c.mov(JitPtr(pc), new_pc);
    }
}

Block& GetBlock(u32 addr)
{
    static_assert(std::has_single_bit(num_pools));
    Pool*& pool = pools[addr >> 8]; // each pool 6 bits, each instruction 2 bits
    if (!pool) {
        pool = allocator.acquire<Pool>(); // TODO: check if OOM
    }
    assert(pool);
    return pool->blocks[addr >> 2 & 63];
}

Status InitRecompiler()
{
    allocator.allocate(16_MiB);
    pools.resize(num_pools, nullptr);
    return OkStatus();
}

void Invalidate(u32 addr)
{
    assert(addr < 0x1000);
    Pool*& pool = pools[addr >> 8]; // each pool 6 bits, each instruction 2 bits
    ResetPool(pool);
}

void InvalidateRange(u32 addr_lo, u32 addr_hi)
{
    assert(addr_lo <= addr_hi);
    assert(addr_hi <= 0x1000);
    u32 pool_lo = addr_lo >> 8;
    u32 pool_hi = addr_lo >> 8;
    for (u32 i = pool_lo; i <= pool_hi; ++i) {
        ResetPool(pools[i]);
    }
}

void RecordBlockCycles()
{
    assert(block_cycles > 0);
    c.add(JitPtr(cycle_counter), block_cycles);
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

u32 RunRecompiler(u32 rsp_cycles)
{
    if (sp.status.halted) return 0;
    cycle_counter = 0;
    if (sp.status.sstep) {
        OnSingleStep();
    } else {
        while (cycle_counter < rsp_cycles && !sp.status.halted && !sp.status.sstep) {
            Block& block = GetBlock(pc);
            if (!block) {
                Compile(block);
            }
            block();
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

} // namespace n64::rsp

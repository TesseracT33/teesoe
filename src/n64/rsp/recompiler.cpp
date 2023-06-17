#include "recompiler.hpp"
#include "build_options.hpp"
#include "bump_allocator.hpp"
#include "disassembler.hpp"
#include "frontend/message.hpp"
#include "interface/mi.hpp"
#include "interpreter.hpp"
#include "rdp/rdp.hpp"
#include "rsp.hpp"

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

static void BlockEpilogWithPcFlush(int pc_offset);
static void BlockRecordCycles();
static void ExecuteBlock(Block* block);
static std::pair<Block*, bool> GetBlock(u32 pc);

constexpr bool use_avx512 = false;
constexpr size_t num_pools = 16; // imem size (0x1000) / bytes per pool (0x100)

static BumpAllocator allocator;
static asmjit::CodeHolder code_holder;
static asmjit::FileLogger jit_logger(stdout);
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;

static auto& c = compiler;

void BlockEpilog()
{
    BlockRecordCycles();
    reg_alloc.BlockEpilog();
    c.ret();
}

void BlockEpilogWithPcFlush(int pc_offset)
{
    c.mov(eax, jit_pc + pc_offset);
    c.mov(dword_ptr(pc), eax);
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
    if constexpr (enable_jit_error_logging) {
        static AsmjitLogErrorHandler asmjit_log_error_handler{};
        code_holder.setErrorHandler(&asmjit_log_error_handler);
    }
    if constexpr (enable_jit_block_logging) {
        jit_logger.addFlags(FormatFlags::kMachineCode);
        code_holder.setLogger(&jit_logger);
    }
    FuncNode* func_node = c.addFunc(FuncSignatureT<void>());
    func_node->frame().setAvxEnabled();
    func_node->frame().setAvxCleanup();
    if constexpr (use_avx512) {
        func_node->frame().setAvx512Enabled();
    }
    if constexpr (enable_jit_block_logging) {
        jit_logger.log("======== RSP BLOCK BEGIN ========\n");
    }
    reg_alloc.BlockProlog();
}

void BlockRecordCycles()
{
    if (block_cycles > 0) {
        c.mov(rax, &cycle_counter);
        if (block_cycles == 1) {
            c.inc(dword_ptr(rax));
        } else {
            c.add(dword_ptr(rax), block_cycles);
        }
    }
}

void ExecuteBlock(Block* block)
{
    (*block)();
    if (jump_is_pending) {
        pc = jump_addr;
        jump_is_pending = in_branch_delay_slot = false;
        return;
    }
    if (in_branch_delay_slot) {
        jump_is_pending = true;
    }
}

std::pair<Block*, bool> GetBlock(u32 pc)
{
    static_assert(std::has_single_bit(num_pools));
    Pool*& pool = pools[pc >> 8 & (num_pools - 1)]; // each pool 6 bits, each instruction 2 bits
    if (!pool) {
        pool = reinterpret_cast<Pool*>(allocator.acquire(sizeof(Pool)));
    }
    Block*& block = pool->blocks[pc >> 2 & 63];
    bool compiled = block != nullptr;
    if (!compiled) {
        block = reinterpret_cast<Block*>(allocator.acquire(sizeof(Block)));
    }
    return { block, compiled };
}

Status InitRecompiler()
{
    allocator.allocate(1_MiB);
    pools.resize(num_pools, nullptr);
    return status_ok();
}

void Invalidate(u32 addr)
{
    if (cpu_impl == CpuImpl::Recompiler) {
        assert(addr <= 0x1000);
        Pool*& pool = pools[addr >> 8]; // each pool 6 bits, each instruction 2 bits
        for (Block* block : pool->blocks) {
            if (block) {
                jit_runtime.release(block->func);
            }
        }
        pool = nullptr;
    }
}

void InvalidateRange(u32 addr_lo, u32 addr_hi)
{
    if (cpu_impl == CpuImpl::Recompiler) {
        ASSUME(addr_lo <= addr_hi);
        assert(addr_hi <= 0x1000);
        addr_lo >>= 8;
        addr_hi >>= 8;
        std::fill(pools.begin() + addr_lo, pools.begin() + addr_hi, nullptr);
    }
}

void LinkJit(u32 reg)
{
    compiler.mov(reg_alloc.GetHostMarkDirty(reg), (jit_pc + 8) & 0xFFF);
}

u32 RunRecompiler(u32 rsp_cycles)
{
    if (sp.status.halted) return 0;
    cycle_counter = 0;
    if (sp.status.sstep) {
        InterpretOneInstruction();
        sp.status.halted = true;
        if (jump_is_pending) { // note for future refactors: this makes rsp::op_break::BREAKWithinDelay pass
            pc = jump_addr;
            jump_is_pending = in_branch_delay_slot = false;
        }
    } else {
        while (cycle_counter < rsp_cycles) {
            auto [block, compiled] = GetBlock(pc);
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
                    disassembler::exec_rsp<CpuImpl::Recompiler>(instr);
                    jit_pc = (jit_pc + 4) & 0xFFC;
                    block_cycles++;
                };
                Instr();

                // If the previously executed block ended with a branch instruction, meaning that the branch delay
                // slot did not fit, execute only the first instruction in this block, before jumping.
                // The jump can be cancelled if the first instruction is also a branch.
                Label l_end = c.newLabel();
                c.mov(al, ptr(in_branch_delay_slot));
                c.test(al, al);
                c.je(l_end);
                BlockEpilog();
                c.bind(l_end);

                while (!branched && (jit_pc & 255)) {
                    branched = branch_hit; // If the branch delay slot instruction fits within the block boundary,
                                           // include it before stopping
                    Instr();
                }

                BlockEpilogWithPcFlush(0);
                c.endFunc();
                asmjit::Error err = c.finalize();
                if (err) {
                    message::error(std::format("Failed to finalize code block; returned {}",
                      asmjit::DebugUtils::errorAsString(err)));
                }
                err = jit_runtime.add(&block->func, &code_holder);
                if (err) {
                    message::error(std::format("Failed to add code to asmjit runtime; returned {}",
                      asmjit::DebugUtils::errorAsString(err)));
                }

                ExecuteBlock(block);
            }
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

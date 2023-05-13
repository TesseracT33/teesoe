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
#include "mips/register_allocator.hpp"
#include "mmu.hpp"
#include "vr4300.hpp"

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
    u16 cycles;
    void operator()() const { func(); }
};

struct Pool {
    std::array<Block*, 64> blocks;
};

static void FinalizeBlock(Block* block);
static std::pair<Block*, bool> GetBlock(u32 pc);

constexpr size_t num_pools = 0x80'0000;

constexpr std::array right_load_mask = {
    0xFFFF'FFFF'FFFF'FF00ull,
    0xFFFF'FFFF'FFFF'0000ull,
    0xFFFF'FFFF'FF00'0000ull,
    0xFFFF'FFFF'0000'0000ull,
    0xFFFF'FF00'0000'0000ull,
    0xFFFF'0000'0000'0000ull,
    0xFF00'0000'0000'0000ull,
    0ull,
};

static BumpAllocator allocator;
static bool branch_hit, branched;
static u64 cycles;
static asmjit::CodeHolder code_holder;
static asmjit::JitRuntime jit_runtime;
static std::vector<Pool*> pools;

void FinalizeBlock(Block* block)
{
    compiler.ret();
    compiler.endFunc();
    compiler.finalize();
    asmjit::Error err = jit_runtime.add(&block->func, &code_holder);
    if (err) {
        message::error(std::format("Failed to add code to asmjit runtime! Returned error code {}.", err));
    }
    code_holder.reset();
}

std::pair<Block*, bool> GetBlock(u32 pc)
{
    static_assert(std::has_single_bit(num_pools));
    pc &= (num_pools - 1);
    Pool*& pool = pools[pc >> 8]; // each pool 6 bits, each instruction 2 bits
    if (!pool) pool = reinterpret_cast<Pool*>(allocator.acquire(sizeof(Pool)));
    Block*& block = pool->blocks[pc >> 2 & 63];
    bool compiled = block != nullptr;
    if (!compiled) {
        block = reinterpret_cast<Block*>(allocator.acquire(sizeof(Block)));
        compiler.addFunc(asmjit::FuncSignatureT<void>());
        branch_hit = branched = false;
        cycles = 0;
    }
    return { block, compiled };
}

Status InitRecompiler()
{
    asmjit::Error err = code_holder.init(jit_runtime.environment(), jit_runtime.cpuFeatures());
    if (err) {
        return status_failure(
          std::format("Failed to init asmjit codeholder with jit runtime information; returned error code {}.", err));
    }
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
}

void JitInstructionEpilogue()
{
    // TODO: optimize to only be done right before branch instruction or at the end of a block?
    compiler.add(ptr(pc), 4);
}

void JitInstructionEpilogueFirstBlockInstruction()
{
    auto& c = compiler;
    Gp v0 = c.newGpw();
    Label l_exit = c.newLabel();
    c.cmp(ptr(in_branch_delay_slot_taken), 0);
    c.je(l_exit);
    Gp v1 = c.newGpq();
    c.mov(ptr(in_branch_delay_slot_taken), 0);
    c.mov(v1, ptr(jump_addr));
    c.mov(ptr(pc), v1);
    c.ret();
    c.bind(l_exit);
    c.add(ptr(pc), 4);
}

void JumpRecompiler()
{ // Assumption: target_address is in rax
  // TODO
  // auto& c = compiler;
  // c.mov(mem(jump_is_pending), 1);
  // c.
}

void OnBranchJit()
{
}

u64 RunRecompiler(u64 cpu_cycles)
{
    // TODO: made change in interpreter to increment pc after instr execution
    auto exec_block = [](Block* block) {
        (*block)();
        AdvancePipeline(block->cycles);
    };

    cycle_counter = 0;
    while (cycle_counter < cpu_cycles) {
        u32 physical_pc = GetPhysicalPC();
        auto [block, compiled] = GetBlock(physical_pc);
        assert(block);
        if (compiled) {
            exec_block(block);
        } else {
            auto Instr = [addr = pc]() mutable {
                u32 instr = FetchInstruction(addr);
                addr += 4;
                disassembler::exec_cpu<CpuImpl::Recompiler>(instr);
                cycles++;
            };
            Instr();
            JitInstructionEpilogueFirstBlockInstruction();
            while (!branched && (pc & 255)) {
                // If the branch delay slot instruction fits within the block boundary, include it before stopping
                branched = branch_hit;
                Instr();
                JitInstructionEpilogue();
            }
            block->cycles = cycles;
            FinalizeBlock(block);
            exec_block(block);
        }
    }
    return cycle_counter - cpu_cycles;
}

void Recompiler::beq(u32 rs, u32 rt, s16 imm) const
{
}

void Recompiler::beql(u32 rs, u32 rt, s16 imm) const
{
}

void Recompiler::bgez(u32 rs, s16 imm) const
{
}

void Recompiler::bgezal(u32 rs, s16 imm) const
{
}

void Recompiler::bgezall(u32 rs, s16 imm) const
{
}

void Recompiler::bgezl(u32 rs, s16 imm) const
{
}

void Recompiler::bgtz(u32 rs, s16 imm) const
{
}

void Recompiler::bgtzl(u32 rs, s16 imm) const
{
}

void Recompiler::blez(u32 rs, s16 imm) const
{
}

void Recompiler::blezl(u32 rs, s16 imm) const
{
}

void Recompiler::bltz(u32 rs, s16 imm) const
{
}

void Recompiler::bltzal(u32 rs, s16 imm) const
{
}

void Recompiler::bltzall(u32 rs, s16 imm) const
{
}

void Recompiler::bltzl(u32 rs, s16 imm) const
{
}

void Recompiler::bne(u32 rs, u32 rt, s16 imm) const
{
}

void Recompiler::bnel(u32 rs, u32 rt, s16 imm) const
{
}

void Recompiler::break_() const
{
    call(c, BreakpointException);
    OnBranchJit();
}

void Recompiler::ddiv(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    c.mov(rax, gpr_ptr(rs));
    c.mov(rcx, gpr_ptr(rt));
    c.test(rcx, rcx);
    c.je(l_divzero);
    c.mov(r8, rax);
    c.mov(r9, rcx);
    c.xor_(r8, 1LL << 63);
    c.not_(r9);
    c.or_(r8, r9);
    c.jne(l_div);
    c.mov(ptr(lo), 1LL << 63);
    c.mov(ptr(hi), 0);
    c.jmp(l_end);
    c.bind(l_divzero);
    c.mov(ptr(hi), rax);
    c.not_(rax);
    c.sar(rax, 63);
    c.or_(rax, 1);
    c.mov(ptr(lo), rax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.idiv(rax, rcx);
    c.mov(ptr(lo), rax);
    c.mov(ptr(hi), rdx);
    c.bind(l_end);
    cycles += 68;
}

void Recompiler::ddivu(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    c.mov(rax, gpr_ptr(rs));
    c.mov(rcx, gpr_ptr(rt));
    c.test(rcx, rcx);
    c.jne(l_div);
    c.mov(ptr(lo), -1);
    c.mov(ptr(hi), rax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.div(rax, rcx);
    c.mov(ptr(lo), rax);
    c.mov(ptr(hi), rdx);
    c.bind(l_end);
    cycles += 68;
}

void Recompiler::div(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_divzero = c.newLabel(), l_end = c.newLabel();
    c.mov(eax, gpr_ptr32(rs));
    c.mov(ecx, gpr_ptr32(rt));
    c.test(ecx, ecx);
    c.je(l_divzero);
    c.mov(r8d, eax);
    c.mov(r9d, ecx);
    c.add(r8d, s32(0x8000'0000));
    c.not_(r9d);
    c.or_(r8d, r9d);
    c.jne(l_div);
    c.mov(ptr(lo), s32(0x8000'0000));
    c.mov(ptr(hi), 0);
    c.jmp(l_end);
    c.bind(l_divzero);
    set_hi32(eax);
    c.not_(eax);
    c.sar(eax, 31);
    c.or_(eax, 1);
    set_lo32(eax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.idiv(eax, ecx);
    set_lo32(eax);
    set_hi32(edx);
    c.bind(l_end);
    cycles += 36;
}

void Recompiler::divu(u32 rs, u32 rt) const
{
    Label l_div = c.newLabel(), l_end = c.newLabel();
    c.mov(eax, gpr_ptr32(rs));
    c.mov(ecx, gpr_ptr32(rt));
    c.test(ecx, ecx);
    c.jne(l_div);
    c.mov(ptr(lo), -1);
    set_hi32(eax);
    c.jmp(l_end);
    c.bind(l_div);
    c.xor_(edx, edx);
    c.div(eax, ecx);
    set_lo32(eax);
    set_hi32(edx);
    c.bind(l_end);
    cycles += 36;
}

void Recompiler::dmult(u32 rs, u32 rt) const
{
    multiply64<false>(rs, rt);
}

void Recompiler::dmultu(u32 rs, u32 rt) const
{
    multiply64<true>(rt, rt);
}

void Recompiler::j(u32 instr) const
{
    // TODO
}

void Recompiler::jal(u32 instr) const
{
    // TODO
}

void Recompiler::jalr(u32 rs, u32 rd) const
{
    // TODO
}

void Recompiler::jr(u32 rs) const
{
    // TODO
}

void Recompiler::lb(u32 rs, u32 rt, s16 imm) const
{
    load<s8>(rs, rt, imm);
}

void Recompiler::lbu(u32 rs, u32 rt, s16 imm) const
{
    load<u8>(rs, rt, imm);
}

void Recompiler::ld(u32 rs, u32 rt, s16 imm) const
{
    load<s64>(rs, rt, imm);
}

void Recompiler::ldl(u32 rs, u32 rt, s16 imm) const
{
    load_left<s64>(rs, rt, imm);
}

void Recompiler::ldr(u32 rs, u32 rt, s16 imm) const
{
    load_right<s64>(rs, rt, imm);
}

void Recompiler::lh(u32 rs, u32 rt, s16 imm) const
{
    load<s16>(rs, rt, imm);
}

void Recompiler::lhu(u32 rs, u32 rt, s16 imm) const
{
    load<u16>(rs, rt, imm);
}

void Recompiler::ll(u32 rs, u32 rt, s16 imm) const
{
    load_linked<s32>(rs, rt, imm);
}

void Recompiler::lld(u32 rs, u32 rt, s16 imm) const
{
    load_linked<s64>(rs, rt, imm);
}

void Recompiler::lw(u32 rs, u32 rt, s16 imm) const
{
    load<s32>(rs, rt, imm);
}

void Recompiler::lwl(u32 rs, u32 rt, s16 imm) const
{
    load_left<s32>(rs, rt, imm);
}

void Recompiler::lwr(u32 rs, u32 rt, s16 imm) const
{
    load_right<s32>(rs, rt, imm);
}

void Recompiler::lwu(u32 rs, u32 rt, s16 imm) const
{
    load<u32>(rs, rt, imm);
}

void Recompiler::mult(u32 rs, u32 rt) const
{
    multiply32<false>(rs, rt);
}

void Recompiler::multu(u32 rs, u32 rt) const
{
    multiply32<true>(rs, rt);
}

void Recompiler::sb(u32 rs, u32 rt, s16 imm) const
{
    store<s8>(rs, rt, imm);
}

void Recompiler::sc(u32 rs, u32 rt, s16 imm) const
{
    store_conditional<s32>(rs, rt, imm);
}

void Recompiler::scd(u32 rs, u32 rt, s16 imm) const
{
    store_conditional<s64>(rs, rt, imm);
}

void Recompiler::sd(u32 rs, u32 rt, s16 imm) const
{
    store<s64>(rs, rt, imm);
}

void Recompiler::sdl(u32 rs, u32 rt, s16 imm) const
{
    store_left<s64>(rs, rt, imm);
}

void Recompiler::sdr(u32 rs, u32 rt, s16 imm) const
{
    store_right<s64>(rs, rt, imm);
}

void Recompiler::sh(u32 rs, u32 rt, s16 imm) const
{
    store<s16>(rs, rt, imm);
}

void Recompiler::sync() const
{
    /* Completes the Load/store instruction currently in the pipeline before the new
       load/store instruction is executed. Is executed as a NOP on the VR4300. */
}

void Recompiler::syscall() const
{
    call(c, SyscallException);
    OnBranchJit();
}

void Recompiler::sw(u32 rs, u32 rt, s16 imm) const
{
    store<s32>(rs, rt, imm);
}

void Recompiler::swl(u32 rs, u32 rt, s16 imm) const
{
    store_left<s32>(rs, rt, imm);
}

void Recompiler::swr(u32 rs, u32 rt, s16 imm) const
{
    store_right<s32>(rs, rt, imm);
}

template<std::integral Int> void Recompiler::load(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    call(c, ReadVirtual<std::make_signed_t<Int>>);
    if (rt) {
        Label l_end = c.newLabel();
        c.cmp(ptr(exception_occurred), 0);
        c.jne(l_end);
        if constexpr (std::same_as<Int, s32>) {
            c.cdqe(rax);
            c.mov(gpr_ptr(rt), rax);
        } else if constexpr (sizeof(Int) == 8) {
            c.mov(gpr_ptr(rt), rax);
        } else {
            if constexpr (std::same_as<Int, s8>) c.movsx(rax, al);
            if constexpr (std::same_as<Int, u8>) c.movzx(rax, al);
            if constexpr (std::same_as<Int, s16>) c.movsx(rax, ax);
            if constexpr (std::same_as<Int, u16>) c.movzx(rax, ax);
            if constexpr (std::same_as<Int, u32>) c.mov(eax, eax);
            c.mov(gpr_ptr(rt), rax);
        }
        c.bind(l_end);
    }
}

template<std::integral Int> void Recompiler::load_left(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    if (rt) {
        Label l_end = c.newLabel();
        c.push(rbx);
        c.mov(rbx, gp[0]);
        call_no_stack_alignment(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedLeft>);
        c.cmp(ptr(exception_occurred), 0);
        c.jne(l_end);
        c.mov(ecx, ebx);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        c.shl(rax, cl);
        c.mov(r8d, 1);
        c.shl(r8, cl);
        c.dec(r8);
        c.and_(r8, gpr_ptr(rt));
        c.or_(rax, r8);
        c.mov(gpr_ptr(rt), rax);
        c.bind(l_end);
        c.pop(rbx);
    } else {
        call(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedLeft>);
    }
}

template<std::integral Int> void Recompiler::load_linked(u32 rs, u32 rt, s16 imm) const
{
    load<Int>(rs, rt, imm);
    c.mov(eax, ptr(last_physical_address_on_load));
    c.shl(eax, 4);
    c.mov(ptr(cop0.ll_addr), eax);
    c.mov(ptr(ll_bit), 1);
}

template<std::integral Int> void Recompiler::load_right(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    if (rt) {
        Label l_end = c.newLabel();
        c.push(rbx);
        c.mov(rbx, gp[0]);
        call_no_stack_alignment(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedRight>);
        c.cmp(ptr(exception_occurred), 0);
        c.jne(l_end);
        c.mov(ecx, ebx);
        c.not_(ecx);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        if constexpr (sizeof(Int) == 4) {
            c.sar(eax, cl);
            c.mov(ebx, 0xFFFF'FF00);
        } else {
            c.sar(rax, cl);
        }

        // TODO
        // c.mov(r8d, 1);
        // c.shl(r8, cl);
        // c.dec(r8);
        // c.and_(r8, gpr_ptr(rt));
        // c.or_(rax, r8);
        // c.mov(gpr_ptr(rt), rax);
        // c.bind(l_end);
        // c.pop(rbx);
    } else {
        call(c, ReadVirtual<std::make_signed_t<Int>, Alignment::UnalignedRight>);
    }
}

template<bool unsig> void Recompiler::multiply32(u32 rs, u32 rt) const
{
    Gp v = get_gpr32(rt);
    c.mov(eax, gpr_ptr32(rs));
    if constexpr (unsig) c.mul(eax, v);
    else c.imul(eax, v);
    set_lo32(eax);
    set_hi32(edx);
    cycles += 4;
}

template<bool unsig> void Recompiler::multiply64(u32 rs, u32 rt) const
{
    Gp v = get_gpr(rt);
    c.mov(rax, gpr_ptr(rs));
    if constexpr (unsig) c.mul(rax, v);
    else c.imul(rax, v);
    c.mov(ptr(lo), rax);
    c.mov(ptr(hi), rdx);
    cycles += 7;
}

template<std::integral Int> void Recompiler::store(u32 rs, u32 rt, s16 imm) const
{
    c.mov(gp[0], gpr_ptr(rs));
    c.add(gp[0], imm);
    c.mov(gp[1], gpr_ptr(rt));
    call(c, WriteVirtual<sizeof(Int)>);
}

template<std::integral Int> void Recompiler::store_conditional(u32 rs, u32 rt, s16 imm) const
{
    Label l_end = c.newLabel();
    c.cmp(ptr(ll_bit), 0);
    c.je(l_end);
    store<Int>(rs, rt, imm);
    c.bind(l_end);
    if (rt) {
        c.movzx(eax, ptr(ll_bit));
        set_gpr(rt, rax);
    }
}

template<std::integral Int> void Recompiler::store_left(u32 rs, u32 rt, s16 imm) const
{
    if constexpr (os.windows) {
        c.mov(eax, gpr_ptr(rs));
        c.add(eax, imm);
        c.mov(ecx, eax);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        c.mov(gp[1], gpr_ptr(rt));
        c.shr(gp[1], cl);
        c.mov(gp[0], eax);
    } else {
        c.mov(gp[0], gpr_ptr(rs));
        c.add(gp[0], imm);
        c.mov(ecx, gp[0]);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        c.mov(gp[1], gpr_ptr(rt));
        c.shr(gp[1], cl);
    }
    call(c, WriteVirtual<sizeof(Int), Alignment::UnalignedLeft>);
}

template<std::integral Int> void Recompiler::store_right(u32 rs, u32 rt, s16 imm) const
{
    if constexpr (os.windows) {
        c.mov(eax, gpr_ptr(rs));
        c.add(eax, imm);
        c.mov(ecx, eax);
        c.not_(ecx);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        c.mov(gp[1], gpr_ptr(rt));
        c.shl(gp[1], cl);
        c.mov(gp[0], eax);
    } else {
        c.mov(gp[0], gpr_ptr(rs));
        c.add(gp[0], imm);
        c.mov(ecx, gp[0]);
        c.not_(ecx);
        c.and_(ecx, sizeof(Int) - 1);
        c.shl(ecx, 3);
        c.mov(gp[1], gpr_ptr(rt));
        c.shl(gp[1], cl);
    }
    call(c, WriteVirtual<sizeof(Int), Alignment::UnalignedRight>);
}

} // namespace n64::vr4300

#include "jit.hpp"
#include "frontend/message.hpp"
#include "log.hpp"

#include <algorithm>
#include <format>

Jit::Jit()
  : allocator(32 * 1024 * 1024),
    branch_hit(false),
    code(),
    compiler(&code),
    cycles(0),
    pools(0x7F'FFFF, {}),
    runtime()
{
    asmjit::Error err = code.init(runtime.environment(), runtime.cpuFeatures());
    if (err) {
        message::error(
          std::format("Failed to init asmjit codeholder with runtime information! Returned error code {}.", err));
    }
}

void Jit::finalize_block(Jit::Block* block)
{
    compiler.ret();
    compiler.endFunc();
    compiler.finalize();
    asmjit::Error err = runtime.add(&block->func, &code);
    if (err) {
        message::error(std::format("Failed to add code to asmjit runtime! Returned error code {}.", err));
    }
    code.reset();
}

std::pair<Jit::Block*, bool> Jit::get_block(u32 pc)
{
    pc &= 0x7FF'FFFF;
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

void Jit::invalidate(u32 pc)
{
    pools[pc >> 8 & 0x7F'FFFF] = nullptr; // each pool 6 bits, each instruction 2 bits
}

Jit::Allocator::Allocator(size_t size) : memory(size, 0), index(0)
{
}

u8* Jit::Allocator::acquire(size_t size)
{
    if (index >= size) {
        log_warn("[JIT] ran out of space for pool allocator; resetting all available memory.");
        std::ranges::fill(memory, 0);
        index = 0;
    }
    u8* ret = &memory[index];
    index += size;
    return ret;
}

#pragma once

#include "types.hpp"

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86compiler.h"

#include <array>
#include <cassert>
#include <vector>

class Jit {
    struct Block;

    using Func = void (*)();

    struct Pool {
        std::array<Block*, 64> blocks;
    };

public:
    struct Block {
        Func fun;
        u64 end_virt_pc;
        u16 cycles;
    };

    Jit() : stop_block(false) {}

    void finalize_block()
    {
        compiler.ret();
        compiler.endFunc(); // End of the function body.
        compiler.finalize(); // Translate and assemble the whole 'cc' content.
        // ----> x86::Compiler is no longer needed from here and can be destroyed <----
        asmjit::Error err = runtime.add(&current_block->fun, &code); // Add the generated code to the runtime.
        // if (err) return 1;
    }

    Block* get_block(u32 pc)
    {
        pc &= 0x7FFF'FFFF;
        Pool*& pool = pools[pc >> 8]; // each pool 6 bits, each instruction 2 bits
        if (!pool) pool = reinterpret_cast<Pool*>(allocator.acquire(sizeof(Pool)));
        Block* block = pool->blocks[pc >> 2 & 63];
        // if (!block) block = (Block*)emit(pc);
        return block;
    }

    void init()
    {
        code.init(runtime.environment(), // Initialize code to match the JIT environment.
          runtime.cpuFeatures());
        pools.resize(0x7F'FFFF, {});
    }

    Block* init_block()
    {
        code.reset();
        compiler.addFunc(asmjit::FuncSignatureT<void>());
        return nullptr;
    }

    void invalidate(u32 pc)
    {
        pc &= 0x7FFF'FFFF;
        pools[pc >> 8] = nullptr; // each pool 6 bits, each instruction 2 bits
    }

    asmjit::x86::Compiler compiler{ &code };

    bool stop_block;

private:
    struct Allocator {
        Allocator(size_t size) : memory(size, 0), index(0) {}
        void* acquire(size_t size)
        {
            void* ret = &memory[index];
            index += size;
            assert(index <= size); // should never fail
            return ret;
        }

    private:
        std::vector<u8> memory;
        size_t index;
    } allocator{ 4 * 1024 * 1024 };

    asmjit::CodeHolder code; // Holds code and relocation information.
    asmjit::JitRuntime runtime;
    std::vector<Pool*> pools;
    Block* current_block;
};

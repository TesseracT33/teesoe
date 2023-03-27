#pragma once

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86compiler.h"
#include "types.hpp"

#include <array>
#include <utility>
#include <vector>

class Jit {
    using Func = void (*)();

public:
    struct Block {
        Func func;
        u16 cycles;
    };

    Jit();

    void finalize_block(Block* block);
    std::pair<Block*, bool> get_block(u32 pc);
    void invalidate(u32 pc);

    asmjit::x86::Compiler compiler;
    bool branch_hit, branched;
    u64 cycles;

private:
    struct Allocator {
        Allocator(size_t size);
        u8* acquire(size_t size);

    private:
        std::vector<u8> memory;
        size_t index;
    };

    struct Pool {
        std::array<Block*, 64> blocks;
    };

    Allocator allocator;
    asmjit::CodeHolder code; // Holds code and relocation information.
    asmjit::JitRuntime runtime;
    std::vector<Pool*> pools;
};

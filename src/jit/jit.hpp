#pragma once

#include "asmjit/core/codeholder.h"
#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86compiler.h"
#include "bump_allocator.hpp"
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
    struct Pool {
        std::array<Block*, 64> blocks;
    };

    BumpAllocator allocator;
    asmjit::CodeHolder code; // Holds code and relocation information.
    asmjit::JitRuntime runtime;
    std::vector<Pool*> pools;
};

constexpr std::array gp = {
#ifdef _WIN32
    asmjit::x86::rcx,
    asmjit::x86::rdx,
    asmjit::x86::r8,
    asmjit::x86::r9,
    asmjit::x86::r10,
    asmjit::x86::r11,
    asmjit::x86::rax,
#else
    asmjit::x86::rdi,
    asmjit::x86::rsi,
    asmjit::x86::rdx,
    asmjit::x86::rcx,
    asmjit::x86::r8,
    asmjit::x86::r9,
    asmjit::x86::r10,
    asmjit::x86::r11,
    asmjit::x86::rax,
#endif
};

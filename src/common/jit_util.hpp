#pragma once

#include "asmjit/x86.h"
#include "asmjit/x86/x86compiler.h"
#include "host.hpp"
#include "types.hpp"

#include <array>
#include <bit>

inline constexpr std::array gp = {
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

#if X64

inline void call(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        c.push(rax);
        c.call(func);
        c.pop(rcx);
    } else {
        c.sub(rsp, 40);
        c.call(func);
        c.add(rsp, 40);
    }
}

inline void call_no_stack_alignment(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
    if constexpr (os.linux) {
        c.call(func);
    } else {
        c.sub(rsp, 32);
        c.call(func);
        c.add(rsp, 32);
    }
}

constexpr asmjit::x86::Mem ptr(auto const& obj)
{
    return asmjit::x86::ptr(std::bit_cast<u64>(&obj), sizeof(obj));
}

constexpr asmjit::x86::Mem byte_ptr(auto const& obj)
    requires(sizeof(obj) == 1)
{
    return asmjit::x86::byte_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem word_ptr(auto const& obj)
    requires(sizeof(obj) == 2)
{
    return asmjit::x86::word_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem dword_ptr(auto const& obj)
    requires(sizeof(obj) == 4)
{
    return asmjit::x86::dword_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem qword_ptr(auto const& obj)
    requires(sizeof(obj) == 8)
{
    return asmjit::x86::qword_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem xmm_ptr(auto const& obj)
    requires(sizeof(obj) == 16)
{
    return asmjit::x86::xmmword_ptr(std::bit_cast<u64>(&obj));
}

constexpr asmjit::x86::Mem ymm_ptr(auto const& obj)
    requires(sizeof(obj) == 32)
{
    return asmjit::x86::ymmword_ptr(std::bit_cast<u64>(&obj));
}

#endif

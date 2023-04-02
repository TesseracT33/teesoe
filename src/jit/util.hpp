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

template<uint qwords_pushed = 0> inline void call(asmjit::x86::Compiler& c, auto func)
{
    using namespace asmjit::x86;
#ifdef _WIN32
    static constexpr uint bytes = qwords_pushed % 2 ? 32 : 40; // stack alignment + shadow space
    c.sub(rsp, bytes);
    c.call(func);
    c.add(rsp, bytes);
#else
    if constexpr (qwords_pushed % 2) {
        c.call(func);
    } else {
        c.push(rax);
        c.call(func);
        c.pop(rcx);
    }
#endif
}

template<typename T> constexpr asmjit::x86::Mem ptr(T const& t)
{
    return asmjit::x86::ptr(std::bit_cast<u64>(&t), sizeof(T));
}

template<typename T>
constexpr asmjit::x86::Mem byte_ptr(T const& t)
    requires(sizeof(T) == 1)
{
    return asmjit::x86::byte_ptr(std::bit_cast<u64>(&t));
}

template<typename T>
constexpr asmjit::x86::Mem word_ptr(T const& t)
    requires(sizeof(T) == 2)
{
    return asmjit::x86::word_ptr(std::bit_cast<u64>(&t));
}

template<typename T>
constexpr asmjit::x86::Mem dword_ptr(T const& t)
    requires(sizeof(T) == 4)
{
    return asmjit::x86::dword_ptr(std::bit_cast<u64>(&t));
}

template<typename T>
constexpr asmjit::x86::Mem qword_ptr(T const& t)
    requires(sizeof(T) == 8)
{
    return asmjit::x86::qword_ptr(std::bit_cast<u64>(&t));
}

template<typename T>
constexpr asmjit::x86::Mem xmm_ptr(T const& t)
    requires(sizeof(T) == 16)
{
    return asmjit::x86::xmmword_ptr(std::bit_cast<u64>(&t));
}

template<typename T>
constexpr asmjit::x86::Mem ymm_ptr(T const& t)
    requires(sizeof(T) == 32)
{
    return asmjit::x86::ymmword_ptr(std::bit_cast<u64>(&t));
}

#endif

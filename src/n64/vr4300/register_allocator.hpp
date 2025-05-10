#pragma once

#include "jit_common.hpp"
#include "mips/register_allocator_state.hpp"
#include "numtypes.hpp"
#include "platform.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <span>
#include <stack>
#include <string>

namespace n64::vr4300 {

constexpr std::array reg_alloc_volatile_gprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{ x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array{ rcx, rdx, rdi, rsi, r8, r9, r10, r11 };
        }
        if constexpr (platform.abi.win64) {
            return std::array{ rcx, rdx, r8, r9, r10, r11 };
        }
    }
}();

constexpr std::array reg_alloc_nonvolatile_gprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{ x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array{ rbx, r12, r13, r14, r15 };
        }
        if constexpr (platform.abi.win64) {
            return std::array{ rdi, rsi, rbx, r12, r13, r14, r15 };
        }
    }
}();

constexpr std::array reg_alloc_volatile_vprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{
            v0,
            v1,
            v2,
            v3,
            v4,
            v5,
            v6,
            v7,
            v16,
            v17,
            v18,
            v19,
            v20,
            v21,
            v22,
            v23,
            v24,
            v25,
            v26,
            v27,
            v28,
            v29,
            v30,
            v31,
        };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        std::array avx2_regs = [] {
            if constexpr (platform.abi.systemv) {
                return std::array{
                    xmm0,
                    xmm1,
                    xmm2,
                    xmm3,
                    xmm4,
                    xmm5,
                    xmm6,
                    xmm7,
                    xmm8,
                    xmm9,
                    xmm10,
                    xmm11,
                    xmm12,
                    xmm13,
                    xmm14,
                    xmm15,
                };
            }
            if constexpr (platform.abi.win64) {
                return std::array{ xmm0, xmm1, xmm2, xmm3, xmm4, xmm5 };
            }
        }();
        if constexpr (platform.avx512) {
            std::array avx512_regs = {
                xmm16,
                xmm17,
                xmm18,
                xmm19,
                xmm20,
                xmm21,
                xmm22,
                xmm23,
                xmm24,
                xmm25,
                xmm26,
                xmm27,
                xmm28,
                xmm29,
                xmm30,
                xmm31,
            };
            // TODO: possibly use in c++26
            // return std::ranges::to<std::array>(std::ranges::views::concat(avx2_regs, avx512_regs));
            std::array<Xmm, avx2_regs.size() + avx512_regs.size()> result;
            std::copy(avx2_regs.begin(), avx2_regs.end(), result.begin());
            std::copy(avx512_regs.begin(), avx512_regs.end(), result.begin() + avx2_regs.size());
            return result;
        } else {
            return avx2_regs;
        }
    }
}();

constexpr std::array reg_alloc_nonvolatile_vprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{ v8, v9, v10, v11, v12, v13, v14, v15 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array<Xmm, 0>{};
        }
        if constexpr (platform.abi.win64) {
            return std::array{ xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 };
        }
    }
}();

constexpr HostGpr64 guest_gpr_mid_ptr_reg = [] {
    if constexpr (platform.a64) return asmjit::a64::x17;
    if constexpr (platform.x64) return asmjit::x86::rbp;
}();

constexpr HostGpr64 guest_fpr_mid_ptr_reg = [] {
    if constexpr (platform.a64) return asmjit::a64::x18;
    if constexpr (platform.x64) return asmjit::x86::rbx;
}();

constexpr size_t reg_alloc_num_gprs = reg_alloc_volatile_gprs.size() + reg_alloc_nonvolatile_gprs.size();
constexpr size_t reg_alloc_num_vprs = reg_alloc_volatile_vprs.size() + reg_alloc_nonvolatile_vprs.size();

class RegisterAllocator {
    using RegisterAllocatorStateGpr =
      mips::RegisterAllocatorState<RegisterAllocator, HostGpr64, reg_alloc_num_gprs, 32>;
    using RegisterAllocatorStateFpr =
      mips::RegisterAllocatorState<RegisterAllocator, HostGpr128, reg_alloc_num_vprs, 32>;

    RegisterAllocatorStateGpr state_gpr;
    RegisterAllocatorStateFpr state_fpr;
    std::span<s64 const, 32> guest_gprs;
    std::span<s64 const, 32> guest_fprs;
    JitCompiler& c;
    s64 const* gpr_mid_ptr;
    s64 const* fpr_mid_ptr;
    s32 allocated_stack;
    s32 fp_stack_alloc_offset;
    bool fp_instructions_used_in_current_block;
    bool gpr_stack_space_setup;
    bool stack_space_setup;
    bool stack_is_16_byte_aligned;

    static s32 get_nonvolatile_host_gpr_stack_offset(HostGpr64 gpr);
    static s32 get_nonvolatile_host_vpr_stack_offset(HostVpr128 vpr);

    s32 GetFprMidPtrOffset(u32 guest) const;
    s32 GetGprMidPtrOffset(u32 guest) const;
    void Reset();

    std::stack<asmjit::x86::Gpq> used_host_nonvolatiles;

public:
    RegisterAllocator(JitCompiler& compiler, std::span<s64 const, 32> guest_gprs, std::span<s64 const, 32> guest_fprs);

    void BlockEpilog();
    void BlockEpilogWithJmp(void* func);
    void BlockProlog();
    void Call(void* func);
    void CallWithStackAlignment(void* func);
    void DestroyVolatile(HostGpr64 gpr);
    void FlushAll() const;
    void FlushAllVolatile();
    void FlushGuest(HostGpr64 host, u32 guest);
    void FlushGuest(HostVpr128 host, u32 guest);
    template<typename Host, typename... Hosts> void Free(Host host, Hosts... hosts);
    void FreeArgs(int args);
    HostGpr64 GetDirtyGpr(u32 guest);
    HostGpr128 GetDirtyVpr(u32 guest);
    HostGpr64 GetGpr(u32 guest);
    std::string GetStatus() const;
    HostGpr128 GetVpr(u32 guest);
    void LoadGuest(HostGpr64 host, u32 guest) const;
    void LoadGuest(HostVpr128 host, u32 guest) const;
    template<typename Host, typename... Hosts> void Reserve(Host host, Hosts... hosts);
    void ReserveArgs(int args);
    void RestoreHost(HostGpr64 host) const;
    void RestoreHost(HostVpr128 host) const;
    void RestoreHosts();
    void SaveHost(HostGpr64 host);
    void SaveHost(HostVpr128 host);
    void SetupFp();
    void SetupGprStackSpace();
};

template<typename Host, typename... Hosts> void RegisterAllocator::Free(Host host, Hosts... hosts)
{
    if constexpr (std::same_as<Host, HostGpr64>) {
        state_gpr.Free(host);
    } else {
        state_fpr.Free(host);
    }
    if constexpr (sizeof...(hosts) > 0) {
        Free(hosts...);
    }
}

template<typename Host, typename... Hosts> void RegisterAllocator::Reserve(Host host, Hosts... hosts)
{
    if constexpr (std::same_as<Host, HostGpr64>) {
        state_gpr.Reserve(host);
    } else {
        state_fpr.Reserve(host);
    }
    if constexpr (sizeof...(hosts) > 0) {
        Reserve(hosts...);
    }
}

} // namespace n64::vr4300

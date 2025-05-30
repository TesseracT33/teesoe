#pragma once

#include "jit_common.hpp"
#include "mips/register_allocator_state.hpp"
#include "numtypes.hpp"
#include "platform.hpp"
#include "vu.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <string>

namespace n64::rsp {

constexpr std::array reg_alloc_volatile_gprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{ w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15, w16 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array{ edi, esi, r8d, r9d, r10d, r11d };
        }
        if constexpr (platform.abi.win64) {
            return std::array{ r8d, r9d, r10d, r11d };
        }
    }
}();

constexpr std::array reg_alloc_nonvolatile_gprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{ w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array{ r12d, r13d, r14d, r15d };
        }
        if constexpr (platform.abi.win64) {
            return std::array{ edi, esi, r12d, r13d, r14d, r15d };
        }
    }
}();

constexpr std::array reg_alloc_volatile_vprs = [] {
    if constexpr (platform.a64) {
        using namespace asmjit::a64;
        return std::array{
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
                // todo: include xmm15 if not on avx512
                return std::array{ xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14 };
            }
            if constexpr (platform.abi.win64) {
                return std::array{ xmm3, xmm4, xmm5 };
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
        return std::array{ v8, v9, v10, v11, v12, v13, v14 };
    }
    if constexpr (platform.x64) {
        using namespace asmjit::x86;
        if constexpr (platform.abi.systemv) {
            return std::array<Xmm, 0>{};
        }
        if constexpr (platform.abi.win64) {
            if constexpr (platform.avx512) {
                return std::array{ xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 };
            } else {
                return std::array{ xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14 };
            }
        }
    }
}();

constexpr HostVpr128 reg_alloc_vte_reg = [] {
    if constexpr (platform.a64) return asmjit::a64::v15;
    if constexpr (platform.x64 && !platform.avx512) return asmjit::x86::xmm15;
    if constexpr (platform.x64 && platform.avx512) return asmjit::x86::xmm31;
}();

constexpr HostGpr64 guest_gpr_mid_ptr_reg = [] {
    if constexpr (platform.a64) return asmjit::a64::x17;
    if constexpr (platform.x64) return asmjit::x86::rbp;
}();

constexpr HostGpr64 guest_vpr_mid_ptr_reg = [] {
    if constexpr (platform.a64) return asmjit::a64::x18;
    if constexpr (platform.x64) return asmjit::x86::rbx;
}();

static_assert(std::ranges::find(reg_alloc_volatile_vprs, reg_alloc_vte_reg) == reg_alloc_volatile_vprs.end());
static_assert(std::ranges::find(reg_alloc_nonvolatile_vprs, reg_alloc_vte_reg) == reg_alloc_nonvolatile_vprs.end());

constexpr size_t reg_alloc_num_gprs = reg_alloc_volatile_gprs.size() + reg_alloc_nonvolatile_gprs.size();
constexpr size_t reg_alloc_num_vprs = reg_alloc_volatile_vprs.size() + reg_alloc_nonvolatile_vprs.size();

class RegisterAllocator {
    using RegisterAllocatorStateGpr =
      mips::RegisterAllocatorState<RegisterAllocator, HostGpr32, reg_alloc_num_gprs, 32>;
    using RegisterAllocatorStateVpr =
      mips::RegisterAllocatorState<RegisterAllocator, HostGpr128, reg_alloc_num_vprs, 35>; // +3 for accumulators

    enum {
        AccLowIndex = 32,
        AccMidIndex,
        AccHighIndex,
    };

    RegisterAllocatorStateGpr state_gpr;
    RegisterAllocatorStateVpr state_vpr;
    std::span<s32 const, 32> guest_gprs;
    std::span<m128i const, 32> guest_vprs;
    JitCompiler& c;
    s32 const* gpr_mid_ptr;
    m128i const* vpr_mid_ptr;
    bool gpr_stack_space_setup;
    bool vte_reg_saved;

    static s32 get_nonvolatile_host_gpr_stack_offset(HostGpr32 gpr);
    static s32 get_nonvolatile_host_vpr_stack_offset(HostVpr128 vpr);

    s32 GetGprMidPtrOffset(u32 guest) const;
    s32 GetVprMidPtrOffset(u32 guest) const;
    void Reset();

public:
    RegisterAllocator(JitCompiler& compiler,
      std::span<s32 const, 32> guest_gprs,
      std::span<m128i const, 32> guest_vprs);

    void BlockEpilog();
    void BlockEpilogWithJmp(void* func);
    void BlockProlog();
    void Call(void* func);
    void DestroyVolatile(HostGpr64 gpr);
    void FlushAll() const;
    void FlushAllVolatile();
    void FlushGuest(HostGpr32 host, u32 guest);
    void FlushGuest(HostVpr128 host, u32 guest);
    template<typename Host, typename... Hosts> void Free(Host host, Hosts... hosts);
    void FreeArgs(int args);
    HostGpr128 GetAccLow();
    HostGpr128 GetAccMid();
    HostGpr128 GetAccHigh();
    HostGpr128 GetDirtyAccLow();
    HostGpr128 GetDirtyAccMid();
    HostGpr128 GetDirtyAccHigh();
    HostGpr32 GetDirtyGpr(u32 guest);
    HostGpr128 GetDirtyVpr(u32 guest);
    HostGpr32 GetGpr(u32 guest);
    std::string GetStatus() const;
    HostGpr128 GetVpr(u32 guest);
    HostVpr128 GetVte();
    void LoadGuest(HostGpr32 host, u32 guest) const;
    void LoadGuest(HostVpr128 host, u32 guest) const;
    template<typename Host, typename... Hosts> void Reserve(Host host, Hosts... hosts);
    void ReserveArgs(int args);
    void RestoreHost(HostGpr32 host) const;
    void RestoreHost(HostVpr128 host) const;
    void SaveHost(HostGpr32 host) const;
    void SaveHost(HostVpr128 host) const;
    void SetupGprStackSpace();
};

template<typename Host, typename... Hosts> void RegisterAllocator::Free(Host host, Hosts... hosts)
{
    if constexpr (std::same_as<Host, HostGpr64>) {
        state_gpr.Free(host.r32());
    } else {
        state_vpr.Free(host);
    }
    if constexpr (sizeof...(hosts) > 0) {
        Free(hosts...);
    }
}

template<typename Host, typename... Hosts> void RegisterAllocator::Reserve(Host host, Hosts... hosts)
{
    if constexpr (std::same_as<Host, HostGpr64>) {
        state_gpr.Reserve(host.r32());
    } else {
        state_vpr.Reserve(host);
    }
    if constexpr (sizeof...(hosts) > 0) {
        Reserve(hosts...);
    }
}

} // namespace n64::rsp

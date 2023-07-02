#pragma once

#include "mips/register_allocator.hpp"
#include "recompiler.hpp"
#include "rsp.hpp"
#include "vu.hpp"

namespace n64::rsp {

using namespace asmjit;

inline constexpr std::array reg_alloc_volatile_gprs = [] {
    if constexpr (arch.a64) {
        using namespace a64;
        return std::array{ x15, x14, x13, x12, x11, x10, x9, x8, x7, x6, x5, x4, x3 };
    } else {
        using namespace x86;
        if constexpr (os.linux) {
            return std::array{ r11, r10, r9, r8, rsi, rdi };
        } else {
            return std::array{ r11, r10, r9, r8 };
        }
    }
}();

inline constexpr std::array reg_alloc_nonvolatile_gprs = [] {
    if constexpr (arch.a64) {
        using namespace a64;
        return std::array{ x19, x20, x21, x22, x23, x24, x25, x26, x27, x28 };
    } else {
        using namespace x86;
        if constexpr (os.linux) {
            return std::array{ r12, r13, r14, r15 };
        } else {
            return std::array{ r12, r13, r14, r15, rdi, rsi };
        }
    }
}();

inline constexpr std::array reg_alloc_volatile_vprs = [] {
    if constexpr (arch.a64) {
        using namespace a64;
        return std::array{ v15, v14, v13, v12, v11, v10, v9, v8, v7, v6, v5, v4, v3 };
    } else {
        using namespace x86;
        if constexpr (os.linux) {
            if constexpr (avx512) {
                // clang-format off
            return std::array{
                xmm16, xmm17, xmm18, xmm19, xmm20, xmm21, xmm22, xmm23, xmm24, xmm25, 
                xmm26, xmm27, xmm28, xmm29, xmm30, xmm31, xmm7, xmm6, xmm5, xmm4, xmm3
            };
                // clang-format on
            } else {
                return std::array{ xmm7, xmm6, xmm5, xmm4, xmm3 };
            }
        } else {
            if constexpr (avx512) {
                // clang-format off
            return std::array{
                xmm16, xmm17, xmm18, xmm19, xmm20, xmm21, xmm22, xmm23, xmm24, 
                xmm25, xmm26, xmm27, xmm28, xmm29, xmm30, xmm31, xmm5, xmm4, xmm3
            };
                // clang-format on
            } else {
                return std::array{ xmm5, xmm4, xmm3 };
            }
        }
    }
}();

inline constexpr std::array reg_alloc_nonvolatile_vprs = [] {
    if constexpr (arch.a64) {
        using namespace a64;
        return std::array{ v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31 };
    } else {
        using namespace x86;
        if constexpr (os.linux) {
            return std::array{ xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14 };
        } else {
            return std::array{ xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14 };
        }
    }
}();

inline constexpr HostVpr128 reg_alloc_vte_reg = [] {
    if constexpr (arch.a64) return a64::x0; // todo
    if constexpr (arch.x64) return x86::xmm15;
}();

inline constexpr HostGpr reg_alloc_base_gpr_ptr_reg = [] {
    if constexpr (arch.a64) return a64::x0; // todo
    if constexpr (arch.x64) return x86::rbp;
}();

inline constexpr HostGpr reg_alloc_base_vpr_ptr_reg = [] {
    if constexpr (arch.a64) return a64::x0; // todo
    if constexpr (arch.x64) return x86::rbx;
}();

class RegisterAllocator {
private:
    using GprRegisterAllocatorState =
      mips::RegisterAllocatorState<HostGpr, reg_alloc_volatile_gprs.size(), reg_alloc_nonvolatile_gprs.size(), 32>;
    using VprRegisterAllocatorState =
      mips::RegisterAllocatorState<HostVpr128, reg_alloc_volatile_vprs.size(), reg_alloc_nonvolatile_vprs.size(), 35>;
    using GprBinding = GprRegisterAllocatorState::Binding;
    using VprBinding = VprRegisterAllocatorState::Binding;

public:
    RegisterAllocator(AsmjitCompiler& compiler)
      : c{ compiler },
        gpr_state{ c, reg_alloc_volatile_gprs, reg_alloc_nonvolatile_gprs, reg_alloc_base_gpr_ptr_reg },
        vpr_state{ c, reg_alloc_volatile_vprs, reg_alloc_nonvolatile_vprs, reg_alloc_base_vpr_ptr_reg },
        guest_gprs_pointer_reg{ reg_alloc_base_gpr_ptr_reg },
        guest_vprs_pointer_reg{ reg_alloc_base_vpr_ptr_reg },
        vte_reg_saved{}
    {
    }

    void BlockEpilog()
    {
        FlushAll();
        if (is_nonvolatile(guest_gprs_pointer_reg)) {
            RestoreHost(guest_gprs_pointer_reg);
        }
        if (is_nonvolatile(guest_vprs_pointer_reg)) {
            RestoreHost(guest_vprs_pointer_reg);
        }
        if (vte_reg_saved && is_nonvolatile(reg_alloc_vte_reg)) {
            RestoreHost(reg_alloc_vte_reg);
        }
        if constexpr (arch.a64) {
        } else {
            c.add(x86::rsp, register_stack_space);
            c.ret();
        }
    }

    void BlockEpilogWithJmp(void* func)
    {
        FlushAll();
        if (is_nonvolatile(guest_gprs_pointer_reg)) {
            RestoreHost(guest_gprs_pointer_reg);
        }
        if (is_nonvolatile(guest_vprs_pointer_reg)) {
            RestoreHost(guest_vprs_pointer_reg);
        }
        if (vte_reg_saved && is_nonvolatile(reg_alloc_vte_reg)) {
            RestoreHost(reg_alloc_vte_reg);
        }
        if constexpr (arch.a64) {
        } else {
            c.add(x86::rsp, register_stack_space);
            c.jmp(func);
        }
    }

    void BlockProlog()
    {
        gpr_state.Reset();
        vpr_state.Reset();
        if constexpr (arch.a64) {
        } else {
            c.sub(x86::rsp, register_stack_space);
            if (is_nonvolatile(guest_gprs_pointer_reg)) {
                SaveHost(guest_gprs_pointer_reg);
            }
            if (is_nonvolatile(guest_vprs_pointer_reg)) {
                SaveHost(guest_vprs_pointer_reg);
            }
            c.mov(guest_gprs_pointer_reg, gpr.ptr(0));
            c.mov(guest_vprs_pointer_reg, vpr.data());
        }
        vte_reg_saved = false;
    }

    void Call(auto func)
    {
        static_assert(
          register_stack_space % 16 == 8); // Given this, the stack should already be aligned with the CALL, unless an
                                           // instruction impl used PUSH. TODO: make this more robust
        FlushAndDestroyAllVolatile();
        if constexpr (arch.a64) {
        } else {
            jit_x86_call_no_stack_alignment(c, func);
        }
        if (is_volatile(guest_gprs_pointer_reg)) {
            c.mov(guest_gprs_pointer_reg, gpr.ptr(0));
        }
        if (is_volatile(guest_vprs_pointer_reg)) {
            c.mov(guest_vprs_pointer_reg, vpr.data());
        }
    }

    void CallWithStackAlignment(auto func)
    {
        FlushAndDestroyAllVolatile();
        if constexpr (arch.a64) {
        } else {
            jit_x64_call(c, func);
        }
        if (is_volatile(guest_gprs_pointer_reg)) {
            c.mov(guest_gprs_pointer_reg, gpr.ptr(0));
        }
        if (is_volatile(guest_vprs_pointer_reg)) {
            c.mov(guest_vprs_pointer_reg, vpr.data());
        }
    }

    void FlushAllVolatile()
    {
        for (auto& b : gpr_state.bindings) {
            if (b.is_volatile && b.Occupied()) {
                Flush(b, true);
            }
        }
        for (auto& b : vpr_state.bindings) {
            if (b.is_volatile && b.Occupied()) {
                Flush(b, true);
            }
        }
    }

    void FlushAndDestroyAllVolatile()
    {
        for (auto& b : gpr_state.bindings) {
            if (b.is_volatile && b.Occupied()) {
                FlushAndDestroyBinding(b, true);
            }
        }
        for (auto& b : vpr_state.bindings) {
            if (b.is_volatile && b.Occupied()) {
                FlushAndDestroyBinding(b, true);
            }
        }
    }

    void Free(HostGpr host) { Free<1>({ host }); }

    void Free(HostVpr128 host) { Free<1>({ host }); }

    template<size_t N> void Free(std::array<HostGpr, N> const& hosts)
    {
        gpr_state.Free(
          hosts,
          [this](GprBinding& freed, bool restore) { FlushAndDestroyBinding(freed, restore); },
          [this](HostGpr gpr) { RestoreHost(gpr); },
          [this](HostGpr gpr) { SaveHost(gpr); });
    }

    template<size_t N> void Free(std::array<HostVpr128, N> const& hosts)
    {
        vpr_state.Free(
          hosts,
          [this](VprBinding& freed, bool restore) { FlushAndDestroyBinding(freed, restore); },
          [this](HostVpr128 vpr) { RestoreHost(vpr); },
          [this](HostVpr128 vpr) { SaveHost(vpr); });
    }

    HostVpr128 GetAccHigh() { return GetHostVpr(acc_high_idx, false); }

    HostVpr128 GetAccHighMarkDirty() { return GetHostVpr(acc_high_idx, true); }

    HostVpr128 GetAccLow() { return GetHostVpr(acc_low_idx, false); }

    HostVpr128 GetAccLowMarkDirty() { return GetHostVpr(acc_low_idx, true); }

    HostVpr128 GetAccMid() { return GetHostVpr(acc_mid_idx, false); }

    HostVpr128 GetAccMidMarkDirty() { return GetHostVpr(acc_mid_idx, true); }

    HostGpr GetHostGpr(u32 idx) { return GetHostGpr(idx, false); }

    HostGpr GetHostGprMarkDirty(u32 idx)
    {
        assert(idx);
        return GetHostGpr(idx, true);
    }

    HostVpr128 GetHostVpr(u32 idx) { return GetHostVpr(idx, false); }

    HostVpr128 GetHostVprMarkDirty(u32 idx) { return GetHostVpr(idx, true); }

    std::string GetStatusString() const
    {
        std::string gprs_state = gpr_state.GetStatusString();
        std::string vprs_state = vpr_state.GetStatusString();
        return std::format("GPRs: {}VPRs: {}", gprs_state, vprs_state);
    }

    HostVpr128 GetVte()
    {
        if (!vte_reg_saved) {
            vte_reg_saved = true;
            SaveHost(reg_alloc_vte_reg);
        }
        return reg_alloc_vte_reg;
    }

    bool IsBound(auto reg) const
    {
        if constexpr (std::same_as<decltype(reg), x86::Gp>) {
            return gpr_state.IsBound(reg);
        } else {
            return vpr_state.IsBound(reg);
        }
    }

    void RestoreHost(HostGpr gpr)
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(gpr.r64(), qword_ptr(x86::rsp, 8 * gpr.id()));
        }
    }

    void RestoreHost(HostVpr128 vpr)
    {
        if constexpr (arch.a64) {
        } else {
            static_assert(gprs_stack_space % 16 == 0);
            c.vmovaps(vpr, xmmword_ptr(x86::rsp, 16 * vpr.id() + gprs_stack_space));
        }
    }

    void SaveHost(HostGpr gpr)
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(qword_ptr(x86::rsp, 8 * gpr.id()), gpr.r64());
        }
    }

    void SaveHost(HostVpr128 vpr)
    {
        if constexpr (arch.a64) {
        } else {
            c.vmovaps(xmmword_ptr(x86::rsp, 16 * vpr.id() + gprs_stack_space), vpr);
        }
    }

private:
    enum : u32 {
        acc_low_idx = 32,
        acc_mid_idx,
        acc_high_idx
    };

    static constexpr int gprs_stack_space = 8 * 16; // todo arm64
    static constexpr int vprs_stack_space = 8 + [] { // +8 to offset stack for faster CALLs
        if constexpr (arch.a64) return 0; // todo
        if constexpr (avx512) return 16 * 32;
        return 16 * 16;
    }();
    static constexpr int register_stack_space = gprs_stack_space + vprs_stack_space;

    AsmjitCompiler& c;
    HostGpr guest_gprs_pointer_reg, guest_vprs_pointer_reg;
    GprRegisterAllocatorState gpr_state;
    VprRegisterAllocatorState vpr_state;
    bool vte_reg_saved;

    void Flush(auto& binding, bool restore)
    {
        if (!binding.Occupied()) return;
        if (binding.dirty) {
            auto guest = binding.guest.value();
            if constexpr (arch.a64) {
            } else {
                if constexpr (std::same_as<decltype(binding.host), HostGpr>) {
                    c.mov(dword_ptr(guest_gprs_pointer_reg, 4 * guest), binding.host.r32());
                } else {
                    if (guest < 32) {
                        c.vmovaps(xmmword_ptr(guest_vprs_pointer_reg, 16 * guest), binding.host);
                    } else {
                        switch (guest) {
                        case acc_low_idx: c.vmovaps(GlobalVarPtr(acc.low), binding.host); break;
                        case acc_mid_idx: c.vmovaps(GlobalVarPtr(acc.mid), binding.host); break;
                        case acc_high_idx: c.vmovaps(GlobalVarPtr(acc.high), binding.host); break;
                        default: assert(false);
                        }
                    }
                }
            }
        }
        if (!binding.is_volatile && restore) {
            RestoreHost(binding.host);
        }
    }

    // This should only be used as part of an instruction epilogue. Thus, there is no need
    // to destroy bindings. In fact, this would be undesirable, since this function could not
    // be called in an epilog emitted mid-block, as part of a code path dependent on a run-time branch.
    void FlushAll()
    {
        for (auto& binding : gpr_state.bindings) {
            Flush(binding, true);
        }
        for (auto& binding : vpr_state.bindings) {
            Flush(binding, true);
        }
    }

    void FlushAndDestroyBinding(auto& binding, bool restore)
    {
        Flush(binding, restore);
        if constexpr (std::same_as<decltype(binding.host), HostGpr>) gpr_state.ResetBinding(binding);
        else vpr_state.ResetBinding(binding);
    }

    HostGpr GetHostGpr(u32 guest, bool make_dirty) { return GetHost<HostGpr>(guest, make_dirty); }

    HostVpr128 GetHostVpr(u32 guest, bool make_dirty) { return GetHost<HostVpr128>(guest, make_dirty); }

    template<typename HostReg> HostReg GetHost(u32 guest, bool make_dirty)
    {
        auto* state = [this] {
            if constexpr (std::same_as<HostReg, HostGpr>) return &gpr_state;
            else return &vpr_state;
        }();

        auto* binding = state->guest_to_host[guest];
        if (binding) {
            binding->dirty |= make_dirty;
            binding->access_index = state->host_access_index++;
            return binding->host;
        }

        bool found_free{};
        u64 min_access = std::numeric_limits<u64>::max();

        auto FindReg = [&](auto start_it, auto end_it) {
            for (auto b = start_it; b != end_it; ++b) {
                if (!b->Occupied()) {
                    found_free = true;
                    binding = &*b;
                    break;
                } else if (b->access_index < min_access) {
                    min_access = b->access_index;
                    binding = &*b;
                }
            }
        };

        FindReg(state->volatile_bindings_begin_it, state->volatile_bindings_end_it);
        if (!found_free) {
            FindReg(state->nonvolatile_bindings_begin_it, state->nonvolatile_bindings_end_it);
        }

        if (!found_free) {
            FlushAndDestroyBinding(*binding, false);
        }
        binding->guest = guest;
        binding->access_index = state->host_access_index++;
        binding->dirty = make_dirty;
        state->guest_to_host[guest] = binding;
        Load(*binding, found_free);
        return binding->host;
    }

    void Load(auto& binding, bool save)
    {
        if (!binding.is_volatile && save) {
            SaveHost(binding.host);
        }
        if constexpr (arch.a64) {
        } else {
            auto guest = binding.guest.value();
            if constexpr (std::same_as<decltype(binding.host), HostGpr>) {
                if (guest == 0) {
                    c.xor_(binding.host.r32(), binding.host.r32());
                } else {
                    c.mov(binding.host.r32(), dword_ptr(guest_gprs_pointer_reg, 4 * guest));
                }
            } else {
                if (guest < 32) {
                    c.vmovaps(binding.host, xmmword_ptr(guest_vprs_pointer_reg, 16 * guest));
                } else {
                    switch (guest) {
                    case acc_low_idx: c.vmovaps(binding.host, GlobalVarPtr(acc.low)); break;
                    case acc_mid_idx: c.vmovaps(binding.host, GlobalVarPtr(acc.mid)); break;
                    case acc_high_idx: c.vmovaps(binding.host, GlobalVarPtr(acc.high)); break;
                    default: assert(false);
                    }
                }
            }
        }
    }
} inline reg_alloc{ compiler };

} // namespace n64::rsp

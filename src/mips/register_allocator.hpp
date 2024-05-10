#pragma once

#include "always_false.hpp"
#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "jit_util.hpp"
#include "mips/disassembler.hpp"
#include "numtypes.hpp"
#include "platform.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>

namespace mips {

using namespace asmjit;

template<typename HostReg, size_t num_volatile_regs, size_t num_nonvolatile_regs, size_t num_guest_regs>
struct RegisterAllocatorState {
    struct Binding {
        HostReg host{};
        std::optional<u32> guest{};
        u64 access_index{};
        bool dirty{};
        bool IsVolatile{};
        bool reserved{};
        bool Occupied() const { return guest.has_value(); }
    };

    RegisterAllocatorState(AsmjitCompiler& compiler,
      std::span<HostReg const, num_volatile_regs> volatile_regs,
      std::span<HostReg const, num_nonvolatile_regs> nonvolatile_regs,
      HostGpr guest_reg_pointer_gpr)
      : c{ compiler },
        guest_reg_pointer_gpr{ guest_reg_pointer_gpr },
        host_access_index{},
        guest_to_host{},
        volatile_bindings_begin_it{ bindings.begin() },
        volatile_bindings_end_it{ bindings.begin() + num_volatile_regs },
        nonvolatile_bindings_begin_it{ bindings.begin() + num_volatile_regs },
        nonvolatile_bindings_end_it{ bindings.begin() + num_volatile_regs + num_nonvolatile_regs }
    {
        assert(std::ranges::all_of(volatile_regs, [](HostReg const& reg) { return IsVolatile(reg); }));
        assert(std::ranges::none_of(nonvolatile_regs, [](HostReg const& reg) { return IsVolatile(reg); }));
        if constexpr (std::same_as<HostReg, HostGpr>) {
            // assert(std::ranges::find(volatile_regs, [](HostReg const& reg) {
            //     return reg.id() == guest_reg_pointer_gpr.id();
            // }) == volatile_regs.end());
            // assert(std::ranges::find(nonvolatile_regs, [](HostReg const& reg) {
            //     return reg.id() == guest_reg_pointer_gpr.id();
            // }) == nonvolatile_regs.end());
            if constexpr (arch.x64) {
                assert(std::ranges::find(nonvolatile_regs, x86::rsp) == nonvolatile_regs.end());
            }
        }
        std::ranges::transform(volatile_regs, volatile_bindings_begin_it, [](HostReg const& reg) {
            return Binding{ .host = reg, .IsVolatile = true };
        });
        std::ranges::transform(nonvolatile_regs, nonvolatile_bindings_begin_it, [](HostReg const& reg) {
            return Binding{ .host = reg, .IsVolatile = false };
        });
    }

    AsmjitCompiler& c;
    std::array<Binding, num_volatile_regs + num_nonvolatile_regs> bindings;
    std::array<Binding*, num_guest_regs> guest_to_host;
    u64 host_access_index;
    HostGpr guest_reg_pointer_gpr;
    typename decltype(bindings)::iterator volatile_bindings_begin_it, volatile_bindings_end_it,
      nonvolatile_bindings_begin_it, nonvolatile_bindings_end_it;

    template<size_t N> void Free(std::array<HostReg, N> hosts)
    {
        for (HostReg host : hosts) {
            auto it = std::ranges::find_if(bindings, [&](Binding const& b) { return b.host == host; });
            if (it != bindings.end()) {
                assert(it->reserved);
                it->reserved = false;
            }
        }
    }

    std::string GetStatusString() const
    {
        std::string used_str, free_str;
        for (Binding const& b : bindings) {
            auto host_reg_str{ JitRegToStr(b.host) };
            if (b.Occupied()) {
                u32 guest = b.guest.value();
                std::string guest_reg_str =
                  b.host.isXmm() ? std::format("$v{}", guest) : std::string(mips::GprIdxToName(guest));
                used_str.append(std::format("{}({},{}),", host_reg_str, guest_reg_str, b.dirty ? 'd' : 'c'));
            } else {
                free_str.append(host_reg_str);
                free_str.push_back(',');
            }
        }
        return std::format("Used: {}; Free: {}\n", used_str, free_str);
    }

    bool IsBound(u32 guest) const { return guest_to_host[guest] != nullptr; }

    bool IsBound(HostReg host) const
    {
        for (Binding const& b : bindings) {
            if (b.host.id() == host.id()) return b.Occupied();
        }
        return false;
    }

    void MoveHost(HostReg dst, HostReg src) const
    {
        if constexpr (std::same_as<HostReg, HostGpr>) {
            if constexpr (arch.a64) {
            } else {
                c.mov(dst, src);
            }
        } else if constexpr (std::same_as<HostReg, HostVpr128>) {
            if constexpr (arch.a64) {
            } else {
                c.vmovaps(dst, src);
            }
        } else {
            static_assert(AlwaysFalse<sizeof(HostReg)>);
        }
    }

    template<size_t N>
    void Reserve(std::array<HostReg, N> hosts,
      auto flush_and_destroy_binding_func,
      auto restore_host_func,
      auto save_host_func)
    {
        for (HostReg host : hosts) {
            auto freed = std::ranges::find_if(bindings, [&](Binding const& b) { return b.host == host; });
            if (freed == bindings.end()) {
                continue;
            }
            freed->reserved = true;
            if (!freed->Occupied()) {
                continue;
            }

            Binding* replacement{};
            bool found_free{};
            u64 min_access = std::numeric_limits<u64>::max();

            for (Binding& b : bindings) {
                if (b.reserved || std::ranges::find(hosts, b.host) != hosts.end()) {
                    continue;
                }
                if (!b.Occupied()) {
                    found_free = true;
                    replacement = &b;
                    break;
                } else if (b.access_index < freed->access_index && b.access_index < min_access) {
                    min_access = b.access_index;
                    replacement = &b;
                }
            }

            if (replacement) {
                if (!found_free) {
                    flush_and_destroy_binding_func(*replacement, false, true);
                } else if (!replacement->IsVolatile) {
                    save_host_func(replacement->host);
                }
                replacement->guest = freed->guest;
                replacement->access_index = freed->access_index;
                replacement->dirty = freed->dirty;
                guest_to_host[replacement->guest.value()] = &*replacement;
                // Do not call ResetBinding(freed); it will reset guest_to_host[replacement->guest] ==
                // guest_to_host[freed->guest]
                freed->guest = {};
                freed->dirty = false;
                MoveHost(replacement->host, freed->host);
                if (!freed->IsVolatile) {
                    restore_host_func(freed->host);
                }
            } else {
                flush_and_destroy_binding_func(*freed, true, true);
            }
        }
    }

    void Reset()
    {
        for (Binding& b : bindings) {
            ResetBinding(b, false);
        }
        guest_to_host = {};
        host_access_index = 0; // todo: what to set?
    }

    void ResetBinding(Binding& b, bool keep_reserved)
    {
        if (b.Occupied()) {
            guest_to_host[b.guest.value()] = {};
            b.guest = {};
            b.dirty = false;
        }
        if (!keep_reserved) {
            b.reserved = false;
        }
        b.access_index = host_access_index;
    }
};

template<typename GuestInt, size_t num_volatile_gprs, size_t num_nonvolatile_gprs> class RegisterAllocator {
protected:
    using RegisterAllocatorState = RegisterAllocatorState<HostGpr, num_volatile_gprs, num_nonvolatile_gprs, 32>;
    using Binding = typename RegisterAllocatorState::Binding;

public:
    RegisterAllocator(std::span<GuestInt const, 32> guest_gprs,
      std::span<HostGpr const, num_volatile_gprs> volatile_gprs,
      std::span<HostGpr const, num_nonvolatile_gprs> nonvolatile_gprs,
      HostGpr guest_gprs_pointer_reg,
      AsmjitCompiler& compiler)
      : c{ compiler },
        guest_gprs(guest_gprs),
        guest_gprs_pointer_reg{ guest_gprs_pointer_reg },
        state{ c, volatile_gprs, nonvolatile_gprs, guest_gprs_pointer_reg }
    {
    }

    void BlockEpilog()
    {
        FlushAndRestoreAll();
        if (!IsVolatile(guest_gprs_pointer_reg)) {
            RestoreHost(guest_gprs_pointer_reg);
        }
        if constexpr (arch.a64) {
        } else {
            c.add(x86::rsp, register_stack_space);
            c.ret();
        }
    }

    void BlockEpilogWithJmp(void* func)
    {
        FlushAndRestoreAll();
        if (!IsVolatile(guest_gprs_pointer_reg)) {
            RestoreHost(guest_gprs_pointer_reg);
        }
        if constexpr (arch.a64) {
        } else {
            c.add(x86::rsp, register_stack_space);
            c.jmp(func);
        }
    }

    void BlockProlog()
    {
        state.Reset();
        if constexpr (arch.a64) {
        } else {
            c.sub(x86::rsp, register_stack_space);
            if (!IsVolatile(guest_gprs_pointer_reg)) {
                SaveHost(guest_gprs_pointer_reg);
            }
            c.mov(guest_gprs_pointer_reg, guest_gprs.data());
        }
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
        if (IsVolatile(guest_gprs_pointer_reg)) {
            c.mov(guest_gprs_pointer_reg, guest_gprs.data());
        }
    }

    void CallWithStackAlignment(auto func)
    {
        FlushAndDestroyAllVolatile();
        if constexpr (arch.a64) {
        } else {
            jit_x64_call_with_stack_alignment(c, func);
        }
        if (IsVolatile(guest_gprs_pointer_reg)) {
            c.mov(guest_gprs_pointer_reg, guest_gprs.data());
        }
    }

    void FlushAll()
    {
        for (Binding& binding : state.bindings) {
            Flush(binding, false);
        }
    }

    void Free(HostGpr host) { Free<1>({ host }); }

    template<size_t N> void Free(std::array<HostGpr, N> const& hosts) { state.Free(hosts); }

    void FreeArgs(int args)
    {
        assert(args > 0);
        switch (args) { // the prettiest code you've seen
        case 1: Free<1>({ host_gpr_arg[0] }); break;
        case 2: Free<2>({ host_gpr_arg[0], host_gpr_arg[1] }); break;
        case 3: Free<3>({ host_gpr_arg[0], host_gpr_arg[1], host_gpr_arg[2] }); break;
        case 4: Free<4>({ host_gpr_arg[0], host_gpr_arg[1], host_gpr_arg[2], host_gpr_arg[3] }); break;
        default: assert(false);
        }
    }

    HostGpr GetHostGpr(u32 guest) { return GetHostGpr(guest, false); }

    HostGpr GetHostGprMarkDirty(u32 guest) { return GetHostGpr(guest, guest != 0); }

    std::string GetStatusString() const { return state.GetStatusString(); }

    HostGpr GetTemporary()
    {
        Binding* binding{};
        bool found_free{};
        u64 min_access = std::numeric_limits<u64>::max();

        for (auto b = state.volatile_bindings_begin_it; b != state.volatile_bindings_end_it; ++b) {
            if (b->reserved) {
                continue;
            }
            if (!b->Occupied()) {
                found_free = true;
                binding = &*b;
                break;
            } else if (b->access_index < min_access) {
                min_access = b->access_index;
                binding = &*b;
            }
        }

        if (!found_free) {
            FlushAndDestroyBinding(*binding, false, false);
        }
        return binding->host;
    }

    bool IsBound(auto reg) const { return state.IsBound(reg); }

    void Reserve(HostGpr host) { Reserve<1>({ host }); }

    template<size_t N> void Reserve(std::array<HostGpr, N> const& hosts)
    {
        state.Reserve(
          hosts,
          [this](Binding& freed, bool restore, bool keep_reserved) {
              FlushAndDestroyBinding(freed, restore, keep_reserved);
          },
          [this](HostGpr gpr) { RestoreHost(gpr); },
          [this](HostGpr gpr) { SaveHost(gpr); });
    }

    void ReserveArgs(int args)
    {
        assert(args > 0);
        switch (args) { // the prettiest code you've seen
        case 1: Reserve<1>({ host_gpr_arg[0] }); break;
        case 2: Reserve<2>({ host_gpr_arg[0], host_gpr_arg[1] }); break;
        case 3: Reserve<3>({ host_gpr_arg[0], host_gpr_arg[1], host_gpr_arg[2] }); break;
        case 4: Reserve<4>({ host_gpr_arg[0], host_gpr_arg[1], host_gpr_arg[2], host_gpr_arg[3] }); break;
        default: assert(false);
        }
    }

    void RestoreHost(HostGpr gpr) const
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(gpr.r64(), qword_ptr(x86::rsp, 8 * gpr.id()));
        }
    }

    void SaveHost(HostGpr gpr) const
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(qword_ptr(x86::rsp, 8 * gpr.id()), gpr.r64());
        }
    }

protected:
    static constexpr int register_stack_space = 8 * 16 + 8; // +8 to offset stack for faster CALLs
    static constexpr bool mips32 = sizeof(GuestInt) == 4;
    static constexpr bool mips64 = sizeof(GuestInt) == 8;
    static_assert(mips32 || mips64);

    AsmjitCompiler& c;
    std::span<GuestInt const, 32> guest_gprs;
    HostGpr guest_gprs_pointer_reg;
    RegisterAllocatorState state;

    void Flush(Binding const& b, bool restore) const
    {
        if (!b.Occupied()) return;
        if (b.dirty) {
            if constexpr (arch.a64) {
            } else {
                if constexpr (mips32) {
                    c.mov(dword_ptr(guest_gprs_pointer_reg, 4 * b.guest.value()), b.host.r32());
                } else {
                    c.mov(qword_ptr(guest_gprs_pointer_reg, 8 * b.guest.value()), b.host.r64());
                }
            }
        }
        if (!b.IsVolatile && restore) {
            RestoreHost(b.host);
        }
    }

    void FlushAndDestroyAllVolatile()
    {
        for (Binding& binding : state.bindings) {
            if (binding.IsVolatile) {
                FlushAndDestroyBinding(binding, false, true);
            }
        }
    }

    void FlushAndDestroyBinding(Binding& b, bool restore, bool keep_reserved)
    {
        Flush(b, restore);
        state.ResetBinding(b, keep_reserved);
    }

    // This should only be used as part of an instruction epilogue. Thus, there is no need
    // to destroy bindings. In fact, this would be undesirable, since this function could not
    // be called in an epilog emitted mid-block, as part of a code path dependent on a run-time branch.
    void FlushAndRestoreAll() const
    {
        for (Binding const& binding : state.bindings) {
            Flush(binding, true);
        }
    }

    HostGpr GetHostGpr(u32 guest, bool make_dirty)
    {
        Binding* binding = state.guest_to_host[guest];
        if (binding) {
            binding->dirty |= make_dirty;
            binding->access_index = state.host_access_index++;
            return binding->host;
        }

        bool found_free{};
        u64 min_access = std::numeric_limits<u64>::max();

        auto FindReg = [&](auto start_it, auto end_it) {
            for (auto b = start_it; b != end_it; ++b) {
                if (b->reserved) {
                    continue;
                }
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

        // 'make_dirty' should be true if the given guest register is a destination in the current instruction.
        // If so, search for a free host register among the nonvolatile ones first, to reduce the number of
        // dirty host registers that need to be flushed on block function calls.

        if (make_dirty) {
            FindReg(state.nonvolatile_bindings_begin_it, state.nonvolatile_bindings_end_it);
            if (!found_free) {
                FindReg(state.volatile_bindings_begin_it, state.volatile_bindings_end_it);
            }
        } else {
            FindReg(state.volatile_bindings_begin_it, state.volatile_bindings_end_it);
            if (!found_free) {
                FindReg(state.nonvolatile_bindings_begin_it, state.nonvolatile_bindings_end_it);
            }
        }

        if (!found_free) {
            FlushAndDestroyBinding(*binding, false, true);
        }
        binding->guest = guest;
        binding->access_index = state.host_access_index++;
        binding->dirty = make_dirty;
        state.guest_to_host[guest] = binding;
        Load(*binding, found_free);
        return binding->host;
    }

    void Load(Binding& binding, bool save)
    {
        if (!binding.IsVolatile && save) {
            SaveHost(binding.host);
        }
        u32 guest = binding.guest.value();
        if constexpr (arch.a64) {
        } else {
            if (guest == 0) {
                c.xor_(binding.host.r32(), binding.host.r32());
            } else {
                if constexpr (mips32) {
                    c.mov(binding.host.r32(), dword_ptr(guest_gprs_pointer_reg, 4 * guest));
                } else {
                    c.mov(binding.host.r64(), qword_ptr(guest_gprs_pointer_reg, 8 * guest));
                }
            }
        }
    }
};

} // namespace mips

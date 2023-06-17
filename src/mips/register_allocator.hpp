#pragma once

#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "common/types.hpp"
#include "host.hpp"
#include "jit_util.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>

namespace mips {

template<typename GuestGprInt, size_t num_volatile_gprs, size_t num_nonvolatile_gprs> class RegisterAllocator {
protected:
    struct Binding;

public:
    RegisterAllocator(std::span<GuestGprInt const, 32> guest_gpr,
      std::span<HostGpr const, num_volatile_gprs> volatile_gprs,
      std::span<HostGpr const, num_nonvolatile_gprs> nonvolatile_gprs,
      HostGpr guest_gpr_pointer_reg,
      AsmjitCompiler& compiler)
      : guest_gpr{ guest_gpr },
        guest_gpr_pointer_reg{ guest_gpr_pointer_reg },
        c{ compiler },
        host_access_index{},
        guest_to_host{},
        volatile_bindings_begin_it{ bindings.begin() },
        volatile_bindings_end_it{ bindings.begin() + num_volatile_gprs },
        nonvolatile_bindings_begin_it{ bindings.begin() + num_volatile_gprs },
        nonvolatile_bindings_end_it{ bindings.begin() + num_volatile_gprs + num_nonvolatile_gprs }
    {
        assert(std::ranges::all_of(volatile_gprs, [](HostGpr const& gpr) { return is_volatile(gpr); }));
        assert(std::ranges::none_of(nonvolatile_gprs, [](HostGpr const& gpr) { return is_volatile(gpr); }));
        if constexpr (arch.x64) {
            assert(std::ranges::find(nonvolatile_gprs, asmjit::x86::rsp) == nonvolatile_gprs.end());
        }
        std::transform(volatile_gprs.begin(), volatile_gprs.end(), volatile_bindings_begin_it, [](HostGpr const& gpr) {
            return Binding{ .host = gpr, .is_volatile = true };
        });
        std::transform(nonvolatile_gprs.begin(),
          nonvolatile_gprs.end(),
          nonvolatile_bindings_begin_it,
          [](HostGpr const& gpr) { return Binding{ .host = gpr, .is_volatile = false }; });
    }

    void BlockEpilog()
    {
        FlushAll();
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            if (is_nonvolatile(guest_gpr_pointer_reg)) {
                RestoreHostGpr(guest_gpr_pointer_reg);
            }
            c.add(rsp, register_stack_space);
            c.ret();
        }
    }

    void BlockEpilogWithJmp(void* func)
    {
        FlushAll();
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            if (is_nonvolatile(guest_gpr_pointer_reg)) {
                RestoreHostGpr(guest_gpr_pointer_reg);
            }
            c.add(rsp, register_stack_space);
            c.jmp(func);
        }
    }

    void BlockProlog()
    {
        for (Binding& b : bindings) {
            b.Reset();
        }
        guest_to_host = {};
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            c.sub(rsp, register_stack_space);
            if (is_nonvolatile(guest_gpr_pointer_reg)) {
                SaveHostGpr(guest_gpr_pointer_reg);
            }
            c.mov(guest_gpr_pointer_reg, guest_gpr.data());
        }
    }

    // In an instruction implementation, make sure to call this after GetHost
    HostGpr AcquireTemporary() { return {}; }

    void Call(auto func)
    {
        static_assert(
          register_stack_space % 16 == 0); // Given this, the stack should already be aligned with the CALL, unless an
                                           // instruction impl used PUSH. TODO: make this more robust
        FlushAllVolatile();
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            if constexpr (os.linux) {
                c.call(func);
            } else {
                c.sub(rsp, 32);
                c.call(func);
                c.add(rsp, 32);
            }
        }
        if (is_volatile(guest_gpr_pointer_reg)) {
            c.mov(guest_gpr_pointer_reg, guest_gpr.data());
        }
    }

    void CallWithStackAlignment(auto func)
    {
        FlushAllVolatile();
        if constexpr (arch.a64) {
        } else {
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
        if (is_volatile(guest_gpr_pointer_reg)) {
            c.mov(guest_gpr_pointer_reg, guest_gpr.data());
        }
    }

    void FlushAll()
    {
        for (Binding& binding : bindings) {
            Flush(binding, true);
            binding.guest = {};
        }
        guest_to_host = {};
    }

    void FlushAllVolatile()
    {
        for (Binding& binding : bindings) {
            if (binding.is_volatile && binding.Occupied()) {
                Flush(binding, true);
                guest_to_host[binding.guest.value()] = {};
                binding.guest = {};
            }
        }
    }

    void Free(HostGpr host)
    {
        auto freed = std::ranges::find_if(bindings, [&](Binding const& b) { return b.host == host; });
        if (freed == bindings.end() || !freed->Occupied()) return;

        Binding* replaced{};
        bool found_free{};
        u64 min_access = std::numeric_limits<u64>::max();

        for (Binding& b : bindings) {
            if (b.host == host) continue;
            if (!b.Occupied()) {
                found_free = true;
                replaced = &b;
                break;
            } else if (b.access_index < freed->access_index && b.access_index < min_access) {
                min_access = b.access_index;
                replaced = &b;
            }
        }

        if (replaced) {
            if (!found_free) {
                Flush(*replaced, false);
                guest_to_host[replaced->guest.value()] = {};
            }
            replaced->guest = freed->guest;
            replaced->access_index = freed->access_index;
            replaced->dirty = freed->dirty;
            guest_to_host[freed->guest.value()] = &*replaced;
            // TODO: mov into new reg
        } else {
            Flush(*freed, false);
            freed->Reset();
            guest_to_host[freed->guest.value()] = {};
        }
    }

    HostGpr GetHost(u32 guest) { return GetHost(guest, false); }

    HostGpr GetHostMarkDirty(u32 guest) { return GetHost(guest, guest != 0); }

    HostGpr GetTemporary() { return {}; }

    bool IsBound(u32 guest) const { return guest_to_host[guest] != nullptr; }

    bool IsBound(HostGpr host) const
    {
        for (Binding const& b : bindings) {
            if (b.host == host) return b.Occupied();
        }
        return false;
    }

    void ReleaseTemporary(Gp t) {}

protected:
    struct Binding {
        HostGpr host{};
        std::optional<u32> guest;
        u64 access_index;
        bool dirty;
        bool is_volatile{};
        bool Occupied() const { return guest.has_value(); }
        void Reset()
        {
            guest = {};
            access_index = access_index;
            dirty = false;
        }
    };

    static constexpr bool mips32 = sizeof(GuestGprInt) == 4;
    static constexpr bool mips64 = sizeof(GuestGprInt) == 8;
    static_assert(mips32 || mips64);

    static constexpr uint register_stack_space = 8 * 16;

    AsmjitCompiler& c;
    std::array<Binding, num_volatile_gprs + num_nonvolatile_gprs> bindings;
    std::array<Binding*, 32> guest_to_host;
    std::span<const GuestGprInt, 32> guest_gpr;
    u64 host_access_index;
    HostGpr guest_gpr_pointer_reg;
    typename decltype(bindings)::iterator volatile_bindings_begin_it, volatile_bindings_end_it,
      nonvolatile_bindings_begin_it, nonvolatile_bindings_end_it;

    void Flush(Binding& b, bool restore)
    {
        if (!b.dirty) return;
        b.dirty = false;
        if constexpr (arch.a64) {
        } else {
            if constexpr (mips32) {
                c.mov(dword_ptr(guest_gpr_pointer_reg, 4 * b.guest.value()), b.host.r32());
            } else {
                c.mov(qword_ptr(guest_gpr_pointer_reg, 8 * b.guest.value()), b.host.r64());
            }
            if (!b.is_volatile && restore) {
                RestoreHostGpr(b.host);
            }
        }
    }

    HostGpr GetHost(u32 guest, bool make_dirty)
    {
        Binding* binding = guest_to_host[guest];
        if (binding) {
            binding->dirty |= make_dirty;
            binding->access_index = host_access_index++;
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

        // 'make_dirty' should be true if the given guest register is a destination in the current instruction.
        // If so, search for a free host register among the nonvolatile ones first, to reduce the number of
        // dirty host registers that need to be flushed on block function calls.

        if (make_dirty) {
            FindReg(nonvolatile_bindings_begin_it, nonvolatile_bindings_end_it);
            if (!found_free) {
                FindReg(volatile_bindings_begin_it, volatile_bindings_end_it);
            }
        } else {
            FindReg(volatile_bindings_begin_it, volatile_bindings_end_it);
            if (!found_free) {
                FindReg(nonvolatile_bindings_begin_it, nonvolatile_bindings_end_it);
            }
        }

        if (!found_free) {
            Flush(*binding, false);
            guest_to_host[binding->guest.value()] = {};
        }
        binding->guest = guest;
        binding->access_index = host_access_index++;
        binding->dirty = make_dirty;
        guest_to_host[guest] = binding;
        Load(*binding, found_free);
        return binding->host;
    }

    void Load(Binding& binding, bool save)
    {
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            if (!binding.is_volatile && save) {
                SaveHostGpr(binding.host);
            }
            auto guest = binding.guest.value();
            if (guest == 0) {
                c.xor_(binding.host.r32(), binding.host.r32());
            } else {
                if constexpr (mips32) {
                    c.mov(binding.host.r32(), dword_ptr(guest_gpr_pointer_reg, 4 * guest));
                } else {
                    c.mov(binding.host.r64(), qword_ptr(guest_gpr_pointer_reg, 8 * guest));
                }
            }
        }
    }

    void RestoreHostGpr(HostGpr gpr)
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(gpr.r64(), qword_ptr(rsp, 8 * gpr.id()));
        }
    }

    void SaveHostGpr(HostGpr gpr)
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(qword_ptr(rsp, 8 * gpr.id()), gpr.r64());
        }
    }
};

} // namespace mips

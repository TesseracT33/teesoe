#pragma once

#include "jit_common.hpp"
#include "mips/disassembler.hpp"
#include "numtypes.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace mips {

// clang-format off
/*
    TODO: possible improvement is to not load register from guest when GetDirtyGpr is called. Right now, in this situation:
        addu $1, $2, $3,
    we would also load $1, even though it is overwritten.
    However, it causes some problems, where given
        addu $1, $1, $2,
    we must load $1 before writing to it. So we could do that when we encounter it the second time. However, then we must make sure that for:
        addu $1, $2, $3,
        addu $1, $1, $2,
    we mark $1 as "loaded" before starting the second addu, else, we may try to load it in the second addu even though it is dirty.
    One might think that checking the condition 'do_load && !is_dirty' could work to decide if to load, but for
        addu $1, $1, $2,
    this would not work, as is_dirty would already be set when we encounter $1 for the second time, so we would not load it there, which we need to.

    Possible solution would be to always get the source registers before the destination one?
*/
// clang-format on

// template<typename T, typename HostGpr>
// concept BaseRegisterAllocator = requires(T t) {
//     { t.SetupGprStackSpace() };
//     { t.SaveHost(std::declval<HostGpr>()) };
//     { t.RestoreHost(std::declval<HostGpr>()) };
//     { t.FlushGuest(std::declval<HostGpr>(), std::declval<u32>()) };
// };

template<typename RegisterAllocator, typename HostGpr, size_t num_host_regs, size_t num_guest_regs>
struct RegisterAllocatorState {
    struct Binding {
        HostGpr host;
        std::optional<u8> guest;
        u16 access_index;
        bool dirty;
        bool is_volatile;
        bool reserved;
        bool Occupied() const { return guest.has_value(); }
    };

    RegisterAllocatorState(RegisterAllocator* reg_alloc) : reg_alloc{ reg_alloc } {}
    std::array<Binding, num_host_regs> bindings{};
    std::array<Binding*, num_guest_regs> guest_to_host{};
    typename decltype(bindings)::iterator next_free_binding_it{ bindings.begin() };
    RegisterAllocator* reg_alloc{};
    u16 host_access_index{};
    bool nonvolatile_gprs_used{};

    void DestroyVolatile(HostGpr gpr)
    {
        assert(IsVolatile(gpr));
        auto it = std::ranges::find_if(bindings, [gpr](Binding const& b) { return b.host == gpr; });
        if (it != bindings.end()) {
            FlushAndDestroyBinding(*it, false);
        }
    }

    void Flush(Binding const& b, bool restore) const
    {
        if (b.Occupied()) {
            if (b.dirty) {
                reg_alloc->FlushGuest(b.host, b.guest.value());
            }
            if (!b.is_volatile && restore) {
                reg_alloc->RestoreHost(b.host);
            }
        }
    }

    void FlushAndDestroyAllVolatile()
    {
        for (Binding& binding : bindings) {
            if (binding.is_volatile) {
                FlushAndDestroyBinding(binding, false);
            }
        }
    }

    void FlushAll() const
    {
        for (Binding const& binding : bindings) {
            Flush(binding, false);
        }
    }

    void FlushAndDestroyBinding(Binding& b, bool restore)
    {
        Flush(b, restore);
        ResetBinding(b);
    }

    // This should only be used as part of an instruction epilogue. Thus, there is no need
    // to destroy bindings. In fact, this would be undesirable, since this function could not
    // be called in an epilog emitted mid-block, as part of a code path dependent on a run-time branch.
    void FlushAndRestoreAll() const
    {
        for (Binding const& binding : bindings) {
            Flush(binding, true);
        }
    }

    void FillBindings(std::span<HostGpr const> volatile_regs, std::span<HostGpr const> nonvolatile_regs)
    {
        assert(std::ranges::all_of(volatile_regs, [](HostGpr reg) { return IsVolatile(reg); }));
        assert(std::ranges::none_of(nonvolatile_regs, [](HostGpr reg) { return IsVolatile(reg); }));
        assert(volatile_regs.size() + nonvolatile_regs.size() == bindings.size());
        std::ranges::transform(volatile_regs, bindings.begin(), [](HostGpr reg) {
            return Binding{
                .host = reg,
                .guest = {},
                .access_index = 0,
                .dirty = false,
                .is_volatile = true,
                .reserved = false,
            };
        });
        std::ranges::transform(nonvolatile_regs, bindings.begin() + volatile_regs.size(), [](HostGpr reg) {
            return Binding{
                .host = reg,
                .guest = {},
                .access_index = 0,
                .dirty = false,
                .is_volatile = false,
                .reserved = false,
            };
        });
    }

    void Free(HostGpr reg)
    {
        auto it = std::ranges::find_if(bindings, [reg](Binding& b) { return b.host == reg; });
        if (it != bindings.end()) {
            it->reserved = false;
        }
    }

    HostGpr GetGpr(u32 guest, bool mark_dirty)
    {
        assert(guest < guest_to_host.size());
        Binding* binding = guest_to_host[guest];
        if (binding) {
            binding->access_index = host_access_index++;
            binding->dirty |= mark_dirty;
            return binding->host;
        }

        bool found_free{};

        if (next_free_binding_it != bindings.end()) {
            binding = &*(next_free_binding_it++);
            found_free = true;
        } else {
            auto min_access_index = std::numeric_limits<decltype(host_access_index)>::max();
            for (Binding& b : bindings) {
                if (b.reserved) {
                    continue;
                }
                if (!b.Occupied()) {
                    found_free = true;
                    binding = &b;
                    break;
                } else if (b.access_index < min_access_index) {
                    min_access_index = b.access_index;
                    binding = &b;
                }
            }
            assert(binding);
            if (!found_free) {
                Flush(*binding, false);
            }
        }

        binding->guest = u8(guest);
        binding->access_index = host_access_index++;
        binding->dirty = mark_dirty;
        guest_to_host[guest] = binding;

        if (!binding->is_volatile) {
            if (!std::exchange(nonvolatile_gprs_used, true)) {
                reg_alloc->SetupGprStackSpace();
            }
            if (found_free) {
                reg_alloc->SaveHost(binding->host);
            }
        }

        reg_alloc->LoadGuest(binding->host, guest);

        return binding->host;
    }

    std::string GetStatus() const
    {
        std::string used_str, free_str; // todo: static buffer
        for (Binding const& b : bindings) {
            auto host_reg_str{ HostRegToStr(b.host) };
            if (b.Occupied()) {
                u32 guest = b.guest.value();
                std::string guest_reg_str;
                if constexpr (HostGpr{}.isXmm()) {
                    guest_reg_str = std::format("$v{}", guest);
                } else {
                    guest_reg_str = std::string(mips::GprIdxToName(guest));
                }
                used_str.append(std::format("{}({},{}),", host_reg_str, guest_reg_str, b.dirty ? 'd' : 'c'));
            } else {
                free_str.append(host_reg_str);
                free_str.push_back(',');
            }
        }
        return std::format("Used: {}; Free: {}\n", used_str, free_str);
    }

    void Reserve(HostGpr reg)
    {
        auto it = std::ranges::find_if(bindings, [reg](Binding& b) { return b.host == reg; });
        if (it != bindings.end()) {
            FlushAndDestroyBinding(*it, true);
            it->reserved = true;
        }
    }

    void Reset()
    {
        for (Binding& b : bindings) {
            b.access_index = 0;
            b.dirty = false;
            b.guest = {};
            b.reserved = false;
        }
        guest_to_host = {};
        host_access_index = 0;
        next_free_binding_it = bindings.begin();
        nonvolatile_gprs_used = false;
    }

    void ResetBinding(Binding& b)
    {
        if (b.Occupied()) {
            guest_to_host[b.guest.value()] = {};
            b.dirty = false;
            b.guest = {};
            // todo: clear reserved?
        }
    }
};

} // namespace mips

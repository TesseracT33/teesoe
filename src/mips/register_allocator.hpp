#pragma once

#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "host.hpp"
#include "jit_util.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>

namespace mips {

template<typename GuestGprInt> class RegisterAllocator {
public:
    RegisterAllocator(std::span<const GuestGprInt, 32> guest_gpr, AsmjitCompiler& compiler)
      : c(compiler),
        guest_gpr(guest_gpr),
        access_index(0),
        num_nonvolatile_regs_used(0),
        guest_to_host()
    {
    }

    void BlockEpilog()
    {
        EmitFlushAll();
        if constexpr (arch.a64) {
        } else {
            c.pop(host_zero_reg);
        }
    }

    void BlockEpilogWithJmp(void* func)
    {
        EmitFlushAll();
        if constexpr (arch.a64) {
        } else {
            c.pop(host_zero_reg);
            c.jmp(func);
        }
    }

    void BlockProlog()
    {
        for (Binding& binding : volatile_bindings) {
            binding.guest = {};
            binding.access_index = access_index;
            binding.dirty = false;
        }
        for (Binding& binding : nonvolatile_bindings) {
            binding.guest = {};
            binding.access_index = access_index;
            binding.dirty = false;
        }
        guest_to_host = {};
        if constexpr (arch.a64) {
        } else {
            c.push(host_zero_reg);
            c.xor_(host_zero_reg.r32(), host_zero_reg.r32());
        }
    }

    void Call(auto func)
    {
        FlushAllVolatile();
        if constexpr (arch.a64) {
        } else {
            size_t num_bound_nonvolatiles = std::accumulate(nonvolatile_bindings.begin(),
                                              nonvolatile_bindings.end(),
                                              size_t{},
                                              [](size_t count, Binding const& b) { return count + b.Occupied(); })
                                          + 1; // +1 for rbx
            using namespace asmjit::x86;
            if constexpr (os.linux) {
                if (num_bound_nonvolatiles % 2) {
                    c.call(func);
                } else {
                    c.push(rax);
                    c.call(func);
                    c.pop(rcx);
                }
            } else {
                auto diff = num_bound_nonvolatiles % 2 ? 32 : 40;
                c.sub(rsp, diff);
                c.call(func);
                c.add(rsp, diff);
            }
        }
    }

    void EmitFlushAll()
    {
        for (Binding& binding : volatile_bindings) {
            if (binding.dirty) {
                Flush(binding);
            }
        }
        for (Binding& binding : nonvolatile_bindings) {
            if (binding.dirty) {
                Flush(binding);
            }
        }
    }

    void Flush(HostGpr reg) {}

    void FlushAll()
    {
        for (Binding& binding : volatile_bindings) {
            if (binding.dirty) {
                binding.dirty = false;
                Flush(binding);
            }
            binding.guest = {};
        }
        for (Binding& binding : nonvolatile_bindings) {
            if (binding.dirty) {
                binding.dirty = false;
                Flush(binding);
            }
            binding.guest = {};
        }
        guest_to_host = {};
    }

    void FlushAllVolatile()
    {
        for (Binding& binding : volatile_bindings) {
            if (binding.is_volatile) {
                if (binding.dirty) {
                    binding.dirty = false;
                    Flush(binding);
                }
                guest_to_host[binding.guest.value()] = {};
                binding.guest = {};
            }
        }
    }

    // void Free(HostGpr host)
    //{
    //     auto destroyed_it =
    //       std::ranges::find_if(bindings, [host](Binding const& binding) { return binding.host == host; });
    //     assert(destroyed_it != bindings.end());

    //    if (!destroyed_it->Occupied()) return;

    //    auto replaced_it = std::ranges::upper_bound(bindings,
    //      destroyed_it->access_index,
    //      [&host](u64 acc_index, Binding const& binding) {
    //          return acc_index < binding.access_index && binding.host != host;
    //      });

    //    if (replaced_it == bindings.end()) {
    //        guest_to_host[destroyed_it->guest.value()] = {};
    //    } else {
    //        if (replaced_it->dirty) Flush(*replaced_it);
    //        guest_to_host[destroyed_it->guest.value()] = replaced_it;
    //        guest_to_host[replaced_it->guest.value()] = {};
    //        replaced_it->guest = destroyed_it->guest;
    //        replaced_it->access_index = destroyed_it->access_index;
    //        replaced_it->dirty = destroyed_it->dirty;
    //    }

    //    if (destroyed_it->dirty) {
    //        Flush(*destroyed_it);
    //        destroyed_it->dirty = false;
    //    }
    //    destroyed_it->guest = {};
    //}

    HostGpr GetHost(u32 guest) { return GetHost(guest, false); }

    HostGpr GetHostMarkDirty(u32 guest) { return GetHost(guest, true); }

    bool IsBound(u32 guest) const { return guest_to_host[guest] != nullptr; }

    bool IsBound(HostGpr host) const
    {
        for (Binding const& b : volatile_bindings) {
            if (b.host == host) return b.Occupied();
        }
        for (Binding const& b : nonvolatile_bindings) {
            if (b.host == host) return b.Occupied();
        }
        return false;
    }

private:
    struct Binding {
        HostGpr const host{};
        std::optional<u32> guest;
        u64 access_index;
        bool dirty;
        bool const is_volatile{};
        bool Occupied() const { return guest.has_value(); }
    };

    static constexpr HostGpr host_zero_reg = [] {
        if constexpr (arch.a64) return asmjit::a64::x19;
        if constexpr (arch.x64) return asmjit::x86::rbx;
    }();

    static constexpr size_t num_volatile_host_gprs = os.linux ? 8 : 6;
    static constexpr size_t num_nonvolatile_host_gprs = os.linux ? 4 : 6;

    std::array<Binding, num_volatile_host_gprs> volatile_bindings = [] {
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            if constexpr (os.linux) {
                return std::array{
                    Binding{ .host = rdi, .is_volatile = true },
                    Binding{ .host = rdi, .is_volatile = true },
                    Binding{ .host = rdx, .is_volatile = true },
                    Binding{ .host = rcx, .is_volatile = true },
                    Binding{ .host = r9, .is_volatile = true },
                    Binding{ .host = r8, .is_volatile = true },
                    Binding{ .host = r10, .is_volatile = true },
                    Binding{ .host = r11, .is_volatile = true },
                };
            } else {
                return std::array{
                    Binding{ .host = rdx, .is_volatile = true },
                    Binding{ .host = rcx, .is_volatile = true },
                    Binding{ .host = r9, .is_volatile = true },
                    Binding{ .host = r8, .is_volatile = true },
                    Binding{ .host = r10, .is_volatile = true },
                    Binding{ .host = r11, .is_volatile = true },
                };
            }
        }
    }();

    std::array<Binding, num_nonvolatile_host_gprs> nonvolatile_bindings = [] {
        if constexpr (arch.a64) {
        } else {
            using namespace asmjit::x86;
            if constexpr (os.linux) {
                return std::array{
                    Binding{ .host = r12, .is_volatile = false },
                    Binding{ .host = r13, .is_volatile = false },
                    Binding{ .host = r14, .is_volatile = false },
                    Binding{ .host = r15, .is_volatile = false },
                };
            } else {
                return std::array{
                    Binding{ .host = r12, .is_volatile = false },
                    Binding{ .host = r13, .is_volatile = false },
                    Binding{ .host = r14, .is_volatile = false },
                    Binding{ .host = r15, .is_volatile = false },
                    Binding{ .host = rdi, .is_volatile = false },
                    Binding{ .host = rsi, .is_volatile = false },
                };
            }
        }
    }();

    AsmjitCompiler& c;
    std::span<const GuestGprInt, 32> guest_gpr;
    std::array<Binding*, 32> guest_to_host;
    u64 access_index;
    uint num_nonvolatile_regs_used;

    void Flush(Binding& binding)
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(ptr(guest_gpr[binding.guest.value()]), binding.host);
            if (binding.is_volatile) {
                c.pop(binding.host);
            }
        }
    }

    HostGpr GetHost(u32 guest, bool make_dirty)
    {
        if (guest == 0) {
            return host_zero_reg;
        }
        if (Binding* binding = guest_to_host[guest]; binding) {
            binding->access_index = access_index++;
            return binding->host;
        }

        Binding* binding{};
        bool replaced{ true };
        u64 min_access = std::numeric_limits<u64>::max();

        for (Binding& b : (make_dirty ? nonvolatile_bindings : volatile_bindings)) {
            if (!b.Occupied()) {
                binding = &b;
                replaced = false;
                break;
            } else if (b.access_index < min_access) {
                min_access = b.access_index;
                binding = &b;
            }
        }

        if (replaced) {
            if (binding->dirty) {
                Flush(*binding);
            }
            guest_to_host[binding->guest.value()] = {};
        }
        binding->guest = guest;
        binding->access_index = access_index++;
        binding->dirty = make_dirty;
        guest_to_host[guest] = binding;
        Load(*binding, guest);
        return binding->host;
    }

    void Load(Binding& binding, u32 guest)
    {
        if constexpr (arch.a64) {
        } else {
            c.mov(binding.host, ptr(guest_gpr[guest]));
        }
    }
};

} // namespace mips

#pragma once

#include "asmjit/a64.h"
#include "asmjit/x86.h"
#include "jit_util.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <span>
#include <type_traits>

namespace mips {

template<std::integral GprInt, typename Compiler>
    requires(std::derived_from<Compiler, asmjit::BaseCompiler>)
class RegisterAllocator {
    static constexpr bool arm64 = std::same_as<Compiler, asmjit::a64::Compiler>;
    static constexpr bool x64 = !arm64;

    using HostReg = std::conditional_t<arm64, asmjit::a64::Gp, asmjit::x86::Gp>;

public:
    RegisterAllocator(std::span<GprInt, 32> gpr, Compiler& compiler) : compiler(compiler), gpr(gpr), access_index(0) {}

    void AllocateAsDirty(u32 guest)
    {
        auto flush_it = bindings.end();
        u64 min_access = std::numeric_limits<u64>::max();
        for (auto it = bindings.begin(); it != bindings.end(); ++it) {
            if (it->guest == guest) {
                it->access_index++;
                it->dirty = true;
                return;
            }
            if (it->access_index < min_access) {
                min_access = it->access_index;
            }
        }
        // if (match_it == bindings.end()) {
        //     Flush(*flush_it);
        //     flush_it->guest = guest;
        //     compiler.mov(v, flush_it->host);
        //     return v;
        // } else {
        //     match_it->access_index++;
        //     return match_it->host;
        // }
    }

    void Bind(HostReg host, u32 guest)
    {
        auto it = std::ranges::find_if(bindings, [guest](Binding const& binding) { return binding.host == host; });
        assert(it != bindings.end());
        if (it->dirty) {
            it->dirty = false;
            compiler.mov(ptr(gpr[it->guest]), it->host);
        }
        it->guest = guest;
    }

    void Destroy(HostReg host)
    {
        auto destroyed_it =
          std::ranges::find_if(bindings, [host](Binding const& binding) { return binding.host == host; });
        assert(destroyed_it != bindings.end());

        // auto replaced_it = std::ranges::upper_bound(bindings, Binding const& binding, []()

        // if (it->dirty) {
        //     it->dirty = false;
        //     compiler.mov(ptr(gpr[it->guest]), it->host);
        // }
    }

    HostReg GetHost(u32 guest)
    {
        auto min_it = bindings.end();
        u64 min_access = std::numeric_limits<u64>::max();
        for (auto it = bindings.begin(); it != bindings.end(); ++it) {
            if (it->guest == guest) {
                it->access_index++;
                return it->host;
            }
            if (it->access_index < min_access) {
                min_access = it->access_index;
                min_it = it;
            }
        }
        if (min_it->dirty) {
            Flush(*min_it);
        }
        min_it->guest = guest;
        min_it->access_index = access_index++;
        compiler.mov(min_it->host, ptr(gpr[guest]));
        return min_it->host;
    }

    void FlushAll()
    {
        for (Binding& binding : bindings) {
            if (binding.dirty) Flush(binding);
        }
    }

private:
    struct Binding {
        HostReg const host;
        u64 access_index;
        u32 guest;
        bool dirty;
    };

    std::span<GprInt, 32> gpr;
    Compiler& compiler;
    u64 access_index;

    std::array<Binding, gp.size()> bindings = [] {
        std::array<Binding, gp.size()> b;
        std::ranges::transform(gp, b.begin(), [](auto reg) { return Binding{ .host = reg }; });
        return b;
    }();

    void Flush(Binding& binding)
    {
        binding.dirty = false;
        compiler.mov(ptr(gpr[binding.guest]), binding.host);
        binding.guest = std::numeric_limits<u64>::max();
    }
};

} // namespace mips

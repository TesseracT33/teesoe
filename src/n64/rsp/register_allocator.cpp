#include "register_allocator.hpp"
#include "jit_common.hpp"
#include "rsp/recompiler.hpp"

#include <algorithm>
#include <cassert>
#include <format>

using namespace asmjit;

namespace n64::rsp {

constexpr int gprs_stack_space = 8 * host_num_gprs;
constexpr int vprs_stack_space = 16 * host_num_vprs;
constexpr int register_stack_space = gprs_stack_space + vprs_stack_space;

static_assert(!IsVolatile(guest_gpr_mid_ptr_reg));

RegisterAllocator::RegisterAllocator(JitCompiler& compiler,
  std::span<s32 const, 32> guest_gprs,
  std::span<m128i const, 32> guest_vprs)
  : state_gpr{ this },
    state_vpr{ this },
    guest_gprs{ guest_gprs },
    guest_vprs{ guest_vprs },
    c{ compiler },
    gpr_mid_ptr{ guest_gprs.data() + guest_gprs.size() / 2 },
    vpr_mid_ptr{ guest_vprs.data() + guest_vprs.size() / 2 },
    gpr_stack_space_setup{},
    vte_reg_saved{}
{
    state_gpr.FillBindings(reg_alloc_volatile_gprs, reg_alloc_nonvolatile_gprs);
    state_vpr.FillBindings(reg_alloc_volatile_vprs, reg_alloc_nonvolatile_vprs);
}

void RegisterAllocator::BlockEpilog()
{
    state_gpr.FlushAndRestoreAll();
    state_vpr.FlushAndRestoreAll();
    if constexpr (!IsVolatile(reg_alloc_vte_reg)) {
        if (vte_reg_saved) {
            RestoreHost(reg_alloc_vte_reg);
        }
    }
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        if (gpr_stack_space_setup) {
            c.add(x86::rsp, register_stack_space);
        }
        c.pop(guest_gpr_mid_ptr_reg);
        c.ret();
    }
}

void RegisterAllocator::BlockEpilogWithJmp(void* func)
{
    state_gpr.FlushAndRestoreAll();
    state_vpr.FlushAndRestoreAll();
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        if (gpr_stack_space_setup) {
            c.add(x86::rsp, register_stack_space);
        }
        c.pop(guest_gpr_mid_ptr_reg);
        c.jmp(func);
    }
}

void RegisterAllocator::BlockProlog()
{
    Reset();
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        c.push(guest_gpr_mid_ptr_reg);
        c.mov(guest_gpr_mid_ptr_reg, host_gpr_arg[0]);
    }
}

void RegisterAllocator::Call(void* func)
{
    static_assert(register_stack_space % 16 == 0);
    FlushAllVolatile();
    jit_call_with_stack_alignment(c, func);
}

void RegisterAllocator::DestroyVolatile(HostGpr64 gpr)
{
    state_gpr.DestroyVolatile(gpr.r32());
}

void RegisterAllocator::FlushGuest(HostGpr32 host, u32 guest)
{
    assert(guest != 0);
    s32 offset = GetGprMidPtrOffset(guest);
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        c.mov(dword_ptr(guest_gpr_mid_ptr_reg, offset), host);
    }
}

void RegisterAllocator::FlushGuest(HostVpr128 host, u32 guest)
{
    if (guest < 32) {
        s32 offset = GetVprMidPtrOffset(guest);
        if constexpr (platform.a64) {}
        if constexpr (platform.x64) {
            c.vmovaps(xmmword_ptr(guest_vpr_mid_ptr_reg, offset), host);
        }
    } else {
        assert(guest <= AccHighIndex);
        static_assert(AccLowIndex == 32);
        if constexpr (platform.a64) {}
        if constexpr (platform.x64) {
            c.vmovaps(JitPtr(acc.elems[guest - 32]), host);
        }
    }
}

void RegisterAllocator::FlushAll() const
{
    state_gpr.FlushAll();
    state_vpr.FlushAll();
}

void RegisterAllocator::FlushAllVolatile()
{
    state_gpr.FlushAndDestroyAllVolatile();
    state_vpr.FlushAndDestroyAllVolatile();
}

void RegisterAllocator::FreeArgs(int args)
{
    for (int i = 0; i < std::min(args, (int)host_gpr_arg.size()); ++i) {
        state_gpr.Free(host_gpr_arg[i].r32());
    }
}

HostGpr128 RegisterAllocator::GetAccLow()
{
    return GetVpr(AccLowIndex);
}

HostGpr128 RegisterAllocator::GetAccMid()
{
    return GetVpr(AccMidIndex);
}

HostGpr128 RegisterAllocator::GetAccHigh()
{
    return GetVpr(AccHighIndex);
}

HostGpr128 RegisterAllocator::GetDirtyAccLow()
{
    return GetDirtyVpr(AccLowIndex);
}

HostGpr128 RegisterAllocator::GetDirtyAccMid()
{
    return GetDirtyVpr(AccMidIndex);
}

HostGpr128 RegisterAllocator::GetDirtyAccHigh()
{
    return GetDirtyVpr(AccHighIndex);
}

HostGpr32 RegisterAllocator::GetDirtyGpr(u32 guest)
{
    bool mark_dirty = guest != 0;
    assert(mark_dirty);
    return state_gpr.GetGpr(guest, mark_dirty);
}

HostGpr128 RegisterAllocator::GetDirtyVpr(u32 guest)
{
    bool mark_dirty = guest != 0;
    assert(mark_dirty);
    return state_vpr.GetGpr(guest, mark_dirty);
}

HostGpr32 RegisterAllocator::GetGpr(u32 guest)
{
    return state_gpr.GetGpr(guest, false);
}

s32 RegisterAllocator::GetGprMidPtrOffset(u32 guest) const
{
    return s32(sizeof(guest_gprs[0])) * (s32(guest) - s32(guest_gprs.size()) / 2);
}

s32 RegisterAllocator::GetVprMidPtrOffset(u32 guest) const
{
    return s32(sizeof(guest_vprs[0])) * (s32(guest) - s32(guest_vprs.size()) / 2);
}

s32 RegisterAllocator::get_nonvolatile_host_gpr_stack_offset(HostGpr32 gpr)
{
    return 8 * gpr.id();
}

s32 RegisterAllocator::get_nonvolatile_host_vpr_stack_offset(HostVpr128 vpr)
{
    return gprs_stack_space + 16 * vpr.id();
}

std::string RegisterAllocator::GetStatus() const
{
    std::string gpr_status = state_gpr.GetStatus();
    std::string vpr_status = state_vpr.GetStatus();
    return std::format("GPRs: {}\nVPRs: {}", gpr_status, vpr_status);
}

HostGpr128 RegisterAllocator::GetVpr(u32 guest)
{
    return state_vpr.GetGpr(guest, false);
}

HostVpr128 RegisterAllocator::GetVte()
{
    if constexpr (!IsVolatile(reg_alloc_vte_reg)) {
        if (!std::exchange(vte_reg_saved, true)) {
            SetupGprStackSpace();
            SaveHost(reg_alloc_vte_reg);
        }
    }
    return reg_alloc_vte_reg;
}

void RegisterAllocator::LoadGuest(HostGpr32 host, u32 guest) const
{
    assert(guest < 32);
    if (guest == 0) {
        if constexpr (platform.a64) {
            c.mov(host, 0);
        }
        if constexpr (platform.x64) {
            c.xor_(host, host);
        }
    } else {
        s32 offset = GetGprMidPtrOffset(guest);
        if constexpr (platform.a64) {
            // TODO
        }
        if constexpr (platform.x64) {
            c.mov(host, dword_ptr(guest_gpr_mid_ptr_reg, offset));
        }
    }
}

void RegisterAllocator::LoadGuest(HostGpr128 host, u32 guest) const
{
    if (guest < 32) {
        s32 offset = GetVprMidPtrOffset(guest);
        if constexpr (platform.a64) {}
        if constexpr (platform.x64) {
            c.vmovaps(host, xmmword_ptr(guest_vpr_mid_ptr_reg, offset));
        }
    } else {
        assert(guest <= AccHighIndex);
        static_assert(AccLowIndex == 32);
        if constexpr (platform.a64) {}
        if constexpr (platform.x64) {
            c.vmovaps(host, JitPtr(acc.elems[guest - 32]));
        }
    }
}

void RegisterAllocator::ReserveArgs(int args)
{
    for (int i = 0; i < std::min(args, (int)host_gpr_arg.size()); ++i) {
        state_gpr.Reserve(host_gpr_arg[i].r32());
    }
}

void RegisterAllocator::Reset()
{
    state_gpr.Reset();
    state_vpr.Reset();
    gpr_stack_space_setup = false;
    vte_reg_saved = false;
}

void RegisterAllocator::RestoreHost(HostGpr32 host) const
{
    assert(!IsVolatile(host));
    s32 stack_offset = get_nonvolatile_host_gpr_stack_offset(host);
    if constexpr (platform.a64) {
        // TODO
    }
    if constexpr (platform.x64) {
        c.mov(host.r64(), qword_ptr(x86::rsp, stack_offset));
    }
}

void RegisterAllocator::RestoreHost(HostVpr128 host) const
{
    assert(!IsVolatile(host));
    s32 stack_offset = get_nonvolatile_host_vpr_stack_offset(host);
    if constexpr (platform.a64) {
        // TODO
    }
    if constexpr (platform.x64) {
        c.vmovaps(host, xmmword_ptr(x86::rsp, stack_offset));
    }
}

void RegisterAllocator::SaveHost(HostGpr32 host) const
{
    assert(!IsVolatile(host));
    s32 stack_offset = get_nonvolatile_host_gpr_stack_offset(host);
    if constexpr (platform.a64) {
        // TODO
    }
    if constexpr (platform.x64) {
        c.mov(qword_ptr(x86::rsp, stack_offset), host.r64());
    }
}

void RegisterAllocator::SaveHost(HostVpr128 host) const
{
    assert(!IsVolatile(host));
    s32 stack_offset = get_nonvolatile_host_vpr_stack_offset(host);
    if constexpr (platform.a64) {
        // TODO
    }
    if constexpr (platform.x64) {
        c.vmovaps(xmmword_ptr(x86::rsp, stack_offset), host);
    }
}

void RegisterAllocator::SetupGprStackSpace()
{
    if (!std::exchange(gpr_stack_space_setup, true)) {
        if constexpr (platform.a64) {
            // TODO
        }
        if constexpr (platform.x64) {
            c.sub(x86::rsp, register_stack_space);
        }
    }
}

} // namespace n64::rsp

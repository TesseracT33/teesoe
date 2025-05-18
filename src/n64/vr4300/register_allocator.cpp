#include "register_allocator.hpp"
#include "jit_common.hpp"

#include <algorithm>
#include <asmjit/x86/x86operand.h>
#include <cassert>
#include <format>

using namespace asmjit;

namespace n64::vr4300 {

constexpr int gprs_stack_space = 8 * host_num_gprs;
constexpr int vprs_stack_space =
  16 * (platform.abi.systemv ? 0 : host_num_vprs); // on SystemV there are no non-volatile XMM registers; no need
                                                   // to allocate stack for them.
constexpr int register_stack_space = gprs_stack_space + vprs_stack_space;

static_assert(!IsVolatile(guest_gpr_mid_ptr_reg));

RegisterAllocator::RegisterAllocator(JitCompiler& compiler,
  std::span<s64 const, 32> guest_gprs,
  std::span<s64 const, 32> guest_fprs)
  : state_gpr{ this },
    state_fpr{ this },
    guest_gprs{ guest_gprs },
    guest_fprs{ guest_fprs },
    c{ compiler },
    gpr_mid_ptr{ guest_gprs.data() + guest_gprs.size() / 2 },
    fpr_mid_ptr{ guest_fprs.data() + guest_fprs.size() / 2 },
    fp_instructions_used_in_current_block{},
    gpr_stack_space_setup{}
{
    state_gpr.FillBindings(reg_alloc_volatile_gprs, reg_alloc_nonvolatile_gprs);
    state_fpr.FillBindings(reg_alloc_volatile_vprs, reg_alloc_nonvolatile_vprs);
}

void RegisterAllocator::BlockEpilog()
{
    state_gpr.FlushAndRestoreAll();
    state_fpr.FlushAndRestoreAll();
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        // shorter-form instruction if immediate is signed 8-bit; the space happens to be 128 bytes.
        // if (stack_space_setup) {
        //     int stack_space_used = gprs_stack_space;
        // }
        // if (fp_instructions_used_in_current_block) {
        //     stack_space_used += vprs_stack_space;
        // }
        // if (stack_space_used > 0) {
        //     c.sub(x86::rsp, -register_stack_space);
        // }
        c.pop(guest_gpr_mid_ptr_reg);
        c.ret();
    }
}

void RegisterAllocator::BlockEpilogWithJmp(void* func)
{
    state_gpr.FlushAndRestoreAll();
    state_fpr.FlushAndRestoreAll();
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
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
    stack_is_16_byte_aligned = true;
}

void RegisterAllocator::Call(void* func)
{
    // static_assert(register_stack_space % 16 == 8); // Given this, the stack should already be aligned with the CALL,
    //  unless an instruction impl used PUSH. TODO: make this more robust
    FlushAllVolatile();
    if (stack_is_16_byte_aligned) {
        jit_call_no_stack_alignment(c, func);
    }
}

void RegisterAllocator::CallWithStackAlignment(void* func)
{
    FlushAllVolatile();
    if (!stack_is_16_byte_aligned) {
        jit_call_with_stack_alignment(c, func);
    }
}

void RegisterAllocator::DestroyVolatile(HostGpr64 gpr)
{
    state_gpr.DestroyVolatile(gpr);
}

void RegisterAllocator::FlushGuest(HostGpr64 host, u32 guest)
{
    assert(guest != 0);
    s32 offset = GetGprMidPtrOffset(guest);
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        c.mov(qword_ptr(guest_gpr_mid_ptr_reg, offset), host);
    }
}

void RegisterAllocator::FlushGuest(HostVpr128 host, u32 guest)
{
    s32 offset = GetFprMidPtrOffset(guest);
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        c.vmovq(qword_ptr(guest_fpr_mid_ptr_reg, offset), host);
        // todo: need to implement all loading/storing logic of fpu registers
    }
}

void RegisterAllocator::FlushAll() const
{
    state_gpr.FlushAll();
    state_fpr.FlushAll();
}

void RegisterAllocator::FlushAllVolatile()
{
    state_gpr.FlushAndDestroyAllVolatile(); // todo: should it destroy?
    state_fpr.FlushAndDestroyAllVolatile();
}

void RegisterAllocator::FreeArgs(int args)
{
    for (int i = 0; i < std::min(args, (int)host_gpr_arg.size()); ++i) {
        state_gpr.Free(host_gpr_arg[i]);
    }
}

HostGpr64 RegisterAllocator::GetDirtyGpr(u32 guest)
{
    bool mark_dirty = guest != 0;
    assert(mark_dirty); // should never happen
    return state_gpr.GetGpr(guest, mark_dirty);
}

HostGpr128 RegisterAllocator::GetDirtyVpr(u32 guest)
{
    if constexpr (!platform.abi.systemv) {
        if (!std::exchange(fp_instructions_used_in_current_block, true)) {
            SetupFp();
        }
    }
    bool mark_dirty = guest != 0;
    assert(mark_dirty); // should never happen
    return state_fpr.GetGpr(guest, mark_dirty);
}

HostGpr64 RegisterAllocator::GetGpr(u32 guest)
{
    return state_gpr.GetGpr(guest, false);
}

s32 RegisterAllocator::GetFprMidPtrOffset(u32 guest) const
{
    return s32(sizeof(guest_fprs[0])) * (s32(guest) - s32(guest_fprs.size()) / 2);
}

s32 RegisterAllocator::GetGprMidPtrOffset(u32 guest) const
{
    return s32(sizeof(guest_gprs[0])) * (s32(guest) - s32(guest_gprs.size()) / 2);
}

s32 RegisterAllocator::get_nonvolatile_host_gpr_stack_offset(HostGpr64 gpr)
{
    return vprs_stack_space + 8 * gpr.id();
}

s32 RegisterAllocator::get_nonvolatile_host_vpr_stack_offset(HostVpr128 vpr)
{
    return 16 * vpr.id();
}

HostGpr128 RegisterAllocator::GetVpr(u32 guest)
{
    if constexpr (!platform.abi.systemv) {
        if (!std::exchange(fp_instructions_used_in_current_block, true)) {
            SetupFp();
        }
    }
    return state_fpr.GetGpr(guest, false);
}

std::string RegisterAllocator::GetStatus() const
{
    std::string gpr_status = state_gpr.GetStatus();
    std::string fpr_status = state_fpr.GetStatus();
    return std::format("GPRs: {}\nFPRs: {}", gpr_status, fpr_status);
}

void RegisterAllocator::LoadGuest(HostGpr64 host, u32 guest) const
{
    assert(guest < 32);
    if (guest == 0) {
        if constexpr (platform.a64) {
            c.mov(host, 0);
        }
        if constexpr (platform.x64) {
            c.xor_(host.r32(), host.r32());
        }
    } else {
        s32 offset = GetGprMidPtrOffset(guest);
        if constexpr (platform.a64) {
            // TODO
        }
        if constexpr (platform.x64) {
            c.mov(host, qword_ptr(guest_gpr_mid_ptr_reg, offset));
        }
    }
}

void RegisterAllocator::LoadGuest(HostGpr128 host, u32 guest) const
{
    assert(guest < 32);
    s32 offset = GetFprMidPtrOffset(guest);
    if constexpr (platform.a64) {}
    if constexpr (platform.x64) {
        c.vmovq(host, qword_ptr(guest_fpr_mid_ptr_reg, offset));
    }
}

void RegisterAllocator::ReserveArgs(int args)
{
    for (int i = 0; i < std::min(args, (int)host_gpr_arg.size()); ++i) {
        state_gpr.Reserve(host_gpr_arg[i]);
    }
}

void RegisterAllocator::Reset()
{
    state_gpr.Reset();
    state_fpr.Reset();
    fp_instructions_used_in_current_block = false;
    gpr_stack_space_setup = false;
}

void RegisterAllocator::RestoreHost(HostGpr64 host) const
{
    assert(!IsVolatile(host));
    // s32 stack_offset = get_nonvolatile_host_gpr_stack_offset(host);
    if constexpr (platform.a64) {
        // TODO
    }
    if constexpr (platform.x64) {
        //  c.mov(host, qword_ptr(x86::rsp, stack_offset));
        c.pop(host);
    }
}

void RegisterAllocator::RestoreHosts()
{
    /*  RSP     storage     operation
        n       <start>
        n-8     r12         push r12
        n-16    r13         push r13
        --------------
        n-32    xmm6
        n-48    xmm7
        n-64    -unused-
        ....
        n-256   -unused
        -------------       sub rsp, 224
        n-264   r14         push r14
        n-272   r15         push r15
    */
    while (!used_host_nonvolatiles.empty()) {
        if (fp_stack_alloc_offset == allocated_stack) {
            c.sub(x86::rsp, vprs_stack_space);
            allocated_stack -= vprs_stack_space;
        } else {
            x86::Gpq reg = used_host_nonvolatiles.top();
            used_host_nonvolatiles.pop();
            c.pop(reg);
            // allocated_stack -= 8;
            // if (reg.isGp64()) {
            //     c.pop(reg.as<x86::Gpq>());
            //     allocated_stack -= 8;
            // } else {
            //     assert(reg.isXmm());
            //     c.vmovaps(reg.as<x86::Xmm>(), xmmword_ptr(x86::rsp, stack_offset));
            //     stack_offset += 16;
            // }
        }
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

void RegisterAllocator::SaveHost(HostGpr64 host)
{
    assert(!IsVolatile(host));
    // s32 stack_offset = get_nonvolatile_host_gpr_stack_offset(host);
    if constexpr (platform.a64) {
        // TODO
    }
    if constexpr (platform.x64) {
        // c.mov(qword_ptr(x86::rsp, stack_offset), host);
        c.push(host);
        used_host_nonvolatiles.push(host);
    }
    stack_is_16_byte_aligned = !stack_is_16_byte_aligned;
}

void RegisterAllocator::SaveHost(HostVpr128 host)
{
    assert(!IsVolatile(host));
    if constexpr (!platform.abi.systemv) {
        if (!std::exchange(fp_instructions_used_in_current_block, true)) {
            SetupFp();
        }
        s32 stack_offset = allocated_stack - fp_stack_alloc_offset;
        if constexpr (platform.a64) {
            // TODO
        }
        if constexpr (platform.x64) {
            c.vmovaps(xmmword_ptr(x86::rsp, stack_offset), host);
        }
    }
}

void RegisterAllocator::SetupFp()
{
    static_assert(!IsVolatile(guest_fpr_mid_ptr_reg));
    state_gpr.Reserve(guest_fpr_mid_ptr_reg);
    SaveHost(guest_fpr_mid_ptr_reg); // todo: should only be done if it wasn't already saved
    s32 fpr_gpr_distance = s32(reinterpret_cast<u8 const*>(fpr_mid_ptr) - reinterpret_cast<u8 const*>(gpr_mid_ptr));
    c.lea(guest_fpr_mid_ptr_reg, ptr(guest_gpr_mid_ptr_reg, fpr_gpr_distance));
    // on SystemV there are no non-volatile XMM registers; no need to allocate stack for them.
    if constexpr (vprs_stack_space != 0) {
        fp_stack_alloc_offset = allocated_stack + vprs_stack_space;
        if constexpr (platform.a64) {
            // todo
        }
        if constexpr (platform.x64) {
            c.sub(x86::rsp, vprs_stack_space);
        }
    }
}

void RegisterAllocator::SetupGprStackSpace()
{
    if (!std::exchange(gpr_stack_space_setup, true)) {
        if constexpr (platform.a64) {
            // TODO
        }
        if constexpr (platform.x64) {
            c.add(x86::rsp, -register_stack_space); // shorter-form instruction if immediate is signed 8-bit; on
                                                    // SystemV, the space happens to be 128 bytes.
        }
    }
}

} // namespace n64::vr4300

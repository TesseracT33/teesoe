#include "exceptions.hpp"
#include "mips/recompiler.hpp"
#include "types.hpp"
#include "vr4300.hpp"

namespace n64::vr4300 {

struct Recompiler : public mips::Recompiler<s64, u64, u64> {
    using mips::Recompiler<s64, u64, u64>::Recompiler;

    void break_() const;
    void ddiv(u32 rs, u32 rt) const;
    void ddivu(u32 rs, u32 rt) const;
    void dmult(u32 rs, u32 rt) const;
    void dmultu(u32 rs, u32 rt) const;
    void lb(u32 rs, u32 rt, s16 imm) const;
    void lbu(u32 rs, u32 rt, s16 imm) const;
    void ld(u32 rs, u32 rt, s16 imm) const;
    void ldl(u32 rs, u32 rt, s16 imm) const;
    void ldr(u32 rs, u32 rt, s16 imm) const;
    void lh(u32 rs, u32 rt, s16 imm) const;
    void lhu(u32 rs, u32 rt, s16 imm) const;
    void ll(u32 rs, u32 rt, s16 imm) const;
    void lld(u32 rs, u32 rt, s16 imm) const;
    void lw(u32 rs, u32 rt, s16 imm) const;
    void lwl(u32 rs, u32 rt, s16 imm) const;
    void lwr(u32 rs, u32 rt, s16 imm) const;
    void lwu(u32 rs, u32 rt, s16 imm) const;
    void sb(u32 rs, u32 rt, s16 imm) const;
    void sc(u32 rs, u32 rt, s16 imm) const;
    void scd(u32 rs, u32 rt, s16 imm) const;
    void sd(u32 rs, u32 rt, s16 imm) const;
    void sdl(u32 rs, u32 rt, s16 imm) const;
    void sdr(u32 rs, u32 rt, s16 imm) const;
    void sh(u32 rs, u32 rt, s16 imm) const;
    void sync() const;
    void syscall() const;
    void sw(u32 rs, u32 rt, s16 imm) const;
    void swl(u32 rs, u32 rt, s16 imm) const;
    void swr(u32 rs, u32 rt, s16 imm) const;
} inline constexpr cpu_recompiler{
    jit,
    gpr,
    lo_reg,
    hi_reg,
    pc,
    in_branch_delay_slot,
    Jump,
    Link,
    SignalException<Exception::IntegerOverflow>,
    SignalException<Exception::Trap>,
};

} // namespace n64::vr4300

#include "cop0.hpp"
#include "exceptions.hpp"
#include "r3000a.hpp"

#include <cstring>

namespace ps1::r3000a {

void mfc0(u32 rd, u32 rt)
{
    if (rd < 16) {
        static constexpr u32 reserved_regs_mask = 0x0000'0417; // 0-2, 4, 10
        if (reserved_regs_mask & 1 << rd) {
            reserved_instruction_exception();
        } else {
            static_assert(sizeof(cop0) == 16 * 4);
            u32 val;
            std::memcpy(&val, reinterpret_cast<u8*>(&cop0) + rd * 4, 4);
            gpr.set(rt, val);
        }
    } else {
        gpr.set(rt, 0); // "garbage"
    }
}

void mtc0(u32 rd, u32 rt)
{
    if (rd > 15) return;

    s32 val = gpr[rt];

    switch (rd) {
    case 3: cop0.bpc = val; break;
    case 5: cop0.bda = val; break;
    case 9: cop0.bdam = val; break;
    case 11: cop0.bpcm = val; break;
    case 12: cop0.status.raw = val & 0xF4C7'9C1F | cop0.status.raw & ~0xF4C7'9C1F; break; // TODO: determine mask
    case 13: cop0.cause.raw = val & 0x300 | cop0.cause.raw & ~0x300; break;
    case 7: std::memcpy(&cop0.dcic, &val, 4); break; // todo: mask
    case 6:
    case 8:
    case 14:
    case 15: break; // read-only
    default: reserved_instruction_exception();
    }
}

void rfe()
{
    cop0.status.raw = cop0.status.raw >> 2 & 15 | cop0.status.raw & ~15;
}

} // namespace ps1::r3000a

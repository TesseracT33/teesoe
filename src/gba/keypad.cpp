#include "keypad.hpp"
#include "bus.hpp"
#include "irq.hpp"
#include "util.hpp"

namespace gba::keypad {

static void UpdateIrq();

static u16 keyinput, keycnt;

void Initialize()
{
    keyinput = 0x3FF;
    keycnt = 0;
}

void NotifyButtonPressed(uint index)
{
    keyinput &= ~(1 << index);
    UpdateIrq();
}

void NotifyButtonReleased(uint index)
{
    keyinput |= 1 << index;
}

template<std::integral Int> Int ReadReg(u32 addr)
{
    if constexpr (sizeof(Int) == 1) {
        switch (addr) {
        case bus::ADDR_KEYINPUT: return get_bit(keyinput, 0);
        case bus::ADDR_KEYINPUT + 1: return get_bit(keyinput, 1);
        case bus::ADDR_KEYCNT: return get_bit(keycnt, 0);
        case bus::ADDR_KEYCNT + 1: return get_bit(keycnt, 1);
        default: std::unreachable();
        }
    }
    if constexpr (sizeof(Int) == 2) {
        switch (addr) {
        case bus::ADDR_KEYINPUT: return keyinput;
        case bus::ADDR_KEYCNT: return keycnt;
        default: std::unreachable();
        }
    }
    if constexpr (sizeof(Int) == 4) {
        return keyinput | keycnt << 16;
    }
}

void StreamState(Serializer& stream)
{
}

void UpdateIrq()
{
    static constexpr u16 irq_enable_mask = 1 << 14;
    static constexpr u16 irq_cond_mask = 1 << 15;
    if (keycnt & irq_enable_mask) {
        if (keycnt & irq_cond_mask) { /* logical AND mode */
            if ((~keyinput & keycnt & 0x3FF) == keycnt) {
                irq::Raise(irq::Source::Keypad);
            }
        } else { /* logical OR mode */
            if (~keyinput & keycnt & 0x3FF) {
                irq::Raise(irq::Source::Keypad);
            }
        }
    }
}

template<std::integral Int> void WriteReg(u32 addr, Int data)
{
    if constexpr (sizeof(Int) == 1) {
        switch (addr) {
        case bus::ADDR_KEYCNT: set_byte(keycnt, 0, data); break;
        case bus::ADDR_KEYCNT + 1: set_byte(keycnt, 1, data); break;
        default: break;
        }
    }
    if constexpr (sizeof(Int) == 2) {
        if (addr == bus::ADDR_KEYCNT) {
            keycnt = data;
        }
    }
    if constexpr (sizeof(Int) == 4) {
        keycnt = data >> 16 & 0xFFFF;
    }
}

template u8 ReadReg<u8>(u32);
template s8 ReadReg<s8>(u32);
template u16 ReadReg<u16>(u32);
template s16 ReadReg<s16>(u32);
template u32 ReadReg<u32>(u32);
template s32 ReadReg<s32>(u32);
template void WriteReg<u8>(u32, u8);
template void WriteReg<s8>(u32, s8);
template void WriteReg<u16>(u32, u16);
template void WriteReg<s16>(u32, s16);
template void WriteReg<u32>(u32, u32);
template void WriteReg<s32>(u32, s32);

} // namespace gba::keypad

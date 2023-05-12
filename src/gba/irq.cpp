#include "irq.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "scheduler.hpp"
#include "util.hpp"

#include <utility>

namespace gba::irq {

static void CheckIrq();

constexpr int irq_event_cycle_delay = 3;

static bool ime, irq;
static u16 IE, IF;

void CheckIrq()
{
    irq = IE & IF & 0x3FF;
    if (ime) {
        scheduler::AddEvent(scheduler::EventType::IrqChange, irq_event_cycle_delay, [] { arm7tdmi::SetIRQ(irq); });
    }
}

void Initialize()
{
    ime = irq = false;
    IE = IF = 0;
    arm7tdmi::SetIRQ(false);
}

void Raise(Source source)
{
    IF |= std::to_underlying(source);
    CheckIrq();
}

u8 ReadIE(u8 byte_index)
{
    return get_byte(IE, byte_index);
}

u16 ReadIE()
{
    return IE;
}

u8 ReadIF(u8 byte_index)
{
    return get_byte(IF, byte_index);
}

u16 ReadIF()
{
    return IF;
}

u16 ReadIME()
{
    return ime;
}

void StreamState(Serializer& stream)
{
}

void WriteIE(u8 data, u8 byte_index)
{
    set_byte(IE, byte_index, data);
    CheckIrq();
}

void WriteIE(u16 data)
{
    IE = data;
    CheckIrq();
}

void WriteIF(u8 data, u8 byte_index)
{
    set_byte(IF, byte_index, IF & ~data & 0xFF);
    CheckIrq();
}

void WriteIF(u16 data)
{
    /* Interrupts must be manually acknowledged by writing a "1" to one of the IRQ bits, the IRQ bit will then be
     * cleared. */
    IF &= ~data; /* todo: handle bits 14-15 better? */
    CheckIrq();
}

void WriteIME(u16 data)
{
    bool prev_ime = ime;
    ime = data & 1;
    if (ime ^ prev_ime) {
        scheduler::AddEvent(scheduler::EventType::IrqChange, irq_event_cycle_delay, [] {
            arm7tdmi::SetIRQ(ime ? irq : false);
        });
    }
}
} // namespace gba::irq

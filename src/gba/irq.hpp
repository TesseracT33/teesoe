#pragma once

#include "serializer.hpp"
#include "types.hpp"

namespace gba::irq {

enum class Source : u16 {
    VBlank = 1 << 0,
    HBlank = 1 << 1,
    VCounter = 1 << 2,
    Timer0 = 1 << 3,
    Timer1 = 1 << 4,
    Timer2 = 1 << 5,
    Timer3 = 1 << 6,
    Serial = 1 << 7,
    Dma0 = 1 << 8,
    Dma1 = 1 << 9,
    Dma2 = 1 << 10,
    Dma3 = 1 << 11,
    Keypad = 1 << 12,
    GamePak = 1 << 13
};

void Initialize();
void Raise(Source source);
u8 ReadIE(u8 byte_index);
u16 ReadIE();
u8 ReadIF(u8 byte_index);
u16 ReadIF();
u16 ReadIME();
void StreamState(Serializer& stream);
void WriteIE(u8 data, u8 byte_index);
void WriteIE(u16 data);
void WriteIF(u8 data, u8 byte_index);
void WriteIF(u16 data);
void WriteIME(u16 data);

} // namespace gba::irq

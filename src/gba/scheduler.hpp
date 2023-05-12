#pragma once

#include "types.hpp"

namespace gba::scheduler {

using DriverRunFunc = u64 (*)(u64);
using DriverSuspendFunc = void (*)();
using EventCallback = void (*)();

enum class DriverType { /* ordered by priority (low to high) */
    Cpu,
    Dma3,
    Dma2,
    Dma1,
    Dma0
};

enum class EventType {
    HBlank,
    HBlankSetFlag,
    IrqChange,
    NewScanline,
    TimerOverflow0,
    TimerOverflow1,
    TimerOverflow2,
    TimerOverflow3
};

void AddEvent(EventType type, u64 time_until_fire, EventCallback callback);
void ChangeEventTime(EventType type, u64 new_time_to_fire);
void DisengageDriver(DriverType type);
void EngageDriver(DriverType type, DriverRunFunc run_func, DriverSuspendFunc suspend_func);
u64 GetGlobalTime();
void Initialize();
void RemoveEvent(EventType type);
void Run();

} // namespace gba::scheduler

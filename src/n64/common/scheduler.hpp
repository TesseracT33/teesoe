#pragma once

#include "types.hpp"

namespace n64::scheduler {

using EventCallback = void (*)();

enum class EventType {
    AudioSample,
    CountCompareMatch,
    PiDmaFinish,
    SiDmaFinish,
    SpDmaFinish,
    VINewHalfline
};

void AddEvent(EventType event, s64 cpu_cycles_until_fire, EventCallback callback);
void ChangeEventTime(EventType event, s64 cpu_cycles_until_fire);
void Initialize();
void RemoveEvent(EventType event);
void Run();
void Stop();

} // namespace n64::scheduler

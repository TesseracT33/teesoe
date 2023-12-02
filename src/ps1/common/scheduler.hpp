#pragma once

#include "ps1.hpp"
#include "types.hpp"

namespace ps1::scheduler {

using EventCallback = void (*)();

enum class EventType {
    CountCompareMatch,
};

void AddEvent(EventType event, s64 cpu_cycles_until_fire, EventCallback callback);
void ChangeEventTime(EventType event, s64 cpu_cycles_until_fire);
void Initialize();
void RemoveEvent(EventType event);
template<CpuImpl cpu_impl> void Run();
void Stop();

} // namespace ps1::scheduler

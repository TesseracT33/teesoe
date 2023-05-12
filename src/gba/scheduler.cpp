#include "scheduler.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "ppu/ppu.hpp"

#include <utility>
#include <vector>

namespace gba::scheduler {

static uint GetDriverPriority(DriverType type);

struct Driver {
    DriverType type;
    DriverRunFunc run_function;
    DriverSuspendFunc suspend_function;
};

struct Event {
    EventCallback callback;
    u64 time;
    EventType type;
};

static u64 global_time;

static std::vector<Driver> drivers; /* orderer by priority */
static std::vector<Event> events; /* ordered by timestamp */

void AddEvent(EventType type, u64 time_until_fire, EventCallback callback)
{
    global_time += arm7tdmi::GetElapsedCycles();
    u64 event_absolute_time = global_time + time_until_fire;
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (event_absolute_time < it->time) {
            events.emplace(it, callback, event_absolute_time, type);
            if (it == events.begin()) {
                drivers.front().suspend_function();
            }
            return;
        }
    }
    events.emplace_back(callback, event_absolute_time, type);
}

void ChangeEventTime(EventType type, u64 new_time_to_fire)
{
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (it->type == type) {
            EventCallback callback = it->callback;
            events.erase(it);
            AddEvent(type, new_time_to_fire, callback);
            break;
        }
    }
}

void DisengageDriver(DriverType type)
{
    for (auto it = drivers.begin(); it != drivers.end(); ++it) {
        if (it->type == type) {
            drivers.erase(it);
            break;
        }
    }
}

void EngageDriver(DriverType type, DriverRunFunc run_func, DriverSuspendFunc suspend_func)
{
    for (auto it = drivers.begin(); it != drivers.end(); ++it) {
        if (GetDriverPriority(it->type) < GetDriverPriority(type)) {
            drivers.emplace(it, type, run_func, suspend_func);
            if (it == drivers.begin()) {
                (++it)->suspend_function();
            }
            return;
        }
    }
    drivers.emplace_back(type, run_func, suspend_func);
}

uint GetDriverPriority(DriverType type)
{
    return std::to_underlying(type);
}

u64 GetGlobalTime()
{
    return global_time + arm7tdmi::GetElapsedCycles();
}

void Initialize()
{
    global_time = 0;
    drivers.clear();
    events.clear();
    ppu::AddInitialEvents();
    EngageDriver(DriverType::Cpu, arm7tdmi::Run, arm7tdmi::SuspendRun);
}

void RemoveEvent(EventType type)
{
    for (auto it = events.begin(); it != events.end();) {
        if (it->type == type) {
            if (it == events.begin()) {
                drivers.front().suspend_function();
            }
            events.erase(it);
            return;
        } else {
            ++it;
        }
    }
}

void Run()
{
    while (true) {
        while (global_time < events.front().time) {
            global_time += drivers.front().run_function(events.front().time - global_time);
        }
        Event top_event = events.front();
        events.erase(events.begin());
        global_time = top_event.time; /* just in case we ran for longer than we should have */
        top_event.callback();
    }
}
} // namespace gba::scheduler

#include "scheduler.hpp"
#include "interface/ai.hpp"
#include "interface/vi.hpp"
#include "ps1.hpp"
#include "vr4300/interpreter.hpp"
#include "vr4300/recompiler.hpp"

#include <vector>

namespace ps1::scheduler {

constexpr s32 rsp_cycles_per_update = 2 * cpu_cycles_per_update / 3;
static_assert(2 * cpu_cycles_per_update == 3 * rsp_cycles_per_update, "CPU cycles per update must be divisible by 3.");

struct Event {
    EventType event_type;
    s64 cpu_cycles_until_fire; /* signed so that we can subtract a duration and check if the result is negative */
    EventCallback callback;
};

static bool quit;
static std::vector<Event> events; /* sorted after when they will occur */
static std::vector<EventCallback> fired_events_callbacks;

static void CheckEvents(s64 cpu_cycle_step);

void AddEvent(EventType event_type, s64 cpu_cycles_until_fire, EventCallback callback)
{
    /* Compensate for the fact that we may be in the middle of a CPU update, and times for other events
        have not updated yet. TODO: We are assuming that only the main CPU can cause an event to be added.
        Is it ok? */
    s64 enqueue_time = cpu_cycles_until_fire + s64(vr4300::GetElapsedCycles());
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (enqueue_time < it->cpu_cycles_until_fire) {
            events.emplace(it, Event{ event_type, enqueue_time, callback });
            return;
        }
    }
    events.emplace_back(Event{ event_type, enqueue_time, callback });
}

void ChangeEventTime(EventType event_type, s64 cpu_cycles_until_fire)
{
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (it->event_type == event_type) {
            EventCallback callback = it->callback;
            events.erase(it);
            AddEvent(event_type, cpu_cycles_until_fire, callback);
            return;
        }
    }
}

void CheckEvents(s64 cpu_cycle_step)
{
    fired_events_callbacks.clear();
    for (auto it = events.begin(); it != events.end();) {
        it->cpu_cycles_until_fire -= cpu_cycle_step;
        if (it->cpu_cycles_until_fire <= 0) {
            /* erase element before invoking callback, in case it mutates the event list */
            fired_events_callbacks.push_back(it->callback);
            it = events.erase(it);
        } else {
            ++it;
        }
    }
    for (EventCallback callback : fired_events_callbacks) {
        callback();
    }
}

void Initialize()
{
    quit = false;
    events.clear();
    events.reserve(16);
    fired_events_callbacks.clear();
    fired_events_callbacks.reserve(16);
    vr4300::AddInitialEvents();
    vi::AddInitialEvents();
}

void RemoveEvent(EventType event_type)
{
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (it->event_type == event_type) {
            events.erase(it);
            return;
        }
    }
}

template<CpuImpl vr4300_impl, CpuImpl rsp_impl> void Run()
{
    Initialize();
    rsp::SetActiveCpuImpl(rsp_impl);
    vr4300::SetActiveCpuImpl(vr4300_impl);

    s32 cpu_cycle_overrun = 0, rsp_cycle_overrun = 0;
    while (!quit) {
        if (cpu_cycle_overrun < cpu_cycles_per_update) {
            u32 cpu_step = u32(cpu_cycles_per_update - cpu_cycle_overrun);
            cpu_cycle_overrun =
              vr4300_impl == CpuImpl::Interpreter ? vr4300::RunInterpreter(cpu_step) : vr4300::RunRecompiler(cpu_step);
            u32 actual_cpu_step = cpu_step + cpu_cycle_overrun;
            ai::Step(actual_cpu_step);
            CheckEvents(actual_cpu_step);
        } else {
            cpu_cycle_overrun -= cpu_cycles_per_update;
        }
        u32 rsp_step = rsp_cycles_per_update + 2 * cpu_cycle_overrun / 3;
        if (rsp_cycle_overrun < rsp_step) {
            rsp_step -= rsp_cycle_overrun;
            rsp_cycle_overrun =
              rsp_impl == CpuImpl::Interpreter ? rsp::RunInterpreter(rsp_step) : rsp::RunRecompiler(rsp_step);
        } else {
            rsp_cycle_overrun -= rsp_step;
        }
    }
}

void Stop()
{
    quit = true;
}

template void Run<CpuImpl::Interpreter, CpuImpl::Interpreter>();
template void Run<CpuImpl::Interpreter, CpuImpl::Recompiler>();
template void Run<CpuImpl::Recompiler, CpuImpl::Interpreter>();
template void Run<CpuImpl::Recompiler, CpuImpl::Recompiler>();

} // namespace ps1::scheduler

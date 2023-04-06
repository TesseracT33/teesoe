#include "scheduler.hpp"
#include "interface/ai.hpp"
#include "interface/vi.hpp"
#include "n64.hpp"
#include "rsp/rsp.hpp"
#include "vr4300/vr4300.hpp"

#include <vector>

namespace n64::scheduler {

constexpr s64 rsp_cycles_per_update = 2 * cpu_cycles_per_update / 3;
static_assert(2 * cpu_cycles_per_update == 3 * rsp_cycles_per_update, "CPU cycles per update must be divisible by 3.");

struct Event {
    EventType event_type;
    s64 cpu_cycles_until_fire; /* signed so that we can subtract a duration and check if the result is negative */
    EventCallback callback;
};

static bool quit;
static std::vector<Event> events; /* sorted after when they will occur */

static void CheckEvents(s64 cpu_cycle_step);

void AddEvent(EventType event_type, s64 cpu_cycles_until_fire, EventCallback callback)
{
    /* Compensate for the fact that we may be in the middle of a CPU update, and times for other events
        have not updated yet. TODO: We are assuming that only the main CPU can cause an event to be added.
        Is it ok? */
    s64 elapsed_cycles_since_step_start = s64(vr4300::GetElapsedCycles());
    s64 enqueue_time = cpu_cycles_until_fire + elapsed_cycles_since_step_start;
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
    s64 elapsed_cycles_since_step_start = s64(vr4300::GetElapsedCycles());
    s64 enqueue_time = cpu_cycles_until_fire + elapsed_cycles_since_step_start;
    for (auto it = events.begin(); it != events.end(); ++it) {
        if (it->event_type == event_type) {
            EventCallback callback = it->callback;
            events.erase(it);
            AddEvent(event_type, enqueue_time, callback);
            return;
        }
    }
}

void CheckEvents(s64 cpu_cycle_step)
{
    for (auto it = events.begin(); it != events.end();) {
        it->cpu_cycles_until_fire -= cpu_cycle_step;
        if (it->cpu_cycles_until_fire <= 0) {
            /* erase element before invoking callback, in case it mutates the event list */
            EventCallback callback = it->callback;
            events.erase(it);
            callback();
            it = events.begin();
        } else {
            ++it;
        }
    }
}

void Initialize()
{
    quit = false;
    events.clear();
    events.reserve(16);
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

    s64 cpu_cycle_overrun = 0, rsp_cycle_overrun = 0;
    while (!quit) {
        s64 cpu_step_dur = cpu_cycles_per_update - cpu_cycle_overrun;
        s64 rsp_step_dur = cpu_cycles_per_update - rsp_cycle_overrun;
        if constexpr (vr4300_impl == CpuImpl::Interpreter) {
            cpu_cycle_overrun = vr4300::RunInterpreter(cpu_step_dur);
        } else {
            cpu_cycle_overrun = vr4300::RunRecompiler(cpu_step_dur);
        }
        if constexpr (rsp_impl == CpuImpl::Interpreter) {
            rsp_cycle_overrun = rsp::Run(rsp_step_dur);
        } else {
            rsp_cycle_overrun = rsp::Run(rsp_step_dur); // TODO
        }
        ai::Step(cpu_step_dur);
        CheckEvents(cpu_step_dur);
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

} // namespace n64::scheduler

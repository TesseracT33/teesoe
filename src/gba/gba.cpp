#include "gba.hpp"
#include "apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "bios.hpp"
#include "bus.hpp"
#include "cart.hpp"
#include "debug.hpp"
#include "dma.hpp"
#include "irq.hpp"
#include "keypad.hpp"
#include "ppu/ppu.hpp"
#include "scheduler.hpp"
#include "serial.hpp"
#include "timers.hpp"

using namespace gba;

void GBA::ApplyConfig(CoreConfiguration config)
{
}

Status GBA::EnableAudio(bool enable)
{
    return UnimplementedStatus();
}

std::span<const std::string_view> GBA::GetInputNames() const
{
    static constexpr std::array<std::string_view, 10>
      names = { "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L" };
    return names;
}

Status GBA::Init()
{
    apu::Initialize();
    arm7tdmi::Initialize();
    bios::Initialize();
    bus::Initialize();
    cart::Initialize();
    dma::Initialize();
    irq::Initialize();
    keypad::Initialize();
    ppu::Initialize();
    scheduler::Initialize();
    serial::Initialize();
    timers::Initialize();
    if constexpr (LoggingIsEnabled()) {
        SetLogPath("F:\\gba.log");
    }
    return OkStatus();
}

Status GBA::InitGraphics()
{
    return UnimplementedStatus();
}

Status GBA::LoadBios(std::filesystem::path const& path)
{
    return bios::Load(path);
}

Status GBA::LoadRom(std::filesystem::path const& path)
{
    return cart::LoadRom(path);
}

void GBA::NotifyAxisState(size_t player, size_t action_index, s32 axis_value)
{
}

void GBA::NotifyButtonState(size_t player, size_t action_index, bool pressed)
{
}

void GBA::Pause()
{
}

void GBA::Reset()
{
}

void GBA::Resume()
{
}

void GBA::Run()
{
    scheduler::Run();
}

void GBA::Stop()
{
}

void GBA::StreamState(Serializer& serializer)
{
}

void GBA::TearDown()
{
}

void GBA::UpdateScreen()
{
}

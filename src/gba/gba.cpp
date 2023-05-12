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

void GBA::apply_configuration(CoreConfiguration config)
{
}

Status GBA::enable_audio(bool enable)
{
    return status_unimplemented();
}

std::span<const std::string_view> GBA::get_input_names() const
{
    static constexpr std::array<std::string_view, 10>
      names = { "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L" };
    return names;
}

Status GBA::init()
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
    return status_ok();
}

Status GBA::init_graphics_system()
{
    return status_unimplemented();
}

Status GBA::load_bios(std::filesystem::path const& path)
{
    return bios::Load(path);
}

Status GBA::load_rom(std::filesystem::path const& path)
{
    return cart::LoadRom(path);
}

void GBA::notify_axis_state(size_t player, size_t action_index, s32 axis_value)
{
}

void GBA::notify_button_state(size_t player, size_t action_index, bool pressed)
{
}

void GBA::pause()
{
}

void GBA::reset()
{
}

void GBA::resume()
{
}

void GBA::run()
{
    scheduler::Run();
}

void GBA::stop()
{
}

void GBA::stream_state(Serializer& serializer)
{
}

void GBA::tear_down()
{
}

void GBA::update_screen()
{
}

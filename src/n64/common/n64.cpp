#include "n64.hpp"
#include "control.hpp"
#include "frontend/message.hpp"
#include "interface/ai.hpp"
#include "interface/mi.hpp"
#include "interface/pi.hpp"
#include "interface/ri.hpp"
#include "interface/si.hpp"
#include "interface/vi.hpp"
#include "memory/cart.hpp"
#include "memory/pif.hpp"
#include "memory/rdram.hpp"
#include "n64_build_options.hpp"
#include "rdp/rdp.hpp"
#include "rsp/rsp.hpp"
#include "scheduler.hpp"
#include "vr4300/vr4300.hpp"

Status N64::enable_audio(bool enable)
{
    return status_unimplemented();
}

size_t N64::get_number_of_inputs() const
{
    return n64::num_controls;
}

Status N64::init()
{
    n64::ai::Initialize();
    n64::mi::Initialize();
    n64::pi::Initialize();
    n64::ri::Initialize();
    n64::si::Initialize();
    n64::vi::Initialize();
    n64::rdram::Initialize();

    n64::vr4300::PowerOn();
    n64::rsp::PowerOn();
    n64::rdp::Initialize();

    n64::scheduler::Initialize(); /* init last */

    return status_ok();
}

Status N64::init_graphics_system()
{
    return n64::rdp::MakeParallelRdp();
}

Status N64::load_bios(std::filesystem::path const& path)
{
    Status status = n64::pif::LoadIPL12(path);
    bios_loaded = status.ok();
    return status;
}

Status N64::load_rom(std::filesystem::path const& path)
{
    Status status = n64::cart::LoadRom(path);
    game_loaded = status.ok();
    return status;
}

void N64::notify_axis_state(size_t player, size_t action_index, s32 axis_value)
{
    n64::pif::OnJoystickMovement(static_cast<n64::Control>(action_index), s16(axis_value));
}

void N64::notify_button_state(size_t player, size_t action_index, bool pressed)
{
    n64::pif::OnButtonAction(static_cast<n64::Control>(action_index), pressed);
}

void N64::pause()
{
}

void N64::reset()
{
}

void N64::resume()
{
}

void N64::run()
{
    if (!running) {
        reset();
        bool hle_pif = !bios_loaded || skip_boot_rom;
        n64::vr4300::InitRun(hle_pif);
        running = true;
    }
    n64::scheduler::Run();
}

void N64::stop()
{
    n64::scheduler::Stop();
    running = false;
}

void N64::stream_state(Serializer& serializer)
{
}

void N64::tear_down()
{
}

void N64::update_screen()
{
    n64::rdp::implementation->UpdateScreen();
}

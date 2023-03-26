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

using namespace n64;

Status N64::enable_audio(bool enable)
{
    return status_unimplemented();
}

std::span<const std::string_view> N64::get_input_names() const
{
    return control_names;
}

Status N64::init()
{
    ai::Initialize();
    mi::Initialize();
    pi::Initialize();
    ri::Initialize();
    si::Initialize();
    vi::Initialize();
    rdram::Initialize();

    vr4300::PowerOn();
    rsp::PowerOn();
    rdp::Initialize();

    scheduler::Initialize(); // init last

    return status_ok();
}

Status N64::init_graphics_system()
{
    return rdp::MakeParallelRdp();
}

Status N64::load_bios(std::filesystem::path const& path)
{
    Status status = pif::LoadIPL12(path);
    bios_loaded = status.ok();
    return status;
}

Status N64::load_rom(std::filesystem::path const& path)
{
    Status status = cart::LoadRom(path);
    game_loaded = status.ok();
    return status;
}

void N64::notify_axis_state(size_t player, size_t action_index, s32 axis_value)
{
    pif::OnJoystickMovement(static_cast<Control>(action_index), s16(axis_value));
}

void N64::notify_button_state(size_t player, size_t action_index, bool pressed)
{
    pif::OnButtonAction(static_cast<Control>(action_index), pressed);
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
        vr4300::InitRun(hle_pif);
        running = true;
    }
    scheduler::Run<CpuImpl::Interpreter, CpuImpl::Interpreter>(); // TODO: make toggleable
}

void N64::stop()
{
    scheduler::Stop();
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
    rdp::implementation->UpdateScreen();
}

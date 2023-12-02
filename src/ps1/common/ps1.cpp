#include "ps1.hpp"
#include "control.hpp"
#include "frontend/message.hpp"
#include "ps1_build_options.hpp"
#include "r3000a/r3000a.hpp"

using namespace ps1;

void PS1::ApplyConfig(CoreConfiguration config)
{
    CpuImpl prev_cpu_impl =
      std::exchange(cpu_impl, config.n64.use_cpu_recompiler ? CpuImpl::Recompiler : CpuImpl::Interpreter);
    CpuImpl prev_rsp_impl =
      std::exchange(rsp_impl, config.n64.use_rsp_recompiler ? CpuImpl::Recompiler : CpuImpl::Interpreter);
    if (running && (cpu_impl != prev_cpu_impl || rsp_impl != prev_rsp_impl)) {
        stop();
        run();
    }
}

Status PS1::EnableAudio(bool enable)
{
    return UnimplementedStatus();
}

std::span<const std::string_view> PS1::GetInputNames() const
{
    return control_names;
}

Status PS1::Init()
{
    r3000a::PowerOn();

    scheduler::Initialize(); // Init last

    return OkStatus();
}

Status PS1::InitGraphics()
{
    return rdp::MakeParallelRdp();
}

Status PS1::LoadBios(std::filesystem::path const& path)
{
    Status status = pif::LoadIPL12(path);
    bios_loaded = status.Ok();
    return status;
}

Status PS1::LoadRom(std::filesystem::path const& path)
{
    Status status = cart::LoadRom(path);
    game_loaded = status.Ok();
    return status;
}

void PS1::NotifyAxisState(size_t player, size_t action_index, s32 axis_value)
{
    pif::OnJoystickMovement(static_cast<Control>(action_index), s16(axis_value));
}

void PS1::NotifyButtonState(size_t player, size_t action_index, bool pressed)
{
    pif::OnButtonAction(static_cast<Control>(action_index), pressed);
}

void PS1::Pause()
{
}

void PS1::Reset()
{
}

void PS1::Resume()
{
}

void PS1::Run()
{
    if (!running) {
        reset();
        bool hle_pif = !bios_loaded || skip_boot_rom;
        vr4300::InitRun(hle_pif);
        running = true;
    }
    cpu_impl == ps1::CpuImpl::Interpreter ? scheduler::Run<CpuImpl::Interpreter>()
                                          : scheduler::Run<CpuImpl::Recompiler>();
}

void PS1::Stop()
{
    scheduler::Stop();
    running = false;
}

void PS1::StreamState(Serializer& serializer)
{
}

void PS1::TearDown()
{
}

void PS1::UpdateScreen()
{
    rdp::implementation->UpdateScreen();
}

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

void N64::ApplyConfig(CoreConfiguration config)
{
    CpuImpl prev_cpu_impl =
      std::exchange(cpu_impl, config.n64.use_cpu_recompiler ? CpuImpl::Recompiler : CpuImpl::Interpreter);
    CpuImpl prev_rsp_impl =
      std::exchange(rsp_impl, config.n64.use_rsp_recompiler ? CpuImpl::Recompiler : CpuImpl::Interpreter);
    // TODO: handle this
    if (running && (cpu_impl != prev_cpu_impl || rsp_impl != prev_rsp_impl)) {
        /*Stop();
        Run();*/
        throw std::runtime_error("Unhandled");
    }
}

Status N64::EnableAudio(bool enable)
{
    (void)enable;
    return UnimplementedStatus();
}

std::span<std::string_view const> N64::GetInputNames() const
{
    return control_names;
}

Status N64::Init()
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

    scheduler::Initialize(); // Init last

    return OkStatus();
}

Status N64::InitGraphics(std::shared_ptr<RenderContext> render_context)
{
    (void)render_context;
    return OkStatus();
}

Status N64::LoadBios(std::filesystem::path const& path)
{
    Status status = pif::LoadIPL12(path);
    bios_loaded = status.Ok();
    return status;
}

Status N64::LoadRom(std::filesystem::path const& path)
{
    Status status = cart::LoadRom(path);
    game_loaded = status.Ok();
    return status;
}

void N64::NotifyAxisState(size_t player, size_t action_index, s32 axis_value)
{
    (void)player; // TODO
    pif::OnJoystickMovement(static_cast<Control>(action_index), s16(axis_value));
}

void N64::NotifyButtonState(size_t player, size_t action_index, bool pressed)
{
    (void)player; // TODO
    pif::OnButtonAction(static_cast<Control>(action_index), pressed);
}

void N64::Pause()
{
}

void N64::Reset()
{
}

void N64::Resume()
{
}

void N64::Run(std::stop_token stop_token)
{
    if (!std::exchange(running, true)) {
        Reset();
        bool hle_pif = !bios_loaded || skip_boot_rom;
        vr4300::InitRun(hle_pif);
    }
    if (cpu_impl == n64::CpuImpl::Interpreter) {
        rsp_impl == n64::CpuImpl::Interpreter ? scheduler::Run<CpuImpl::Interpreter, CpuImpl::Interpreter>(stop_token)
                                              : scheduler::Run<CpuImpl::Interpreter, CpuImpl::Recompiler>(stop_token);
    } else {
        rsp_impl == n64::CpuImpl::Interpreter ? scheduler::Run<CpuImpl::Recompiler, CpuImpl::Interpreter>(stop_token)
                                              : scheduler::Run<CpuImpl::Recompiler, CpuImpl::Recompiler>(stop_token);
    }
}

void N64::StreamState(Serializer& serializer)
{
    (void)serializer;
}

void N64::UpdateScreen()
{
    rdp::implementation->UpdateScreen();
}

#pragma once

#include "core.hpp"
#include "core_configuration.hpp"

#include <stop_token>

namespace n64 {

enum class Cpu {
    RSP,
    VR4300
};

enum class CpuImpl {
    Interpreter,
    Recompiler
};

inline constexpr uint cpu_cycles_per_second = 93'750'000;
inline constexpr uint rsp_cycles_per_second = 62'500'500;
inline constexpr uint cpu_cycles_per_frame = cpu_cycles_per_second / 60; /* 1,562,500 */
inline constexpr uint rsp_cycles_per_frame = rsp_cycles_per_second / 60; /* 1,041,675 */
inline constexpr s64 cpu_cycles_per_update = 90;

} // namespace n64

class N64 : public Core {
public:
    void ApplyConfig(CoreConfiguration config) override;
    Status EnableAudio(bool enable) override;
    std::span<std::string_view const> GetInputNames() const override;
    Status Init() override;
    Status InitGraphics(std::shared_ptr<RenderContext> render_context) override;
    Status LoadBios(std::filesystem::path const& path) override;
    Status LoadRom(std::filesystem::path const& path) override;
    void NotifyAxisState(size_t player, size_t action_index, s32 axis_value) override;
    void NotifyButtonState(size_t player, size_t action_index, bool pressed) override;
    void Pause() override;
    void Reset() override;
    void Resume() override;
    void Run(std::stop_token stop_token) override;
    void StreamState(Serializer& serializer) override;
    void UpdateScreen() override;

private:
    bool bios_loaded{};
    bool game_loaded{};
    bool running{};
    n64::CpuImpl cpu_impl{}, rsp_impl{};
};

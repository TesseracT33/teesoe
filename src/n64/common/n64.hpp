#pragma once

#include "core.hpp"

class N64 : public Core {
public:
    Status enable_audio(bool enable) override;
    std::span<const std::string_view> get_input_names() const override;
    Status init() override;
    Status init_graphics_system() override;
    Status load_bios(std::filesystem::path const& path) override;
    Status load_rom(std::filesystem::path const& path) override;
    void notify_axis_state(size_t player, size_t action_index, s32 axis_value) override;
    void notify_button_state(size_t player, size_t action_index, bool pressed) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void run() override;
    void stop() override;
    void stream_state(Serializer& serializer) override;
    void tear_down() override;
    void update_screen() override;

private:
    bool bios_loaded{};
    bool game_loaded{};
    bool running{};
};

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

#pragma once

#include "core.hpp"
#include "types.hpp"

struct GBA : public Core {
    void apply_configuration(CoreConfiguration config) override;
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
};

#pragma once

#include "core_configuration.hpp"
#include "serializer.hpp"
#include "status.hpp"
#include "types.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

class Serializer;

class Core {
public:
    virtual ~Core() = default;
    virtual void apply_configuration(CoreConfiguration config) = 0;
    virtual Status enable_audio(bool enable) = 0;
    virtual std::span<const std::string_view> get_input_names() const = 0;
    virtual Status init() = 0;
    virtual Status init_graphics_system() = 0;
    virtual Status load_bios(std::filesystem::path const& path) = 0;
    virtual Status load_rom(std::filesystem::path const& path) = 0;
    virtual void notify_axis_state(size_t player, size_t action_index, s32 axis_value) = 0;
    virtual void notify_button_state(size_t player, size_t action_index, bool pressed) = 0;
    virtual void pause() = 0;
    virtual void reset() = 0;
    virtual void resume() = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void stream_state(Serializer& serializer) = 0;
    virtual void tear_down() {}
    virtual void update_screen() = 0;
};

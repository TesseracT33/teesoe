#pragma once

#include "serializer.hpp"
#include "status.hpp"
#include "types.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

class Serializer;

class Core {
public:
    virtual ~Core() = default;
    virtual Status enable_audio(bool enable) = 0;
    virtual size_t get_number_of_inputs() const = 0;
    virtual Status init() = 0;
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
    virtual void tear_down(){};
};

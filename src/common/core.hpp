#pragma once

#include "core_configuration.hpp"
#include "serializer.hpp"
#include "status.hpp"
#include "numtypes.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

class Serializer;

class Core {
public:
    virtual ~Core() = default;
    virtual void ApplyConfig(CoreConfiguration config) = 0;
    virtual Status EnableAudio(bool enable) = 0;
    virtual std::span<std::string_view const> GetInputNames() const = 0;
    virtual Status Init() = 0;
    virtual Status InitGraphics() = 0;
    virtual Status LoadBios(std::filesystem::path const& path) = 0;
    virtual Status LoadRom(std::filesystem::path const& path) = 0;
    virtual void NotifyAxisState(size_t player, size_t action_index, s32 axis_value) = 0;
    virtual void NotifyButtonState(size_t player, size_t action_index, bool pressed) = 0;
    virtual void Pause() = 0;
    virtual void Reset() = 0;
    virtual void Resume() = 0;
    virtual void Run() = 0;
    virtual void Stop() = 0;
    virtual void StreamState(Serializer& serializer) = 0;
    virtual void TearDown() {}
    virtual void UpdateScreen() = 0;
};

#pragma once

#include "core.hpp"
#include "types.hpp"

#include <stop_token>

struct GBA : public Core {
    void ApplyConfig(CoreConfiguration config) override;
    Status EnableAudio(bool enable) override;
    std::span<std::string_view const> GetInputNames() const override;
    Status Init() override;
    Status InitGraphics() override;
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
};

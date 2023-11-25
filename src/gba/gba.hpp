#pragma once

#include "core.hpp"
#include "types.hpp"

struct GBA : public Core {
    void ApplyConfig(CoreConfiguration config) override;
    Status EnableAudio(bool enable) override;
    std::span<const std::string_view> GetInputNames() const override;
    Status Init() override;
    Status InitGraphics() override;
    Status LoadBios(std::filesystem::path const& path) override;
    Status LoadRom(std::filesystem::path const& path) override;
    void NotifyAxisState(size_t player, size_t action_index, s32 axis_value) override;
    void NotifyButtonState(size_t player, size_t action_index, bool pressed) override;
    void Pause() override;
    void Reset() override;
    void Resume() override;
    void Run() override;
    void Stop() override;
    void StreamState(Serializer& serializer) override;
    void TearDown() override;
    void UpdateScreen() override;
};

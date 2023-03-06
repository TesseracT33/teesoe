#pragma once

#include "core.hpp"
#include "status.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

namespace frontend {

enum class System {
    None,
    CHIP8,
    GB,
    GBA,
    N64,
    NES,
    PS2
};

inline const std::map<std::filesystem::path, System> rom_ext_to_system{
    { ".ch8", System::CHIP8 },
    { ".CH8", System::CHIP8 },
    { ".gb", System::GB },
    { ".GB", System::GB },
    { ".gbc", System::GB },
    { ".GBC", System::GB },
    { ".gba", System::GBA },
    { ".GBA", System::GBA },
    { ".n64", System::N64 },
    { ".N64", System::N64 },
    { ".z64", System::N64 },
    { ".Z64", System::N64 },
    { ".nes", System::NES },
    { ".NES", System::NES },
    { ".bin", System::PS2 },
    { ".BIN", System::PS2 },
    { ".iso", System::PS2 },
    { ".ISO", System::PS2 },
};

bool core_loaded();
std::unique_ptr<Core> const& get_core();
System get_system();
Status load_core(System system);
Status load_core_and_game(std::filesystem::path const& rom_path);
std::string_view system_to_string(System system);

} // namespace frontend

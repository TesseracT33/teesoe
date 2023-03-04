#pragma once

#include "core.hpp"
#include "status.hpp"

#include <filesystem>
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

bool core_loaded();
std::unique_ptr<Core> const& get_core();
System get_system();
Status load_core(System system);
Status load_core_and_game(std::filesystem::path const& rom_path);
std::optional<System> rom_extension_to_system(std::filesystem::path const& ext);
std::string_view system_to_string(System system);

} // namespace frontend

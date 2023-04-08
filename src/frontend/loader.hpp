#pragma once

#include "core.hpp"
#include "status.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string_view>

namespace frontend {

namespace fs = std::filesystem;
namespace rng = std::ranges;

enum class System {
    None,
    CHIP8,
    GB,
    GBA,
    N64,
    NES,
    PS2
};

inline const std::map<System, std::vector<fs::path>> system_to_rom_exts = [] {
    std::map<System, std::vector<fs::path>> system_exts{
        { System::CHIP8, { ".ch8", ".CH8" } },
        { System::GB, { ".gb", ".GB", ".gbc", ".GBC" } },
        { System::GBA, { ".gba", ".GBA" } },
        { System::N64, { ".n64", ".N64", ".z64", ".Z64" } },
        { System::NES, { ".nes", ".NES" } },
        { System::PS2, { ".bin", ".BIN", ".iso", ".ISO" } },
    };
    for (auto& [system, exts] : system_exts) {
        exts.push_back(".zip");
        exts.push_back(".7z");
    }
    return system_exts;
}();

inline const std::map<fs::path, System> rom_ext_to_system = [] {
    std::map<fs::path, System> ext_to_system;
    std::vector<fs::path> colliding_exts;
    for (auto const& [system, exts] : system_to_rom_exts) {
        for (auto const& ext : exts) {
            if (rng::contains(colliding_exts, ext)) continue;
            if (auto it = ext_to_system.find(ext); it == ext_to_system.end()) {
                ext_to_system[ext] = system;
            } else {
                ext_to_system.erase(it);
                colliding_exts.push_back(ext);
            }
        }
    }
    return ext_to_system;
}();

bool core_loaded();
std::unique_ptr<Core> const& get_core();
System get_system();
Status load_core(System system);
Status load_core_and_game(fs::path const& rom_path);
std::string_view system_to_string(System system);

} // namespace frontend

#pragma once

#include "core.hpp"
#include "status.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
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

constexpr std::array systems = { System::CHIP8, System::GB, System::GBA, System::N64, System::NES, System::PS2 };

inline std::map<System, std::vector<fs::path>> const system_to_rom_exts = [] {
    std::map<System, std::vector<fs::path>> system_exts{
        { System::CHIP8, { ".ch8", ".CH8" } },
        { System::GB, { ".gb", ".GB", ".gbc", ".GBC" } },
        { System::GBA, { ".gba", ".GBA" } },
        { System::N64, { ".n64", ".N64", ".z64", ".Z64" } },
        { System::NES, { ".nes", ".NES" } },
        { System::PS2, { ".bin", ".BIN", ".iso", ".ISO" } },
    };
    for (auto& [system, exts] : system_exts) {
        exts.push_back(".7z");
        exts.push_back(".7Z");
        exts.push_back(".zip");
        exts.push_back(".ZIP");
    }
    return system_exts;
}();

inline std::map<fs::path, System> const rom_ext_to_system = [] {
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

bool CoreIsLoaded();
std::unique_ptr<Core> const& GetCore();
System GetSystem();
Status LoadCore(System system);
Status LoadCoreAndGame(fs::path rom_path);
std::string_view SystemToString(System system);

} // namespace frontend

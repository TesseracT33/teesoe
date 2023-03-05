#include "loader.hpp"
#include "frontend/message.hpp"
#include "gui.hpp"
#include "n64.hpp"

#include <algorithm>
#include <map>

namespace fs = std::filesystem;
namespace rng = std::ranges;

namespace frontend {

static std::unique_ptr<Core> core;
static System system;

bool core_loaded()
{
    return core != nullptr;
}

std::unique_ptr<Core> const& get_core()
{
    return core;
}

System get_system()
{
    return system;
}

Status load_core(System system_arg)
{
    if (core_loaded()) {
        core->tear_down();
    }
    switch (system_arg) {
    case System::CHIP8: core = nullptr; break;
    case System::GB: core = nullptr; break;
    case System::GBA: core = nullptr; break;
    case System::N64: core = std::make_unique<N64>(); break;
    case System::NES: core = nullptr; break;
    case System::PS2: core = nullptr; break;
    }
    if (core) {
        Status status = core->init();
        system = status.ok() ? system_arg : System::None;
        return status;
    } else {
        system = System::None;
        return status_failure("Core could not be created; factory returned null.");
    }
}

Status load_core_and_game(fs::path const& rom_path)
{
    std::optional<System> new_system = rom_extension_to_system(rom_path);
    if (new_system) {
        if (!core_loaded() || new_system.value() != system) {
            Status status = load_core(new_system.value());
            if (!status.ok()) {
                return status;
            }
        }
        return gui::LoadGame(rom_path);
    } else {
        if (core_loaded()) {
            return gui::LoadGame(rom_path);
        } else {
            return status_failure(
              "Failed to identify which system the selected rom is associated with. Please load a core first.");
        }
    }
}

std::string_view system_to_string(System system_arg)
{
    switch (system_arg) {
    case System::None: return "UNLOADED";
    case System::CHIP8: return "Chip-8";
    case System::GB: return "GB";
    case System::GBA: return "GBA";
    case System::N64: return "N64";
    case System::NES: return "NES";
    case System::PS2: return "PS2";
    default: return "UNKNOWN";
    }
}

std::optional<System> rom_extension_to_system(fs::path const& ext)
{
    static const std::map<fs::path, System> ext_to_system{ { ".ch8", System::CHIP8 },
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
        { ".ISO", System::PS2 } };

    auto lookup_it = ext_to_system.find(ext);
    if (lookup_it == ext_to_system.end()) {
        return {};
    } else {
        return lookup_it->second;
    }
}

} // namespace frontend

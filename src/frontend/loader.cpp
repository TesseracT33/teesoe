#include "loader.hpp"
#include "frontend/message.hpp"
#include "gui.hpp"
#include "n64.hpp"

#include <algorithm>

namespace fs = std::filesystem;

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
    auto it = rom_ext_to_system.find(rom_path.extension());
    if (it == rom_ext_to_system.end()) {
        if (core_loaded()) {
            return gui::LoadGame(rom_path);
        } else {
            return status_failure(
              "Failed to identify which system the selected rom is associated with. Please load a core first.");
        }
    } else {
        System new_system = it->second;
        if (!core_loaded() || new_system != system) {
            Status status = load_core(new_system);
            if (!status.ok()) {
                return status;
            }
        }
        return gui::LoadGame(rom_path);
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

} // namespace frontend

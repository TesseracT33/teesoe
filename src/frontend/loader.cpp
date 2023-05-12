#include "loader.hpp"
#include "bit7z/bitfileextractor.hpp"
#include "frontend/message.hpp"
#include "gba/gba.hpp"
#include "gui.hpp"
#include "log.hpp"
#include "n64.hpp"

#include <algorithm>
#include <cassert>

namespace frontend {

static std::optional<fs::path> extract_archive(fs::path const& path);

static std::unique_ptr<Core> core;
static System system;

static const std::array<fs::path, 4> rom_archive_exts = { ".7z", ".7Z", ".zip", ".ZIP" };

bool core_loaded()
{
    return core != nullptr;
}

static std::optional<fs::path> extract_archive(fs::path const& path)
{
    try {
        bit7z::BitInOutFormat const& format = [&path]() -> bit7z::BitInOutFormat const& {
            fs::path ext = path.extension();
            if (ext == ".7z" || ext == ".7Z") {
                return bit7z::BitFormat::SevenZip;
            } else if (ext == ".zip" || ext == ".ZIP") {
                return bit7z::BitFormat::Zip;
            } else {
                throw std::logic_error(std::format("File with unexpected extension given to {}", __FUNCTION__));
            }
        }();
        bit7z::Bit7zLibrary lib{ "7z.dll" };
        bit7z::BitFileExtractor extractor{ lib, format };
        // TODO: remove extracted files on exit
        fs::path out_path = path.parent_path() / "extracted";
        extractor.extract(path.generic_string(), out_path.generic_string());
        return out_path;
    } catch (bit7z::BitException const& e) {
        log_warn(std::format("Failed to extract archive; caught bit7z exception: {}", e.what()));
        return {};
    }
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
    case System::GBA: core = std::make_unique<GBA>(); break;
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

Status load_core_and_game(fs::path rom_path)
{
    fs::path rom_ext = rom_path.extension();
    if (rng::contains(rom_archive_exts, rom_ext)) {
        std::optional<fs::path> extracted_dir = extract_archive(rom_path);
        if (!extracted_dir) {
            return status_failure("Failed to extract archive");
        }
        fs::path selected_file_path;
        for (fs::directory_entry const& file : fs::directory_iterator(extracted_dir.value())) {
            if (file.is_regular_file()) { // TODO: use a more intelligent way of choosing which file to use
                selected_file_path = file.path();
                break;
            }
        }
        if (selected_file_path.empty()) {
            return status_failure("The selected archive did not contain any viable rom files");
        }
        rom_path = selected_file_path;
        rom_ext = rom_path.extension();
    }
    auto it = rom_ext_to_system.find(rom_ext);
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

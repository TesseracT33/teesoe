#include "config.hpp"
#include "log.hpp"
#include "yaml-cpp/yaml.h"

#include <cassert>
#include <format>
#include <fstream>
#include <string_view>

namespace fs = std::filesystem;

namespace frontend::config {

static void EmitN64(YAML::Emitter& emitter);
static void Flush();
static void Flush(YAML::Emitter& emitter);
template<typename T> static std::optional<T> Get(YAML::Node&& n);
static void Rebuild();
constexpr char const* SystemToNode(System system);

static YAML::Node config;
std::string config_path;

constexpr char const* rom_path_id = "rom_path";
constexpr char const* filter_game_list_id = "filter_game_list";
constexpr char const* n64_use_cpu_recompiler_id = "use_cpu_recompiler";
constexpr char const* n64_use_rsp_recompiler_id = "use_cpu_recompiler";

void EmitN64(YAML::Emitter& out)
{
    out << YAML::BeginMap;
    out << YAML::Key << SystemToNode(System::N64);
    out << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << filter_game_list_id;
    out << YAML::Value << true;
    out << YAML::Key << rom_path_id;
    out << YAML::Value << fs::current_path().generic_string();
    out << YAML::Key << "use_cpu_recompiler";
    out << YAML::Value << false;
    out << YAML::Key << "use_rsp_recompiler";
    out << YAML::Value << false;
    out << YAML::EndMap;
    out << YAML::EndMap;
    if (!out.good()) {
        LogError(std::format("yaml emitter error: {}", out.GetLastError()));
    }
}

void Flush()
{
    YAML::Emitter emitter;
    emitter << config;
    Flush(emitter);
}

void Flush(YAML::Emitter& emitter)
{
    std::ofstream ofs{ config_path };
    ofs << emitter.c_str();
}

template<typename T> std::optional<T> Get(YAML::Node&& n)
{
    if (n) return n.as<T>();
    return {};
}

std::optional<bool> GetFilterGameList(System system)
{
    return Get<bool>(config[SystemToNode(system)][filter_game_list_id]);
}

std::optional<std::string> GetGamePath(System system)
{
    return Get<std::string>(config[SystemToNode(system)][rom_path_id]);
}

std::optional<bool> GetN64UseCpuRecompiler()
{
    return Get<bool>(config[SystemToNode(System::N64)][n64_use_cpu_recompiler_id]);
}

std::optional<bool> GetN64UseRspRecompiler()
{
    return Get<bool>(config[SystemToNode(System::N64)][n64_use_rsp_recompiler_id]);
}

void Open(fs::path const& work_path)
{
    config_path = (work_path / "config.yaml").generic_string(); // TODO: conv to generic std::string ok?
    bool rebuild{};
    try {
        config = YAML::LoadFile(config_path);
        rebuild = config.IsNull();
    } catch (YAML::BadFile const&) {
        rebuild = true;
    }
    if (rebuild) {
        Rebuild();
    }
}

void Rebuild()
{
    YAML::Emitter emitter;
    EmitN64(emitter);
    Flush(emitter);
    config = YAML::LoadFile(config_path);
}

template<typename T> void Set(YAML::Node&& n, T val)
{
    n = val;
    Flush(); // TODO: only flush at certain times?
}

void SetFilterGameList(System system, bool filter)
{
    Set(config[SystemToNode(system)][filter_game_list_id], filter);
}

void SetGamePath(System system, fs::path const& path)
{
    Set(config[SystemToNode(system)][rom_path_id], path.generic_string());
}

void SetN64UseCpuRecompiler(bool use)
{
    Set(config[SystemToNode(System::N64)][n64_use_cpu_recompiler_id], use);
}

void SetN64UseRspRecompiler(bool use)
{
    Set(config[SystemToNode(System::N64)][n64_use_rsp_recompiler_id], use);
}

constexpr char const* SystemToNode(System system)
{
    switch (system) {
    case System::CHIP8: return "chip8";
    case System::GB: return "gb";
    case System::GBA: return "gba";
    case System::N64: return "n64";
    case System::NES: return "nes";
    case System::PS2: return "ps2";
    default: assert(false);
    }
}

} // namespace frontend::config

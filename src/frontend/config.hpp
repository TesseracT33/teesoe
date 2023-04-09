#pragma once

#include "loader.hpp"
#include "status.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace frontend::config {

std::optional<std::string> GetGamePath(System system);
std::optional<bool> GetFilterGameList(System system);
std::optional<bool> GetN64UseCpuRecompiler();
std::optional<bool> GetN64UseRspRecompiler();
void Open(std::filesystem::path const& work_path);
void SetGamePath(System system, std::filesystem::path const& path);
void SetFilterGameList(System system, bool filter);
void SetN64UseCpuRecompiler(bool use);
void SetN64UseRspRecompiler(bool use);

} // namespace frontend::config

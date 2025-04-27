#pragma once

#include "numtypes.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

std::expected<std::vector<u8>, std::string> OpenFile(std::filesystem::path const& path, size_t expected_size = 0);

#include "files.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <string>

std::expected<std::vector<u8>, std::string> OpenFile(std::filesystem::path const& path, size_t expected_size)
{
    std::basic_ifstream<u8> ifs{ path, std::ios::in | std::ios::binary };
    if (!ifs) {
        return std::unexpected("Could not open the file");
    }
    std::vector vec(std::istreambuf_iterator<u8>(ifs), {});
    if (expected_size > 0 && vec.size() != expected_size) {
        return std::unexpected(
          std::format("The file was of the wrong size; expected {}, got {}.", expected_size, vec.size()));
    }
    return vec;
}

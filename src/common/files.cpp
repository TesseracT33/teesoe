#include "files.hpp"

#include <format>
#include <fstream>
#include <iterator>

std::expected<std::vector<u8>, std::string> OpenFile(std::filesystem::path const& path, size_t expected_size)
{
    std::ifstream ifs{ path, std::ios::binary };
    if (!ifs) {
        return std::unexpected("Could not open the file");
    }
    // Apparently, std::istreambuf_iterator<u8> cannot be used in GCC. You will get a std::bad_cast thrown when
    // constructing the vector.
    std::vector<u8> vec(std::istreambuf_iterator<char>(ifs), {});
    if (expected_size > 0 && vec.size() != expected_size) {
        return std::unexpected(
          std::format("The file was of the wrong size; expected {}, got {}.", expected_size, vec.size()));
    }
    return vec;
}

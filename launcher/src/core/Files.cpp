#include "core/Files.h"

#include "core/Text.h"

#include <fstream>
#include <iterator>

namespace black::core {

Result<std::vector<std::uint8_t>> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Error{"Could not open " + toUtf8(path)};
    }
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        return Error{"Could not read " + toUtf8(path)};
    }
    return bytes;
}

}  // namespace black::core

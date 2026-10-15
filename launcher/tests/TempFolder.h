#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace black::tests {

class TempFolder {
public:
    TempFolder() {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("black-launcher-test-" + std::to_string(stamp) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(m_path);
    }
    ~TempFolder() {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;
    TempFolder(TempFolder&&) = delete;
    TempFolder& operator=(TempFolder&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

    void write(const std::filesystem::path& relative, std::string_view contents) const {
        const std::filesystem::path file = m_path / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

private:
    std::filesystem::path m_path;
};

}  // namespace black::tests

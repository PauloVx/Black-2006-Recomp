#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace black::core {

/// A text log for many threads. With a limit, lines that would pass it are counted instead, and a footer says how
/// many were dropped, so a long diagnostic run cannot fill the disk.
class TraceLog {
public:
    static constexpr std::uint64_t kDiagnosticLimit = 1024 * 1024;

    /// limit 0: no limit. `tag` starts the footer line ("[KTRACE] TRUNCATED: ...").
    TraceLog(const std::filesystem::path& path, std::uint64_t limit = 0, std::string tag = {});
    TraceLog(const TraceLog&) = delete;
    TraceLog& operator=(const TraceLog&) = delete;
    TraceLog(TraceLog&&) = delete;
    TraceLog& operator=(TraceLog&&) = delete;
    ~TraceLog();

    void write(std::string_view line);
    /// Writes the footer; later lines are ignored.
    void complete();
    /// Empty while every write succeeded.
    [[nodiscard]] std::string failure() const;

private:
    static constexpr std::uint64_t kFooterReserve = 128;

    std::filesystem::path m_path;
    std::ofstream m_out;
    std::uint64_t m_limit;
    std::string m_tag;
    std::uint64_t m_written = 0;
    std::uint64_t m_dropped = 0;
    bool m_complete = false;
    std::string m_failure;
    mutable std::mutex m_mutex;
};

}  // namespace black::core

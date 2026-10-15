#pragma once

#include "core/Result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace black::core::xdvdfs {

/// XDVDFS, the Xbox disc file system: 2048-byte sectors, a volume header at sector 32 of the game partition, and
/// directories stored as binary trees of 4-byte-aligned entries (layout as in xboxdev/extract-xiso).
inline constexpr std::uint64_t kSectorSize = 2048;
inline constexpr std::uint64_t kHeaderOffset = 0x10000;

/// Where the game partition starts in the images players have: an XISO (0), or a full Redump-style dump of an
/// XGD1, XGD2 or XGD3 disc.
inline constexpr std::array<std::uint64_t, 4> kPartitionOffsets = {0, 0x18300000, 0x0FD90000, 0x02080000};

struct Entry {
    std::string path;  ///< relative, '/'-separated, as on the disc (ASCII; the file system ignores case)
    std::uint32_t sector = 0;
    std::uint64_t size = 0;
    bool directory = false;
};

class Image {
public:
    [[nodiscard]] static Result<Image> open(const std::filesystem::path& file,
                                            std::span<const std::uint64_t> partitionOffsets = kPartitionOffsets);

    [[nodiscard]] std::uint64_t partitionOffset() const noexcept { return m_partition; }

    /// Every file and directory, parents before their contents. Fails on a damaged tree (entries out of range,
    /// loops, or names that are not plain file names).
    [[nodiscard]] Result<std::vector<Entry>> list() const;

    [[nodiscard]] Result<std::vector<std::uint8_t>> read(const Entry& file) const;

    /// Copies a file out in chunks, calling `copied` with the bytes written so far after each one; returning false
    /// from it stops the copy (the partial file is left for the caller).
    [[nodiscard]] Result<bool> copyTo(const Entry& file, const std::filesystem::path& destination,
                                      const std::function<bool(std::uint64_t)>& copied) const;

private:
    Image(std::unique_ptr<std::ifstream> stream, std::filesystem::path file, std::uint64_t partition,
          std::uint64_t fileSize, std::uint32_t rootSector, std::uint32_t rootSize);

    [[nodiscard]] bool readAt(std::uint64_t offset, std::span<std::uint8_t> out) const;
    [[nodiscard]] Result<bool> listDirectory(std::uint32_t sector, std::uint32_t size, const std::string& prefix,
                                             int depth, std::vector<std::uint32_t>& visiting,
                                             std::vector<Entry>& out) const;

    std::unique_ptr<std::ifstream> m_stream;  // one reader per Image; not shared between threads
    std::filesystem::path m_file;
    std::uint64_t m_partition = 0;
    std::uint64_t m_fileSize = 0;
    std::uint32_t m_rootSector = 0;
    std::uint32_t m_rootSize = 0;
};

/// True for a single safe path component: not empty, not "." or "..", no separators, drive colons or control
/// characters. Disc names that fail this are refused rather than written.
[[nodiscard]] bool isSafeName(std::string_view name) noexcept;

}  // namespace black::core::xdvdfs

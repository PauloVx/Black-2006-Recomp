#include "core/Xdvdfs.h"

#include "core/Text.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace black::core::xdvdfs {
namespace {

constexpr std::string_view kMagic = "MICROSOFT*XBOX*MEDIA";
constexpr std::uint8_t kDirectoryAttribute = 0x10;
constexpr std::uint16_t kPadding = 0xFFFF;
constexpr std::size_t kNameOffset = 14;
constexpr int kMaxDepth = 32;
constexpr std::uint32_t kMaxDirectorySize = 64U * 1024U * 1024U;
constexpr std::size_t kCopyChunk = 4U * 1024U * 1024U;

std::uint16_t read16(std::span<const std::uint8_t> data, std::size_t at) noexcept {
    return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8));
}

std::uint32_t read32(std::span<const std::uint8_t> data, std::size_t at) noexcept {
    return std::uint32_t{data[at]} | (std::uint32_t{data[at + 1]} << 8) | (std::uint32_t{data[at + 2]} << 16) |
           (std::uint32_t{data[at + 3]} << 24);
}

}  // namespace

bool isSafeName(std::string_view name) noexcept {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    return std::none_of(name.begin(), name.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte < 0x20 || c == '/' || c == '\\' || c == ':' || byte == 0x7F;
    });
}

Image::Image(std::unique_ptr<std::ifstream> stream, std::filesystem::path file, std::uint64_t partition,
             std::uint64_t fileSize, std::uint32_t rootSector, std::uint32_t rootSize)
    : m_stream(std::move(stream)),
      m_file(std::move(file)),
      m_partition(partition),
      m_fileSize(fileSize),
      m_rootSector(rootSector),
      m_rootSize(rootSize) {}

Result<Image> Image::open(const std::filesystem::path& file, std::span<const std::uint64_t> partitionOffsets) {
    std::error_code error;
    const std::uint64_t fileSize = std::filesystem::file_size(file, error);
    if (error) {
        return Error{"Could not open " + toUtf8(file)};
    }
    auto stream = std::make_unique<std::ifstream>(file, std::ios::binary);
    if (!*stream) {
        return Error{"Could not open " + toUtf8(file)};
    }
    for (const std::uint64_t partition : partitionOffsets) {
        if (partition + kHeaderOffset + kSectorSize > fileSize) {
            continue;
        }
        std::array<std::uint8_t, kSectorSize> header{};
        stream->clear();
        stream->seekg(static_cast<std::streamoff>(partition + kHeaderOffset));
        if (!stream->read(
                reinterpret_cast<char*>(header.data()),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                static_cast<std::streamsize>(header.size()))) {
            continue;
        }
        const bool magicAtStart = std::memcmp(header.data(), kMagic.data(), kMagic.size()) == 0;
        const bool magicAtEnd = std::memcmp(header.data() + 0x7EC, kMagic.data(), kMagic.size()) == 0;
        if (!magicAtStart || !magicAtEnd) {
            continue;
        }
        const std::uint32_t rootSector = read32(header, 20);
        const std::uint32_t rootSize = read32(header, 24);
        return Image(std::move(stream), file, partition, fileSize, rootSector, rootSize);
    }
    return Error{"This is not an Xbox disc image: " + toUtf8(file.filename())};
}

bool Image::readAt(std::uint64_t offset, std::span<std::uint8_t> out) const {
    if (offset > m_fileSize || m_fileSize - offset < out.size()) {
        return false;
    }
    m_stream->clear();
    m_stream->seekg(static_cast<std::streamoff>(offset));
    return static_cast<bool>(
        m_stream->read(reinterpret_cast<char*>(out.data()),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       static_cast<std::streamsize>(out.size())));
}

Result<bool> Image::listDirectory(std::uint32_t sector, std::uint32_t size, const std::string& prefix, int depth,
                                  std::vector<std::uint32_t>& visiting, std::vector<Entry>& out) const {
    if (size == 0) {
        return true;  // an empty directory
    }
    if (depth > kMaxDepth || size > kMaxDirectorySize ||
        std::find(visiting.begin(), visiting.end(), sector) != visiting.end()) {
        return Error{"The disc image's folders are damaged"};
    }
    std::vector<std::uint8_t> table(size);
    if (!readAt(m_partition + std::uint64_t{sector} * kSectorSize, table)) {
        return Error{"The disc image is incomplete (a folder lies past its end)"};
    }
    visiting.push_back(sector);

    // Walk the tree with an explicit stack of entry offsets (in bytes); each offset is visited at most once.
    std::vector<std::size_t> pending{0};
    std::vector<bool> seen(table.size() / 4 + 1, false);
    std::vector<Entry> directories;
    while (!pending.empty()) {
        const std::size_t at = pending.back();
        pending.pop_back();
        if (at + kNameOffset > table.size() || seen[at / 4]) {
            continue;
        }
        seen[at / 4] = true;
        const std::uint16_t left = read16(table, at);
        if (left == kPadding) {
            continue;  // the rest of this sector is padding
        }
        const std::uint16_t right = read16(table, at + 2);
        const std::uint8_t attributes = table[at + 12];
        const std::size_t nameLength = table[at + 13];
        if (nameLength == 0 || at + kNameOffset + nameLength > table.size()) {
            return Error{"The disc image's folders are damaged"};
        }
        const std::string name(reinterpret_cast<const char*>(table.data() + at + kNameOffset),  // NOLINT
                               nameLength);
        if (!isSafeName(name)) {
            return Error{"The disc image has an unsafe file name: " + name};
        }
        Entry entry;
        entry.path = prefix + name;
        entry.sector = read32(table, at + 4);
        entry.size = read32(table, at + 8);
        entry.directory = (attributes & kDirectoryAttribute) != 0;
        out.push_back(entry);
        if (entry.directory) {
            directories.push_back(entry);
        }
        if (left != 0) {
            pending.push_back(std::size_t{left} * 4);
        }
        if (right != 0) {
            pending.push_back(std::size_t{right} * 4);
        }
    }
    for (const Entry& directory : directories) {
        Result<bool> listed = listDirectory(directory.sector, static_cast<std::uint32_t>(directory.size),
                                            directory.path + "/", depth + 1, visiting, out);
        if (!listed) {
            return listed;
        }
    }
    visiting.pop_back();
    return true;
}

Result<std::vector<Entry>> Image::list() const {
    std::vector<Entry> entries;
    std::vector<std::uint32_t> visiting;
    Result<bool> listed = listDirectory(m_rootSector, m_rootSize, "", 0, visiting, entries);
    if (!listed) {
        return listed.error();
    }
    for (const Entry& entry : entries) {
        if (!entry.directory && m_partition + std::uint64_t{entry.sector} * kSectorSize + entry.size > m_fileSize) {
            return Error{"The disc image is incomplete: " + entry.path +
                         " lies past its end. Was the download "
                         "or copy finished?"};
        }
    }
    return entries;
}

Result<std::vector<std::uint8_t>> Image::read(const Entry& file) const {
    std::vector<std::uint8_t> bytes(file.size);
    if (!readAt(m_partition + std::uint64_t{file.sector} * kSectorSize, bytes)) {
        return Error{"Could not read " + file.path + " from the disc image"};
    }
    return bytes;
}

Result<bool> Image::copyTo(const Entry& file, const std::filesystem::path& destination,
                           const std::function<bool(std::uint64_t)>& copied) const {
    std::ofstream out(destination, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Error{"Could not write " + toUtf8(destination)};
    }
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(std::min<std::uint64_t>(kCopyChunk, file.size)));
    std::uint64_t done = 0;
    while (done < file.size) {
        const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), file.size - done));
        const std::span<std::uint8_t> part(buffer.data(), chunk);
        if (!readAt(m_partition + std::uint64_t{file.sector} * kSectorSize + done, part)) {
            return Error{"Could not read " + file.path + " from the disc image"};
        }
        out.write(reinterpret_cast<const char*>(part.data()),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                  static_cast<std::streamsize>(chunk));
        if (!out) {
            return Error{"Could not write " + toUtf8(destination) + ". Is the disk full?"};
        }
        done += chunk;
        if (copied && !copied(done)) {
            return false;
        }
    }
    return true;
}

}  // namespace black::core::xdvdfs

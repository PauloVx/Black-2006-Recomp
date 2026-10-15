#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace black::tests {

/// Builds small Xbox disc images (XDVDFS) in memory: a header at sector 32, one directory table per folder
/// (entries chained through their right pointers, never crossing a sector), then the file data.
class XisoBuilder {
public:
    static constexpr std::size_t kSector = 2048;

    void addFile(const std::string& path, std::string contents) { m_files[path] = std::move(contents); }

    /// The image, preceded by `partitionOffset` bytes of zeros (as in a full Redump-style dump).
    [[nodiscard]] std::vector<std::uint8_t> build(std::size_t partitionOffset = 0) const {
        std::map<std::string, std::vector<std::pair<std::string, bool>>> folders{{"", {}}};
        for (const auto& [path, contents] : m_files) {
            std::string parent;
            std::size_t start = 0;
            for (std::size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', start)) {
                const std::string name = path.substr(start, slash - start);
                const std::string folder = path.substr(0, slash);
                auto& children = folders[parent];
                if (std::find(children.begin(), children.end(), std::make_pair(name, true)) == children.end()) {
                    children.emplace_back(name, true);
                }
                folders.try_emplace(folder);
                parent = folder;
                start = slash + 1;
            }
            folders[parent].emplace_back(path.substr(start), false);
        }

        // Lay out: sectors 0-31 empty, 32 the header, then the folder tables, then the files
        std::size_t nextSector = 33;
        std::map<std::string, std::pair<std::size_t, std::size_t>> tables;  // folder -> sector, bytes
        for (const auto& [folder, children] : folders) {
            const std::size_t bytes = tableBytes(children);
            tables[folder] = {nextSector, bytes};
            nextSector += (bytes + kSector - 1) / kSector;
        }
        std::map<std::string, std::size_t> fileSectors;
        for (const auto& [path, contents] : m_files) {
            fileSectors[path] = nextSector;
            nextSector += std::max<std::size_t>(1, (contents.size() + kSector - 1) / kSector);
        }

        std::vector<std::uint8_t> image(partitionOffset + nextSector * kSector, 0);
        std::uint8_t* base = image.data() + partitionOffset;
        std::uint8_t* header = base + 32 * kSector;
        std::memcpy(header, "MICROSOFT*XBOX*MEDIA", 20);
        put32(header + 20, static_cast<std::uint32_t>(tables[""].first));
        put32(header + 24, static_cast<std::uint32_t>(tables[""].second));
        std::memcpy(header + 0x7EC, "MICROSOFT*XBOX*MEDIA", 20);

        for (const auto& [folder, children] : folders) {
            std::uint8_t* table = base + tables[folder].first * kSector;
            std::fill(table, table + ((tables[folder].second + kSector - 1) / kSector) * kSector, 0xFF);
            std::size_t at = 0;
            for (std::size_t i = 0; i < children.size(); ++i) {
                const auto& [name, isFolder] = children[i];
                at = placeEntry(at, name);
                const std::string full = folder.empty() ? name : folder + "/" + name;
                const std::size_t size = entrySize(name);
                std::size_t next = 0;
                if (i + 1 < children.size()) {
                    next = placeEntry(at + size, children[i + 1].first);
                }
                std::uint8_t* entry = table + at;
                put16(entry, 0);
                put16(entry + 2, static_cast<std::uint16_t>(next / 4));
                if (isFolder) {
                    put32(entry + 4, static_cast<std::uint32_t>(tables[full].first));
                    put32(entry + 8, static_cast<std::uint32_t>(tables[full].second));
                    entry[12] = 0x10;
                } else {
                    put32(entry + 4, static_cast<std::uint32_t>(fileSectors[full]));
                    put32(entry + 8, static_cast<std::uint32_t>(m_files.at(full).size()));
                    entry[12] = 0x20;
                }
                entry[13] = static_cast<std::uint8_t>(name.size());
                std::memcpy(entry + 14, name.data(), name.size());
                at += size;
            }
        }
        for (const auto& [path, contents] : m_files) {
            std::memcpy(base + fileSectors[path] * kSector, contents.data(), contents.size());
        }
        return image;
    }

private:
    static void put16(std::uint8_t* at, std::uint16_t value) {
        at[0] = static_cast<std::uint8_t>(value);
        at[1] = static_cast<std::uint8_t>(value >> 8);
    }
    static void put32(std::uint8_t* at, std::uint32_t value) {
        for (int i = 0; i < 4; ++i) {
            at[i] = static_cast<std::uint8_t>(value >> (8 * i));
        }
    }
    static std::size_t entrySize(const std::string& name) { return (14 + name.size() + 3) / 4 * 4; }
    /// An entry starts where it fits in the current sector, else at the next one.
    static std::size_t placeEntry(std::size_t at, const std::string& name) {
        const std::size_t room = kSector - at % kSector;
        return entrySize(name) <= room ? at : at + room;
    }
    static std::size_t tableBytes(const std::vector<std::pair<std::string, bool>>& children) {
        std::size_t at = 0;
        for (const auto& child : children) {
            at = placeEntry(at, child.first) + entrySize(child.first);
        }
        return at;
    }

    std::map<std::string, std::string> m_files;
};

}  // namespace black::tests

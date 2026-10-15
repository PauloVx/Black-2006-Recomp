#pragma once

#include "core/GameFiles.h"
#include "core/Result.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace black::core {

struct DiscSource {
    enum class Kind { Image, Folder };

    /// One file or folder of the disc, relative to its root with '/' separators.
    struct File {
        std::string path;
        std::uint64_t size = 0;
        std::uint32_t sector = 0;  ///< images only
        bool directory = false;
    };

    Kind kind = Kind::Image;
    std::filesystem::path location;
    std::uint64_t partitionOffset = 0;  ///< images only: where the game partition starts
    std::vector<File> files;
    std::uint64_t totalBytes = 0;
    std::size_t fileCount = 0;  ///< files, not counting folders
    GameFilesStatus release;
};

/// Hashes default.xbe and lists the files, copying nothing.
[[nodiscard]] Result<DiscSource> inspectDiscSource(const std::filesystem::path& location);

struct ImportProgress {
    enum class Stage { Reading, Checking, Copying, Finishing, Done };

    Stage stage = Stage::Reading;
    std::string item;  ///< the file being copied, relative to the disc root
    std::uint64_t doneBytes = 0;
    std::uint64_t totalBytes = 0;
    std::size_t doneFiles = 0;
    std::size_t totalFiles = 0;
};

using ImportProgressCallback = std::function<void(const ImportProgress&)>;

/// Files already there with the right size are kept, so an interrupted import resumes; nothing else is removed (saves
/// live in game/UDATA and TDATA); default.xbe is written last, so an unfinished import never counts as ready.
/// Returns false when cancelled.
[[nodiscard]] Result<bool> importDisc(const DiscSource& source, const std::filesystem::path& gameDirectory,
                                      const ImportProgressCallback& progress, std::stop_token stop);

[[nodiscard]] std::uint64_t bytesStillNeeded(const DiscSource& source, const std::filesystem::path& gameDirectory);

}  // namespace black::core

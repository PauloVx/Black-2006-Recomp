#pragma once

#include "core/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace black::core {

class Sha256 {
public:
    using Digest = std::array<std::uint8_t, 32>;

    Sha256() noexcept;

    void update(std::span<const std::byte> data) noexcept;
    /// Finishes the hash. The object must not be updated afterwards.
    [[nodiscard]] Digest finish() noexcept;

    /// Upper-case hexadecimal, the form the project's scripts and docs use.
    [[nodiscard]] static std::string toHex(const Digest& digest);

private:
    void compress(const std::uint8_t* block) noexcept;

    std::array<std::uint32_t, 8> m_state{};
    std::array<std::uint8_t, 64> m_block{};
    std::size_t m_blockSize = 0;
    std::uint64_t m_totalBytes = 0;
};

[[nodiscard]] Result<Sha256::Digest> sha256File(const std::filesystem::path& path);

}  // namespace black::core

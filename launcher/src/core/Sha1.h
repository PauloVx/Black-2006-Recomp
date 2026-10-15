#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace black::core {

/// SHA-1, only for the HMAC that signs BLACK's save profiles (the format's choice, not a security measure here).
class Sha1 {
public:
    using Digest = std::array<std::uint8_t, 20>;
    static constexpr std::size_t kBlockSize = 64;

    Sha1() noexcept;
    void update(std::span<const std::uint8_t> data) noexcept;
    [[nodiscard]] Digest finish() noexcept;

private:
    void compress(const std::uint8_t* block) noexcept;
    std::array<std::uint32_t, 5> m_state{};
    std::array<std::uint8_t, kBlockSize> m_block{};
    std::size_t m_blockSize = 0;
    std::uint64_t m_totalBytes = 0;
};

[[nodiscard]] Sha1::Digest hmacSha1(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data);

}  // namespace black::core

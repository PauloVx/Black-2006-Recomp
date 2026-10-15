#include "core/Sha1.h"

#include <algorithm>
#include <bit>

namespace black::core {

Sha1::Sha1() noexcept : m_state{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U} {}

void Sha1::compress(const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 80> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        w[i] = (std::uint32_t{block[i * 4]} << 24U) | (std::uint32_t{block[i * 4 + 1]} << 16U) |
               (std::uint32_t{block[i * 4 + 2]} << 8U) | std::uint32_t{block[i * 4 + 3]};  // NOLINT
    }
    for (std::size_t i = 16; i < 80; ++i) {
        w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    auto [a, b, c, d, e] = m_state;
    for (std::size_t i = 0; i < 80; ++i) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999U;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }
        const std::uint32_t next = std::rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = std::rotl(b, 30);
        b = a;
        a = next;
    }
    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
}

void Sha1::update(std::span<const std::uint8_t> data) noexcept {
    m_totalBytes += data.size();
    while (!data.empty()) {
        const std::size_t take = std::min(kBlockSize - m_blockSize, data.size());
        std::copy_n(data.begin(), take, m_block.begin() + static_cast<std::ptrdiff_t>(m_blockSize));
        m_blockSize += take;
        data = data.subspan(take);
        if (m_blockSize == kBlockSize) {
            compress(m_block.data());
            m_blockSize = 0;
        }
    }
}

Sha1::Digest Sha1::finish() noexcept {
    const std::uint64_t bits = m_totalBytes * 8U;
    const std::array<std::uint8_t, 1> one{0x80};
    update(one);
    const std::array<std::uint8_t, 1> zero{0};
    while (m_blockSize != kBlockSize - 8) {
        update(zero);
    }
    std::array<std::uint8_t, 8> length{};
    for (std::size_t i = 0; i < 8; ++i) {
        length[i] = static_cast<std::uint8_t>(bits >> (56U - i * 8U));
    }
    update(length);
    Digest digest{};
    for (std::size_t i = 0; i < 20; ++i) {
        digest[i] = static_cast<std::uint8_t>(m_state[i / 4] >> (24U - (i % 4) * 8U));
    }
    return digest;
}

Sha1::Digest hmacSha1(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data) {
    std::array<std::uint8_t, Sha1::kBlockSize> block{};
    if (key.size() > block.size()) {
        Sha1 hashed;
        hashed.update(key);
        const Sha1::Digest digest = hashed.finish();
        std::copy(digest.begin(), digest.end(), block.begin());
    } else {
        std::copy(key.begin(), key.end(), block.begin());
    }
    std::array<std::uint8_t, Sha1::kBlockSize> pad{};
    for (std::size_t i = 0; i < block.size(); ++i) {
        pad[i] = block[i] ^ 0x36U;
    }
    Sha1 inner;
    inner.update(pad);
    inner.update(data);
    const Sha1::Digest innerDigest = inner.finish();
    for (std::size_t i = 0; i < block.size(); ++i) {
        pad[i] = block[i] ^ 0x5CU;
    }
    Sha1 outer;
    outer.update(pad);
    outer.update(innerDigest);
    return outer.finish();
}

}  // namespace black::core

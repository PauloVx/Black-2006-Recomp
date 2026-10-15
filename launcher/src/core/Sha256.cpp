#include "core/Sha256.h"

#include "core/Text.h"

#include <bit>
#include <fstream>
#include <vector>

namespace black::core {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr std::array<std::uint32_t, 8> kInitialState = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

constexpr std::uint32_t loadBigEndian(const std::uint8_t* bytes) noexcept {
    return (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) |
           std::uint32_t{bytes[3]};
}

}  // namespace

Sha256::Sha256() noexcept : m_state(kInitialState) {}

void Sha256::compress(const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = loadBigEndian(block + i * 4);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto [a, b, c, d, e, f, g, h] = m_state;
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + choose + kRoundConstants[i] + w[i];
        const std::uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    const std::array<std::uint32_t, 8> add = {a, b, c, d, e, f, g, h};
    for (std::size_t i = 0; i < m_state.size(); ++i) {
        m_state[i] += add[i];
    }
}

void Sha256::update(std::span<const std::byte> data) noexcept {
    m_totalBytes += data.size();
    for (const std::byte value : data) {
        m_block[m_blockSize++] = std::to_integer<std::uint8_t>(value);
        if (m_blockSize == m_block.size()) {
            compress(m_block.data());
            m_blockSize = 0;
        }
    }
}

Sha256::Digest Sha256::finish() noexcept {
    const std::uint64_t totalBits = m_totalBytes * 8;
    m_block[m_blockSize++] = 0x80;
    if (m_blockSize > 56) {
        while (m_blockSize < m_block.size()) {
            m_block[m_blockSize++] = 0;
        }
        compress(m_block.data());
        m_blockSize = 0;
    }
    while (m_blockSize < 56) {
        m_block[m_blockSize++] = 0;
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        m_block[m_blockSize++] = static_cast<std::uint8_t>(totalBits >> shift);
    }
    compress(m_block.data());

    Digest digest{};
    for (std::size_t i = 0; i < m_state.size(); ++i) {
        digest[i * 4 + 0] = static_cast<std::uint8_t>(m_state[i] >> 24);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(m_state[i] >> 16);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(m_state[i] >> 8);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(m_state[i]);
    }
    return digest;
}

std::string Sha256::toHex(const Digest& digest) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string text;
    text.reserve(digest.size() * 2);
    for (const std::uint8_t value : digest) {
        text.push_back(kDigits[value >> 4]);
        text.push_back(kDigits[value & 0x0F]);
    }
    return text;
}

Result<Sha256::Digest> sha256File(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Error{"Could not open " + toUtf8(path)};
    }
    Sha256 hash;
    std::vector<char> buffer(1 << 20);
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(file.gcount());
        hash.update(std::as_bytes(std::span(buffer.data(), count)));
    }
    if (file.bad()) {
        return Error{"Could not read " + toUtf8(path)};
    }
    return hash.finish();
}

}  // namespace black::core

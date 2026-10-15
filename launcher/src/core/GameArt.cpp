#include "core/GameArt.h"

#include "core/Files.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace black::core {
namespace {

constexpr std::uint32_t kMaxTextureSide = 2048;

/// Bounds-checked little-endian reads: out of range gives 0, so malformed files fail validation instead of
/// reading past the buffer.
std::uint32_t read32(std::span<const std::uint8_t> data, std::size_t at) noexcept {
    if (at > data.size() || data.size() - at < 4) {
        return 0;
    }
    return std::uint32_t{data[at]} | (std::uint32_t{data[at + 1]} << 8) | (std::uint32_t{data[at + 2]} << 16) |
           (std::uint32_t{data[at + 3]} << 24);
}

float readFloat(std::span<const std::uint8_t> data, std::size_t at) noexcept {
    const std::uint32_t bits = read32(data, at);
    float value = 0;
    std::memcpy(&value, &bits, sizeof value);
    return std::isfinite(value) ? value : 0.0f;
}

std::array<std::uint8_t, 3> rgb565(unsigned color) noexcept {
    const unsigned r = (color >> 11) & 31U;
    const unsigned g = (color >> 5) & 63U;
    const unsigned b = color & 31U;
    return {static_cast<std::uint8_t>((r << 3) | (r >> 2)), static_cast<std::uint8_t>((g << 2) | (g >> 4)),
            static_cast<std::uint8_t>((b << 3) | (b >> 2))};
}

std::array<std::uint8_t, 16> blockAlpha(const std::uint8_t* block, bool dxt3) noexcept {
    std::array<std::uint8_t, 16> alpha{};
    if (dxt3) {
        for (unsigned i = 0; i < 16; ++i) {
            alpha[i] = static_cast<std::uint8_t>(((block[i / 2] >> ((i & 1U) * 4)) & 15U) * 17U);
        }
        return alpha;
    }
    const unsigned a0 = block[0];
    const unsigned a1 = block[1];
    std::array<unsigned, 8> palette{a0, a1};
    if (a0 > a1) {
        for (unsigned i = 0; i < 6; ++i) {
            palette[2 + i] = ((6 - i) * a0 + (1 + i) * a1 + 3) / 7;
        }
    } else {
        for (unsigned i = 0; i < 4; ++i) {
            palette[2 + i] = ((4 - i) * a0 + (1 + i) * a1 + 2) / 5;
        }
        palette[6] = 0;
        palette[7] = 255;
    }
    std::uint64_t bits = 0;
    for (unsigned i = 0; i < 6; ++i) {
        bits |= std::uint64_t{block[2 + i]} << (8 * i);
    }
    for (unsigned i = 0; i < 16; ++i) {
        alpha[i] = static_cast<std::uint8_t>(palette[(bits >> (3 * i)) & 7U]);
    }
    return alpha;
}

}  // namespace

Result<RgbaImage> decodeDxt(std::span<const std::uint8_t> data, std::uint32_t width, std::uint32_t height,
                            std::uint32_t format) {
    const bool dxt3 = format == static_cast<std::uint32_t>(DxtFormat::Dxt3);
    if (!dxt3 && format != static_cast<std::uint32_t>(DxtFormat::Dxt5)) {
        return Error{"Unsupported texture format " + std::to_string(format)};
    }
    if (width == 0 || height == 0 || width % 4 != 0 || height % 4 != 0 || width > kMaxTextureSide ||
        height > kMaxTextureSide) {
        return Error{"Unsupported texture size " + std::to_string(width) + " x " + std::to_string(height)};
    }
    // 16 bytes per 4 x 4 block: one byte per texel
    if (data.size() < std::size_t{width} * height) {
        return Error{"Texture data is truncated"};
    }

    RgbaImage image{width, height, std::vector<std::uint8_t>(std::size_t{width} * height * 4)};
    const std::uint8_t* block = data.data();
    for (std::uint32_t by = 0; by < height; by += 4) {
        for (std::uint32_t bx = 0; bx < width; bx += 4, block += 16) {
            const std::array<std::uint8_t, 16> alpha = blockAlpha(block, dxt3);
            const unsigned c0 = block[8] | (unsigned{block[9]} << 8);
            const unsigned c1 = block[10] | (unsigned{block[11]} << 8);
            std::array<std::array<std::uint8_t, 3>, 4> palette{rgb565(c0), rgb565(c1)};
            for (std::size_t i = 0; i < 3; ++i) {
                palette[2][i] = static_cast<std::uint8_t>((2U * palette[0][i] + palette[1][i] + 1U) / 3U);
                palette[3][i] = static_cast<std::uint8_t>((palette[0][i] + 2U * palette[1][i] + 1U) / 3U);
            }
            const std::uint32_t indices = std::uint32_t{block[12]} | (std::uint32_t{block[13]} << 8) |
                                          (std::uint32_t{block[14]} << 16) | (std::uint32_t{block[15]} << 24);
            for (unsigned i = 0; i < 16; ++i) {
                const std::array<std::uint8_t, 3>& color = palette[(indices >> (2 * i)) & 3U];
                const std::size_t x = bx + (i & 3U);
                const std::size_t y = by + (i >> 2);
                std::uint8_t* out = &image.pixels[(y * width + x) * 4];
                out[0] = color[0];
                out[1] = color[1];
                out[2] = color[2];
                out[3] = alpha[i];
            }
        }
    }
    return image;
}

const BitmapGlyph* BitmapFont::find(char16_t code) const noexcept {
    const auto lookup = [this](char16_t wanted) -> const BitmapGlyph* {
        const auto it = std::find_if(glyphs.begin(), glyphs.end(),
                                     [wanted](const BitmapGlyph& glyph) { return glyph.code == wanted; });
        return it == glyphs.end() ? nullptr : &*it;
    };
    if (const BitmapGlyph* glyph = lookup(code)) {
        return glyph;
    }
    const bool lowerAscii = code >= u'a' && code <= u'z';
    const bool lowerLatin1 = code >= 0xE0 && code <= 0xFE && code != 0xF7;
    if (lowerAscii || lowerLatin1) {
        return lookup(static_cast<char16_t>(code - 32));
    }
    return nullptr;
}

Result<BitmapFont> parseBitmapFont(std::span<const std::uint8_t> data) {
    constexpr std::size_t kGlyphTable = 0x220;
    constexpr std::size_t kGlyphSize = 0x28;
    constexpr std::size_t kTextureHeader = 0x80;
    if (data.size() < 0x300 || std::memcmp(data.data(), "4.1v", 4) != 0) {
        return Error{"Not a BLACK font file"};
    }
    const std::size_t texture = read32(data, 4);
    if (texture < kGlyphTable || texture > data.size() || data.size() - texture < kTextureHeader) {
        return Error{"The font's texture is out of range"};
    }
    const std::uint32_t format = read32(data, texture + 0x34);
    const std::uint32_t width = read32(data, texture + 0x38);
    const std::uint32_t height = read32(data, texture + 0x3C);
    Result<RgbaImage> atlas = decodeDxt(data.subspan(texture + kTextureHeader), width, height, format);
    if (!atlas) {
        return atlas.error();
    }

    BitmapFont font;
    font.atlas = std::move(atlas).value();
    const auto w = static_cast<float>(width);
    const auto h = static_cast<float>(height);
    for (std::size_t at = kGlyphTable; at + kGlyphSize <= texture; at += kGlyphSize) {
        const auto code = static_cast<char16_t>(data[at + 0x1C] | (data[at + 0x1D] << 8));
        if (code == 0) {
            continue;
        }
        BitmapGlyph glyph;
        glyph.code = code;
        glyph.u = readFloat(data, at) * w;
        glyph.v = readFloat(data, at + 4) * h;
        glyph.width = readFloat(data, at + 8) * w;
        glyph.height = readFloat(data, at + 12) * h;
        glyph.offsetX = readFloat(data, at + 16) * w;
        glyph.offsetY = readFloat(data, at + 20) * h;
        glyph.advance = readFloat(data, at + 24) * w;
        if (glyph.u < 0 || glyph.v < 0) {  // the space has no cell
            glyph.width = 0;
            glyph.height = 0;
        }
        font.glyphs.push_back(glyph);
    }
    const float ratio = readFloat(data, 12);
    font.nominalSize = ratio > 0 ? h / ratio : 0.0f;
    if (font.glyphs.empty() || font.nominalSize <= 1.0f || font.nominalSize >= 200.0f) {
        return Error{"The font has no usable glyphs"};
    }
    if (const BitmapGlyph* capital = font.find(u'A')) {
        font.capTop = capital->offsetY;
    }
    return font;
}

GameArt loadGameArt(const ProjectLayout& project) {
    GameArt art;
    const std::filesystem::path fonts = project.gameDirectory() / "language" / "fonts";
    if (auto bytes = readFile(fonts / "Small.bin")) {
        if (auto font = parseBitmapFont(bytes.value())) {
            art.smallFont = std::move(font).value();
        }
    }
    if (auto bytes = readFile(fonts / "Big.bin")) {
        if (auto font = parseBitmapFont(bytes.value())) {
            art.bigFont = std::move(font).value();
        }
    }
    return art;
}

}  // namespace black::core

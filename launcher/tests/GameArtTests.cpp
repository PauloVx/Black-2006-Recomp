#include "TempFolder.h"
#include "core/GameArt.h"

#include <doctest/doctest.h>

#include <cstring>
#include <vector>

using namespace black::core;

namespace {

void put32(std::vector<std::uint8_t>& data, std::size_t at, std::uint32_t value) {
    if (data.size() < at + 4) {
        data.resize(at + 4);
    }
    std::memcpy(data.data() + at, &value, 4);
}

void putFloat(std::vector<std::uint8_t>& data, std::size_t at, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    put32(data, at, bits);
}

/// One 4 x 4 DXT5 block: every texel colour 0 (pure red, 0xF800) at full alpha.
std::vector<std::uint8_t> redBlock() {
    return {255, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0};
}

void putBlock(std::vector<std::uint8_t>& data, std::size_t at) {
    const std::vector<std::uint8_t> block = redBlock();
    if (data.size() < at + block.size()) {
        data.resize(at + block.size());
    }
    std::memcpy(data.data() + at, block.data(), block.size());
}

/// The smallest valid "4.1v" font: 'A' and a space over a 4 x 4 atlas.
std::vector<std::uint8_t> tinyFont() {
    constexpr std::size_t kTexture = 0x220 + 2 * 0x28;
    std::vector<std::uint8_t> data(0x300, 0);
    std::memcpy(data.data(), "4.1v", 4);
    put32(data, 4, kTexture);
    putFloat(data, 12, 0.25f);  // atlas height 4 / 0.25: one texel is one unit at size 16
    // 'A': half the atlas, drawn 1 texel down, advancing 3 texels
    putFloat(data, 0x220 + 8, 0.5f);
    putFloat(data, 0x220 + 12, 0.5f);
    putFloat(data, 0x220 + 20, 0.25f);
    putFloat(data, 0x220 + 24, 0.75f);
    data[0x220 + 0x1C] = 'A';
    // ' ': no cell (u < 0), advancing 2 texels
    putFloat(data, 0x248, -1.0f);
    putFloat(data, 0x248 + 24, 0.5f);
    data[0x248 + 0x1C] = ' ';
    put32(data, kTexture + 0x34, 0x0F);
    put32(data, kTexture + 0x38, 4);
    put32(data, kTexture + 0x3C, 4);
    putBlock(data, kTexture + 0x80);
    return data;
}

}  // namespace

TEST_CASE("DXT5 decodes colours and interpolated alpha") {
    const auto image = decodeDxt(redBlock(), 4, 4, 0x0F);
    REQUIRE(image.ok());
    CHECK(image.value().pixels.size() == 64);
    CHECK(image.value().pixels[0] == 255);
    CHECK(image.value().pixels[1] == 0);
    CHECK(image.value().pixels[2] == 0);
    CHECK(image.value().pixels[3] == 255);
}

TEST_CASE("DXT3 uses its explicit 4-bit alpha") {
    std::vector<std::uint8_t> block = redBlock();
    std::fill(block.begin(), block.begin() + 8, std::uint8_t{0x88});  // alpha 8 of 15 everywhere
    const auto image = decodeDxt(block, 4, 4, 0x0E);
    REQUIRE(image.ok());
    CHECK(image.value().pixels[3] == 8 * 17);
}

TEST_CASE("DXT decoding refuses what it cannot read") {
    CHECK_FALSE(decodeDxt(redBlock(), 4, 4, 0x0C).ok());
    CHECK_FALSE(decodeDxt(redBlock(), 6, 4, 0x0F).ok());
    CHECK_FALSE(decodeDxt(redBlock(), 8, 8, 0x0F).ok());
}

TEST_CASE("A game font gives its glyphs, size and cap line") {
    const auto font = parseBitmapFont(tinyFont());
    REQUIRE(font.ok());
    CHECK(font.value().nominalSize == doctest::Approx(16.0f));
    REQUIRE(font.value().glyphs.size() == 2);
    const BitmapGlyph* a = font.value().find(u'A');
    REQUIRE(a != nullptr);
    CHECK(a->width == doctest::Approx(2.0f));
    CHECK(a->advance == doctest::Approx(3.0f));
    CHECK(font.value().capTop == doctest::Approx(1.0f));
    CHECK(font.value().find(u'a') == a);
    const BitmapGlyph* space = font.value().find(u' ');
    REQUIRE(space != nullptr);
    CHECK(space->width == 0.0f);
    CHECK(font.value().find(u'Z') == nullptr);
}

TEST_CASE("Font parsing rejects other files") {
    std::vector<std::uint8_t> data = tinyFont();
    data[0] = 'X';
    CHECK_FALSE(parseBitmapFont(data).ok());
    CHECK_FALSE(parseBitmapFont(std::vector<std::uint8_t>(16, 0)).ok());
    std::vector<std::uint8_t> badTexture = tinyFont();
    put32(badTexture, 4, 0xFFFFFF);
    CHECK_FALSE(parseBitmapFont(badTexture).ok());
}

TEST_CASE("loadGameArt takes what the disc provides") {
    const black::tests::TempFolder folder;
    folder.write("CMakeLists.txt", "\n");
    folder.write("scripts/generate.py", "\n");
    const ProjectLayout project(folder.path());
    CHECK_FALSE(loadGameArt(project).complete());

    const std::vector<std::uint8_t> font = tinyFont();
    folder.write("game/language/fonts/Small.bin",
                 std::string_view(reinterpret_cast<const char*>(font.data()), font.size()));
    folder.write("game/language/fonts/Big.bin",
                 std::string_view(reinterpret_cast<const char*>(font.data()), font.size()));
    const GameArt art = loadGameArt(project);
    CHECK(art.complete());
}

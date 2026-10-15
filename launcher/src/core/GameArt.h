#pragma once

#include "core/ProjectLayout.h"
#include "core/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace black::core {

/// Straight-alpha RGBA8 pixels, row by row.
struct RgbaImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;  ///< width * height * 4 bytes: R, G, B, A
};

/// Xbox DXT texture formats as BLACK stores them (not swizzled).
enum class DxtFormat : std::uint32_t {
    Dxt3 = 0x0E,  ///< DXT2/3: explicit 4-bit alpha
    Dxt5 = 0x0F,  ///< DXT4/5: interpolated alpha
};

/// Decodes a DXT3/DXT5 texture. Width and height must be multiples of 4.
[[nodiscard]] Result<RgbaImage> decodeDxt(std::span<const std::uint8_t> data, std::uint32_t width, std::uint32_t height,
                                          std::uint32_t format);

/// One glyph of a game font, in atlas texels.
struct BitmapGlyph {
    char16_t code = 0;
    float u = 0, v = 0, width = 0, height = 0;  ///< the glyph's cell in the atlas (0 x 0 for a space)
    float offsetX = 0, offsetY = 0;             ///< where the cell is drawn relative to the pen
    float advance = 0;                          ///< how far the pen moves
};

/// A BLACK menu font (language/fonts/Small.bin, Big.bin: EA "4.1v" bitmap fonts).
struct BitmapFont {
    RgbaImage atlas;
    std::vector<BitmapGlyph> glyphs;
    /// The text size at which one atlas texel is one unit (256 / 15 for Small, 256 / 30 for Big).
    float nominalSize = 0;
    /// The top of the capitals ('A'), so text is placed by its cap line like the game's menus.
    float capTop = 0;

    /// The glyph for a character; lower-case letters fall back to capitals (the fonts are mostly upper-case).
    [[nodiscard]] const BitmapGlyph* find(char16_t code) const noexcept;
};

/// Parses a "4.1v" font: texture offset at +4, size ratio at +12, 0x28-byte glyphs from +0x220 up to the texture
/// ({u, v, w, h, x, y, advance} as fractions of the atlas, code at +0x1C), the texture header, then DXT pixels.
[[nodiscard]] Result<BitmapFont> parseBitmapFont(std::span<const std::uint8_t> data);

/// Images from an APT bundle (export/FrontEnd/*.bin): the named movie's texture page, cut by its image map.
struct BundleImage {
    std::uint32_t id = 0;
    RgbaImage image;
};
[[nodiscard]] Result<std::vector<BundleImage>> parseBundleImages(std::span<const std::uint8_t> data,
                                                                 std::string_view movie);

/// Art from the player's own disc files, used to make the launcher look like the game. Absent pieces stay empty.
struct GameArt {
    std::optional<BitmapFont> smallFont;  ///< menu rows and help text
    std::optional<BitmapFont> bigFont;    ///< page titles
    std::optional<RgbaImage> logo;        ///< BLACK_Logo.tif, 128 x 16

    [[nodiscard]] bool complete() const noexcept { return smallFont && bigFont && logo; }
};

/// Loads what it can from game/language/fonts and game/export/FrontEnd/Core.bin.
[[nodiscard]] GameArt loadGameArt(const ProjectLayout& project);

}  // namespace black::core

#pragma once

#include "core/ProjectLayout.h"
#include "core/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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

[[nodiscard]] Result<RgbaImage> decodeDxt(std::span<const std::uint8_t> data, std::uint32_t width, std::uint32_t height,
                                          std::uint32_t format);

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
    /// The top of 'A': text is placed by its cap line, like the game's menus.
    float capTop = 0;

    [[nodiscard]] const BitmapGlyph* find(char16_t code) const noexcept;
};

/// Parses a "4.1v" font: texture offset at +4, size ratio at +12, 0x28-byte glyphs from +0x220 up to the texture
/// ({u, v, w, h, x, y, advance} as fractions of the atlas, code at +0x1C), the texture header, then DXT pixels.
[[nodiscard]] Result<BitmapFont> parseBitmapFont(std::span<const std::uint8_t> data);

/// Fonts from the player's disc files; missing ones stay empty.
struct GameArt {
    std::optional<BitmapFont> smallFont;
    std::optional<BitmapFont> bigFont;

    [[nodiscard]] bool complete() const noexcept { return smallFont && bigFont; }
};

[[nodiscard]] GameArt loadGameArt(const ProjectLayout& project);

}  // namespace black::core

#pragma once

#include "core/GameArt.h"

#include "imgui.h"

#include <memory>
#include <optional>
#include <string_view>

namespace Walnut {
class Image;
}

namespace black::app {

/// A typeface for menu text: one of BLACK's own bitmap fonts from the player's disc, or a bundled TrueType font
/// until the disc is in place. Text is placed by the top of its capitals, as the game's menus place it.
class MenuFont {
public:
    MenuFont() = default;
    /// Uploads the font's atlas to the GPU.
    explicit MenuFont(core::BitmapFont font);
    explicit MenuFont(ImFont* font) : m_trueType(font) {}

    [[nodiscard]] bool fromGame() const noexcept { return m_bitmap.has_value(); }
    [[nodiscard]] bool usable() const noexcept { return fromGame() || m_trueType != nullptr; }

    /// Width of `text` (UTF-8) at a cap height of `size` pixels.
    [[nodiscard]] float width(std::string_view text, float size) const;
    /// Draws `text` with the top of its capitals at `capTop`.
    void draw(ImDrawList& list, ImVec2 capTop, std::string_view text, float size, ImU32 color) const;

private:
    std::optional<core::BitmapFont> m_bitmap;
    std::shared_ptr<Walnut::Image> m_atlas;
    ImFont* m_trueType = nullptr;
};

}  // namespace black::app

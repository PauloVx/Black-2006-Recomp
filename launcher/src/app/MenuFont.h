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

/// A game bitmap font or a TrueType stand-in. Text is placed by the top of its capitals, as the game's menus do.
class MenuFont {
public:
    MenuFont() = default;
    explicit MenuFont(core::BitmapFont font);
    explicit MenuFont(ImFont* font) : m_trueType(font) {}

    [[nodiscard]] bool fromGame() const noexcept { return m_bitmap.has_value(); }
    [[nodiscard]] bool usable() const noexcept { return fromGame() || m_trueType != nullptr; }

    /// `size` is the cap height in pixels.
    [[nodiscard]] float width(std::string_view text, float size) const;
    void draw(ImDrawList& list, ImVec2 capTop, std::string_view text, float size, ImU32 color) const;

private:
    std::optional<core::BitmapFont> m_bitmap;
    std::shared_ptr<Walnut::Image> m_atlas;
    ImFont* m_trueType = nullptr;
};

}  // namespace black::app

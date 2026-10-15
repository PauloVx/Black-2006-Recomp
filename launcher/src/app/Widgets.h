#pragma once

#include "app/MenuFont.h"

#include "imgui.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Walnut {
class Image;
}

namespace black::app {

/// One row of a menu page, as the game's own pages show them: a label, an optional value on the right, and the
/// help shown under the list while the row is selected.
struct MenuRow {
    std::string label;
    std::string value;
    std::string help;
    bool enabled = true;
};

/// A vertical menu in the style of BLACK's pages: green-grey rows, the selected one bright with a bar and the
/// selector line running to the edge. Mouse, arrow keys and Enter, like the game.
class MenuList {
public:
    struct Layout {
        ImVec2 origin;    ///< top-left of the first row's text
        float width = 0;  ///< from the text start to the right end of the values
        float rowHeight = 34;
        float textSize = 15;    ///< cap height in pixels
        float selectorEnd = 0;  ///< where the selector line stops (usually the window's right edge)
    };

    /// Draws the rows; returns the index of a row the player activated this frame.
    [[nodiscard]] std::optional<std::size_t> draw(const std::vector<MenuRow>& rows, const MenuFont& font,
                                                  const Layout& layout);
    [[nodiscard]] std::size_t selected() const noexcept { return m_selected; }
    void select(std::size_t row) noexcept { m_selected = row; }

private:
    std::size_t m_selected = 0;
};

/// The ruler-like rails down the left of the game's menu pages.
void drawRails(ImDrawList& list, float x, float top, float bottom);

/// The header of the window: BLACK's logo over a dark, reticle-marked backdrop.
struct BannerArt {
    const Walnut::Image* logo = nullptr;
    const MenuFont* caption = nullptr;
};
void drawBanner(ImDrawList& list, ImVec2 topLeft, ImVec2 size, const BannerArt& art);

/// Text with extra space between letters, as the game sets its captions.
void drawSpaced(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view text, float size, float spacing,
                ImU32 color);
[[nodiscard]] float spacedWidth(const MenuFont& font, std::string_view text, float size, float spacing);

/// A key cap and its action ("ENTER  SELECT"), like the game's button prompts. Returns the width used.
float drawPrompt(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view key, std::string_view action,
                 float size);

/// A square icon cut from a font's glyph, tan on transparent or on the titlebar colour.
struct IconPixels {
    int size = 0;
    std::vector<unsigned char> rgba;
};
[[nodiscard]] IconPixels makeGlyphIcon(ImFont& font, ImWchar glyph, int size, bool background);

}  // namespace black::app

#pragma once

#include "app/MenuFont.h"

#include "imgui.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Walnut {
class Image;
}

namespace black::app {

struct MenuRow {
    std::string label;
    std::string value;
    std::string help;  // shown under the list while the row is selected
    bool enabled = true;
};

struct Page {
    std::string id;  // a new id resets the selection
    std::string title;
    std::function<float(ImDrawList&, ImVec2 topLeft, float width)> body;  // returns its height
    std::vector<MenuRow> rows;
    std::size_t initialRow = 0;
    std::string notice;
    bool noticeIsProblem = false;
};

class MenuList {
public:
    struct Layout {
        ImVec2 origin;  // top-left of the first row's text
        float width = 0;
        float rowHeight = 34;
        float textSize = 15;  // cap height in pixels
        float selectorEnd = 0;
    };

    [[nodiscard]] std::optional<std::size_t> draw(const std::vector<MenuRow>& rows, const MenuFont& font,
                                                  const Layout& layout, bool acceptKeys);
    [[nodiscard]] std::size_t selected() const noexcept { return m_selected; }
    void select(std::size_t row) noexcept { m_selected = row; }

private:
    std::size_t m_selected = 0;
};

void drawRails(ImDrawList& list, float x, float top, float bottom);

struct BannerArt {
    const Walnut::Image* logo = nullptr;
    const MenuFont* caption = nullptr;
};
void drawBanner(ImDrawList& list, ImVec2 topLeft, ImVec2 size, const BannerArt& art);

/// `fraction` in [0, 1]; without one the bar sweeps (work of unknown length).
void drawProgressBar(ImDrawList& list, ImVec2 topLeft, float width, std::optional<float> fraction, double time);

[[nodiscard]] std::vector<std::string> wrapText(const MenuFont& font, std::string_view text, float size,
                                                float maxWidth);
/// Returns the height used.
float drawWrapped(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view text, float size,
                  float maxWidth, ImU32 color);

void drawSpaced(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view text, float size, float spacing,
                ImU32 color);
/// Returns the width used.
float drawPrompt(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view key, std::string_view action,
                 float size);

[[nodiscard]] std::vector<unsigned char> makeWindowIcon(ImFont& font, ImWchar glyph, int size);

}  // namespace black::app

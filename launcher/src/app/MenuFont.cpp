#include "app/MenuFont.h"

#include "core/Text.h"

#include "Walnut/Image.h"

#include <string>

namespace black::app {
namespace {

/// Cap height and cap top of a TrueType font at its atlas size, from its 'H'.
struct TrueTypeCaps {
    float top = 0;
    float height = 1;
};

TrueTypeCaps capsOf(const ImFont& font) {
    if (const ImFontGlyph* glyph = font.FindGlyphNoFallback(static_cast<ImWchar>('H'))) {
        if (glyph->Y1 > glyph->Y0) {
            return {glyph->Y0, glyph->Y1 - glyph->Y0};
        }
    }
    return {font.FontSize * 0.2f, font.FontSize * 0.7f};
}

/// The game's menu fonts are capitals only (lower case maps to them); the stand-in sets text the same way.
std::string capitals(std::string_view text) {
    std::string upper(text);
    for (char& c : upper) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return upper;
}

/// The bitmap font's cap height in atlas texels.
float capHeightOf(const core::BitmapFont& font) {
    const core::BitmapGlyph* capital = font.find(u'A');
    return capital != nullptr && capital->height > 0 ? capital->height : font.nominalSize * 0.7f;
}

}  // namespace

MenuFont::MenuFont(core::BitmapFont font)
    : m_bitmap(std::move(font)),
      m_atlas(std::make_shared<Walnut::Image>(m_bitmap->atlas.width, m_bitmap->atlas.height, Walnut::ImageFormat::RGBA,
                                              m_bitmap->atlas.pixels.data())) {}

float MenuFont::width(std::string_view text, float size) const {
    if (m_bitmap) {
        const float scale = size / capHeightOf(*m_bitmap);
        float pen = 0;
        for (const char16_t code : core::utf8ToUtf16(text)) {
            if (const core::BitmapGlyph* glyph = m_bitmap->find(code)) {
                pen += glyph->advance;
            }
        }
        return pen * scale;
    }
    if (m_trueType == nullptr) {
        return 0;
    }
    const TrueTypeCaps caps = capsOf(*m_trueType);
    const float fontSize = m_trueType->FontSize * size / caps.height;
    const std::string upper = capitals(text);
    return m_trueType->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, upper.c_str()).x;
}

void MenuFont::draw(ImDrawList& list, ImVec2 capTop, std::string_view text, float size, ImU32 color) const {
    if (m_bitmap) {
        const float scale = size / capHeightOf(*m_bitmap);
        const auto atlasWidth = static_cast<float>(m_bitmap->atlas.width);
        const auto atlasHeight = static_cast<float>(m_bitmap->atlas.height);
        const ImTextureID texture = m_atlas->GetDescriptorSet();
        float pen = capTop.x;
        for (const char16_t code : core::utf8ToUtf16(text)) {
            const core::BitmapGlyph* glyph = m_bitmap->find(code);
            if (glyph == nullptr) {
                continue;
            }
            if (glyph->width > 0 && glyph->height > 0) {
                const ImVec2 topLeft(pen + glyph->offsetX * scale,
                                     capTop.y + (glyph->offsetY - m_bitmap->capTop) * scale);
                const ImVec2 bottomRight(topLeft.x + glyph->width * scale, topLeft.y + glyph->height * scale);
                const ImVec2 uv0(glyph->u / atlasWidth, glyph->v / atlasHeight);
                const ImVec2 uv1((glyph->u + glyph->width) / atlasWidth, (glyph->v + glyph->height) / atlasHeight);
                list.AddImage(texture, topLeft, bottomRight, uv0, uv1, color);
            }
            pen += glyph->advance * scale;
        }
        return;
    }
    if (m_trueType == nullptr) {
        return;
    }
    const TrueTypeCaps caps = capsOf(*m_trueType);
    const float scale = size / caps.height;
    const std::string upper = capitals(text);
    list.AddText(m_trueType, m_trueType->FontSize * scale, ImVec2(capTop.x, capTop.y - caps.top * scale), color,
                 upper.c_str());
}

}  // namespace black::app

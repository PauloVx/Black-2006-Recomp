#include "app/MenuFont.h"

#include "core/Text.h"

#include "Walnut/Image.h"

#include <string>

namespace black::app {
namespace {

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

/// The game's menu fonts are capitals only (lower case maps to them); the stand-in sets text the same way,
/// accented Latin-1 letters included.
std::string capitals(std::string_view text) {
    std::string upper(text);
    for (std::size_t i = 0; i < upper.size(); ++i) {
        const auto c = static_cast<unsigned char>(upper[i]);
        if (c >= 'a' && c <= 'z') {
            upper[i] = static_cast<char>(c - 'a' + 'A');
        } else if (c == 0xC3 && i + 1 < upper.size()) {  // UTF-8 for U+00C0..U+00FF
            const auto next = static_cast<unsigned char>(upper[i + 1]);
            if (next >= 0xA0 && next <= 0xBE && next != 0xB7) {  // a-grave..thorn, not the division sign
                upper[i + 1] = static_cast<char>(next - 0x20);
            }
        }
    }
    return upper;
}

/// Letters the game's fonts lack (it never needed them), drawn as their base letter under the tilde of the
/// font's own N-tilde.
char16_t tildeBase(char16_t code) {
    switch (code) {
        case u'\u00C3': return u'A';
        case u'\u00E3': return u'a';
        case u'\u00D5': return u'O';
        case u'\u00F5': return u'o';
        default: return 0;
    }
}

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
            const core::BitmapGlyph* glyph = m_bitmap->find(code);
            if (glyph == nullptr && tildeBase(code) != 0) {
                glyph = m_bitmap->find(tildeBase(code));
            }
            if (glyph != nullptr) {
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
        const auto place = [&](const core::BitmapGlyph& glyph, ImVec2 origin, float glyphScale) {
            if (glyph.width <= 0 || glyph.height <= 0) {
                return;
            }
            const ImVec2 topLeft(origin.x + glyph.offsetX * glyphScale,
                                 origin.y + (glyph.offsetY - m_bitmap->capTop) * glyphScale);
            const ImVec2 bottomRight(topLeft.x + glyph.width * glyphScale, topLeft.y + glyph.height * glyphScale);
            const ImVec2 uv0(glyph.u / atlasWidth, glyph.v / atlasHeight);
            const ImVec2 uv1((glyph.u + glyph.width) / atlasWidth, (glyph.v + glyph.height) / atlasHeight);
            list.AddImage(texture, topLeft, bottomRight, uv0, uv1, color);
        };
        const core::BitmapGlyph* withTilde = m_bitmap->find(u'\u00D1');
        float pen = capTop.x;
        for (const char16_t code : core::utf8ToUtf16(text)) {
            const core::BitmapGlyph* glyph = m_bitmap->find(code);
            if (glyph == nullptr && tildeBase(code) != 0) {
                glyph = m_bitmap->find(tildeBase(code));
                if (glyph != nullptr && withTilde != nullptr) {
                    // Only the part of the N-tilde above the cap line, centred over the letter
                    const float x = pen + (glyph->advance - withTilde->advance) * scale * 0.5f;
                    list.PushClipRect(ImVec2(x - size, capTop.y - size), ImVec2(x + size * 2.0f, capTop.y - 0.5f),
                                      true);
                    place(*withTilde, ImVec2(x, capTop.y), scale);
                    list.PopClipRect();
                }
            }
            if (glyph == nullptr) {
                continue;
            }
            place(*glyph, ImVec2(pen, capTop.y), scale);
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

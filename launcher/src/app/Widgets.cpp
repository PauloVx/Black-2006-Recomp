#include "app/Widgets.h"

#include "app/Theme.h"

#include "Walnut/Image.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace black::app {

std::optional<std::size_t> MenuList::draw(const std::vector<MenuRow>& rows, const MenuFont& font, const Layout& layout,
                                          bool acceptKeys) {
    if (rows.empty()) {
        return std::nullopt;
    }
    m_selected = std::min(m_selected, rows.size() - 1);
    std::optional<std::size_t> activated;

    if (acceptKeys) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_selected = (m_selected + 1) % rows.size();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_selected = (m_selected + rows.size() - 1) % rows.size();
        }
        const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
                             ImGui::IsKeyPressed(ImGuiKey_Space);
        if (confirm && rows[m_selected].enabled) {
            activated = m_selected;
        }
    }

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const float barPadding = 12.0f;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const MenuRow& row = rows[i];
        const ImVec2 rowMin(layout.origin.x - barPadding, layout.origin.y + static_cast<float>(i) * layout.rowHeight);
        const ImVec2 rowMax(layout.origin.x + layout.width + barPadding, rowMin.y + layout.rowHeight);

        ImGui::SetCursorScreenPos(rowMin);
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("row", ImVec2(rowMax.x - rowMin.x, layout.rowHeight));
        ImGui::PopID();
        const ImVec2 mouseMove = ImGui::GetIO().MouseDelta;
        if (ImGui::IsItemHovered() && (mouseMove.x != 0.0f || mouseMove.y != 0.0f)) {
            m_selected = i;
        }
        if (ImGui::IsItemClicked() && row.enabled) {
            m_selected = i;
            activated = i;
        }

        const bool selected = i == m_selected;
        const float textTop = rowMin.y + (layout.rowHeight - layout.textSize) * 0.5f;
        if (selected) {
            list.AddRectFilled(rowMin, rowMax, toU32(colors::kText, 0.055f));
            list.AddRectFilled(rowMin, ImVec2(rowMin.x + 3.0f, rowMax.y), toU32(colors::kRail, 0.9f));
            const float midY = std::floor((rowMin.y + rowMax.y) * 0.5f);
            if (layout.selectorEnd > rowMax.x) {
                list.AddLine(ImVec2(rowMax.x, midY), ImVec2(layout.selectorEnd, midY), toU32(colors::kRail, 0.45f));
            }
        }
        const ImU32 color = !row.enabled ? toU32(colors::kMuted)
                            : selected   ? toU32(colors::kText)
                                         : toU32(colors::kRow);
        font.draw(list, ImVec2(layout.origin.x, textTop), row.label, layout.textSize, color);
        if (!row.value.empty()) {
            const float valueWidth = font.width(row.value, layout.textSize);
            font.draw(list, ImVec2(layout.origin.x + layout.width - valueWidth, textTop), row.value, layout.textSize,
                      color);
        }
    }
    return activated;
}

void drawRails(ImDrawList& list, float x, float top, float bottom) {
    const ImU32 strong = toU32(colors::kRail, 0.55f);
    const ImU32 faint = toU32(colors::kRail, 0.22f);
    list.AddLine(ImVec2(x, top), ImVec2(x, bottom), strong);
    list.AddLine(ImVec2(x + 7.0f, top), ImVec2(x + 7.0f, bottom), faint);
    list.AddLine(ImVec2(x + 10.0f, top), ImVec2(x + 10.0f, bottom), faint);
    int tick = 0;
    for (float y = top + 6.0f; y < bottom - 2.0f; y += 8.0f, ++tick) {
        const float length = tick % 5 == 0 ? 10.0f : 5.0f;
        list.AddLine(ImVec2(x - length, y), ImVec2(x, y), tick % 5 == 0 ? strong : faint);
    }
}

void drawSpaced(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view text, float size, float spacing,
                ImU32 color) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        const std::string_view letter = text.substr(i, 1);
        font.draw(list, capTop, letter, size, color);
        capTop.x += font.width(letter, size) + spacing;
    }
}

void drawBanner(ImDrawList& list, ImVec2 topLeft, ImVec2 size, const BannerArt& art) {
    const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);
    list.PushClipRect(topLeft, bottomRight, true);

    const ImU32 top = toU32(ImVec4(0.086f, 0.122f, 0.114f, 1.0f));
    const ImU32 bottom = toU32(colors::kBackground);
    list.AddRectFilledMultiColor(topLeft, bottomRight, top, top, bottom, bottom);

    const ImU32 hatch = toU32(colors::kRail, 0.035f);
    for (float x = topLeft.x + size.x * 0.42f; x < bottomRight.x + size.y; x += 7.0f) {
        list.AddLine(ImVec2(x, topLeft.y), ImVec2(x - size.y, bottomRight.y), hatch);
    }

    const ImVec2 centre(topLeft.x + size.x * 0.80f, topLeft.y + size.y * 0.52f);
    const float radius = size.y * 0.78f;
    const ImU32 reticle = toU32(colors::kRail, 0.10f);
    list.AddCircle(centre, radius, reticle, 96, 1.0f);
    list.AddCircle(centre, radius * 0.62f, reticle, 96, 1.0f);
    list.AddLine(ImVec2(centre.x - radius - 30.0f, centre.y), ImVec2(centre.x + radius + 30.0f, centre.y), reticle);
    list.AddLine(ImVec2(centre.x, centre.y - radius - 30.0f), ImVec2(centre.x, centre.y + radius + 30.0f), reticle);
    for (int i = 0; i < 72; ++i) {
        const float angle = static_cast<float>(i) * std::numbers::pi_v<float> * 2.0f / 72.0f;
        const float inner = radius - (i % 6 == 0 ? 12.0f : 5.0f);
        list.AddLine(ImVec2(centre.x + std::cos(angle) * inner, centre.y + std::sin(angle) * inner),
                     ImVec2(centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius), reticle);
    }

    const float logoHeight = std::round(size.y * 0.44f);
    const ImVec2 at(topLeft.x + 60.0f, topLeft.y + (size.y - logoHeight) * 0.5f - 14.0f);
    float logoBottom = at.y + logoHeight;
    if (art.logo != nullptr) {
        const float scale = logoHeight / static_cast<float>(art.logo->GetHeight());
        const ImVec2 logoSize(static_cast<float>(art.logo->GetWidth()) * scale, logoHeight);
        const ImTextureID texture = art.logo->GetDescriptorSet();
        list.AddImage(texture, ImVec2(at.x + 4.0f, at.y + 5.0f),
                      ImVec2(at.x + 4.0f + logoSize.x, at.y + 5.0f + logoSize.y), ImVec2(0, 0), ImVec2(1, 1),
                      toU32(ImVec4(0, 0, 0, 0.55f)));
        list.AddImage(texture, at, ImVec2(at.x + logoSize.x, at.y + logoSize.y));
        logoBottom = at.y + logoSize.y;
    }
    if (art.caption != nullptr && art.caption->usable()) {
        drawSpaced(list, *art.caption, ImVec2(at.x + 6.0f, logoBottom + 18.0f), "XBOX 2006  /  RECOMPILED FOR PC",
                   11.0f, 3.2f, toU32(colors::kRow));
    }

    const ImU32 rule = toU32(colors::kRail, 0.30f);
    list.AddLine(ImVec2(topLeft.x, bottomRight.y - 1.0f), ImVec2(bottomRight.x, bottomRight.y - 1.0f), rule);
    int tick = 0;
    for (float x = topLeft.x + 24.0f; x < bottomRight.x; x += 12.0f, ++tick) {
        list.AddLine(ImVec2(x, bottomRight.y - (tick % 5 == 0 ? 9.0f : 4.0f)), ImVec2(x, bottomRight.y - 1.0f), rule);
    }
    list.PopClipRect();
}

void drawProgressBar(ImDrawList& list, ImVec2 topLeft, float width, std::optional<float> fraction, double time) {
    constexpr float kHeight = 14.0f;
    const ImVec2 bottomRight(topLeft.x + width, topLeft.y + kHeight);
    list.AddRectFilled(topLeft, bottomRight, toU32(colors::kRail, 0.08f));
    list.AddRect(topLeft, bottomRight, toU32(colors::kRail, 0.45f));
    const ImVec2 innerMin(topLeft.x + 3.0f, topLeft.y + 3.0f);
    const float innerWidth = width - 6.0f;
    const float innerBottom = bottomRight.y - 3.0f;
    if (fraction) {
        const float filled = innerWidth * std::clamp(*fraction, 0.0f, 1.0f);
        list.AddRectFilled(innerMin, ImVec2(innerMin.x + filled, innerBottom), toU32(colors::kLogo));
    } else {
        const float segment = innerWidth * 0.22f;
        const auto phase = static_cast<float>(std::fmod(time * 0.6, 1.0));
        const float start = innerMin.x - segment + (innerWidth + segment) * phase;
        list.PushClipRect(innerMin, ImVec2(innerMin.x + innerWidth, innerBottom), true);
        list.AddRectFilled(ImVec2(start, innerMin.y), ImVec2(start + segment, innerBottom), toU32(colors::kLogo, 0.8f));
        list.PopClipRect();
    }
    for (int i = 1; i < 10; ++i) {
        const float x = std::round(topLeft.x + width * static_cast<float>(i) / 10.0f);
        list.AddLine(ImVec2(x, bottomRight.y + 2.0f), ImVec2(x, bottomRight.y + (i == 5 ? 8.0f : 5.0f)),
                     toU32(colors::kRail, 0.35f));
    }
}

std::vector<std::string> wrapText(const MenuFont& font, std::string_view text, float size, float maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find(' ', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        const std::string word(text.substr(start, end - start));
        const std::string candidate = line.empty() ? word : line + ' ' + word;
        if (!line.empty() && font.width(candidate, size) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        start = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

float drawWrapped(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view text, float size,
                  float maxWidth, ImU32 color) {
    const float lineHeight = size + std::round(size * 0.75f);
    float y = capTop.y;
    for (const std::string& line : wrapText(font, text, size, maxWidth)) {
        font.draw(list, ImVec2(capTop.x, y), line, size, color);
        y += lineHeight;
    }
    return y - capTop.y;
}

float drawPrompt(ImDrawList& list, const MenuFont& font, ImVec2 capTop, std::string_view key, std::string_view action,
                 float size) {
    const float padX = 6.0f;
    const float keyWidth = std::max(font.width(key, size) + padX * 2.0f, size + padX * 2.0f);
    const ImVec2 capMin(capTop.x, capTop.y - 5.0f);
    const ImVec2 capMax(capTop.x + keyWidth, capTop.y + size + 5.0f);
    list.AddRectFilled(capMin, capMax, toU32(colors::kRail, 0.16f), 3.0f);
    list.AddRect(capMin, capMax, toU32(colors::kRail, 0.55f), 3.0f);
    font.draw(list, ImVec2(capTop.x + (keyWidth - font.width(key, size)) * 0.5f, capTop.y), key, size,
              toU32(colors::kText));
    const float actionX = capMax.x + 8.0f;
    font.draw(list, ImVec2(actionX, capTop.y), action, size, toU32(colors::kText));
    return actionX - capTop.x + font.width(action, size);
}

std::vector<unsigned char> makeWindowIcon(ImFont& font, ImWchar glyph, int size) {
    const auto side = static_cast<std::size_t>(size);
    std::vector<unsigned char> rgba(side * side * 4);
    unsigned char* atlas = nullptr;
    int atlasWidth = 0;
    int atlasHeight = 0;
    font.ContainerAtlas->GetTexDataAsRGBA32(&atlas, &atlasWidth, &atlasHeight);
    const ImFontGlyph* source = font.FindGlyphNoFallback(glyph);
    if (atlas == nullptr || source == nullptr) {
        return rgba;
    }

    const float u0 = source->U0 * static_cast<float>(atlasWidth);
    const float v0 = source->V0 * static_cast<float>(atlasHeight);
    const float cellWidth = (source->U1 - source->U0) * static_cast<float>(atlasWidth);
    const float cellHeight = (source->V1 - source->V0) * static_cast<float>(atlasHeight);
    const float margin = static_cast<float>(size) * 0.16f;
    const float scale = (static_cast<float>(size) - margin * 2.0f) / std::max(cellWidth, cellHeight);
    const float offsetX = (static_cast<float>(size) - cellWidth * scale) * 0.5f;
    const float offsetY = (static_cast<float>(size) - cellHeight * scale) * 0.5f;
    const ImVec4 tan = colors::kLogo;
    const ImVec4 back = colors::kTitlebar;

    for (std::size_t y = 0; y < side; ++y) {
        for (std::size_t x = 0; x < side; ++x) {
            float coverage = 0;  // 4 x 4 samples per icon pixel
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const float gx = (static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / 4.0f - offsetX) / scale;
                    const float gy = (static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / 4.0f - offsetY) / scale;
                    if (gx >= 0 && gy >= 0 && gx < cellWidth && gy < cellHeight) {
                        const auto ax = static_cast<std::size_t>(u0 + gx);
                        const auto ay = static_cast<std::size_t>(v0 + gy);
                        coverage += static_cast<float>(atlas[(ay * static_cast<std::size_t>(atlasWidth) + ax) * 4 + 3]);
                    }
                }
            }
            coverage /= 16.0f * 255.0f;
            const auto mix = [coverage](float fg, float bg) {
                return static_cast<unsigned char>(std::lround((fg * coverage + bg * (1.0f - coverage)) * 255.0f));
            };
            unsigned char* out = &rgba[(y * side + x) * 4];
            out[0] = mix(tan.x, back.x);
            out[1] = mix(tan.y, back.y);
            out[2] = mix(tan.z, back.z);
            out[3] = 255;
        }
    }
    return rgba;
}

}  // namespace black::app

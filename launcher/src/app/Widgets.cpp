#include "app/Widgets.h"

#include "app/Theme.h"
#include "app/Translations.h"

#include "Walnut/Image.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace black::app {

std::optional<MenuEvent> MenuList::draw(const std::vector<MenuRow>& rows, const MenuFont& font, const Layout& layout,
                                        bool acceptInput) {
    m_shown = 0;
    if (rows.empty()) {
        return std::nullopt;
    }
    m_selected = std::min(m_selected, rows.size() - 1);
    std::optional<MenuEvent> event;

    if (acceptInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_selected = (m_selected + 1) % rows.size();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_selected = (m_selected + rows.size() - 1) % rows.size();
        }
        const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
                             ImGui::IsKeyPressed(ImGuiKey_Space);
        const MenuRow& current = rows[m_selected];
        if (confirm && current.enabled) {
            event = MenuEvent{m_selected, 0};
        } else if (current.enabled && current.adjustable && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            event = MenuEvent{m_selected, -1};
        } else if (current.enabled && current.adjustable && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            event = MenuEvent{m_selected, 1};
        }
    }

    const std::size_t visible = layout.visibleRows == 0 ? rows.size() : std::min(layout.visibleRows, rows.size());
    const float barPadding = 12.0f;
    const ImVec2 listMin(layout.origin.x - barPadding, layout.origin.y);
    const ImVec2 listMax(layout.origin.x + layout.width + barPadding,
                         layout.origin.y + static_cast<float>(visible) * layout.rowHeight);
    if (acceptInput && visible < rows.size() && ImGui::IsMouseHoveringRect(listMin, listMax)) {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel > 0.0f && m_first > 0) {
            --m_first;
        } else if (wheel < 0.0f && m_first + visible < rows.size()) {
            ++m_first;
        }
        m_selected = std::clamp(m_selected, m_first, m_first + visible - 1);
    }
    if (m_selected < m_first) {
        m_first = m_selected;
    } else if (m_selected >= m_first + visible) {
        m_first = m_selected + 1 - visible;
    }
    m_first = std::min(m_first, rows.size() - visible);
    m_shown = visible;

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const float arrow = layout.textSize * 0.42f;
    for (std::size_t slot = 0; slot < visible; ++slot) {
        const std::size_t i = m_first + slot;
        const MenuRow& row = rows[i];
        const ImVec2 rowMin(listMin.x, layout.origin.y + static_cast<float>(slot) * layout.rowHeight);
        const ImVec2 rowMax(listMax.x, rowMin.y + layout.rowHeight);
        const bool selected = i == m_selected;
        const bool arrows = row.adjustable && row.enabled && selected;
        const float valueWidth = row.value.empty() ? 0.0f : font.width(row.value, layout.textSize);
        const float valueRight = layout.origin.x + layout.width - (arrows ? arrow * 2.0f + 10.0f : 0.0f);
        const float valueLeft = valueRight - valueWidth;

        ImGui::SetCursorScreenPos(rowMin);
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("row", ImVec2(rowMax.x - rowMin.x, layout.rowHeight),
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        ImGui::PopID();
        const ImVec2 mouseMove = ImGui::GetIO().MouseDelta;
        if (acceptInput && ImGui::IsItemHovered() && (mouseMove.x != 0.0f || mouseMove.y != 0.0f)) {
            m_selected = i;
        }
        if (acceptInput && row.enabled && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_selected = i;
            const float x = ImGui::GetIO().MousePos.x;
            const bool onLeftArrow = row.adjustable && x < valueLeft - 2.0f && x > valueLeft - arrow * 2.0f - 24.0f;
            event = MenuEvent{i, onLeftArrow ? -1 : (row.adjustable ? 1 : 0)};
        } else if (acceptInput && row.enabled && row.adjustable && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            m_selected = i;
            event = MenuEvent{i, -1};
        }

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
            font.draw(list, ImVec2(valueLeft, textTop), row.value, layout.textSize,
                      row.adjustable && !selected && row.enabled ? toU32(colors::kLogo) : color);
        }
        for (std::size_t cell = 0; cell < row.cells.size(); ++cell) {
            const float right =
                layout.origin.x + layout.width - layout.cellWidth * static_cast<float>(row.cells.size() - 1 - cell);
            const float left = right - layout.cellWidth + 12.0f;
            const std::string& text = row.cells[cell];
            const float x = (left + right - font.width(text, layout.textSize)) * 0.5f;
            if (static_cast<int>(cell) == row.markedCell) {
                list.AddRect(ImVec2(left, rowMin.y + 4.0f), ImVec2(right, rowMax.y - 4.0f), toU32(colors::kLogo), 2.0f);
            }
            font.draw(list, ImVec2(x, textTop), text, layout.textSize,
                      static_cast<int>(cell) == row.markedCell ? toU32(colors::kLogo) : color);
        }
        if (arrows) {
            const float midY = textTop + layout.textSize * 0.5f;
            const float leftTip = valueLeft - 10.0f - arrow * 2.0f;
            const float rightTip = valueRight + 10.0f + arrow * 2.0f;
            list.AddTriangleFilled(ImVec2(leftTip, midY), ImVec2(leftTip + arrow * 2.0f, midY + arrow),
                                   ImVec2(leftTip + arrow * 2.0f, midY - arrow), toU32(colors::kLogo));
            list.AddTriangleFilled(ImVec2(rightTip, midY), ImVec2(rightTip - arrow * 2.0f, midY - arrow),
                                   ImVec2(rightTip - arrow * 2.0f, midY + arrow), toU32(colors::kLogo));
        }
    }

    if (visible < rows.size()) {  // a scroll track beside long lists
        const float x = listMax.x + 10.0f;
        const float height = listMax.y - listMin.y;
        list.AddLine(ImVec2(x, listMin.y), ImVec2(x, listMax.y), toU32(colors::kRail, 0.22f));
        const float thumbTop = listMin.y + height * static_cast<float>(m_first) / static_cast<float>(rows.size());
        const float thumbHeight = height * static_cast<float>(visible) / static_cast<float>(rows.size());
        list.AddRectFilled(ImVec2(x - 1.5f, thumbTop), ImVec2(x + 1.5f, thumbTop + thumbHeight),
                           toU32(colors::kRail, 0.8f));
    }
    return event;
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
        drawSpaced(list, *art.caption, ImVec2(at.x + 6.0f, logoBottom + 18.0f), tr("XBOX 2006  /  RECOMPILED FOR PC"),
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

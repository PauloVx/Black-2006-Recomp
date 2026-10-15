#include "app/TextEntry.h"

#include "imgui.h"

#include <cmath>
#include <utility>

namespace black::app {

void TextEntry::begin(std::string text, std::string allowed, std::size_t maxLength) {
    m_active = true;
    m_text = std::move(text);
    m_allowed = std::move(allowed);
    m_maxLength = maxLength;
}

bool TextEntry::update() {
    if (!m_active) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    for (const ImWchar c : io.InputQueueCharacters) {
        const bool printable = c >= 0x20 && c <= 0x7E;
        const bool allowed = m_allowed.empty() || m_allowed.find(static_cast<char>(c)) != std::string::npos;
        if (printable && allowed && m_text.size() < m_maxLength) {
            m_text.push_back(static_cast<char>(c));
        }
    }
    io.InputQueueCharacters.resize(0);
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !m_text.empty()) {
        m_text.pop_back();
    }
    return ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
}

std::string TextEntry::shown(double time) const {
    return m_text + (std::fmod(time, 1.0) < 0.5 ? "_" : " ");
}

}  // namespace black::app

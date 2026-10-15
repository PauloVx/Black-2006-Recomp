#pragma once

#include <cstddef>
#include <string>

namespace black::app {

/// Typing a value into a menu row: characters and Backspace edit it, Enter confirms.
class TextEntry {
public:
    /// `allowed`: the characters that can be typed; empty allows any printable ASCII.
    void begin(std::string text, std::string allowed, std::size_t maxLength);
    void end() noexcept { m_active = false; }
    [[nodiscard]] bool active() const noexcept { return m_active; }
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    /// Reads this frame's typing. True when Enter was pressed.
    bool update();
    /// The text with a blinking cursor.
    [[nodiscard]] std::string shown(double time) const;

private:
    bool m_active = false;
    std::string m_text;
    std::string m_allowed;
    std::size_t m_maxLength = 0;
};

}  // namespace black::app

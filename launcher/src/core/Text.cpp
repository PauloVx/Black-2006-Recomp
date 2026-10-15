#include "core/Text.h"

#include <cstdint>

namespace black::core {

std::u16string utf8ToUtf16(std::string_view text) {
    constexpr char32_t kReplacement = 0xFFFD;
    std::u16string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<std::uint8_t>(text[i]);
        std::size_t length = 0;
        char32_t code = 0;
        if (lead < 0x80) {
            length = 1;
            code = lead;
        } else if ((lead & 0xE0U) == 0xC0) {
            length = 2;
            code = lead & 0x1FU;
        } else if ((lead & 0xF0U) == 0xE0) {
            length = 3;
            code = lead & 0x0FU;
        } else if ((lead & 0xF8U) == 0xF0) {
            length = 4;
            code = lead & 0x07U;
        }
        bool valid = length != 0 && i + length <= text.size();
        for (std::size_t k = 1; valid && k < length; ++k) {
            const auto next = static_cast<std::uint8_t>(text[i + k]);
            valid = (next & 0xC0U) == 0x80;
            code = (code << 6) | (next & 0x3FU);
        }
        // Reject overlong forms, surrogates and values past U+10FFFF.
        constexpr char32_t kMinimum[] = {0, 0, 0x80, 0x800, 0x10000};
        valid = valid && code >= kMinimum[length] && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF);
        if (!valid) {
            out.push_back(static_cast<char16_t>(kReplacement));
            ++i;
            continue;
        }
        if (code >= 0x10000) {
            code -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (code >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (code & 0x3FFU)));
        } else {
            out.push_back(static_cast<char16_t>(code));
        }
        i += length;
    }
    return out;
}

}  // namespace black::core

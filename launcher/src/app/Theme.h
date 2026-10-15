#pragma once

#include "imgui.h"

namespace black::app {

/// Colours sampled from BLACK's own menus (the in-game Video Settings page).
namespace colors {
inline const ImVec4 kBackground{0.051f, 0.075f, 0.071f, 1.0f};
inline const ImVec4 kTitlebar{0.035f, 0.051f, 0.047f, 1.0f};
inline const ImVec4 kPanel{0.075f, 0.106f, 0.098f, 1.0f};
inline const ImVec4 kPanelHover{0.110f, 0.149f, 0.137f, 1.0f};
inline const ImVec4 kText{0.925f, 0.925f, 0.910f, 1.0f};
inline const ImVec4 kRow{0.424f, 0.557f, 0.478f, 1.0f};
inline const ImVec4 kMuted{0.333f, 0.412f, 0.380f, 1.0f};
inline const ImVec4 kRail{0.588f, 0.620f, 0.569f, 1.0f};
inline const ImVec4 kLogo{0.784f, 0.729f, 0.612f, 1.0f};
inline const ImVec4 kGood{0.553f, 0.741f, 0.502f, 1.0f};
inline const ImVec4 kBad{0.851f, 0.373f, 0.341f, 1.0f};
}  // namespace colors

[[nodiscard]] inline ImU32 toU32(const ImVec4& color, float alpha = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w * alpha));
}

void applyTheme();

}  // namespace black::app

#include "app/Theme.h"

namespace black::app {

void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(14.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = colors::kBackground;
    c[ImGuiCol_ChildBg] = colors::kBackground;
    c[ImGuiCol_PopupBg] = colors::kPanel;
    c[ImGuiCol_Text] = colors::kText;
    c[ImGuiCol_TextDisabled] = colors::kMuted;
    c[ImGuiCol_Border] = colors::kPanelHover;
    c[ImGuiCol_Separator] = colors::kPanelHover;
    c[ImGuiCol_FrameBg] = colors::kPanel;
    c[ImGuiCol_FrameBgHovered] = colors::kPanelHover;
    c[ImGuiCol_FrameBgActive] = colors::kPanelHover;
    c[ImGuiCol_Button] = colors::kPanel;
    c[ImGuiCol_ButtonHovered] = colors::kPanelHover;
    c[ImGuiCol_ButtonActive] = colors::kPanelHover;
    c[ImGuiCol_Header] = colors::kPanel;
    c[ImGuiCol_HeaderHovered] = colors::kPanelHover;
    c[ImGuiCol_HeaderActive] = colors::kPanelHover;
    c[ImGuiCol_CheckMark] = colors::kText;
    c[ImGuiCol_SliderGrab] = colors::kRail;
    c[ImGuiCol_SliderGrabActive] = colors::kText;
    c[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0, 0);  // menu rows draw their own selection
}

}  // namespace black::app

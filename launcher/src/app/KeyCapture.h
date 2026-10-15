#pragma once

#include <optional>
#include <string>

struct GLFWwindow;

namespace black::app {

/// Reads keys for a binding by their position (GLFW's key codes follow the US layout's positions on every layout),
/// so a binding means the same physical key as in the game. While capturing, presses do not reach the menus.
void installKeyCapture(GLFWwindow* window);
void beginKeyCapture();
void endKeyCapture();

struct CapturedKey {
    enum class Kind { Key, Cancel, Reserved, Unbindable };
    Kind kind = Kind::Key;
    std::string token;  // for Kind::Key
};
[[nodiscard]] std::optional<CapturedKey> takeCapturedKey();

}  // namespace black::app

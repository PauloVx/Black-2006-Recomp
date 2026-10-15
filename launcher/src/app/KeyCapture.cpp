#include "app/KeyCapture.h"

#include <GLFW/glfw3.h>
#include "backends/imgui_impl_glfw.h"

#include <array>
#include <mutex>
#include <utility>

namespace black::app {
namespace {

struct Mapping {
    int key;
    const char* token;
};

constexpr std::array kKeys = {
    Mapping{GLFW_KEY_SPACE, "Space"},
    Mapping{GLFW_KEY_APOSTROPHE, "Quote"},
    Mapping{GLFW_KEY_COMMA, "Comma"},
    Mapping{GLFW_KEY_MINUS, "Minus"},
    Mapping{GLFW_KEY_PERIOD, "Period"},
    Mapping{GLFW_KEY_SLASH, "Slash"},
    Mapping{GLFW_KEY_SEMICOLON, "Semicolon"},
    Mapping{GLFW_KEY_EQUAL, "Equals"},
    Mapping{GLFW_KEY_LEFT_BRACKET, "LBracket"},
    Mapping{GLFW_KEY_BACKSLASH, "Backslash"},
    Mapping{GLFW_KEY_RIGHT_BRACKET, "RBracket"},
    Mapping{GLFW_KEY_GRAVE_ACCENT, "Grave"},
    Mapping{GLFW_KEY_ENTER, "Enter"},
    Mapping{GLFW_KEY_TAB, "Tab"},
    Mapping{GLFW_KEY_BACKSPACE, "Backspace"},
    Mapping{GLFW_KEY_INSERT, "Insert"},
    Mapping{GLFW_KEY_DELETE, "Delete"},
    Mapping{GLFW_KEY_RIGHT, "Right"},
    Mapping{GLFW_KEY_LEFT, "Left"},
    Mapping{GLFW_KEY_DOWN, "Down"},
    Mapping{GLFW_KEY_UP, "Up"},
    Mapping{GLFW_KEY_PAGE_UP, "PageUp"},
    Mapping{GLFW_KEY_PAGE_DOWN, "PageDown"},
    Mapping{GLFW_KEY_HOME, "Home"},
    Mapping{GLFW_KEY_END, "End"},
    Mapping{GLFW_KEY_CAPS_LOCK, "CapsLock"},
    Mapping{GLFW_KEY_F12, "F12"},
    Mapping{GLFW_KEY_KP_DECIMAL, "NumpadDecimal"},
    Mapping{GLFW_KEY_KP_DIVIDE, "NumpadDivide"},
    Mapping{GLFW_KEY_KP_MULTIPLY, "NumpadMultiply"},
    Mapping{GLFW_KEY_KP_SUBTRACT, "NumpadMinus"},
    Mapping{GLFW_KEY_KP_ADD, "NumpadPlus"},
    Mapping{GLFW_KEY_KP_ENTER, "NumpadEnter"},
    Mapping{GLFW_KEY_LEFT_SHIFT, "LShift"},
    Mapping{GLFW_KEY_LEFT_CONTROL, "LCtrl"},
    Mapping{GLFW_KEY_LEFT_ALT, "LAlt"},
    Mapping{GLFW_KEY_RIGHT_SHIFT, "RShift"},
    Mapping{GLFW_KEY_RIGHT_CONTROL, "RCtrl"},
    Mapping{GLFW_KEY_RIGHT_ALT, "RAlt"},
};

std::mutex g_mutex;
bool g_capturing = false;
std::optional<CapturedKey> g_captured;

std::optional<std::string> tokenFor(int key) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        return std::string(1, static_cast<char>('A' + (key - GLFW_KEY_A)));
    }
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        return std::string(1, static_cast<char>('0' + (key - GLFW_KEY_0)));
    }
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F10) {
        return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
    }
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
        return "Numpad" + std::to_string(key - GLFW_KEY_KP_0);
    }
    for (const Mapping& mapping : kKeys) {
        if (mapping.key == key) {
            return mapping.token;
        }
    }
    return std::nullopt;
}

void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
    {
        const std::scoped_lock lock(g_mutex);
        if (g_capturing && action == GLFW_PRESS) {
            if (key == GLFW_KEY_ESCAPE) {
                g_captured = CapturedKey{CapturedKey::Kind::Cancel, {}};
            } else if (key == GLFW_KEY_F11) {
                g_captured = CapturedKey{CapturedKey::Kind::Reserved, {}};
            } else if (const auto token = tokenFor(key)) {
                g_captured = CapturedKey{CapturedKey::Kind::Key, *token};
            } else {
                g_captured = CapturedKey{CapturedKey::Kind::Unbindable, {}};
            }
            return;
        }
        if (g_capturing && action == GLFW_REPEAT) {
            return;
        }
    }
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

}  // namespace

void installKeyCapture(GLFWwindow* window) {
    glfwSetKeyCallback(window, onKey);
}

void beginKeyCapture() {
    const std::scoped_lock lock(g_mutex);
    g_capturing = true;
    g_captured.reset();
}

void endKeyCapture() {
    const std::scoped_lock lock(g_mutex);
    g_capturing = false;
    g_captured.reset();
}

std::optional<CapturedKey> takeCapturedKey() {
    const std::scoped_lock lock(g_mutex);
    return std::exchange(g_captured, std::nullopt);
}

}  // namespace black::app

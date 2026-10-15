#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace black::app {

/// Block until the player answers; nothing means cancelled (or no zenity/kdialog on Linux).
[[nodiscard]] std::optional<std::filesystem::path> chooseDiscImage(GLFWwindow* owner);
[[nodiscard]] std::optional<std::filesystem::path> chooseFolder(GLFWwindow* owner);

void acceptDroppedFiles(GLFWwindow* window);
[[nodiscard]] std::vector<std::filesystem::path> takeDroppedFiles();

}  // namespace black::app

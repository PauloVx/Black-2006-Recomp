# BLACK Launcher: sets up, builds and starts BLACK for players, in C++20 on Walnut (Vulkan + Dear ImGui).
#
#   cmake --preset windows-release   (or linux-release)
#   cmake --build --preset windows-release
#   ctest --preset windows-release
#
# Needs only a C++20 compiler and CMake: the Vulkan headers are vendored and it links against the Vulkan loader
# that graphics drivers install, which is all players need too.
cmake_minimum_required(VERSION 3.21)
project(BlackLauncher VERSION 0.1.0 LANGUAGES C CXX)

# The release version (the repository's VERSION file), shared with the game
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" BLACK_VERSION LIMIT_COUNT 1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <format>
int main() { return std::format(\"{}\", 1).empty(); }" BLACK_LAUNCHER_HAS_STD_FORMAT)
if(NOT BLACK_LAUNCHER_HAS_STD_FORMAT)
    message(FATAL_ERROR "BLACK Launcher needs C++20 std::format support (GCC 13 or newer, or Visual Studio 2022).")
endif()

option(BLACK_LAUNCHER_TESTS "Build the launcher unit tests" ON)
option(BLACK_LAUNCHER_WARNINGS_AS_ERRORS "Treat warnings in launcher code as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LauncherWarnings)
include(LauncherDependencies)

add_subdirectory(src/core)
add_subdirectory(src/app)

if(BLACK_LAUNCHER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

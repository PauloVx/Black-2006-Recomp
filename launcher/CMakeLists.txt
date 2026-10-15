# BLACK Launcher: sets up, builds and starts BLACK for players, in C++20 on Walnut (Vulkan + Dear ImGui).
#
#   cmake --preset windows-release   (or linux-release)
#   cmake --build --preset windows-release
#   ctest --preset windows-release
#
# Needs the Vulkan headers and loader to build (the Vulkan SDK on Windows, libvulkan-dev on Linux).
# Players only need a Vulkan-capable graphics driver.
cmake_minimum_required(VERSION 3.21)
project(BlackLauncher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

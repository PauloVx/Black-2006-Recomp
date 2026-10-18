# Third-party code, all pinned submodules under launcher/vendor:
#   walnut   PauloVx/Walnut, branch black (StudioCherno/Walnut dev: custom titlebar; embedding and Linux fixes)
#   imgui    TheCherno/imgui at the docking + stack-layout commit Walnut is written against (1.87)
#   glfw     TheCherno/glfw at the commit Walnut dev pins (Win32 custom titlebar hit testing; X11/Wayland as GLFW)
#   glm      g-truc/glm 1.0.1
#   spdlog   gabime/spdlog at the commit Walnut dev pins (header-only)
#   json     nlohmann/json 3.11.3 (header-only)
#   vulkan-headers  KhronosGroup/Vulkan-Headers v1.3.296 (see LauncherVulkan.cmake)
#   doctest  doctest/doctest 2.4.11 (tests only)
set(BLACK_VENDOR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
foreach(dependency walnut/Walnut/Source imgui/imgui.cpp glfw/CMakeLists.txt glm/glm/glm.hpp spdlog/include/spdlog/spdlog.h
                   json/include/nlohmann/json.hpp vulkan-headers/include/vulkan/vulkan.h)
    if(NOT EXISTS "${BLACK_VENDOR}/${dependency}")
        message(FATAL_ERROR "launcher/vendor/${dependency} is missing. Run: git submodule update --init --recursive")
    endif()
endforeach()

include(LauncherVulkan)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${BLACK_VENDOR}/glfw" "${CMAKE_CURRENT_BINARY_DIR}/vendor/glfw" EXCLUDE_FROM_ALL)

# glm, spdlog and json (header only)
add_library(black_glm INTERFACE)
target_include_directories(black_glm SYSTEM INTERFACE "${BLACK_VENDOR}/glm")
add_library(black_spdlog INTERFACE)
target_include_directories(black_spdlog SYSTEM INTERFACE "${BLACK_VENDOR}/spdlog/include")
add_library(black_json INTERFACE)
target_include_directories(black_json SYSTEM INTERFACE "${BLACK_VENDOR}/json/include")

# Dear ImGui core. Walnut compiles the GLFW and Vulkan backends itself (ImGui/ImGuiBuild.cpp).
add_library(black_imgui STATIC
    "${BLACK_VENDOR}/imgui/imgui.cpp"
    "${BLACK_VENDOR}/imgui/imgui_demo.cpp"
    "${BLACK_VENDOR}/imgui/imgui_draw.cpp"
    "${BLACK_VENDOR}/imgui/imgui_tables.cpp"
    "${BLACK_VENDOR}/imgui/imgui_widgets.cpp")
target_include_directories(black_imgui SYSTEM PUBLIC "${BLACK_VENDOR}/imgui")

# Build the Walnut GUI services used by the launcher. Its unused console and serialization
# modules rely on MSVC-specific declarations and are not part of this application's file handling.
set(BLACK_WALNUT "${BLACK_VENDOR}/walnut/Walnut")
add_library(black_walnut STATIC
    "${BLACK_WALNUT}/Source/Walnut/Core/Log.cpp"
    "${BLACK_WALNUT}/Source/Walnut/Utils/StringUtils.cpp"
    "${BLACK_WALNUT}/Platform/GUI/Walnut/ApplicationGUI.cpp"
    "${BLACK_WALNUT}/Platform/GUI/Walnut/Image.cpp"
    "${BLACK_WALNUT}/Platform/GUI/Walnut/Input/Input.cpp"
    "${BLACK_WALNUT}/Platform/GUI/Walnut/ImGui/ImGuiBuild.cpp"
    "${BLACK_WALNUT}/Platform/GUI/Walnut/ImGui/ImGuiTheme.cpp"
    "${BLACK_WALNUT}/Platform/GUI/Walnut/UI/UI.cpp")
target_include_directories(black_walnut SYSTEM PUBLIC
    "${BLACK_WALNUT}/Source"
    "${BLACK_WALNUT}/Platform/GUI"
    "${BLACK_VENDOR}/walnut/vendor/stb_image")
target_link_libraries(black_walnut PUBLIC black_imgui black_glm black_spdlog glfw black_vulkan)
target_compile_features(black_walnut PUBLIC cxx_std_20)

# Walnut uses Vulkan. GLFW must not pull in unused OpenGL headers on Linux or its consumers.
target_compile_definitions(black_walnut PUBLIC GLFW_INCLUDE_NONE=)
target_compile_definitions(black_walnut PUBLIC $<$<CONFIG:Debug>:WL_DEBUG> $<$<NOT:$<CONFIG:Debug>>:WL_RELEASE>)
if(WIN32)
    target_compile_definitions(black_walnut PUBLIC WL_PLATFORM_WINDOWS)
else()
    target_compile_definitions(black_walnut PUBLIC WL_PLATFORM_LINUX)
endif()
if(MSVC)
    # Walnut's log macros use __VA_OPT__, which needs MSVC's conforming preprocessor (also in code that includes them)
    target_compile_options(black_walnut PRIVATE /utf-8 PUBLIC /Zc:preprocessor)
endif()

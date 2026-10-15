cmake_minimum_required(VERSION 3.20)
project(controls_test LANGUAGES C)
if(NOT WIN32)
    message(FATAL_ERROR "The Controls page regressions require Windows.")
endif()
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Pinned XboxRecomp checkout with patches/xboxrecomp.patch applied")
set(runtime_video "${XBOXRECOMP_DIR}/src/video")
set(runtime_input "${XBOXRECOMP_DIR}/src/input")
set(menu_source "${CMAKE_CURRENT_SOURCE_DIR}/../../src/black_native_menu.inc")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(definition "${CMAKE_CURRENT_BINARY_DIR}/menus.ini")
add_custom_command(OUTPUT "${definition}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_definition.py"
        --source "${menu_source}" --output "${definition}"
    DEPENDS "${menu_source}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_definition.py"
    COMMENT "Extracting the built-in native menu definition" VERBATIM)
add_custom_target(controls-definition ALL DEPENDS "${definition}")

add_executable(controls-test test_controls.c
    "${runtime_video}/pc_menu.c" "${runtime_video}/pc_menu_assets.c" "${runtime_video}/pc_movie_skip.c"
    "${runtime_video}/pc_video.c" "${runtime_video}/pc_video_menu.c"
    "${runtime_input}/pc_input.c" "${runtime_input}/pc_controls.c")
add_dependencies(controls-test controls-definition)
target_include_directories(controls-test PRIVATE "${runtime_video}" "${runtime_input}")
target_compile_definitions(controls-test PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(controls-test PRIVATE gdi32 user32)
set_property(TARGET controls-test PROPERTY C_STANDARD 11)
set_property(TARGET controls-test PROPERTY C_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(controls-test PRIVATE /W4 /UNDEBUG)
else()
    target_compile_options(controls-test PRIVATE -Wall -Wextra -UNDEBUG)
endif()

enable_testing()
add_test(NAME controls COMMAND controls-test "${definition}" "${CMAKE_CURRENT_BINARY_DIR}")

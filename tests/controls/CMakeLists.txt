cmake_minimum_required(VERSION 3.20)
project(controls_test LANGUAGES C)
if(NOT WIN32)
    message(FATAL_ERROR "The Controls page regressions require Windows.")
endif()
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "XboxRecomp submodule (third_party/xboxrecomp)")
set(runtime_video "${XBOXRECOMP_DIR}/src/video")
set(runtime_input "${XBOXRECOMP_DIR}/src/input")
set(menu_source "${CMAKE_CURRENT_SOURCE_DIR}/../../src/black_native_menu.inc")
set(BLACK_MANUAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
    CACHE FILEPATH "Source containing the BLACK_PC_CONTROLS manual wrapper")
set(BLACK_PC_CONTROLS_SOURCE "${runtime_input}/pc_controls.c"
    CACHE FILEPATH "Controller implementation compiled by this regression target")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(definition "${CMAKE_CURRENT_BINARY_DIR}/menus.ini")
add_custom_command(OUTPUT "${definition}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_definition.py"
        --source "${menu_source}" --output "${definition}"
    DEPENDS "${menu_source}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_definition.py"
    COMMENT "Extracting the built-in native menu definition" VERBATIM)
add_custom_target(controls-definition ALL DEPENDS "${definition}")

set(controls_wrapper "${CMAKE_CURRENT_BINARY_DIR}/black_pc_controls_wrapper.inc")
add_custom_command(OUTPUT "${controls_wrapper}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_controls_wrapper.py"
        --source "${BLACK_MANUAL_SOURCE}" --output "${controls_wrapper}"
    DEPENDS "${BLACK_MANUAL_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_controls_wrapper.py"
    COMMENT "Extracting the production BLACK_PC_CONTROLS wrapper" VERBATIM)
set_source_files_properties("${controls_wrapper}" PROPERTIES HEADER_FILE_ONLY TRUE)
add_custom_target(controls-wrapper DEPENDS "${controls_wrapper}")

add_executable(controls-test test_controls.c
    "${controls_wrapper}"
    "${runtime_video}/pc_menu.c" "${runtime_video}/pc_menu_assets.c" "${runtime_video}/pc_movie_skip.c"
    "${runtime_video}/pc_video.c" "${runtime_video}/pc_video_menu.c"
    "${runtime_input}/pc_input.c" "${BLACK_PC_CONTROLS_SOURCE}" "${runtime_video}/pc_photo.c")
add_dependencies(controls-test controls-definition controls-wrapper)
target_include_directories(controls-test PRIVATE "${runtime_video}" "${runtime_input}" "${CMAKE_CURRENT_BINARY_DIR}")
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
add_test(NAME controls-restore-default COMMAND controls-test "${definition}" "${CMAKE_CURRENT_BINARY_DIR}" --restore-default)
add_test(NAME controls-capture-keyboard COMMAND controls-test "${definition}" "${CMAKE_CURRENT_BINARY_DIR}" --capture-keyboard)
add_test(NAME controls-capture-mouse COMMAND controls-test "${definition}" "${CMAKE_CURRENT_BINARY_DIR}" --capture-mouse)
add_test(NAME controls-capture-pad COMMAND controls-test "${definition}" "${CMAKE_CURRENT_BINARY_DIR}" --capture-pad)

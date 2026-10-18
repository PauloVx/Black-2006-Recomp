cmake_minimum_required(VERSION 3.20)
project(ambient_occlusion_test LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "AO shader, settings and native-menu regressions require Windows.")
endif()
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "XboxRecomp submodule (third_party/xboxrecomp)")
set(BLACK_MANUAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
    CACHE FILEPATH "Production authored camera hook source")
set(runtime_video "${XBOXRECOMP_DIR}/src/video")
set(runtime_input "${XBOXRECOMP_DIR}/src/input")
set(runtime_post "${XBOXRECOMP_DIR}/src/nv2a/kelvin/kelvin_gpu_post.inc")

add_executable(ao-video-test test_video.c
    "${runtime_video}/pc_video.c" "${runtime_video}/pc_video_menu.c" "${runtime_input}/pc_input.c")
add_executable(ao-crosshair-test test_crosshair.c "${runtime_video}/pc_crosshair.c" "${runtime_video}/pc_video.c")
add_executable(ao-debug-test test_debug.c "${runtime_video}/pc_debug.c" "${runtime_video}/pc_video.c")
add_executable(ao-split-test test_split.c "${runtime_video}/pc_split.c" "${runtime_video}/pc_video.c" "${runtime_input}/pc_input.c")
target_link_libraries(ao-split-test PRIVATE xinput)
add_executable(ao-menu-test test_menu.c
    "${runtime_video}/pc_menu.c" "${runtime_video}/pc_menu_assets.c"
    "${runtime_video}/pc_video.c" "${runtime_video}/pc_video_menu.c" "${runtime_input}/pc_input.c"
    "${runtime_input}/pc_controls.c" "${runtime_video}/pc_photo.c")
foreach(target ao-video-test ao-menu-test ao-crosshair-test ao-debug-test ao-split-test)
    target_include_directories(${target} PRIVATE "${runtime_video}" "${runtime_input}")
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(${target} PRIVATE gdi32 user32 shell32 ole32)
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    set_property(TARGET ${target} PROPERTY C_STANDARD_REQUIRED ON)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -UNDEBUG)
    endif()
endforeach()

# Compile and execute the production embedded shaders on D3D11's software WARP device.
add_executable(ao-warp-test ao_warp_test.cpp)
target_link_libraries(ao-warp-test PRIVATE d3d11 d3dcompiler)
set_property(TARGET ao-warp-test PROPERTY CXX_STANDARD 17)
set_property(TARGET ao-warp-test PROPERTY CXX_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(ao-warp-test PRIVATE /W4 /EHsc /UNDEBUG)
endif()

add_executable(ao-post-order-test test_post_order.c)
target_include_directories(ao-post-order-test PRIVATE "${XBOXRECOMP_DIR}/src/nv2a/kelvin")
set_property(TARGET ao-post-order-test PROPERTY C_STANDARD 11)
set_property(TARGET ao-post-order-test PROPERTY C_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(ao-post-order-test PRIVATE /W4 /UNDEBUG)
else()
    target_compile_options(ao-post-order-test PRIVATE -Wall -Wextra -UNDEBUG)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(hit_attribution_extracted "${CMAKE_CURRENT_BINARY_DIR}/hit_attribution.extracted.inc")
add_custom_command(OUTPUT "${hit_attribution_extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_hit_attribution.py"
        --source "${BLACK_MANUAL_SOURCE}" --output "${hit_attribution_extracted}"
    DEPENDS "${BLACK_MANUAL_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_hit_attribution.py"
    COMMENT "Extracting the production hit-marker attribution" VERBATIM)
set_source_files_properties("${hit_attribution_extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
target_sources(ao-crosshair-test PRIVATE "${hit_attribution_extracted}")
target_include_directories(ao-crosshair-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set(native_menu_extracted "${CMAKE_CURRENT_BINARY_DIR}/native_menu.extracted.inc")
add_custom_command(OUTPUT "${native_menu_extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_menu_default.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}/../../src/black_native_menu.inc" --output "${native_menu_extracted}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/black_native_menu.inc" "${CMAKE_CURRENT_SOURCE_DIR}/extract_menu_default.py"
    COMMENT "Extracting the production native menu definition" VERBATIM)
set_source_files_properties("${native_menu_extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
target_sources(ao-menu-test PRIVATE "${native_menu_extracted}")
target_include_directories(ao-menu-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
set(scene_retry_extracted "${CMAKE_CURRENT_BINARY_DIR}/scene_retry.extracted.inc")
add_custom_command(OUTPUT "${scene_retry_extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_scene_retry.py"
        --source "${runtime_post}" --output "${scene_retry_extracted}"
    DEPENDS "${runtime_post}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_scene_retry.py"
    COMMENT "Extracting the production AO scene coordinator" VERBATIM)
set_source_files_properties("${scene_retry_extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_executable(ao-scene-retry-test test_scene_retry.c "${scene_retry_extracted}")
target_include_directories(ao-scene-retry-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${XBOXRECOMP_DIR}/src/nv2a/kelvin")
set_property(TARGET ao-scene-retry-test PROPERTY C_STANDARD 11)
set_property(TARGET ao-scene-retry-test PROPERTY C_STANDARD_REQUIRED ON)

set(focus_gap_extracted "${CMAKE_CURRENT_BINARY_DIR}/focus_gap.extracted.inc")
add_custom_command(OUTPUT "${focus_gap_extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_focus_gap.py"
        --source "${runtime_post}" --output "${focus_gap_extracted}"
    DEPENDS "${runtime_post}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_focus_gap.py"
    COMMENT "Extracting the production autofocus coordinator" VERBATIM)
set_source_files_properties("${focus_gap_extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_executable(ao-focus-gap-test test_focus_gap.c "${focus_gap_extracted}")
target_include_directories(ao-focus-gap-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${XBOXRECOMP_DIR}/src/nv2a/kelvin")
set_property(TARGET ao-focus-gap-test PROPERTY C_STANDARD 11)
set_property(TARGET ao-focus-gap-test PROPERTY C_STANDARD_REQUIRED ON)

enable_testing()
add_test(NAME ao-video COMMAND ao-video-test)
add_test(NAME ao-menu COMMAND ao-menu-test)
add_test(NAME ao-crosshair COMMAND ao-crosshair-test)
add_test(NAME ao-debug COMMAND ao-debug-test)
add_test(NAME ao-split COMMAND ao-split-test)
add_test(NAME ao-warp COMMAND ao-warp-test "${runtime_post}")
add_test(NAME ao-post-order COMMAND ao-post-order-test)
add_test(NAME ao-scene-retry COMMAND ao-scene-retry-test)
add_test(NAME ao-focus-gap COMMAND ao-focus-gap-test)
set_tests_properties(ao-video ao-menu ao-warp ao-post-order PROPERTIES TIMEOUT 120)
# Retail files are optional for local visual checks. CI exercises the source-only page model.
set_tests_properties(ao-menu PROPERTIES ENVIRONMENT "BLACK_GAME_DIR=")

add_subdirectory(camera)

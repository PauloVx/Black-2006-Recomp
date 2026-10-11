cmake_minimum_required(VERSION 3.20)
project(ambient_occlusion_test LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "AO shader, settings and native-menu regressions require Windows.")
endif()
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Pinned XboxRecomp checkout with patches/xboxrecomp.patch applied")
set(BLACK_MANUAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
    CACHE FILEPATH "Production authored camera hook source")
set(BLACK_LAUNCHER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/launcher.cs"
    CACHE FILEPATH "Production launcher source")
set(runtime_video "${XBOXRECOMP_DIR}/src/video")
set(runtime_input "${XBOXRECOMP_DIR}/src/input")
set(runtime_post "${XBOXRECOMP_DIR}/src/nv2a/kelvin/kelvin_gpu_post.inc")

add_executable(ao-video-test test_video.c
    "${runtime_video}/pc_video.c" "${runtime_video}/pc_video_menu.c" "${runtime_input}/pc_input.c")
add_executable(ao-menu-test test_menu.c
    "${runtime_video}/pc_menu.c" "${runtime_video}/pc_menu_assets.c"
    "${runtime_video}/pc_video.c" "${runtime_video}/pc_video_menu.c" "${runtime_input}/pc_input.c")
foreach(target ao-video-test ao-menu-test)
    target_include_directories(${target} PRIVATE "${runtime_video}" "${runtime_input}")
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(${target} PRIVATE gdi32 user32)
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

enable_testing()
add_test(NAME ao-video COMMAND ao-video-test)
add_test(NAME ao-menu COMMAND ao-menu-test)
add_test(NAME ao-warp COMMAND ao-warp-test "${runtime_post}")
add_test(NAME ao-post-order COMMAND ao-post-order-test)
add_test(NAME ao-scene-retry COMMAND ao-scene-retry-test)
set_tests_properties(ao-video ao-menu ao-warp ao-post-order PROPERTIES TIMEOUT 120)
# Retail files are optional for local visual checks. CI exercises the source-only page model.
set_tests_properties(ao-menu PROPERTIES ENVIRONMENT "BLACK_GAME_DIR=")
find_program(AO_POWERSHELL NAMES pwsh powershell REQUIRED)
add_test(NAME ao-launcher COMMAND "${AO_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/test-launcher.ps1"
    -LauncherSource "${BLACK_LAUNCHER_SOURCE}"
    -BuildDirectory "${CMAKE_CURRENT_BINARY_DIR}/launcher-$<CONFIG>")
set_tests_properties(ao-launcher PROPERTIES TIMEOUT 60)

add_subdirectory(camera)

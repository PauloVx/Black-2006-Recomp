cmake_minimum_required(VERSION 3.20)
project(shader_manifest_test LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "The shader manifest regression uses the Windows file APIs from XboxRecomp.")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(BLACK_MANIFEST_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp/src/nv2a/kelvin/kelvin_gpu_vsh.inc"
    CACHE FILEPATH "Production vertex shader source containing the manifest implementation")
set(manifest_include "${CMAKE_CURRENT_BINARY_DIR}/manifest.extracted.inc")
add_custom_command(OUTPUT "${manifest_include}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_manifest.py"
        --source "${BLACK_MANIFEST_SOURCE}" --output "${manifest_include}"
    DEPENDS "${BLACK_MANIFEST_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_manifest.py"
    COMMENT "Extracting the production vertex shader manifest" VERBATIM)
set_source_files_properties("${manifest_include}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)

add_executable(shader-manifest-test test_manifest.c "${manifest_include}")
target_include_directories(shader-manifest-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(shader-manifest-test PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET shader-manifest-test PROPERTY C_STANDARD 11)
set_property(TARGET shader-manifest-test PROPERTY C_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(shader-manifest-test PRIVATE /W4)
else()
    target_compile_options(shader-manifest-test PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME shader-manifest COMMAND shader-manifest-test "${CMAKE_CURRENT_BINARY_DIR}/manifest-cases")
set_tests_properties(shader-manifest PROPERTIES TIMEOUT 30)

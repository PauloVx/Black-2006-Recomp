cmake_minimum_required(VERSION 3.20)
project(scope_mask_test C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(scope_include "${CMAKE_CURRENT_BINARY_DIR}/scope.extracted.inc")
add_custom_command(OUTPUT "${scope_include}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c" "${scope_include}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../ambient_occlusion/camera/extract_camera.py")
add_executable(scope-mask-test test_scope.c "${scope_include}")
target_include_directories(scope-mask-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(scope-mask-test PRIVATE /W4 /fp:strict)
else()
    target_compile_options(scope-mask-test PRIVATE -Wall -Wextra -frounding-math -fno-fast-math)
endif()
enable_testing()
add_test(NAME scope-mask COMMAND scope-mask-test)

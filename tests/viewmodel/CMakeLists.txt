cmake_minimum_required(VERSION 3.20)
project(black_viewmodel_test C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(extracted "${CMAKE_CURRENT_BINARY_DIR}/viewmodel.extracted.inc")
add_custom_command(OUTPUT "${extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py" "${extracted}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../ambient_occlusion/camera/extract_camera.py")
add_executable(viewmodel-test test_viewmodel.c "${extracted}")
target_include_directories(viewmodel-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(viewmodel-test PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(viewmodel-test PRIVATE /W4 /fp:strict)
else()
    target_compile_options(viewmodel-test PRIVATE -Wall -Wextra -frounding-math -fno-fast-math)
endif()
enable_testing()
add_test(NAME viewmodel COMMAND viewmodel-test)
add_test(NAME viewmodel-disabled COMMAND viewmodel-test off)

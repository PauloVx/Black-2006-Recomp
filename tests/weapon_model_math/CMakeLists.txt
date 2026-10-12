cmake_minimum_required(VERSION 3.20)
project(weapon_model_math_test C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(normalize_include "${CMAKE_CURRENT_BINARY_DIR}/normalize.extracted.inc")
add_custom_command(OUTPUT "${normalize_include}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_normalize.py" "${normalize_include}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/extract_normalize.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/generate.py")
add_executable(weapon-model-math-test test_normalize.c "${normalize_include}")
target_include_directories(weapon-model-math-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(weapon-model-math-test PRIVATE /W4)
endif()
enable_testing()
add_test(NAME weapon-model-math COMMAND weapon-model-math-test)

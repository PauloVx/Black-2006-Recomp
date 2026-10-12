cmake_minimum_required(VERSION 3.20)
project(weapon_model_trace_test C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(trace_include "${CMAKE_CURRENT_BINARY_DIR}/trace.extracted.inc")
add_custom_command(OUTPUT "${trace_include}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify.py"
            --extract "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c" "${trace_include}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/verify.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/../ambient_occlusion/camera/extract_camera.py")
add_executable(weapon-model-trace-test test_trace.c "${trace_include}")
target_include_directories(weapon-model-trace-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(weapon-model-trace-test PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(weapon-model-trace-test PRIVATE /W4 /fp:strict)
endif()
enable_testing()
add_test(NAME weapon-model-trace COMMAND "${Python3_EXECUTABLE}"
         "${CMAKE_CURRENT_SOURCE_DIR}/verify.py" --run "$<TARGET_FILE:weapon-model-trace-test>")

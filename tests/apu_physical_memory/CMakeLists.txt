cmake_minimum_required(VERSION 3.20)
project(apu_physical_memory_test C)
set(CMAKE_C_STANDARD 11)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "XboxRecomp submodule (third_party/xboxrecomp)")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(MEMORY_LAYOUT_SOURCE "${XBOXRECOMP_DIR}/src/kernel/xbox_memory_layout.c"
    CACHE FILEPATH "Optional physical-memory source for a negative control")
set(memory_source "${MEMORY_LAYOUT_SOURCE}")
set(main_source "${CMAKE_CURRENT_SOURCE_DIR}/../../src/main.c")
set(extracted "${CMAKE_CURRENT_BINARY_DIR}/physical_memory.inc")
add_custom_command(OUTPUT "${extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
        --source "${memory_source}" --main "${main_source}" --out "${extracted}"
    DEPENDS "${memory_source}" "${main_source}" extract.py VERBATIM)
set_source_files_properties("${extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_subdirectory("${XBOXRECOMP_DIR}" runtime)
add_executable(apu-physical-memory-test test_main.c "${extracted}")
target_include_directories(apu-physical-memory-test PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${XBOXRECOMP_DIR}/src/apu"
    "${XBOXRECOMP_DIR}/src")
target_link_libraries(apu-physical-memory-test PRIVATE xbox_apu)
if(MSVC)
    target_compile_definitions(apu-physical-memory-test PRIVATE
        _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
endif()
enable_testing()
add_test(NAME apu-physical-memory COMMAND apu-physical-memory-test)
set_tests_properties(apu-physical-memory PROPERTIES TIMEOUT 15)

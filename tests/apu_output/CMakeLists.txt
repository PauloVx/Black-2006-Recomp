cmake_minimum_required(VERSION 3.20)
project(apu_output_test C)
set(CMAKE_C_STANDARD 11)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Patched pinned XboxRecomp runtime")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(extracted "${CMAKE_CURRENT_BINARY_DIR}/backend.inc")
set(source "${XBOXRECOMP_DIR}/src/apu/apu_xaudio2.c")
set(core_source "${XBOXRECOMP_DIR}/src/apu/apu_core.c")
set(core_extracted "${CMAKE_CURRENT_BINARY_DIR}/frame_thread.inc")
add_custom_command(OUTPUT "${extracted}" "${core_extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
        --source "${source}" --out "${extracted}"
        --core-source "${core_source}" --core-out "${core_extracted}"
    DEPENDS "${source}" "${core_source}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py" VERBATIM)
set_source_files_properties("${extracted}" "${core_extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_executable(apu-output-test test_main.c "${extracted}" "${core_extracted}")
target_include_directories(apu-output-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${XBOXRECOMP_DIR}/src/apu")
if(WIN32)
    # Also compile the unmodified complete backend against the actual SDK ABI.
    add_library(apu-xaudio2-compile OBJECT "${source}")
endif()
enable_testing()
add_test(NAME apu-output COMMAND apu-output-test)

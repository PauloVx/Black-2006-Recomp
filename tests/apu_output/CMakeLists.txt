cmake_minimum_required(VERSION 3.20)
project(apu_output_test C)
set(CMAKE_C_STANDARD 11)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Patched pinned XboxRecomp runtime")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(extracted "${CMAKE_CURRENT_BINARY_DIR}/backend.inc")
set(APU_OUTPUT_BACKEND_SOURCE "${XBOXRECOMP_DIR}/src/apu/apu_xaudio2.c" CACHE FILEPATH "Backend source (also supports negative controls)")
set(APU_OUTPUT_CORE_SOURCE "${XBOXRECOMP_DIR}/src/apu/apu_core.c" CACHE FILEPATH "Core source (also supports negative controls)")
set(source "${APU_OUTPUT_BACKEND_SOURCE}")
set(core_source "${APU_OUTPUT_CORE_SOURCE}")
set(core_extracted "${CMAKE_CURRENT_BINARY_DIR}/frame_thread.inc")
set(delivery_extracted "${CMAKE_CURRENT_BINARY_DIR}/packet_delivery.inc")
set(wait_extracted "${CMAKE_CURRENT_BINARY_DIR}/packet_wait.inc")
set(mixer_extracted "${CMAKE_CURRENT_BINARY_DIR}/mixer_render.inc")
set(unavailable_extracted "${CMAKE_CURRENT_BINARY_DIR}/monitor_unavailable.inc")
add_custom_command(OUTPUT "${extracted}" "${core_extracted}" "${delivery_extracted}" "${wait_extracted}" "${mixer_extracted}" "${unavailable_extracted}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
        --source "${source}" --out "${extracted}"
        --core-source "${core_source}" --core-out "${core_extracted}"
        --delivery-out "${delivery_extracted}"
        --wait-out "${wait_extracted}" --mixer-out "${mixer_extracted}"
        --unavailable-out "${unavailable_extracted}"
    DEPENDS "${source}" "${core_source}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py" VERBATIM)
set_source_files_properties("${extracted}" "${core_extracted}" "${delivery_extracted}" "${wait_extracted}" "${mixer_extracted}" "${unavailable_extracted}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_executable(apu-output-test test_main.c "${extracted}" "${core_extracted}" "${delivery_extracted}" "${wait_extracted}" "${mixer_extracted}" "${unavailable_extracted}")
target_include_directories(apu-output-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${XBOXRECOMP_DIR}/src/apu")
if(WIN32)
    # Also compile the unmodified complete backend against the actual SDK ABI.
    add_library(apu-xaudio2-compile OBJECT "${source}")
    target_include_directories(apu-xaudio2-compile PRIVATE "${XBOXRECOMP_DIR}/src/apu")
endif()
enable_testing()
add_test(NAME apu-output COMMAND apu-output-test)

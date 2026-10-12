cmake_minimum_required(VERSION 3.20)
project(kernel_dpc_queue_test C)
set(CMAKE_C_STANDARD 11)
if(NOT WIN32)
    message(FATAL_ERROR "DPC queue concurrency fixture uses Windows thread primitives")
endif()
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Patched pinned XboxRecomp runtime")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(BRIDGE_SOURCE "${XBOXRECOMP_DIR}/src/kernel/kernel_bridge.c")
foreach(variant current old)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/dpc_${variant}.c")
    add_custom_command(OUTPUT "${generated}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_fixture.py"
            "${BRIDGE_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.c"
            "${generated}" "${variant}"
        DEPENDS "${BRIDGE_SOURCE}" extract_fixture.py test_main.c
        VERBATIM)
    add_executable(kernel-dpc-queue-${variant} "${generated}")
endforeach()
enable_testing()
add_test(NAME kernel-dpc-queue COMMAND kernel-dpc-queue-current)
add_test(NAME kernel-dpc-queue-old-control COMMAND kernel-dpc-queue-old)
set_tests_properties(kernel-dpc-queue-old-control PROPERTIES WILL_FAIL TRUE)
set_tests_properties(kernel-dpc-queue kernel-dpc-queue-old-control PROPERTIES TIMEOUT 30)

cmake_minimum_required(VERSION 3.20)
project(kernel_timer_waitset_test C)
set(CMAKE_C_STANDARD 11)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Patched pinned XboxRecomp runtime")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(BRIDGE_SOURCE "${XBOXRECOMP_DIR}/src/kernel/kernel_bridge.c")
foreach(variant current old)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/waitset_${variant}.c")
    add_custom_command(OUTPUT "${generated}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_fixture.py"
            "${BRIDGE_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_main.c"
            "${generated}" "${variant}"
        DEPENDS "${BRIDGE_SOURCE}" extract_fixture.py test_main.c
        VERBATIM)
    add_executable(kernel-timer-waitset-${variant} "${generated}")
endforeach()
enable_testing()
add_test(NAME kernel-timer-waitset COMMAND kernel-timer-waitset-current)
add_test(NAME kernel-timer-waitset-old-control COMMAND kernel-timer-waitset-old)
set_tests_properties(kernel-timer-waitset-old-control PROPERTIES WILL_FAIL TRUE)

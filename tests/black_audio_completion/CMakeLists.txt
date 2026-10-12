cmake_minimum_required(VERSION 3.20)
project(black_audio_completion_test C)
set(CMAKE_C_STANDARD 11)
if(NOT WIN32)
    message(FATAL_ERROR "Audio completion exclusion fixture uses Windows threads")
endif()
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Patched pinned XboxRecomp runtime")
set(BLACK_MANUAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
    CACHE FILEPATH "BLACK production manual wrappers")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(HAL_SOURCE "${XBOXRECOMP_DIR}/src/kernel/kernel_hal.c")
foreach(variant current old old-membership)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/completion_${variant}.c")
    add_custom_command(OUTPUT "${generated}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract_fixture.py"
            "${HAL_SOURCE}" "${BLACK_MANUAL_SOURCE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_main.c" "${generated}" "${variant}"
        DEPENDS "${HAL_SOURCE}" "${BLACK_MANUAL_SOURCE}" extract_fixture.py test_main.c
        VERBATIM)
    add_executable(black-audio-completion-${variant} "${generated}")
endforeach()
enable_testing()
add_test(NAME black-audio-completion COMMAND black-audio-completion-current)
add_test(NAME black-audio-completion-old-control COMMAND black-audio-completion-old)
add_test(NAME black-audio-completion-old-membership-control COMMAND black-audio-completion-old-membership)
set_tests_properties(black-audio-completion-old-control black-audio-completion-old-membership-control
    PROPERTIES WILL_FAIL TRUE)
set_tests_properties(black-audio-completion black-audio-completion-old-control black-audio-completion-old-membership-control
    PROPERTIES TIMEOUT 30)

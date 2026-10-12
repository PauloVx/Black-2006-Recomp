cmake_minimum_required(VERSION 3.20)
project(apu_timing_test C)
set(CMAKE_C_STANDARD 11)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Patched pinned XboxRecomp runtime")
add_executable(apu-timing-test test_main.c)
target_include_directories(apu-timing-test PRIVATE "${XBOXRECOMP_DIR}/src/apu")
enable_testing()
add_test(NAME apu-timing COMMAND apu-timing-test)

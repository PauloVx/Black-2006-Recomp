cmake_minimum_required(VERSION 3.20)
project(apu_spatial_test C)
set(CMAKE_C_STANDARD 11)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/xboxrecomp"
    CACHE PATH "Pinned runtime with patches/xboxrecomp.patch applied")
set(APU_DSP_TEST_SOURCE "" CACHE FILEPATH "Optional prior apu_dsp.c for a negative control")
add_subdirectory("${XBOXRECOMP_DIR}" runtime)
add_executable(apu-spatial-test test_main.c)
if(APU_DSP_TEST_SOURCE)
    target_sources(apu-spatial-test PRIVATE "${APU_DSP_TEST_SOURCE}")
endif()
target_include_directories(apu-spatial-test PRIVATE
    "${XBOXRECOMP_DIR}/src" "${XBOXRECOMP_DIR}/src/apu")
target_link_libraries(apu-spatial-test PRIVATE xbox_apu)
if(MSVC)
    target_compile_definitions(apu-spatial-test PRIVATE
        _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
endif()
enable_testing()
add_test(NAME apu-spatial COMMAND apu-spatial-test)
set_tests_properties(apu-spatial PROPERTIES
    ENVIRONMENT "RECOMP_APU_MIXDOWN_ALL=1;APU_SPATIAL_FRONT_ONLY=0" TIMEOUT 15)
add_test(NAME apu-spatial-front-only COMMAND apu-spatial-test)
set_tests_properties(apu-spatial-front-only PROPERTIES
    ENVIRONMENT "RECOMP_APU_MIXDOWN_ALL=0;APU_SPATIAL_FRONT_ONLY=1" TIMEOUT 15)

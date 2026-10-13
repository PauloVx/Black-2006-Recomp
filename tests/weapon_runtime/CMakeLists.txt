cmake_minimum_required(VERSION 3.20)
project(weapon_runtime_test C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(mode_include "${CMAKE_CURRENT_BINARY_DIR}/weapon_modes.extracted.inc")
add_custom_command(OUTPUT "${mode_include}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c" "${mode_include}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp_manual.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/extract.py")
add_executable(weapon-runtime-test test_modes.c "${mode_include}")
target_include_directories(weapon-runtime-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(weapon-runtime-test PRIVATE /W4)
else()
    target_compile_options(weapon-runtime-test PRIVATE -Wall -Wextra -Werror)
endif()
enable_testing()
# Each CTest runs a fresh process, as these launcher environment settings are
# immutable during a game run. Both native entitlement values are checked.
foreach(silver IN ITEMS native on off)
    if(silver STREQUAL "native")
        set(silver_env "")
        set(silver_expected -1)
    elseif(silver STREQUAL "on")
        set(silver_env 1)
        set(silver_expected 1)
    else()
        set(silver_env 0)
        set(silver_expected 0)
    endif()
    foreach(ammo IN ITEMS 0 1)
        set(test_name "weapon-runtime-${silver}-ammo-${ammo}")
        add_test(NAME "${test_name}"
            COMMAND weapon-runtime-test "${silver_expected}" "${ammo}")
        set_tests_properties("${test_name}" PROPERTIES ENVIRONMENT
            "RECOMP_BLACK_SILVER_WEAPONS=${silver_env};RECOMP_BLACK_INFINITE_AMMO=${ammo}")
    endforeach()
endforeach()
add_test(NAME weapon-runtime-invalid-settings COMMAND weapon-runtime-test -1 0)
set_tests_properties(weapon-runtime-invalid-settings PROPERTIES ENVIRONMENT
    "RECOMP_BLACK_SILVER_WEAPONS=invalid;RECOMP_BLACK_INFINITE_AMMO=invalid")

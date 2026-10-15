# black_vulkan: the Vulkan headers from launcher/vendor/vulkan-headers and the loader every graphics driver
# installs (vulkan-1.dll on Windows, libvulkan.so.1 elsewhere), so building needs no Vulkan SDK.
# -DBLACK_LAUNCHER_VULKAN_SDK=ON uses an installed SDK (find_package(Vulkan)) instead.
option(BLACK_LAUNCHER_VULKAN_SDK "Build against an installed Vulkan SDK instead of the vendored headers" OFF)

add_library(black_vulkan INTERFACE)
if(BLACK_LAUNCHER_VULKAN_SDK)
    find_package(Vulkan REQUIRED)
    target_link_libraries(black_vulkan INTERFACE Vulkan::Vulkan)
    return()
endif()

set(BLACK_VULKAN_HEADERS "${BLACK_VENDOR}/vulkan-headers/include")
target_include_directories(black_vulkan SYSTEM INTERFACE "${BLACK_VULKAN_HEADERS}")

if(WIN32)
    # The import library for vulkan-1.dll, made from the commands the headers declare (only those called are
    # imported, so commands the loader does not export cost nothing).
    set(prototypes "")
    foreach(header vulkan_core.h vulkan_win32.h)
        file(READ "${BLACK_VULKAN_HEADERS}/vulkan/${header}" text)
        string(REGEX MATCHALL "VKAPI_CALL vk[A-Za-z0-9]+\\(" found "${text}")
        list(APPEND prototypes ${found})
    endforeach()
    set(definition "LIBRARY vulkan-1\nEXPORTS\n")
    foreach(prototype IN LISTS prototypes)
        string(REGEX REPLACE "VKAPI_CALL (vk[A-Za-z0-9]+)\\(" "\\1" command "${prototype}")
        string(APPEND definition "    ${command}\n")
    endforeach()
    set(def "${CMAKE_CURRENT_BINARY_DIR}/vulkan-1.def")
    set(lib "${CMAKE_CURRENT_BINARY_DIR}/vulkan-1.lib")
    file(WRITE "${def}" "${definition}")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(machine x64)
    else()
        set(machine x86)
    endif()
    execute_process(COMMAND "${CMAKE_AR}" /nologo "/def:${def}" "/out:${lib}" "/machine:${machine}"
                    RESULT_VARIABLE failed OUTPUT_QUIET)
    if(failed)
        message(FATAL_ERROR "Could not make the Vulkan import library with ${CMAKE_AR}")
    endif()
    target_link_libraries(black_vulkan INTERFACE "${lib}")
else()
    find_library(BLACK_VULKAN_LOADER NAMES vulkan libvulkan.so.1 libvulkan.1.dylib REQUIRED)
    target_link_libraries(black_vulkan INTERFACE "${BLACK_VULKAN_LOADER}")
endif()

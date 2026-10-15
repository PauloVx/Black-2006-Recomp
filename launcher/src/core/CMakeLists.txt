# The launcher's logic, with no UI or graphics dependencies, so it can be unit-tested on its own.
add_library(black_launcher_core STATIC
    GameArt.cpp
    GameFiles.cpp
    Process.cpp
    ProjectLayout.cpp
    Sha256.cpp
    Text.cpp)
target_include_directories(black_launcher_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(black_launcher_core PUBLIC cxx_std_20)
black_launcher_warnings(black_launcher_core)
if(WIN32)
    target_compile_definitions(black_launcher_core PRIVATE UNICODE _UNICODE)
    target_link_libraries(black_launcher_core PRIVATE shell32)
endif()

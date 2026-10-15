# Unit tests for black_launcher_core. They need no game files, GPU or window.
add_executable(black_launcher_tests
    GameArtTests.cpp
    GameFilesTests.cpp
    ProjectLayoutTests.cpp
    Sha256Tests.cpp
    TestMain.cpp
    TextTests.cpp)
target_include_directories(black_launcher_tests SYSTEM PRIVATE "${BLACK_VENDOR}/doctest")
target_link_libraries(black_launcher_tests PRIVATE black_launcher_core)
black_launcher_warnings(black_launcher_tests)
add_test(NAME black_launcher_tests COMMAND black_launcher_tests)

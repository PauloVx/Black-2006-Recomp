include(LauncherEmbed)
set(BLACK_ASSETS "${PROJECT_SOURCE_DIR}/assets")
set(BLACK_FONTS "${BLACK_ASSETS}/fonts")
black_embed("${CMAKE_CURRENT_BINARY_DIR}/EmbeddedAssets.cpp" black::assets
    "logoImage=${BLACK_ASSETS}/images/black-logo.png"
    "rowFont=${BLACK_FONTS}/RussoOne-Regular.ttf"
    "titleFont=${BLACK_FONTS}/SairaCondensed-ExtraBold.ttf")

add_executable(black_launcher
    "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedAssets.cpp"
    DiscFlow.cpp
    KeyCapture.cpp
    LauncherLayer.cpp
    Main.cpp
    MenuFont.cpp
    NativeDialogs.cpp
    SaveFlow.cpp
    SettingsFlow.cpp
    TextEntry.cpp
    Theme.cpp
    Widgets.cpp)
target_include_directories(black_launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(black_launcher PRIVATE black_launcher_core black_walnut)
set_target_properties(black_launcher PROPERTIES
    OUTPUT_NAME "BLACK Launcher"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(black_launcher PRIVATE BLACK_LAUNCHER_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_compile_definitions(black_launcher PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(black_launcher PRIVATE ole32 shell32)
    # Release builds open no console window; Debug keeps the console for logs. Both start at main().
    if(MSVC)
        target_link_options(black_launcher PRIVATE $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>)
    endif()
endif()
black_launcher_warnings(black_launcher)

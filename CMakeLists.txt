cmake_minimum_required(VERSION 3.29)

project(RuneGemQuickUpgrade VERSION 0.1.67 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RUNE_GEM_QUICK_UPGRADE_LOCAL_SDK_DIR "" CACHE PATH
    "Optional local D2RLoader PluginSDK include tree for offline builds")
if(RUNE_GEM_QUICK_UPGRADE_LOCAL_SDK_DIR)
    add_library(D2RLPlugin INTERFACE)
    add_library(D2RLPlugin::D2RLPlugin ALIAS D2RLPlugin)
    target_include_directories(D2RLPlugin INTERFACE
        "${RUNE_GEM_QUICK_UPGRADE_LOCAL_SDK_DIR}/include")
else()
    include(FetchContent)
    set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(D2RLPLUGIN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        D2RLoaderPluginSDK
        GIT_REPOSITORY https://github.com/D2RLoader/PluginSDK.git
        GIT_TAG 6eb8f8b6192868214706bd6d528c5294f2f551b7
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(D2RLoaderPluginSDK)
endif()

add_library(rune_gem_quick_upgrade SHARED
    src/plugin.cpp
    src/plugin.rc
)
target_link_libraries(rune_gem_quick_upgrade PRIVATE D2RLPlugin::D2RLPlugin user32)
target_compile_definitions(rune_gem_quick_upgrade PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
)
set_target_properties(rune_gem_quick_upgrade PROPERTIES
    OUTPUT_NAME "d2rl-rune-gem-quick-upgrade"
    PREFIX ""
)

if(MSVC)
    target_compile_options(rune_gem_quick_upgrade PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_options(rune_gem_quick_upgrade PRIVATE /INCREMENTAL:NO /Brepro)
endif()

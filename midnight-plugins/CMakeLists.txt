# Top-level CMake project for Midnight CLAP plugins.
cmake_minimum_required(VERSION 3.21)
project(midnight_plugins LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
    # CLAP is a C API with C99 designated initializers; MSVC needs /std:c11+ for C files.
    set(CMAKE_C_STANDARD 11)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Fetch the official CLAP headers (header-only, MIT).
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.6
)
FetchContent_MakeAvailable(clap)

# CLAP exposes target `clap` (interface, header-only).

# ---------------------------------------------------------------------------
# Fetch the VST3 SDK (base + pluginterfaces + public.sdk, no GUI, no examples).
# ---------------------------------------------------------------------------
set(SMTG_ADD_VSTGUI                  OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_STANDALONE_APP     OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK          OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR           OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.7.14_build_55
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vst3sdk)
# The SDK creates targets: pluginterfaces, base, sdk_common, sdk

add_subdirectory(plugins/melody_maker)

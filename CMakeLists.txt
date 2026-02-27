cmake_minimum_required(VERSION 3.28)

project(nive
    VERSION 0.3.0
    LANGUAGES C CXX
    DESCRIPTION "Lightweight image viewer for Windows"
)

# C++ Standard - C++26 with MSVC
# CMake doesn't fully support C++26 for MSVC yet, so we use C++23 as base
# and add /std:c++latest manually for MSVC to get C++26 features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Note: /std:c++latest is applied per-target in nive_configure_target()
# to avoid polluting external libraries (bit7z, libaom, etc.)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(NIVE_BUILD_TESTS "Build unit tests" ON)
option(NIVE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(NIVE_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)

# External dependencies directory
set(EXTERNALS_DIR "${CMAKE_SOURCE_DIR}/externals")

# spdlog (header-only library)
set(SPDLOG_INCLUDE_DIR "${EXTERNALS_DIR}/spdlog/include")

# SQLite configuration
set(SQLITE_DIR "${EXTERNALS_DIR}/sqlite")
if(EXISTS "${SQLITE_DIR}/CMakeLists.txt")
    add_subdirectory(${SQLITE_DIR})
    message(STATUS "SQLite found and added via add_subdirectory()")
else()
    message(WARNING "SQLite not found - cache database disabled")
endif()

# bit7z configuration
set(BIT7Z_DIR "${EXTERNALS_DIR}/bit7z")
if(EXISTS "${BIT7Z_DIR}/CMakeLists.txt")
    set(NIVE_HAS_BIT7Z ON)
    # bit7z build options
    set(BIT7Z_AUTO_FORMAT ON CACHE BOOL "Enable automatic format detection" FORCE)
    set(BIT7Z_USE_NATIVE_STRING ON CACHE BOOL "Use native string type (wstring on Windows)" FORCE)
    set(BIT7Z_BUILD_TESTS OFF CACHE BOOL "Build bit7z tests" FORCE)
    set(BIT7Z_BUILD_DOCS OFF CACHE BOOL "Build bit7z docs" FORCE)
    # bit7z uses C++17, avoid C++20 char8_t issues
    set(BIT7Z_LANGUAGE_STANDARD 17 CACHE STRING "C++ standard for bit7z" FORCE)
    add_subdirectory(${BIT7Z_DIR})
    message(STATUS "bit7z found and added via add_subdirectory()")
else()
    set(NIVE_HAS_BIT7Z OFF)
    message(STATUS "bit7z not found - archive support disabled")
endif()

# zstd configuration
set(ZSTD_DIR "${EXTERNALS_DIR}/zstd")
if(EXISTS "${ZSTD_DIR}/build/cmake/CMakeLists.txt")
    set(NIVE_HAS_ZSTD ON)
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ZSTD_DIR}/build/cmake" "${CMAKE_BINARY_DIR}/zstd")
    message(STATUS "zstd found and added via add_subdirectory()")
else()
    set(NIVE_HAS_ZSTD OFF)
    message(STATUS "zstd not found - cache compression disabled")
endif()

# toml++ configuration (header-only)
set(TOMLPLUSPLUS_DIR "${EXTERNALS_DIR}/tomlplusplus")
if(EXISTS "${TOMLPLUSPLUS_DIR}/include/toml++/toml.hpp")
    set(NIVE_HAS_TOMLPLUSPLUS ON)
    add_library(tomlplusplus INTERFACE)
    target_include_directories(tomlplusplus INTERFACE "${TOMLPLUSPLUS_DIR}/include")
    message(STATUS "toml++ found (header-only)")
else()
    set(NIVE_HAS_TOMLPLUSPLUS OFF)
    message(STATUS "toml++ not found")
endif()

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(StaticAnalysis)
include(VersionInfo)

# Windows-specific definitions (applied globally)
# WIN32_LEAN_AND_MEAN: Exclude rarely-used stuff from Windows headers
# VC_EXTRALEAN: Exclude even more rarely-used stuff (MFC-related)
# NOMINMAX: Prevent Windows.h from defining min/max macros
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

# Add subdirectories
add_subdirectory(src/core)
add_subdirectory(src/ui)
add_subdirectory(src)

if(NIVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS nive
    RUNTIME DESTINATION bin
)

install(FILES include/nive/plugin_api.h
    DESTINATION include/nive
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== nive Configuration ===")
message(STATUS "Version:        ${PROJECT_VERSION} (${NIVE_GIT_HASH_SHORT})")
message(STATUS "C++ Standard:   C++26 (via /std:c++latest)")
message(STATUS "Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Tests:    ${NIVE_BUILD_TESTS}")
message(STATUS "Clang-Tidy:     ${NIVE_ENABLE_CLANG_TIDY}")
message(STATUS "")

# plugins/avif/CMakeLists.txt
# Self-contained AVIF decoder plugin using libavif via FetchContent

include(FetchContent)

# ---------------------------------------------------------------------------
# libaom requires Perl; hint Git-bundled Perl if not found in PATH
# ---------------------------------------------------------------------------
find_program(_PERL_EXE perl)
if(NOT _PERL_EXE)
    find_program(_GIT_EXE git)
    if(_GIT_EXE)
        get_filename_component(_GIT_DIR "${_GIT_EXE}" DIRECTORY)
        get_filename_component(_GIT_ROOT "${_GIT_DIR}" DIRECTORY)
        list(APPEND CMAKE_PROGRAM_PATH "${_GIT_ROOT}/usr/bin")
    endif()
endif()

# ---------------------------------------------------------------------------
# Patch libaom NASM detection for NASM 3.x compatibility
#
# libaom source must be pre-fetched and patched BEFORE libavif configures it.
# We download libaom under a separate FetchContent name, apply the patch, and
# redirect libavif via FETCHCONTENT_SOURCE_DIR_LIBAOM.
# ---------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/PatchAomNasm.cmake")

# libaom v3.12.1 — same version used by libavif v1.3.0
# SOURCE_SUBDIR set to nonexistent path to download only (skip add_subdirectory)
FetchContent_Declare(
    libaom_patch
    URL "https://aomedia.googlesource.com/aom/+archive/v3.12.1.tar.gz"
    SOURCE_SUBDIR _download_only
)
FetchContent_MakeAvailable(libaom_patch)
patch_aom_nasm_detection("${libaom_patch_SOURCE_DIR}")
set(FETCHCONTENT_SOURCE_DIR_LIBAOM "${libaom_patch_SOURCE_DIR}" CACHE PATH "" FORCE)

# ---------------------------------------------------------------------------
# libavif via FetchContent (v1.3.0)
# ---------------------------------------------------------------------------
set(AVIF_CODEC_AOM "LOCAL" CACHE STRING "Use local libaom" FORCE)
set(AVIF_CODEC_DAV1D OFF CACHE BOOL "" FORCE)
set(AVIF_LIBYUV OFF CACHE STRING "Disable libyuv (not needed for decode)" FORCE)
set(AVIF_LIBSHARPYUV OFF CACHE STRING "Disable libsharpyuv" FORCE)
set(AVIF_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(AVIF_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libavif
    GIT_REPOSITORY https://github.com/AOMediaCodec/libavif.git
    GIT_TAG        v1.3.0
    GIT_SHALLOW    TRUE
)

# Save base compile flags before libavif/libaom (libaom pollutes them via CACHE FORCE)
set(_SAVED_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(_SAVED_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(_SAVED_CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

FetchContent_MakeAvailable(libavif)

# Restore base flags (libaom overrides them via CACHE FORCE)
set(CMAKE_C_FLAGS "${_SAVED_CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${_SAVED_CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${_SAVED_CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "" FORCE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
add_library(avif_plugin SHARED avif_plugin.cpp)

target_compile_definitions(avif_plugin PRIVATE NIVE_PLUGIN_EXPORT)

target_include_directories(avif_plugin PRIVATE
    ${NIVE_PLUGIN_API_DIR}
)

target_link_libraries(avif_plugin PRIVATE avif)

if(MSVC)
    target_compile_options(avif_plugin PRIVATE /W4 /permissive-)
    # Disable warnings-as-errors for plugin (external library headers may emit warnings)
    target_compile_options(avif_plugin PRIVATE /WX-)
endif()

set_target_properties(avif_plugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    OUTPUT_NAME "nive_avif"
)

# Post-build: Copy libavif DLL to bin/ so the plugin can load it at runtime
add_custom_command(TARGET avif_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:avif>" "${CMAKE_BINARY_DIR}/bin/"
    COMMENT "Copying libavif DLL to bin directory"
)

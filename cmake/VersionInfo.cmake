# cmake/VersionInfo.cmake
# Generate version.h from project version and git information

find_package(Git QUIET)

if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NIVE_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE git_result
    )
    if(NOT git_result EQUAL 0)
        set(NIVE_GIT_HASH "unknown")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NIVE_GIT_HASH_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE git_result
    )
    if(NOT git_result EQUAL 0)
        set(NIVE_GIT_HASH_SHORT "unknown")
    endif()
else()
    set(NIVE_GIT_HASH "unknown")
    set(NIVE_GIT_HASH_SHORT "unknown")
endif()

set(NIVE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(NIVE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(NIVE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(NIVE_VERSION ${PROJECT_VERSION})

configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/version.h"
    @ONLY
)

# Re-configure when git HEAD changes
if(EXISTS "${CMAKE_SOURCE_DIR}/.git/HEAD")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/.git/HEAD"
        "${CMAKE_SOURCE_DIR}/.git/index"
    )
endif()


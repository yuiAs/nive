# SQLite3 Amalgamation
# Downloaded from https://www.sqlite.org/download.html

cmake_minimum_required(VERSION 3.20)

add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Windows-specific settings
if(WIN32)
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_OS_WIN=1
        SQLITE_THREADSAFE=2  # Multi-thread mode (separate connections for each thread)
    )
endif()

# Disable warnings for third-party code
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

# src/core/CMakeLists.txt
# Core library - no UI dependencies

add_library(nive_core STATIC)

target_sources(nive_core
    PRIVATE
        # Utility module
        util/hash.cpp
        util/string_utils.cpp
        util/thread_pool.cpp

        # Config module
        config/settings.cpp
        config/settings_manager.cpp

        # Image module
        image/wic_decoder.cpp
        image/image_scaler.cpp

        # Thumbnail module
        thumbnail/thumbnail_queue.cpp
        thumbnail/thumbnail_generator.cpp

        # Cache module
        cache/thumbnail_data.cpp
        cache/cache_database.cpp
        cache/cache_manager.cpp

        # File system module
        fs/file_metadata.cpp
        fs/natural_sort.cpp
        fs/directory.cpp
        fs/file_conflict.cpp
        fs/file_operations.cpp
        fs/trash.cpp

        # Archive module
        archive/archive_entry.cpp
        archive/virtual_path.cpp
        archive/archive_reader.cpp
        archive/archive_manager.cpp

        # Plugin module
        plugin/plugin_loader.cpp
        plugin/plugin_manager.cpp

        # i18n module
        i18n/i18n.cpp
        i18n/message_store.cpp
        i18n/locale_detector.cpp
        i18n/icu_formatter.cpp
)

target_include_directories(nive_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/generated
)

# Suppress warnings from spdlog headers (external library)
target_include_directories(nive_core SYSTEM
    PUBLIC
        ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(nive_core
    PUBLIC
        # Windows SDK libraries
        windowscodecs   # WIC (Windows Imaging Component)
        shlwapi         # Shell lightweight utilities
        shell32         # Shell API
        ole32           # COM
        oleaut32        # COM automation
        uuid            # COM GUIDs
        bcrypt          # CNG (Cryptography Next Generation)
        comctl32        # Common Controls
        icu             # Windows built-in ICU (Win10 1903+)
    PRIVATE
        sqlite3         # SQLite database
)

# Link bit7z library if available
if(NIVE_HAS_BIT7Z)
    target_link_libraries(nive_core PRIVATE bit7z)
    target_compile_definitions(nive_core PRIVATE NIVE_HAS_BIT7Z)
endif()

# Link zstd library if available
if(NIVE_HAS_ZSTD)
    target_link_libraries(nive_core PRIVATE libzstd_static)
    target_compile_definitions(nive_core PRIVATE NIVE_HAS_ZSTD)
endif()

# Link toml++ (header-only) if available
if(NIVE_HAS_TOMLPLUSPLUS)
    target_link_libraries(nive_core PRIVATE tomlplusplus)
    target_compile_definitions(nive_core PRIVATE NIVE_HAS_TOMLPLUSPLUS)
endif()

# Apply project-wide settings
nive_configure_target(nive_core)

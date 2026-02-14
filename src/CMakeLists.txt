# src/CMakeLists.txt
# Main executable

add_executable(nive WIN32
    main.cpp
)

target_link_libraries(nive
    PRIVATE
        nive_core
        nive_ui
)

# Set Windows subsystem properties
set_target_properties(nive PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DPI_AWARE "PerMonitor"
)

# Add application manifest if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/nive.manifest")
    target_sources(nive PRIVATE "${CMAKE_SOURCE_DIR}/resources/nive.manifest")
endif()

# Apply project-wide settings
nive_configure_target(nive)

# Post-build: Create plugins directory
add_custom_command(TARGET nive POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:nive>/plugins"
    COMMENT "Creating plugins directory"
)

# Post-build: Copy locale files
add_custom_command(TARGET nive POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:nive>/locales"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/locales" "$<TARGET_FILE_DIR:nive>/locales"
    COMMENT "Copying locale files"
)

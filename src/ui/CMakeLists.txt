# src/ui/CMakeLists.txt
# UI library - Win32 API based

# D2D UI framework subdirectory
add_subdirectory(d2d)

add_library(nive_ui STATIC)

target_sources(nive_ui
    PRIVATE
        # Application
        app.cpp
        image_viewer_window.cpp
        main_window.cpp
        file_operation_manager.cpp

        # Components
        components/directory_tree.cpp
        components/file_list_view.cpp
        components/thumbnail_grid.cpp

        # Drag and drop
        dnd/drop_source.cpp
        dnd/drop_target.cpp
        dnd/file_data_object.cpp

        # State management
        state/app_state.cpp

        # Dialogs (legacy GDI dialogs removed — migrated to D2D)
)

target_include_directories(nive_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nive_ui
    PUBLIC
        nive_core
        nive_d2d_ui     # D2D UI framework
    PRIVATE
        # Additional Windows UI libraries
        uxtheme         # Visual Styles
        dwmapi          # Desktop Window Manager
)

# Apply project-wide settings
nive_configure_target(nive_ui)

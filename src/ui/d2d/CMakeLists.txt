# src/ui/d2d/CMakeLists.txt
# Direct2D-based UI framework

add_library(nive_d2d_ui STATIC)

target_sources(nive_d2d_ui
    PRIVATE
        # Core
        core/d2d_factory.cpp
        core/device_resources.cpp
        core/bitmap_utils.cpp

        # Base
        base/component.cpp

        # Components
        components/label.cpp
        components/button.cpp
        components/checkbox.cpp
        components/radiobutton.cpp
        components/groupbox.cpp
        components/editbox.cpp
        components/spinedit.cpp
        components/tabcontrol.cpp
        components/combobox.cpp
        components/listbox.cpp

        # Dialog
        dialog/d2d_dialog.cpp
        dialog/settings/d2d_settings_dialog.cpp
        dialog/file_conflict/d2d_file_conflict_dialog.cpp
        dialog/delete_confirm/d2d_delete_confirm_dialog.cpp
)

# Debug test dialog - enabled for Debug builds or explicitly via option
option(NIVE_DEBUG_D2D_TEST "Enable D2D test dialog in non-Debug builds" OFF)

# For multi-config generators (MSVC), we add sources unconditionally
# but use compile definitions to control what gets built
target_sources(nive_d2d_ui PRIVATE
    dialog/test/d2d_test_dialog.cpp
)

# For single-config generators
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NIVE_DEBUG_D2D_TEST)
    target_compile_definitions(nive_d2d_ui PUBLIC NIVE_DEBUG_D2D_TEST)
    message(STATUS "D2D test dialog enabled")
endif()

# For multi-config generators (MSVC)
if(CMAKE_CONFIGURATION_TYPES)
    target_compile_definitions(nive_d2d_ui PUBLIC $<$<CONFIG:Debug>:NIVE_DEBUG_D2D_TEST>)
    message(STATUS "D2D test dialog enabled for Debug configuration")
endif()

target_include_directories(nive_d2d_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nive_d2d_ui
    PUBLIC
        nive_core
    PRIVATE
        d2d1
        dwrite
)

# Apply project-wide settings
nive_configure_target(nive_d2d_ui)

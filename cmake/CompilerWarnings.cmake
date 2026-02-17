# cmake/CompilerWarnings.cmake
# Compiler warning configuration for MSVC

function(set_target_warnings target)
    if(MSVC)
        target_compile_options(${target}
            PRIVATE
                /W4             # Warning level 4
                /WX             # Warnings as errors
                /permissive-    # Strict standards conformance
                /Zc:__cplusplus # Report correct __cplusplus value
                /Zc:preprocessor # Use conforming preprocessor
                /utf-8          # Source and execution charset UTF-8

                # Additional useful warnings
                /w14242         # 'identifier': conversion, possible loss of data
                /w14254         # 'operator': conversion, possible loss of data
                /w14263         # Member function does not override base class virtual
                /w14265         # Class has virtual functions but destructor is not virtual
                /w14287         # 'operator': unsigned/negative constant mismatch
                /w14296         # 'operator': expression is always false
                /w14311         # 'variable': pointer truncation
                /w14545         # Expression before comma evaluates to a function
                /w14546         # Function call before comma missing argument list
                /w14547         # Operator before comma has no effect
                /w14549         # Operator before comma has no effect
                /w14555         # Expression has no effect
                /w14619         # #pragma warning invalid
                /w14640         # Thread-unsafe static member initialization
                /w14826         # Conversion is sign-extended
                /w14905         # Wide string literal cast to LPSTR
                /w14906         # String literal cast to LPWSTR
                /w14928         # Illegal copy-initialization

                # Disable some noisy warnings
                /wd4100         # Unreferenced formal parameter
                /wd4201         # Nonstandard extension: nameless struct/union
        )

        # Enable ASAN if requested
        if(NIVE_ENABLE_ASAN)
            target_compile_options(${target} PRIVATE /fsanitize=address)
            target_link_options(${target} PRIVATE /fsanitize=address)
        endif()
    else()
        # GCC/Clang warnings (for cross-compilation or future use)
        target_compile_options(${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
                -Wconversion
                -Wsign-conversion
                -Wshadow
                -Wnon-virtual-dtor
                -Wold-style-cast
                -Wcast-align
                -Wunused
                -Woverloaded-virtual
                -Wformat=2
                -Wnull-dereference
                -Wdouble-promotion
        )

        if(NIVE_ENABLE_ASAN)
            target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address)
        endif()
    endif()
endfunction()

# Function to set up common target properties
function(nive_configure_target target)
    set_target_warnings(${target})

    # C++26 via /std:c++latest (applied per-target to avoid polluting externals)
    if(MSVC)
        target_compile_options(${target} PRIVATE /std:c++latest)
    endif()

    # Set output directories
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Strip debug info and optimize binary size for Release builds
    if(MSVC)
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:/DEBUG:NONE>   # No PDB generation
            $<$<CONFIG:Release>:/OPT:REF>      # Remove unreferenced functions/data
            $<$<CONFIG:Release>:/OPT:ICF>      # Fold identical COMDATs
        )
    else()
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Release>:-s>            # Strip all symbols
        )
    endif()
endfunction()

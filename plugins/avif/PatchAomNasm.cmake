# PatchAom.cmake
# Patches for libaom when used as a FetchContent dependency
#
# 1. NASM 3.x detection fix:
#    libaom's test_nasm() runs `nasm -hf` and checks for `-Ox` in the output.
#    NASM 3.x changed `-hf` to only output the format list. The `-Ox` flag
#    info is now under `nasm -h -O`. This patch appends that output so the
#    multipass optimization check passes.
#
# 2. Skip install/export rules:
#    libaom's install rules validate INTERFACE_INCLUDE_DIRECTORIES for
#    absolute paths, which fails under CMake 4.x when libaom is consumed
#    via FetchContent (libavif sets absolute INTERFACE include paths).
#    Since we don't install libaom, the export rules are unnecessary.

function(patch_aom_nasm_detection AOM_SOURCE_DIR)
    set(_file "${AOM_SOURCE_DIR}/build/cmake/aom_optimization.cmake")
    if(NOT EXISTS "${_file}")
        return()
    endif()

    file(READ "${_file}" _content)

    # Skip if already patched
    if(_content MATCHES "_nasm_opt_help")
        return()
    endif()

    # Append `nasm -h -O` output after the existing `nasm -hf` call
    string(REPLACE
        [[execute_process(COMMAND ${CMAKE_ASM_NASM_COMPILER} -hf
                  OUTPUT_VARIABLE nasm_helptext)]]
        [[execute_process(COMMAND ${CMAKE_ASM_NASM_COMPILER} -hf
                  OUTPUT_VARIABLE nasm_helptext)
  # NASM 3.x: -hf only shows format list; also check -h -O for -Ox flag
  execute_process(COMMAND ${CMAKE_ASM_NASM_COMPILER} -h -O
                  OUTPUT_VARIABLE _nasm_opt_help)
  string(APPEND nasm_helptext "${_nasm_opt_help}")]]
        _content "${_content}"
    )

    file(WRITE "${_file}" "${_content}")
    message(STATUS "Patched libaom NASM detection for NASM 3.x compatibility")
endfunction()

function(patch_aom_skip_install AOM_SOURCE_DIR)
    set(_file "${AOM_SOURCE_DIR}/CMakeLists.txt")
    if(NOT EXISTS "${_file}")
        return()
    endif()

    file(READ "${_file}" _content)

    # Skip if already patched
    if(_content MATCHES "AOM_SKIP_INSTALL")
        return()
    endif()

    string(REPLACE
        "setup_aom_install_targets()"
        "if(NOT AOM_SKIP_INSTALL)\n  setup_aom_install_targets()\nendif()"
        _content "${_content}"
    )

    file(WRITE "${_file}" "${_content}")
    message(STATUS "Patched libaom to skip install rules (AOM_SKIP_INSTALL)")
endfunction()

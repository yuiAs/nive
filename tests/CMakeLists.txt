# tests/CMakeLists.txt
# Unit tests using Catch2

# For now, create a placeholder test main
# Catch2 integration will be added when the library is available

add_executable(nive_tests
    test_main.cpp
)

target_link_libraries(nive_tests
    PRIVATE
        nive_core
)

# Apply project-wide settings
nive_configure_target(nive_tests)

# Register tests with CTest
add_test(NAME nive_tests COMMAND nive_tests)

# Helper function to add individual test files
function(nive_add_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE nive_core)
    nive_configure_target(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Test configuration
enable_testing()

# Test source files
set(TEST_SOURCES
    test_main.cpp
    test_nexussynth_engine.cpp
)

# Create test executable
add_executable(nexussynth_tests ${TEST_SOURCES})

# Link test libraries (to be added when test framework is chosen)
target_link_libraries(nexussynth_tests nexussynth_lib)

# Set test properties
set_target_properties(nexussynth_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Register tests
add_test(NAME nexussynth_unit_tests COMMAND nexussynth_tests)
# Test configuration
enable_testing()

# Test source files grouped by category
set(CORE_TEST_SOURCES
    test_main.cpp
    test_nexussynth_engine.cpp
)

set(SYNTHESIS_TEST_SOURCES
    test_pbp_synthesis_engine.cpp
    test_overlap_add.cpp
    test_enhanced_synthesis.cpp
    test_fft_performance.cpp
)

set(MLPG_TEST_SOURCES
    test_mlpg_engine.cpp
    test_quality_metrics.cpp
    test_convergence_simple.cpp
    test_model_convergence.cpp
    test_global_variance.cpp
)

set(HMM_TEST_SOURCES
    test_hmm_trainer.cpp
    test_hmm_simple_integration.cpp
    test_context_hmm_integration.cpp
    test_enhanced_viterbi.cpp
    test_viterbi_interface.cpp
    test_parallel_hmm_training.cpp
)

set(FEATURE_TEST_SOURCES
    test_label_file_generator.cpp
    test_label_simple.cpp
    test_data_augmentation.cpp
)

# Create individual test executables for better granularity
function(create_test_executable TEST_NAME TEST_SOURCES)
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    
    target_link_libraries(${TEST_NAME}
        nexussynth_lib
        GTest::gtest_main
        GTest::gtest
        pthread
    )
    
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${TEST_NAME} OpenMP::OpenMP_CXX)
    endif()
    
    target_link_libraries(${TEST_NAME}
        world
        Eigen3::Eigen
        asmjit::asmjit
        cjson
    )
    
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Register with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "FAIL|ERROR|failed"
    )
endfunction()

# Create individual test executables (one per test file to avoid main() conflicts)
create_test_executable(nexussynth_overlap_add_test "test_overlap_add.cpp")

# Create separate tests to avoid main() conflicts
set(MAIN_TEST_FILES test_main.cpp test_nexussynth_engine.cpp)
create_test_executable(nexussynth_main_test "${MAIN_TEST_FILES}")

# Create a comprehensive test suite that runs all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --parallel 4
    DEPENDS 
        nexussynth_overlap_add_test
        nexussynth_main_test
    COMMENT "Running all NexusSynth tests"
)
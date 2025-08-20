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
    test_streaming_buffer_manager.cpp
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
    
    # Add cJSON include directory for tests when it's available
    if(cjson_ADDED)
        target_include_directories(${TEST_NAME} PRIVATE
            ${cjson_SOURCE_DIR}
        )
    endif()
    
    target_link_libraries(${TEST_NAME}
        nexussynth_lib
        GTest::gtest_main
        GTest::gtest
    )
    
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${TEST_NAME} OpenMP::OpenMP_CXX)
    endif()
    
    # Dependencies are transitively linked through nexussynth_lib
    # But we may need to link them directly for some test cases
    target_link_libraries(${TEST_NAME}
        world
        Eigen3::Eigen
        asmjit::asmjit
        cjson
    )
    
    # Link pthread on Unix systems
    if(UNIX)
        target_link_libraries(${TEST_NAME} pthread)
    endif()
    
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

# Create window optimization test
create_test_executable(nexussynth_window_optimization_test test_window_optimization.cpp)

# Create window performance benchmark test
create_test_executable(nexussynth_window_performance_test test_window_performance_benchmark.cpp)

# Create streaming buffer manager test
create_test_executable(nexussynth_streaming_buffer_test test_streaming_buffer_manager.cpp)

# Create performance profiler test
create_test_executable(nexussynth_performance_profiler_test test_performance_profiler.cpp)

# Create UTAU argument parser test
create_test_executable(nexussynth_utau_argument_parser_test test_utau_argument_parser.cpp)

# Create UTAU flag converter test  
create_test_executable(nexussynth_utau_flag_converter_test test_utau_flag_converter.cpp)

# Create UTAU logger test
create_test_executable(nexussynth_utau_logger_test test_utau_logger.cpp)

# Create UTAU error handler test
create_test_executable(nexussynth_utau_error_handler_test test_utau_error_handler.cpp)

# Create voice bank scanner test
create_test_executable(nexussynth_voicebank_scanner_test test_voicebank_scanner.cpp)

# Create conditioning configuration test
create_test_executable(nexussynth_conditioning_config_test test_conditioning_config.cpp)

# Create additional component tests (moved from src directory)
create_test_executable(nexussynth_world_wrapper_test test_world_wrapper.cpp)
create_test_executable(nexussynth_audio_utils_test test_audio_utils.cpp)
create_test_executable(nexussynth_compression_checksum_test test_compression_checksum.cpp)
create_test_executable(nexussynth_version_management_test test_version_management.cpp)
create_test_executable(nexussynth_utau_oto_parser_test test_utau_oto_parser.cpp)
create_test_executable(nexussynth_vcv_pattern_recognizer_test test_vcv_pattern_recognizer.cpp)
create_test_executable(nexussynth_midi_phoneme_integrator_test test_midi_phoneme_integrator.cpp)

# Create a comprehensive test suite that runs all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --parallel 4
    DEPENDS 
        nexussynth_overlap_add_test
        nexussynth_main_test
        nexussynth_window_optimization_test
        nexussynth_window_performance_test
        nexussynth_streaming_buffer_test
        nexussynth_performance_profiler_test
        nexussynth_utau_argument_parser_test
        nexussynth_utau_flag_converter_test
        nexussynth_utau_logger_test
        nexussynth_utau_error_handler_test
        nexussynth_voicebank_scanner_test
        nexussynth_world_wrapper_test
        nexussynth_audio_utils_test
        nexussynth_compression_checksum_test
        nexussynth_version_management_test
        nexussynth_utau_oto_parser_test
        nexussynth_vcv_pattern_recognizer_test
        nexussynth_midi_phoneme_integrator_test
        nexussynth_conditioning_config_test
    COMMENT "Running all NexusSynth tests"
)

# Include integration tests
add_subdirectory(integration)
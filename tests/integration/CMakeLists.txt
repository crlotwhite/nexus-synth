# Integration Tests for NexusSynth
# End-to-end testing of voice synthesis pipeline

# Create integration test data directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_results)

# Integration test configuration
set(INTEGRATION_TEST_TIMEOUT 300) # 5 minutes for long-running tests

# Voice bank conversion tests
set(CONVERSION_TEST_SOURCES
    conversion/test_voicebank_conversion.cpp
    conversion/test_nvm_generation.cpp
    conversion/test_conditioning_pipeline.cpp
)

# End-to-end synthesis tests  
set(SYNTHESIS_TEST_SOURCES
    synthesis/test_e2e_synthesis.cpp
    synthesis/test_utau_compatibility.cpp
    synthesis/test_parameter_validation.cpp
)

# Performance benchmark tests
set(BENCHMARK_TEST_SOURCES
    benchmarks/test_rendering_performance.cpp
    benchmarks/test_memory_usage.cpp
    benchmarks/test_scalability.cpp
)

# Quality comparison tests
set(QUALITY_TEST_SOURCES
    quality/test_quality_metrics.cpp
    quality/test_ab_comparison.cpp
    quality/test_spectral_analysis.cpp
)

# Test utility library
add_library(integration_test_utils STATIC
    utils/test_data_manager.cpp
    utils/audio_comparator.cpp
    utils/performance_monitor.cpp
    utils/quality_analyzer.cpp
)

target_include_directories(integration_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(integration_test_utils
    nexussynth_lib
    world
    Eigen3::Eigen
)

# Function to create integration test executable
function(create_integration_test TEST_NAME TEST_SOURCES)
    add_executable(${TEST_NAME} ${TEST_SOURCES})
    
    target_link_libraries(${TEST_NAME}
        nexussynth_lib
        integration_test_utils
        GTest::gtest_main
        GTest::gtest
        world
        Eigen3::Eigen
        asmjit::asmjit
        cjson
    )
    
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${TEST_NAME} OpenMP::OpenMP_CXX)
    endif()
    
    # Link pthread on Unix systems
    if(UNIX)
        target_link_libraries(${TEST_NAME} pthread)
    endif()
    
    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    # Register with CTest with extended timeout
    add_test(NAME ${TEST_NAME} 
             COMMAND ${TEST_NAME}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT ${INTEGRATION_TEST_TIMEOUT}
        LABELS "integration"
    )
endfunction()

# Create integration test executables
create_integration_test(integration_voicebank_conversion "conversion/test_voicebank_conversion.cpp")
create_integration_test(integration_nvm_generation "conversion/test_nvm_generation.cpp")
create_integration_test(integration_e2e_synthesis "synthesis/test_e2e_synthesis.cpp")
# create_integration_test(integration_utau_compatibility "synthesis/test_utau_compatibility.cpp")  # TODO: Create this file
create_integration_test(integration_rendering_performance "benchmarks/test_rendering_performance.cpp")
# create_integration_test(integration_quality_metrics "quality/test_quality_metrics.cpp")  # TODO: Create this file

# A/B comparison test executable
add_executable(integration_ab_comparison ab_comparison/test_ab_comparator.cpp)
target_link_libraries(integration_ab_comparison
    ab_comparator_lib
    integration_test_utils
    nexussynth_lib
    GTest::gtest_main
    GTest::gtest
    world
    Eigen3::Eigen
    asmjit::asmjit
    cjson
)

# A/B comparator library
add_library(ab_comparator_lib STATIC
    ab_comparison/ab_comparator.cpp
)
target_link_libraries(ab_comparator_lib
    integration_test_utils
    nexussynth_lib
)

# A/B comparison command-line tool
add_executable(ab_comparison_tool 
    ab_comparison/ab_comparison_tool.cpp
)
target_link_libraries(ab_comparison_tool
    ab_comparator_lib
    integration_test_utils
    nexussynth_lib
)

# Performance Benchmark Framework Library
add_library(performance_benchmark_lib STATIC
    benchmarks/performance_benchmark.cpp
    benchmarks/benchmark_data_collector.cpp
)
target_link_libraries(performance_benchmark_lib
    integration_test_utils
    nexussynth_lib
    world
    Eigen3::Eigen
)
target_include_directories(performance_benchmark_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
)

# Performance Benchmark Test Suite
add_executable(integration_performance_benchmark benchmarks/test_performance_benchmark.cpp)
target_link_libraries(integration_performance_benchmark
    performance_benchmark_lib
    integration_test_utils
    nexussynth_lib
    GTest::gtest_main
    GTest::gtest
    world
    Eigen3::Eigen
    asmjit::asmjit
    cjson
)

# Performance Benchmark CLI Tool
add_executable(performance_benchmark_tool benchmarks/performance_benchmark_tool.cpp)
target_link_libraries(performance_benchmark_tool
    performance_benchmark_lib
    integration_test_utils
    nexussynth_lib
)

# Custom targets for running test categories
add_custom_target(test_conversion
    COMMAND ${CMAKE_CTEST_COMMAND} -L "conversion" --verbose
    DEPENDS integration_voicebank_conversion integration_nvm_generation
    COMMENT "Running voice bank conversion tests"
)

add_custom_target(test_synthesis  
    COMMAND ${CMAKE_CTEST_COMMAND} -L "synthesis" --verbose
    DEPENDS integration_e2e_synthesis
    COMMENT "Running end-to-end synthesis tests"
)

add_custom_target(test_benchmarks
    COMMAND ${CMAKE_CTEST_COMMAND} -L "benchmarks" --verbose  
    DEPENDS integration_rendering_performance integration_performance_benchmark
    COMMENT "Running performance benchmark tests"
)

add_custom_target(test_quality
    COMMAND ${CMAKE_CTEST_COMMAND} -L "quality" --verbose
    DEPENDS integration_ab_comparison
    COMMENT "Running quality comparison tests"
)

# A/B comparison custom target
add_custom_target(test_ab_comparison
    COMMAND ${CMAKE_CTEST_COMMAND} -R "integration_ab_comparison" --verbose
    DEPENDS integration_ab_comparison
    COMMENT "Running A/B comparison tests"
)

# Performance benchmark custom target
add_custom_target(test_performance_benchmark
    COMMAND ${CMAKE_CTEST_COMMAND} -R "integration_performance_benchmark" --verbose
    DEPENDS integration_performance_benchmark
    COMMENT "Running comprehensive performance benchmarks"
)

# Benchmark CLI tool custom target
add_custom_target(run_benchmark_suite
    COMMAND performance_benchmark_tool --suite=full --output=${CMAKE_CURRENT_BINARY_DIR}/benchmark_results
    DEPENDS performance_benchmark_tool
    COMMENT "Running full benchmark suite with CLI tool"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Main integration test target
add_custom_target(integration_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -L "integration" --verbose --parallel 2
    DEPENDS 
        integration_voicebank_conversion
        integration_nvm_generation
        integration_e2e_synthesis
        integration_rendering_performance
        integration_ab_comparison
        integration_performance_benchmark
    COMMENT "Running all integration tests"
)

# Copy test configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/test_scenarios.json.in 
               ${CMAKE_CURRENT_BINARY_DIR}/config/test_scenarios.json @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/quality_thresholds.json.in
               ${CMAKE_CURRENT_BINARY_DIR}/config/quality_thresholds.json @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/ab_comparison_config.json.in
               ${CMAKE_CURRENT_BINARY_DIR}/config/ab_comparison_config.json @ONLY)

# Set test labels for categorization
set_tests_properties(integration_voicebank_conversion PROPERTIES LABELS "integration;conversion")
set_tests_properties(integration_nvm_generation PROPERTIES LABELS "integration;conversion")
set_tests_properties(integration_e2e_synthesis PROPERTIES LABELS "integration;synthesis")
# set_tests_properties(integration_utau_compatibility PROPERTIES LABELS "integration;synthesis")
set_tests_properties(integration_rendering_performance PROPERTIES LABELS "integration;benchmarks")
# set_tests_properties(integration_quality_metrics PROPERTIES LABELS "integration;quality")

# Register A/B comparison test with CTest
add_test(NAME integration_ab_comparison 
         COMMAND integration_ab_comparison
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
         
set_tests_properties(integration_ab_comparison PROPERTIES
    TIMEOUT ${INTEGRATION_TEST_TIMEOUT}
    LABELS "integration;ab_comparison;quality"
)

# Register Performance Benchmark test with CTest
add_test(NAME integration_performance_benchmark 
         COMMAND integration_performance_benchmark
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
         
set_tests_properties(integration_performance_benchmark PROPERTIES
    TIMEOUT ${INTEGRATION_TEST_TIMEOUT}
    LABELS "integration;performance;benchmarks"
)
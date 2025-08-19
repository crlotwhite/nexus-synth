# Library source files organized by functionality
set(NEXUSSYNTH_LIB_SOURCES
    # Core engine
    nexussynth_engine.cpp
    
    # Interface modules (UTAU compatibility)
    interface/utau_argument_parser.cpp
    interface/utau_oto_parser.cpp
    
    # Synthesis engine modules
    synthesis/pbp_synthesis_engine.cpp
    synthesis/streaming_buffer_manager.cpp
    synthesis/window_optimizer.cpp
    
    # Machine learning modules
    ml/hmm_trainer.cpp
    ml/mlpg_engine.cpp
    ml/gaussian_mixture.cpp
    ml/context_features.cpp
    ml/context_feature_extractor.cpp
    ml/context_hmm_bridge.cpp
    ml/data_augmentation.cpp
    ml/quality_metrics.cpp
    
    # Utility modules
    utils/audio_utils.cpp
    utils/performance_profiler.cpp
    utils/fft_transform_manager.cpp
    
    # I/O and format modules
    io/nvm_format.cpp
    io/voice_metadata.cpp
    io/world_wrapper.cpp
    io/world_parameter_extractor.cpp
    io/vcv_pattern_recognizer.cpp
    io/midi_phoneme_integrator.cpp
    io/label_file_generator.cpp
)

# Create library
add_library(nexussynth_lib STATIC ${NEXUSSYNTH_LIB_SOURCES})

# Set library properties
set_target_properties(nexussynth_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add include directories for public headers
target_include_directories(nexussynth_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Create main executable
add_executable(nexussynth main.cpp)

# Find zlib
find_package(ZLIB REQUIRED)

# Link dependencies to library
target_link_libraries(nexussynth_lib 
    world 
    Eigen3::Eigen
    asmjit::asmjit
    cjson
    ZLIB::ZLIB
)

# Link OpenMP if available
if(OpenMP_CXX_FOUND)
    target_link_libraries(nexussynth_lib OpenMP::OpenMP_CXX)
endif()

# Link library to executable
target_link_libraries(nexussynth nexussynth_lib)

# Set executable properties
set_target_properties(nexussynth PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Note: All test executables are now managed in tests/CMakeLists.txt
# Test files have been moved to the tests/ directory for better organization
# This separation keeps the main source directory clean and focused on implementation
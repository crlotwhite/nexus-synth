# Library source files
set(NEXUSSYNTH_LIB_SOURCES
    nexussynth_engine.cpp
    world_wrapper.cpp
    world_parameter_extractor.cpp
    audio_utils.cpp
    context_features.cpp
    voice_metadata.cpp
    gaussian_mixture.cpp
    nvm_format.cpp
    utau_oto_parser.cpp
    vcv_pattern_recognizer.cpp
    midi_phoneme_integrator.cpp
)

# Create library
add_library(nexussynth_lib STATIC ${NEXUSSYNTH_LIB_SOURCES})

# Set library properties
set_target_properties(nexussynth_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add dependency include directories
target_include_directories(nexussynth_lib PRIVATE 
    ${CMAKE_SOURCE_DIR}/external/WORLD/src
    ${CMAKE_SOURCE_DIR}/external/cJSON
)

# Add include directories for public headers
target_include_directories(nexussynth_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/cJSON
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

# Link library to executable
target_link_libraries(nexussynth nexussynth_lib)

# Set executable properties
set_target_properties(nexussynth PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create WORLD wrapper test executable
add_executable(test_world_wrapper test_world_wrapper.cpp)
target_link_libraries(test_world_wrapper nexussynth_lib)
set_target_properties(test_world_wrapper PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create audio utilities test executable
add_executable(test_audio_utils test_audio_utils.cpp)
target_link_libraries(test_audio_utils nexussynth_lib)
set_target_properties(test_audio_utils PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create compression and checksum test executable
add_executable(test_compression_checksum test_compression_checksum.cpp)
target_link_libraries(test_compression_checksum nexussynth_lib)
set_target_properties(test_compression_checksum PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create version management test executable
add_executable(test_version_management test_version_management.cpp)
target_link_libraries(test_version_management nexussynth_lib)
set_target_properties(test_version_management PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create UTAU oto.ini parser test executable
add_executable(test_utau_oto_parser test_utau_oto_parser.cpp)
target_link_libraries(test_utau_oto_parser nexussynth_lib)
set_target_properties(test_utau_oto_parser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create VCV pattern recognizer test executable
add_executable(test_vcv_pattern_recognizer test_vcv_pattern_recognizer.cpp)
target_link_libraries(test_vcv_pattern_recognizer nexussynth_lib)
set_target_properties(test_vcv_pattern_recognizer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create MIDI phoneme integrator test executable
add_executable(test_midi_phoneme_integrator test_midi_phoneme_integrator.cpp)
target_link_libraries(test_midi_phoneme_integrator nexussynth_lib)
set_target_properties(test_midi_phoneme_integrator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
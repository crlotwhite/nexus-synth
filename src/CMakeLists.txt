# Library source files
set(NEXUSSYNTH_LIB_SOURCES
    nexussynth_engine.cpp
    world_wrapper.cpp
    world_parameter_extractor.cpp
    audio_utils.cpp
    context_features.cpp
    voice_metadata.cpp
    gaussian_mixture.cpp
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

# Link dependencies to library
target_link_libraries(nexussynth_lib 
    world 
    Eigen3::Eigen
    asmjit::asmjit
    cjson
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
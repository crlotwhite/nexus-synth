# Library source files
set(NEXUSSYNTH_LIB_SOURCES
    nexussynth_engine.cpp
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
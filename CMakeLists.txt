cmake_minimum_required(VERSION 3.16)

# Project configuration
project(NexusSynth VERSION 1.0.0 LANGUAGES CXX)

# C++ standard configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific configuration
if(WIN32)
    # Windows x64 specific settings
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    
    # Enable DLL symbol exports for Windows
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    
    # Compiler flags for Windows x64
    if(MSVC)
        add_compile_options(/W4)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /D_DEBUG")
        
        # Windows-specific defines
        add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
        
        # Enable multiprocessor compilation
        add_compile_options(/MP)
        
        # Static runtime for easier distribution
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    
    # Windows x64 architecture check
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Building for Windows x64")
        add_definitions(-D_WIN64)
    else()
        message(WARNING "Building for Windows x86 - x64 is recommended for NexusSynth")
    endif()
else()
    # Cross-platform compiler flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Project directories
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Include directories
include_directories(${INCLUDE_DIR})

# External dependencies
add_subdirectory(external/WORLD)

# Eigen3 (header-only library)
add_subdirectory(external/Eigen)

# AsmJit (JIT compilation)
set(ASMJIT_STATIC TRUE)
add_subdirectory(external/asmjit)

# cJSON (JSON processing)
add_subdirectory(external/cJSON)

# Enable testing
enable_testing()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Generate pkg-config file
configure_file(nexussynth.pc.in "${CMAKE_BINARY_DIR}/nexussynth.pc" @ONLY)

# Install pkg-config file
install(FILES "${CMAKE_BINARY_DIR}/nexussynth.pc" 
        DESTINATION lib/pkgconfig)

# Install headers
install(DIRECTORY include/nexussynth 
        DESTINATION include)

# Install library
install(TARGETS nexussynth_lib 
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Project information
message(STATUS "Building ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
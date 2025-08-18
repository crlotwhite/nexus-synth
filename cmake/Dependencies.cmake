# Dependencies.cmake
# Central dependency management for NexusSynth project

# Include CPM
include(cmake/CPM.cmake)

message(STATUS "Setting up NexusSynth dependencies...")

# =============================================================================
# Eigen3 - Linear algebra library (header-only)
# =============================================================================
# Try to find system Eigen3 first
find_package(Eigen3 QUIET)

if(NOT Eigen3_FOUND)
  message(STATUS "System Eigen3 not found, downloading...")
  CPMAddPackage(
    NAME Eigen3
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    OPTIONS
      "BUILD_TESTING OFF"
      "EIGEN_BUILD_TESTING OFF"
      "EIGEN_BUILD_DOC OFF"
  )
  
  if(Eigen3_ADDED)
    # Create alias for Eigen3::Eigen target if it doesn't exist
    if(NOT TARGET Eigen3::Eigen)
      add_library(Eigen3::Eigen ALIAS eigen)
    endif()
    message(STATUS "Using downloaded Eigen3 3.4.0")
  endif()
else()
  message(STATUS "Using system Eigen3")
endif()

# =============================================================================
# WORLD Vocoder - Core vocoding library  
# =============================================================================
CPMAddPackage(
  NAME world
  GITHUB_REPOSITORY mmorise/World
  GIT_TAG master
  OPTIONS
    "BUILD_EXAMPLES OFF"
    "BUILD_TESTING OFF"
)

if(world_ADDED)
  message(STATUS "Using downloaded WORLD vocoder 0.3.2")
  
  # Ensure WORLD headers are available
  target_include_directories(world INTERFACE 
    $<BUILD_INTERFACE:${world_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/world>
  )
endif()

# =============================================================================
# AsmJit - JIT compilation for performance optimization
# =============================================================================
CPMAddPackage(
  NAME asmjit
  GITHUB_REPOSITORY asmjit/asmjit
  GIT_TAG master
  OPTIONS
    "ASMJIT_STATIC TRUE"
    "ASMJIT_BUILD_TEST OFF"
    "ASMJIT_BUILD_PERF OFF"
)

if(asmjit_ADDED)
  message(STATUS "Using downloaded AsmJit")
endif()

# =============================================================================
# cJSON - Lightweight JSON parser
# =============================================================================
CPMAddPackage(
  NAME cjson
  VERSION 1.7.18
  GITHUB_REPOSITORY DaveGamble/cJSON
  GIT_TAG v1.7.18
  OPTIONS
    "ENABLE_CJSON_TEST OFF"
    "ENABLE_CJSON_UTILS ON"
    "BUILD_SHARED_LIBS OFF"
    "ENABLE_CUSTOM_COMPILER_FLAGS OFF"
)

if(cjson_ADDED)
  message(STATUS "Using downloaded cJSON 1.7.18")
endif()

# =============================================================================
# Google Test - Only for testing builds
# =============================================================================
if(NEXUSSYNTH_BUILD_TESTS)
  # Try to find system GTest first
  find_package(GTest QUIET)
  
  if(NOT GTest_FOUND)
    message(STATUS "System GTest not found, downloading...")
    CPMAddPackage(
      NAME googletest
      VERSION 1.14.0
      GITHUB_REPOSITORY google/googletest
      GIT_TAG v1.14.0
      OPTIONS
        "INSTALL_GTEST OFF"
        "gtest_force_shared_crt ON"
    )
    
    if(googletest_ADDED)
      message(STATUS "Using downloaded Google Test 1.14.0")
    endif()
  else()
    message(STATUS "Using system Google Test")
  endif()
endif()

# =============================================================================
# Dependency Summary
# =============================================================================
message(STATUS "=== NexusSynth Dependencies Summary ===")
if(Eigen3_ADDED)
  message(STATUS "Eigen3: Downloaded")
else()
  message(STATUS "Eigen3: System")
endif()
if(world_ADDED)
  message(STATUS "WORLD: Downloaded")
endif()
if(asmjit_ADDED)
  message(STATUS "AsmJit: Downloaded")
endif()
if(cjson_ADDED)
  message(STATUS "cJSON: Downloaded")
endif()
if(NEXUSSYNTH_BUILD_TESTS)
  if(googletest_ADDED)
    message(STATUS "GoogleTest: Downloaded")
  else()
    message(STATUS "GoogleTest: System")
  endif()
endif()
message(STATUS "========================================")
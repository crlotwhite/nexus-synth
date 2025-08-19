# CPM.cmake - CMake Package Manager
# Version: 0.40.2
#
# This file provides CPM functionality for managing C++ dependencies
# See: https://github.com/cpm-cmake/CPM.cmake for full documentation

set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_HASH_SUM "c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d")

if(CPM_DOWNLOAD_LOCATION)
  # Externally provided location
  set(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION})
else()
  # Default download location
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Get CPM.cmake if it doesn't exist
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD 
       https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
       ${CPM_DOWNLOAD_LOCATION}
       EXPECTED_HASH SHA256=${CPM_HASH_SUM}
       SHOW_PROGRESS
  )
endif()

# Include CPM functionality
message(STATUS "Including CPM from: ${CPM_DOWNLOAD_LOCATION}")
include(${CPM_DOWNLOAD_LOCATION})

# Set default cache directory for offline builds
if(NOT DEFINED CPM_SOURCE_CACHE)
  set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cpm_cache")
endif()

message(STATUS "Using CPM source cache: ${CPM_SOURCE_CACHE}")
# NexusSynth Dependencies

NexusSynth uses **CPM (CMake Package Manager)** for modern, automatic dependency management.

## Dependencies Overview

| Library | Version | Purpose | License |
|---------|---------|---------|---------|
| **WORLD** | 0.3.2 | Vocoder analysis/synthesis | Modified BSD |
| **Eigen3** | 3.4.0 | Linear algebra operations | MPL2 |
| **AsmJit** | 1.0.0 | JIT compilation optimization | Zlib |
| **cJSON** | 1.7.18 | JSON configuration parsing | MIT |
| **Google Test** | 1.14.0 | Unit testing framework | BSD-3-Clause |

## Build Process

### Standard Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Build Options
```bash
# Disable tests (faster build)
cmake -DNEXUSSYNTH_BUILD_TESTS=OFF ..

# Use system packages when available
cmake -DNEXUSSYNTH_USE_SYSTEM_DEPS=ON ..

# Specify CPM cache directory
cmake -DCPM_SOURCE_CACHE=/path/to/cache ..
```

## Offline Builds

CPM automatically caches downloaded dependencies in `.cpm_cache/` directory:

```bash
# First build downloads dependencies
mkdir build && cd build
cmake ..  # Downloads to .cpm_cache/
make

# Subsequent builds use cached dependencies
rm -rf build && mkdir build && cd build
cmake ..  # Uses .cpm_cache/ (no network required)
make
```

## Dependency Details

### WORLD Vocoder Library
- **Purpose**: F0/SP/AP analysis and synthesis
- **Integration**: Direct CMake integration
- **Headers**: Available as `#include "world/*.h"`

### Eigen3 Linear Algebra
- **Purpose**: Matrix operations, FFT, numerical computation
- **Integration**: Header-only library with `Eigen3::Eigen` target
- **Fallback**: Uses system-installed Eigen3 if available

### AsmJit JIT Compiler
- **Purpose**: Runtime code generation for performance optimization  
- **Integration**: Static library with `asmjit::asmjit` target
- **Usage**: Optimized DSP kernels, vectorized operations

### cJSON Parser
- **Purpose**: Configuration file parsing (.nvm format)
- **Integration**: Static library with utilities
- **Features**: Lightweight, fast JSON processing

## Migration from Submodules

If upgrading from the old git submodule system:

```bash
# Run the migration script
./migrate_dependencies.sh

# Or manually:
git submodule deinit --all -f
rm -rf .git/modules/external external
rm .gitmodules
```

## Troubleshooting

### Network Issues
```bash
# Use system packages instead
cmake -DNEXUSSYNTH_USE_SYSTEM_DEPS=ON ..

# Or set up offline cache
export CPM_SOURCE_CACHE=$HOME/.cpm_cache
```

### Build Failures
```bash
# Clean CPM cache
rm -rf .cpm_cache build

# Rebuild with verbose output
mkdir build && cd build
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
make VERBOSE=1
```

### Missing System Dependencies
```bash
# Ubuntu/Debian
sudo apt-get install libeigen3-dev libgtest-dev

# macOS
brew install eigen googletest

# Arch Linux
sudo pacman -S eigen gtest
```

## Advanced Configuration

### Custom Dependency Versions
Edit `cmake/Dependencies.cmake` to modify versions:

```cmake
CPMAddPackage(
  NAME world
  VERSION 0.3.3  # Change version here
  GITHUB_REPOSITORY mmorise/World
  GIT_TAG v0.3.3_1
)
```

### Development Dependencies
For development builds with latest dependency versions:

```bash
cmake -DCPM_USE_LOCAL_PACKAGES=OFF ..
```

## Performance Considerations

- **First build**: Downloads dependencies (~50MB total)
- **Cached builds**: No network required, faster configuration
- **System packages**: Fastest, but version may vary
- **Parallel builds**: Use `-j$(nproc)` for optimal speed

## License Compliance

All dependencies are compatible with NexusSynth's licensing:
- WORLD: Modified BSD (permissive)
- Eigen3: MPL2 (weak copyleft, header-only)
- AsmJit: Zlib (permissive) 
- cJSON: MIT (permissive)
- Google Test: BSD-3-Clause (testing only)
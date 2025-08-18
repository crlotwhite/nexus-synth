# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**NexusSynth** is a next-generation vocal synthesis resampler engine for the UTAU community, written primarily in C++ with Korean documentation. It combines modern parametric synthesis with statistical modeling to overcome limitations of existing concatenative synthesis approaches.

### Core Technologies
- **Language**: C++ (C++17 standard)
- **Build System**: CMake (minimum version 3.16)
- **Target Platform**: Windows x64 (with cross-platform considerations)
- **Key Dependencies**:
  - WORLD vocoder library (F0/SP/AP analysis)
  - Eigen (matrix operations)
  - Asmjit (JIT compilation)
  - cJSON (configuration serialization)
  - HTS Engine (statistical parameter generation)

### Architecture
- **Hybrid Parametric/Statistical Synthesis Engine**: WORLD vocoder-based source-filter model with HTS-based statistical parameter generation
- **Dual-stage Processing**: Offline voice bank conditioning + real-time synthesis
- **Pulse-by-Pulse (PbP) Synthesis**: Inspired by libllsm2 approach
- **JIT Optimization**: Runtime optimization with SIMD acceleration

## Development Setup

### Project Structure
```
nexus-synth/
├── src/                    # Main source code
├── include/nexussynth/     # Public API headers
├── lib/                    # External dependencies
├── tests/                  # Unit tests
├── build/                  # CMake build artifacts
└── .taskmaster/           # Task management system
```

### Build Commands
The project uses CMake with CPM (CMake Package Manager) for automatic dependency management:

```bash
mkdir build && cd build
cmake ..                    # Downloads dependencies automatically on first run
make -j$(nproc)            # Build with parallel jobs
```

**Build Options:**
```bash
cmake -DNEXUSSYNTH_BUILD_TESTS=OFF ..     # Disable tests for faster builds
cmake -DNEXUSSYNTH_USE_SYSTEM_DEPS=ON ..  # Prefer system-installed dependencies  
```

**Dependencies are auto-downloaded**: WORLD vocoder, Eigen3, AsmJit, cJSON, Google Test

### Task Management Commands
This project heavily uses **Task Master AI** for development workflow:

```bash
# Core workflow
task-master list                    # Show all tasks
task-master next                    # Get next available task
task-master show <id>              # View task details (e.g., 1.2)
task-master set-status --id=<id> --status=done

# Task development
task-master expand --id=<id> --research
task-master update-subtask --id=<id> --prompt="implementation notes"
```

### Current Development State
- **Phase**: Early development/planning stage
- **Status**: 10 major tasks broken into 77+ subtasks
- **Priority Areas**: Build system setup, WORLD vocoder integration, HMM model design
- **Language Preference**: Korean for communication and documentation

## Key Technical Areas

### High-Priority Components (Status: Pending)
1. **Project Setup & Build System** (Task 1) - CMake configuration and dependency integration
2. **WORLD Vocoder Integration** (Task 2) - F0/SP/AP analysis pipeline
3. **HMM Model Structure** (Task 3) - Statistical modeling framework
4. **UTAU Interface** (Task 8) - Drop-in resampler compatibility

### Complex Implementation Areas
- **HMM Model Training System** (Task 5, Complexity: 9/10) - Statistical ML implementation
- **MLPG Parameter Generation** (Task 6) - Trajectory optimization
- **Real-time Synthesis Engine** (Task 7) - Performance-critical audio processing

## UTAU Ecosystem Integration

### Compatibility Requirements
- **100% UTAU compatibility**: Drop-in replacement for existing resamplers
- **Standard flag support**: g, bre, bri, t flags for expression control
- **File format support**: 
  - Input: Standard UTAU voice banks with oto.ini
  - Output: .nvm optimized model format

### Quality Goals
- Eliminate "chipmunk effect" in large pitch conversions
- Smooth phoneme transitions at boundaries
- Enhanced expressiveness over existing resamplers

## Task Master AI Instructions
**Import Task Master's development workflow commands and guidelines, treat as if import is in the main CLAUDE.md file.**
@./.taskmaster/CLAUDE.md

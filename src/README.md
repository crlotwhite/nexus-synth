# NexusSynth Source Code Structure

## Directory Organization

The source code is organized into functional modules for better maintainability and code navigation:

### 📁 `interface/` - UTAU Compatibility Layer
- `utau_argument_parser.cpp` - Command line argument parsing with Windows/UTF-8 support
- `utau_oto_parser.cpp` - UTAU oto.ini configuration file parser

### 📁 `synthesis/` - Audio Synthesis Engine
- `pbp_synthesis_engine.cpp` - Pulse-by-Pulse (PbP) synthesis engine
- `streaming_buffer_manager.cpp` - Real-time audio streaming buffer management
- `window_optimizer.cpp` - Window function optimization for overlap-add synthesis

### 📁 `ml/` - Machine Learning & Statistical Models
- `hmm_trainer.cpp` - Hidden Markov Model training engine
- `mlpg_engine.cpp` - Maximum Likelihood Parameter Generation
- `gaussian_mixture.cpp` - Gaussian Mixture Model implementation
- `context_features.cpp` - Context feature extraction and processing
- `context_feature_extractor.cpp` - Feature extraction utilities
- `context_hmm_bridge.cpp` - Bridge between context and HMM systems
- `data_augmentation.cpp` - Training data augmentation
- `quality_metrics.cpp` - Audio quality assessment metrics

### 📁 `utils/` - Utility Functions
- `audio_utils.cpp` - Audio processing utilities
- `performance_profiler.cpp` - Performance monitoring and profiling
- `fft_transform_manager.cpp` - FFT transform management and optimization

### 📁 `io/` - Input/Output & File Formats
- `nvm_format.cpp` - NexusSynth Voice Model (.nvm) format handler
- `voice_metadata.cpp` - Voice bank metadata management
- `world_wrapper.cpp` - WORLD vocoder library wrapper
- `world_parameter_extractor.cpp` - WORLD parameter extraction utilities
- `vcv_pattern_recognizer.cpp` - VCV (Vowel-Consonant-Vowel) pattern recognition
- `midi_phoneme_integrator.cpp` - MIDI note to phoneme integration
- `label_file_generator.cpp` - HTS-style label file generation

### 📁 Root Level Files
- `nexussynth_engine.cpp` - Main synthesis engine coordinator
- `main.cpp` - Application entry point
- `CMakeLists.txt` - Build configuration

## Build System

The CMakeLists.txt file is organized to reflect this structure:

```cmake
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
# ... etc
```

## Dependencies

Each module has well-defined dependencies:
- **Interface**: Standard library, filesystem support
- **Synthesis**: WORLD vocoder, Eigen3, AsmJit
- **ML**: Eigen3, OpenMP (optional)
- **Utils**: Standard library, platform-specific optimizations
- **IO**: WORLD vocoder, cJSON, filesystem support

## Testing

All tests have been moved to the `../tests/` directory for cleaner separation of concerns. Each module can be tested independently using the corresponding test executables.
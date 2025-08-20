# Integration Test Suite for NexusSynth

This directory contains end-to-end integration tests for the NexusSynth voice synthesis engine.

## Test Categories

### 1. Voice Bank Conversion Tests (`conversion/`)
- Tests full pipeline from UTAU voice banks to .nvm format
- Validates conditioning and preprocessing quality
- Tests various voice bank types and configurations

### 2. End-to-End Synthesis Tests (`synthesis/`)
- Tests complete UTAU resampler interface workflow
- Validates audio quality and synthesis accuracy
- Tests various pitch, tempo, and expression parameters

### 3. Performance Benchmarks (`benchmarks/`)
- Measures rendering speed and resource usage
- Compares against baseline performance metrics
- Tests scalability and memory efficiency

### 4. Quality Comparison Tests (`quality/`)
- A/B testing against reference implementations (moresampler)
- Objective quality metrics (SNR, spectral similarity)
- Subjective quality evaluation framework

### 5. Regression Tests (`regression/`)
- Automated detection of quality/performance degradation
- Version-to-version consistency validation
- Critical functionality preservation tests

## Test Data Structure

```
test_data/
├── voice_banks/          # Standard UTAU voice banks for testing
│   ├── japanese/
│   │   ├── basic_cv/     # Basic CV voice bank
│   │   ├── vcv/          # VCV voice bank  
│   │   └── vccv/         # VCCV voice bank
│   ├── english/
│   └── multilingual/
├── reference_audio/      # Expected output samples
├── test_scenarios/       # Predefined test cases
└── benchmarks/          # Performance baseline data
```

## Running Tests

```bash
# Run all integration tests
make integration_tests

# Run specific test category
make test_conversion
make test_synthesis
make test_benchmarks

# Run with detailed output
ctest -V --parallel 4
```

## Test Configuration

Tests are configured through JSON files in `config/` directory:
- `test_scenarios.json` - Defines test cases and parameters
- `quality_thresholds.json` - Quality acceptance criteria
- `performance_baselines.json` - Performance regression thresholds
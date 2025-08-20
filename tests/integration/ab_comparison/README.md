# A/B Quality Comparison System

The A/B Quality Comparison System provides objective evaluation and comparison of different vocal synthesis resamplers, specifically designed to compare NexusSynth against existing tools like moresampler.

## Overview

This system implements advanced audio quality metrics and statistical analysis to provide objective, reproducible comparisons between synthesis systems. It extends the integration testing framework with specialized A/B comparison capabilities.

## Features

### Advanced Quality Metrics

- **Basic Audio Metrics**
  - Signal-to-Noise Ratio (SNR)
  - Audio similarity score
  - Spectral similarity analysis
  - Cross-correlation analysis

- **Advanced Vocal Synthesis Metrics**
  - **Mel Cepstral Distortion (MCD)** - Standard measure for speech quality assessment
  - **F0 RMSE** - Fundamental frequency error measurement
  - **Spectral Distortion** - Frequency domain quality analysis
  - **Formant Deviation** - Vocal formant preservation measurement
  - **Phase Coherence** - Phase relationship quality assessment
  - **Perceptual Quality Measures** - Roughness and brightness scoring

### Statistical Analysis

- **Statistical Significance Testing** - T-test based comparison with p-values
- **Confidence Intervals** - 95% confidence intervals for all metrics
- **Win Rate Analysis** - Comprehensive comparison winner analysis
- **Multi-metric Aggregation** - Combined quality scoring across metrics

### Reporting & Visualization

- **HTML Reports** - Interactive visual reports with charts and tables
- **CSV Export** - Raw data export for external analysis
- **Statistical Summary** - Detailed statistical analysis reports
- **Performance Metrics** - Render time and resource usage comparison

## Architecture

### Core Components

```
ab_comparison/
├── ab_comparator.h/.cpp       # Core A/B comparison engine
├── ab_comparison_tool.cpp     # Command-line interface
├── test_ab_comparator.cpp     # Unit tests
└── README.md                  # This documentation
```

### Key Classes

- **`ABComparator`** - Main comparison engine
- **`AdvancedQualityMetrics`** - Advanced audio quality metrics
- **`ABComparisonResult`** - Comparison result container
- **`ABComparisonConfig`** - Configuration management

## Usage

### Command Line Tool

The A/B comparison tool provides a comprehensive CLI interface:

```bash
# Single file comparison
./ab_comparison_tool single input.wav reference.wav -o ./reports --html --csv

# Batch comparison of multiple voice banks
./ab_comparison_tool batch ./voice_banks/ -o ./reports --statistical-analysis

# Using custom configuration
./ab_comparison_tool -c config.json batch ./test_data/ --html --csv
```

### Configuration

The system uses JSON configuration files to define comparison parameters:

```json
{
  "system_a": {
    "name": "NexusSynth",
    "executable_path": "nexussynth",
    "command_args": ["synthesize", "--input", "{INPUT}", "--output", "{OUTPUT}"]
  },
  "system_b": {
    "name": "moresampler",
    "executable_path": "moresampler.exe",
    "command_args": ["{INPUT}", "{OUTPUT}", "C4", "100", "0", "0", "0"]
  },
  "test_parameters": {
    "repetitions_per_test": 5,
    "significance_threshold": 0.05
  }
}
```

### Programmatic API

```cpp
#include "ab_comparator.h"

using namespace nexussynth::integration_test;

// Create comparator
ABComparator comparator;
comparator.load_config("ab_config.json");

// Single comparison
ABComparisonResult result = comparator.compare_single_test("input.wav", "reference.wav");

if (result.comparison_successful) {
    std::cout << "Winner: " << result.winner << std::endl;
    std::cout << "Quality difference: " << result.overall_quality_difference << std::endl;
}

// Batch comparison
std::vector<std::string> test_files = {"test1.wav", "test2.wav", "test3.wav"};
auto batch_results = comparator.compare_batch(test_files, "./reports/batch_report");

// Statistical analysis
std::string analysis_report;
comparator.perform_statistical_analysis(batch_results, analysis_report);
std::cout << analysis_report << std::endl;
```

## Quality Metrics Detail

### Mel Cepstral Distortion (MCD)

MCD is the industry standard for evaluating speech synthesis quality:
- **Range**: 0-20+ (lower is better)
- **Typical Values**: High-quality synthesis < 6.0 MCD
- **Calculation**: Distance between mel-frequency cepstral coefficients
- **Use Case**: Overall spectral envelope quality assessment

### F0 RMSE (Fundamental Frequency Root Mean Square Error)

Measures pitch accuracy:
- **Range**: 0-∞ Hz (lower is better)  
- **Typical Values**: Good synthesis < 20 Hz RMSE
- **Calculation**: RMS difference in fundamental frequency contours
- **Use Case**: Pitch conversion accuracy evaluation

### Spectral Distortion

Evaluates frequency domain preservation:
- **Range**: 0-1 (lower is better)
- **Calculation**: Log spectral distance across frequency bands
- **Use Case**: Voice character preservation assessment

### Formant Deviation

Measures vocal tract characteristic preservation:
- **Range**: 0-1 (lower is better)
- **Calculation**: Distance between formant frequencies
- **Use Case**: Voice identity preservation evaluation

## Integration with Testing Framework

The A/B comparison system integrates seamlessly with the existing integration testing framework:

### CMake Integration

```cmake
# Build A/B comparison tests
make integration_ab_comparison

# Run A/B comparison tests
make test_ab_comparison

# Build command-line tool
make ab_comparison_tool
```

### Test Execution

```bash
# Run all integration tests (includes A/B comparison)
ctest -L integration

# Run only A/B comparison tests
ctest -R integration_ab_comparison

# Run with verbose output
ctest -R integration_ab_comparison --verbose
```

## Output Reports

### HTML Report Structure

The HTML reports include:
- **Summary Statistics** - Overall win rates and averages
- **Detailed Results Table** - Per-test metrics and winners
- **Statistical Analysis** - Significance testing and confidence intervals
- **Performance Comparison** - Render times and resource usage
- **Visual Charts** - Metric distribution graphs (when enabled)

### CSV Export Format

```csv
Test,SystemA_SNR,SystemB_SNR,SystemA_Similarity,SystemB_Similarity,SystemA_Time_ms,SystemB_Time_ms,Winner
1,22.5,20.1,0.85,0.78,150,200,NexusSynth
2,24.1,21.3,0.88,0.82,145,195,NexusSynth
...
```

## Performance Considerations

### Computational Requirements

- **Memory Usage**: ~100-200MB for typical voice bank comparison
- **CPU Requirements**: Multi-threaded analysis, 2+ cores recommended
- **Storage**: ~10MB per comparison report (with full analysis)
- **Runtime**: ~30-60 seconds per voice bank comparison (5 repetitions)

### Optimization Features

- **Parallel Processing** - Multi-threaded metric calculation
- **Batch Processing** - Efficient handling of multiple test files
- **Memory Management** - RAII-based cleanup of temporary files
- **Configurable Repetitions** - Adjustable statistical confidence vs. speed

## Testing & Validation

### Unit Test Coverage

The system includes comprehensive unit tests covering:

- Configuration loading and validation
- Advanced metrics calculation accuracy  
- Single and batch comparison execution
- Statistical analysis correctness
- Report generation functionality
- Error handling and edge cases
- Performance metrics validation

### Quality Assurance

- **Metric Validation** - Comparison against reference implementations
- **Statistical Verification** - Validation of statistical analysis methods  
- **Cross-platform Testing** - Windows/Linux/macOS compatibility
- **Performance Benchmarks** - Consistent performance measurement

## Extension Points

### Custom Metrics

Add new quality metrics by extending `AdvancedQualityMetrics`:

```cpp
struct AdvancedQualityMetrics {
    // ... existing metrics ...
    double custom_vocal_quality_metric = 0.0;
    double custom_prosody_score = 0.0;
};
```

### Custom Systems

Add support for new resampler systems via configuration:

```json
{
  "system_c": {
    "name": "NewResampler",
    "executable_path": "new_resampler",
    "command_args": ["--synth", "{INPUT}", "--output", "{OUTPUT}"]
  }
}
```

### Report Formats

Extend report generation with new formats:

```cpp
class ABComparator {
    // ... existing methods ...
    bool generate_json_report(const std::vector<ABComparisonResult>& results, 
                             const std::string& output_path);
    bool generate_xml_report(const std::vector<ABComparisonResult>& results,
                            const std::string& output_path);
};
```

## Troubleshooting

### Common Issues

1. **Configuration Not Loading**
   - Check JSON syntax in configuration file
   - Verify file permissions and path accessibility

2. **Resampler Execution Failures**
   - Verify executable paths in configuration
   - Check command-line argument formatting
   - Ensure input files are accessible

3. **Metric Calculation Errors**
   - Verify audio file format compatibility
   - Check for corrupted or empty audio files
   - Ensure sufficient disk space for temporary files

4. **Statistical Analysis Issues**
   - Ensure sufficient sample size (≥3 repetitions recommended)
   - Check for extreme outliers in results
   - Verify statistical significance thresholds

### Debug Mode

Enable verbose logging for detailed troubleshooting:

```bash
./ab_comparison_tool batch ./test_data/ --verbose -o ./debug_reports
```

## Future Enhancements

### Planned Features

- **Real-time Comparison** - Live A/B testing during synthesis
- **Perceptual Metrics** - PESQ/STOI integration for perceptual quality
- **Machine Learning Integration** - ML-based quality prediction
- **Visual Spectrograms** - Integrated spectrogram comparison
- **Cloud Integration** - Distributed testing capabilities

### Research Integration

- **Academic Metric Integration** - Support for latest research metrics  
- **Crowd-sourced Evaluation** - Integration with human evaluation systems
- **Multi-language Support** - Extended language-specific quality metrics
- **Cross-domain Evaluation** - Support for singing voice synthesis evaluation

---

This A/B comparison system provides a robust foundation for objective quality evaluation in vocal synthesis systems, enabling data-driven development and validation of NexusSynth's quality improvements over existing resamplers.
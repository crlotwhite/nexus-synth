#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include "nexussynth/pbp_synthesis_engine.h"
#include "nexussynth/world_wrapper.h"

using namespace nexussynth;
using namespace nexussynth::synthesis;

/**
 * @brief Generate synthetic WORLD parameters for testing
 */
AudioParameters generate_test_parameters(int sample_rate = 44100, double duration_seconds = 1.0) {
    AudioParameters params;
    params.sample_rate = sample_rate;
    params.frame_period = 5.0;
    params.fft_size = 2048;
    
    int num_frames = static_cast<int>(duration_seconds * 1000.0 / params.frame_period);
    params.length = num_frames;
    
    // Generate synthetic F0 contour (simple sine wave pattern)
    params.f0.resize(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        double time = i * params.frame_period / 1000.0;
        params.f0[i] = 220.0 + 50.0 * std::sin(2.0 * M_PI * 2.0 * time); // F0 around 220Hz with vibrato
    }
    
    // Generate synthetic spectral envelope
    int spectrum_size = params.fft_size / 2 + 1;
    params.spectrum.resize(num_frames);
    for (int frame = 0; frame < num_frames; ++frame) {
        params.spectrum[frame].resize(spectrum_size);
        for (int bin = 0; bin < spectrum_size; ++bin) {
            double freq = static_cast<double>(bin) * sample_rate / params.fft_size;
            
            // Create formant-like structure
            double formant1 = 800.0;  // F1
            double formant2 = 1200.0; // F2
            double formant3 = 2600.0; // F3
            
            double env1 = std::exp(-std::pow((freq - formant1) / 150.0, 2));
            double env2 = 0.7 * std::exp(-std::pow((freq - formant2) / 200.0, 2));
            double env3 = 0.5 * std::exp(-std::pow((freq - formant3) / 300.0, 2));
            
            params.spectrum[frame][bin] = env1 + env2 + env3;
            
            // Add high frequency rolloff
            if (freq > 4000.0) {
                params.spectrum[frame][bin] *= std::exp(-(freq - 4000.0) / 2000.0);
            }
        }
    }
    
    // Generate synthetic aperiodicity
    params.aperiodicity.resize(num_frames);
    for (int frame = 0; frame < num_frames; ++frame) {
        params.aperiodicity[frame].resize(spectrum_size);
        for (int bin = 0; bin < spectrum_size; ++bin) {
            double freq = static_cast<double>(bin) * sample_rate / params.fft_size;
            
            // Higher aperiodicity at high frequencies
            if (freq < 1000.0) {
                params.aperiodicity[frame][bin] = 0.1;
            } else {
                params.aperiodicity[frame][bin] = 0.1 + 0.4 * (freq - 1000.0) / (sample_rate / 2.0 - 1000.0);
            }
        }
    }
    
    // Generate time axis
    params.time_axis.resize(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        params.time_axis[i] = i * params.frame_period / 1000.0;
    }
    
    return params;
}

/**
 * @brief Test basic synthesis functionality
 */
bool test_basic_synthesis() {
    std::cout << "\n=== Testing Basic PbP Synthesis ===" << std::endl;
    
    try {
        // Create synthesis engine with default configuration
        PbpConfig config;
        config.sample_rate = 44100;
        config.fft_size = 1024;
        config.max_harmonics = 50;
        
        PbpSynthesisEngine engine(config);
        
        // Generate test parameters
        AudioParameters test_params = generate_test_parameters(44100, 0.5); // 0.5 seconds
        
        std::cout << "Generated test parameters:" << std::endl;
        std::cout << "  Frames: " << test_params.length << std::endl;
        std::cout << "  F0 range: " << 
                     *std::min_element(test_params.f0.begin(), test_params.f0.end()) << " - " <<
                     *std::max_element(test_params.f0.begin(), test_params.f0.end()) << " Hz" << std::endl;
        std::cout << "  Spectrum size: " << test_params.spectrum[0].size() << std::endl;
        
        // Perform synthesis
        SynthesisStats stats;
        std::vector<double> synthesized_audio = engine.synthesize(test_params, &stats);
        
        std::cout << "Synthesis completed:" << std::endl;
        std::cout << "  Output samples: " << synthesized_audio.size() << std::endl;
        std::cout << "  Synthesis time: " << stats.synthesis_time_ms << " ms" << std::endl;
        std::cout << "  Average frame time: " << stats.average_frame_time_ms << " ms" << std::endl;
        std::cout << "  Peak frame time: " << stats.peak_frame_time_ms << " ms" << std::endl;
        std::cout << "  Harmonics generated: " << stats.harmonics_generated << std::endl;
        std::cout << "  Harmonic energy ratio: " << stats.harmonic_energy_ratio << std::endl;
        
        // Basic validation
        if (synthesized_audio.empty()) {
            std::cerr << "ERROR: Synthesis produced empty output" << std::endl;
            return false;
        }
        
        // Check for reasonable signal levels
        double max_amplitude = *std::max_element(synthesized_audio.begin(), synthesized_audio.end());
        double min_amplitude = *std::min_element(synthesized_audio.begin(), synthesized_audio.end());
        double peak_amplitude = std::max(std::abs(max_amplitude), std::abs(min_amplitude));
        
        std::cout << "  Peak amplitude: " << peak_amplitude << std::endl;
        
        if (peak_amplitude < 1e-6) {
            std::cerr << "ERROR: Synthesis output amplitude too low" << std::endl;
            return false;
        }
        
        if (peak_amplitude > 10.0) {
            std::cerr << "ERROR: Synthesis output amplitude too high" << std::endl;
            return false;
        }
        
        std::cout << "✓ Basic synthesis test passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in basic synthesis test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test different window functions
 */
bool test_window_functions() {
    std::cout << "\n=== Testing Window Functions ===" << std::endl;
    
    try {
        AudioParameters test_params = generate_test_parameters(44100, 0.2);
        
        std::vector<PbpConfig::WindowType> window_types = {
            PbpConfig::WindowType::HANN,
            PbpConfig::WindowType::HAMMING,
            PbpConfig::WindowType::BLACKMAN,
            PbpConfig::WindowType::GAUSSIAN
        };
        
        std::vector<std::string> window_names = {
            "Hann", "Hamming", "Blackman", "Gaussian"
        };
        
        for (size_t i = 0; i < window_types.size(); ++i) {
            PbpConfig config;
            config.window_type = window_types[i];
            config.sample_rate = 44100;
            
            PbpSynthesisEngine engine(config);
            SynthesisStats stats;
            
            std::vector<double> result = engine.synthesize(test_params, &stats);
            
            std::cout << "  " << window_names[i] << " window:" << std::endl;
            std::cout << "    Synthesis time: " << stats.synthesis_time_ms << " ms" << std::endl;
            std::cout << "    Temporal smoothness: " << stats.temporal_smoothness << std::endl;
            
            if (result.empty()) {
                std::cerr << "ERROR: " << window_names[i] << " window synthesis failed" << std::endl;
                return false;
            }
        }
        
        std::cout << "✓ Window function tests passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in window function test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test single pulse synthesis
 */
bool test_single_pulse() {
    std::cout << "\n=== Testing Single Pulse Synthesis ===" << std::endl;
    
    try {
        PbpConfig config;
        PbpSynthesisEngine engine(config);
        
        // Create single pulse parameters
        PulseParams pulse_params;
        pulse_params.f0 = 440.0; // A4
        pulse_params.amplitude_scale = 1.0;
        
        // Generate spectrum for single pulse
        int spectrum_size = config.fft_size / 2 + 1;
        pulse_params.spectrum.resize(spectrum_size);
        pulse_params.aperiodicity.resize(spectrum_size);
        
        for (int bin = 0; bin < spectrum_size; ++bin) {
            double freq = static_cast<double>(bin) * config.sample_rate / config.fft_size;
            
            // Simple resonant filter
            pulse_params.spectrum[bin] = std::exp(-std::pow((freq - 1000.0) / 300.0, 2));
            pulse_params.aperiodicity[bin] = 0.2;
        }
        
        // Synthesize single pulse
        std::vector<double> pulse_waveform = engine.synthesize_pulse(pulse_params, 0.0);
        
        std::cout << "Single pulse synthesis:" << std::endl;
        std::cout << "  Pulse length: " << pulse_waveform.size() << " samples" << std::endl;
        
        if (pulse_waveform.empty()) {
            std::cerr << "ERROR: Single pulse synthesis failed" << std::endl;
            return false;
        }
        
        double pulse_energy = 0.0;
        for (double sample : pulse_waveform) {
            pulse_energy += sample * sample;
        }
        pulse_energy = std::sqrt(pulse_energy / pulse_waveform.size());
        
        std::cout << "  RMS energy: " << pulse_energy << std::endl;
        
        if (pulse_energy < 1e-6) {
            std::cerr << "ERROR: Pulse energy too low" << std::endl;
            return false;
        }
        
        std::cout << "✓ Single pulse synthesis test passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in single pulse test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test performance benchmarking
 */
bool test_performance_benchmark() {
    std::cout << "\n=== Testing Performance Benchmark ===" << std::endl;
    
    try {
        // Generate longer test signal
        AudioParameters test_params = generate_test_parameters(44100, 2.0); // 2 seconds
        
        PbpConfig config;
        config.sample_rate = 44100;
        config.use_fast_fft = true;
        
        PbpSynthesisEngine engine(config);
        
        // Benchmark synthesis performance
        auto benchmark_function = [&]() {
            return engine.synthesize(test_params);
        };
        
        SynthesisStats benchmark_stats = pbp_utils::benchmark_synthesis_performance(benchmark_function, 5);
        
        std::cout << "Performance benchmark (5 iterations):" << std::endl;
        std::cout << "  Total time: " << benchmark_stats.synthesis_time_ms << " ms" << std::endl;
        std::cout << "  Average time: " << benchmark_stats.average_frame_time_ms << " ms" << std::endl;
        std::cout << "  Peak time: " << benchmark_stats.peak_frame_time_ms << " ms" << std::endl;
        
        // Calculate real-time factor
        double audio_duration_ms = 2000.0; // 2 seconds
        double realtime_factor = audio_duration_ms / benchmark_stats.average_frame_time_ms;
        
        std::cout << "  Real-time factor: " << realtime_factor << "x" << std::endl;
        
        if (realtime_factor < 1.0) {
            std::cout << "  WARNING: Synthesis slower than real-time" << std::endl;
        } else {
            std::cout << "  ✓ Synthesis faster than real-time" << std::endl;
        }
        
        std::cout << "✓ Performance benchmark completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in performance benchmark: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test utility functions
 */
bool test_utility_functions() {
    std::cout << "\n=== Testing Utility Functions ===" << std::endl;
    
    try {
        AudioParameters test_params = generate_test_parameters(44100, 0.5);
        
        // Test world_to_pulse_params conversion
        std::vector<PulseParams> pulse_sequence = pbp_utils::world_to_pulse_params(test_params);
        
        std::cout << "WORLD to PulseParams conversion:" << std::endl;
        std::cout << "  Input frames: " << test_params.length << std::endl;
        std::cout << "  Output pulses: " << pulse_sequence.size() << std::endl;
        
        if (pulse_sequence.size() != test_params.length) {
            std::cerr << "ERROR: Pulse sequence size mismatch" << std::endl;
            return false;
        }
        
        // Test buffer size calculation
        int buffer_size = pbp_utils::calculate_synthesis_buffer_size(1.0, 44100, 2.0);
        std::cout << "Buffer size calculation:" << std::endl;
        std::cout << "  1 second at 44.1kHz with 2x overlap: " << buffer_size << " samples" << std::endl;
        
        if (buffer_size != 88200) {
            std::cerr << "ERROR: Buffer size calculation incorrect" << std::endl;
            return false;
        }
        
        std::cout << "✓ Utility function tests passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in utility function test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test error handling
 */
bool test_error_handling() {
    std::cout << "\n=== Testing Error Handling ===" << std::endl;
    
    try {
        // Test invalid configuration
        PbpConfig invalid_config;
        invalid_config.sample_rate = -1;
        invalid_config.fft_size = 0;
        
        try {
            PbpSynthesisEngine engine(invalid_config);
            std::cerr << "ERROR: Expected exception for invalid configuration" << std::endl;
            return false;
        } catch (const std::invalid_argument&) {
            std::cout << "✓ Invalid configuration properly rejected" << std::endl;
        }
        
        // Test invalid parameters
        PbpConfig valid_config;
        PbpSynthesisEngine engine(valid_config);
        
        AudioParameters invalid_params;
        // Leave parameters empty
        
        try {
            engine.synthesize(invalid_params);
            std::cerr << "ERROR: Expected exception for invalid parameters" << std::endl;
            return false;
        } catch (const std::invalid_argument&) {
            std::cout << "✓ Invalid parameters properly rejected" << std::endl;
        }
        
        std::cout << "✓ Error handling tests passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in error handling test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Main test function
 */
int main() {
    std::cout << "NexusSynth Pulse-by-Pulse Synthesis Engine Test Suite" << std::endl;
    std::cout << "======================================================" << std::endl;
    
    bool all_tests_passed = true;
    
    // Run all tests
    all_tests_passed &= test_basic_synthesis();
    all_tests_passed &= test_window_functions();
    all_tests_passed &= test_single_pulse();
    all_tests_passed &= test_utility_functions();
    all_tests_passed &= test_error_handling();
    all_tests_passed &= test_performance_benchmark();
    
    std::cout << "\n======================================================" << std::endl;
    if (all_tests_passed) {
        std::cout << "🎉 All PbP Synthesis Engine tests PASSED!" << std::endl;
        std::cout << "   ✓ Basic synthesis functionality working" << std::endl;
        std::cout << "   ✓ All window functions operational" << std::endl;
        std::cout << "   ✓ Single pulse synthesis working" << std::endl;
        std::cout << "   ✓ Utility functions working correctly" << std::endl;
        std::cout << "   ✓ Error handling robust" << std::endl;
        std::cout << "   ✓ Performance benchmarking operational" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Some PbP Synthesis Engine tests FAILED!" << std::endl;
        std::cout << "Please review the error messages above." << std::endl;
        return 1;
    }
}
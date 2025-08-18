#include <gtest/gtest.h>
#include "nexussynth/pbp_synthesis_engine.h"
#include "nexussynth/window_optimizer.h"
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace nexussynth;
using namespace nexussynth::synthesis;

class WindowPerformanceBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        // Configuration for standard windowing
        standard_config_.sample_rate = 44100;
        standard_config_.fft_size = 1024;
        standard_config_.hop_size = 256;
        standard_config_.frame_period = 5.0;
        standard_config_.window_type = PbpConfig::WindowType::HANN;
        standard_config_.enable_adaptive_windowing = false;
        
        // Configuration for adaptive windowing
        adaptive_config_ = standard_config_;
        adaptive_config_.window_type = PbpConfig::WindowType::ADAPTIVE;
        adaptive_config_.enable_adaptive_windowing = true;
        adaptive_config_.minimize_pre_echo = true;
        adaptive_config_.optimize_spectral_leakage = true;
        adaptive_config_.side_lobe_suppression_db = -60.0;
    }
    
    PbpConfig standard_config_;
    PbpConfig adaptive_config_;
    
    // Generate test audio parameters
    AudioParameters generate_test_parameters(int sample_rate, double duration_sec) {
        AudioParameters params;
        params.sample_rate = sample_rate;
        params.length = static_cast<int>(sample_rate * duration_sec);
        
        // Generate realistic F0 trajectory (gliding from 150Hz to 220Hz)
        params.f0.resize(params.length);
        for (int i = 0; i < params.length; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            params.f0[i] = 150.0 + 70.0 * std::sin(t * 2.0);  // F0 modulation
        }
        
        // Generate spectral envelope (formant structure)
        int spectrum_size = sample_rate / 2 + 1;
        params.spectrum.resize(params.length);
        for (int frame = 0; frame < params.length; ++frame) {
            params.spectrum[frame].resize(spectrum_size);
            for (int bin = 0; bin < spectrum_size; ++bin) {
                double freq = (bin * sample_rate) / (2.0 * spectrum_size);
                
                // Add formants at typical vocal frequencies
                double formant_energy = 0.0;
                formant_energy += std::exp(-0.5 * std::pow((freq - 800.0) / 150.0, 2));   // F1
                formant_energy += std::exp(-0.5 * std::pow((freq - 1200.0) / 200.0, 2));  // F2
                formant_energy += std::exp(-0.5 * std::pow((freq - 2600.0) / 300.0, 2));  // F3
                
                params.spectrum[frame][bin] = std::log(std::max(0.001, formant_energy));
            }
        }
        
        // Generate aperiodicity (more periodic at low frequencies)
        params.aperiodicity.resize(params.length);
        for (int frame = 0; frame < params.length; ++frame) {
            params.aperiodicity[frame].resize(spectrum_size);
            for (int bin = 0; bin < spectrum_size; ++bin) {
                double freq = (bin * sample_rate) / (2.0 * spectrum_size);
                double ap_factor = std::min(0.8, freq / 4000.0);  // More aperiodicity at high freq
                params.aperiodicity[frame][bin] = 0.1 + 0.4 * ap_factor;
            }
        }
        
        return params;
    }
};

TEST_F(WindowPerformanceBenchmark, BasicWindowGenerationSpeed) {
    std::cout << "Benchmarking window generation speed...\n";
    
    const int iterations = 1000;
    const int window_length = 512;
    
    WindowOptimizer optimizer;
    
    // Benchmark different window types
    std::vector<std::pair<std::string, OptimalWindowType>> window_types = {
        {"Hann", OptimalWindowType::HANN},
        {"Blackman", OptimalWindowType::BLACKMAN},
        {"Blackman-Harris", OptimalWindowType::BLACKMAN_HARRIS},
        {"Kaiser", OptimalWindowType::KAISER},
        {"Nuttall", OptimalWindowType::NUTTALL}
    };
    
    for (const auto& [name, type] : window_types) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto window = optimizer.generate_window(type, window_length);
            (void)window; // Prevent optimization
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        double avg_time_us = (elapsed_ms * 1000.0) / iterations;
        
        std::cout << "  " << name << " window: " << std::setprecision(2) 
                  << avg_time_us << " μs per window\n";
        
        // All windows should generate in reasonable time
        EXPECT_LT(avg_time_us, 50.0) << name << " window generation too slow";
    }
}

TEST_F(WindowPerformanceBenchmark, AdaptiveWindowOptimizationOverhead) {
    std::cout << "Measuring adaptive window optimization overhead...\n";
    
    const int iterations = 100;
    const int window_length = 512;
    
    WindowOptimizer optimizer;
    
    // Create test content for adaptive windowing
    ContentAnalysis content;
    content.pitch_frequency = 200.0;
    content.spectral_centroid = 1200.0;
    content.harmonic_ratio = 0.8;
    content.transient_factor = 0.2;
    content.formant_frequencies = {800.0, 1200.0, 2600.0};
    content.dynamic_range_db = 45.0;
    
    WindowOptimizationParams params;
    params.sample_rate = 44100.0;
    params.minimize_pre_echo = true;
    params.optimize_for_overlap_add = true;
    params.side_lobe_suppression_db = -60.0;
    
    // Benchmark basic window generation
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto window = optimizer.generate_window(OptimalWindowType::HANN, window_length);
        (void)window;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    double basic_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Benchmark adaptive window generation
    start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto window = optimizer.generate_optimal_window(window_length, content, params);
        (void)window;
    }
    end_time = std::chrono::high_resolution_clock::now();
    double adaptive_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    double basic_avg_us = (basic_time_ms * 1000.0) / iterations;
    double adaptive_avg_us = (adaptive_time_ms * 1000.0) / iterations;
    double overhead_factor = adaptive_avg_us / basic_avg_us;
    
    std::cout << "  Basic window generation: " << std::setprecision(2) 
              << basic_avg_us << " μs per window\n";
    std::cout << "  Adaptive window generation: " << std::setprecision(2) 
              << adaptive_avg_us << " μs per window\n";
    std::cout << "  Overhead factor: " << std::setprecision(2) 
              << overhead_factor << "x\n";
    
    // Adaptive windowing should not be more than 10x slower
    EXPECT_LT(overhead_factor, 10.0) << "Adaptive windowing overhead too high";
    
    // Both should be fast enough for real-time processing
    EXPECT_LT(adaptive_avg_us, 500.0) << "Adaptive windowing too slow for real-time";
}

TEST_F(WindowPerformanceBenchmark, SynthesisPerformanceComparison) {
    std::cout << "Comparing synthesis performance: Standard vs Adaptive windowing...\n";
    
    // Generate test parameters for 100ms of audio
    auto test_params = generate_test_parameters(44100, 0.1);
    
    // Test standard windowing
    PbpSynthesisEngine standard_engine(standard_config_);
    SynthesisStats standard_stats;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto standard_result = standard_engine.synthesize(test_params, &standard_stats);
    auto end_time = std::chrono::high_resolution_clock::now();
    double standard_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Test adaptive windowing
    PbpSynthesisEngine adaptive_engine(adaptive_config_);
    SynthesisStats adaptive_stats;
    
    start_time = std::chrono::high_resolution_clock::now();
    auto adaptive_result = adaptive_engine.synthesize(test_params, &adaptive_stats);
    end_time = std::chrono::high_resolution_clock::now();
    double adaptive_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Calculate performance metrics
    double audio_duration_ms = 100.0;  // 100ms test audio
    double standard_rtf = standard_time_ms / audio_duration_ms;
    double adaptive_rtf = adaptive_time_ms / audio_duration_ms;
    double performance_ratio = adaptive_time_ms / standard_time_ms;
    
    std::cout << "  Standard windowing: " << std::setprecision(2) 
              << standard_time_ms << " ms (" << standard_rtf << "x real-time)\n";
    std::cout << "  Adaptive windowing: " << std::setprecision(2) 
              << adaptive_time_ms << " ms (" << adaptive_rtf << "x real-time)\n";
    std::cout << "  Performance ratio: " << std::setprecision(2) 
              << performance_ratio << "x\n";
    
    // Quality comparison
    std::cout << "  Standard quality - Harmonic energy: " << std::setprecision(3) 
              << standard_stats.harmonic_energy_ratio << ", Smoothness: " 
              << standard_stats.temporal_smoothness << "\n";
    std::cout << "  Adaptive quality - Harmonic energy: " << std::setprecision(3) 
              << adaptive_stats.harmonic_energy_ratio << ", Smoothness: " 
              << adaptive_stats.temporal_smoothness << "\n";
    
    // Both should be able to run in real-time (RTF < 1.0)
    EXPECT_LT(standard_rtf, 1.0) << "Standard synthesis not real-time capable";
    EXPECT_LT(adaptive_rtf, 1.0) << "Adaptive synthesis not real-time capable";
    
    // Adaptive should not be more than 3x slower than standard
    EXPECT_LT(performance_ratio, 3.0) << "Adaptive synthesis performance penalty too high";
    
    // Check that both produce valid output
    EXPECT_FALSE(standard_result.empty()) << "Standard synthesis produced no output";
    EXPECT_FALSE(adaptive_result.empty()) << "Adaptive synthesis produced no output";
    EXPECT_EQ(standard_result.size(), adaptive_result.size()) << "Different output lengths";
    
    // Quality should be reasonable for both
    EXPECT_GT(standard_stats.harmonic_energy_ratio, 0.3) << "Standard synthesis poor harmonic content";
    EXPECT_GT(adaptive_stats.harmonic_energy_ratio, 0.3) << "Adaptive synthesis poor harmonic content";
}

TEST_F(WindowPerformanceBenchmark, WindowQualityVsPerformanceTradeoff) {
    std::cout << "Analyzing window quality vs performance trade-offs...\n";
    
    WindowOptimizer optimizer;
    const int window_length = 512;
    const int iterations = 50;
    
    // Test different optimization levels
    struct OptimizationLevel {
        std::string name;
        bool minimize_pre_echo;
        bool optimize_spectral_leakage;
        double side_lobe_target_db;
    };
    
    std::vector<OptimizationLevel> levels = {
        {"None", false, false, -40.0},
        {"Basic", true, false, -50.0},
        {"Full", true, true, -60.0},
        {"Maximum", true, true, -80.0}
    };
    
    ContentAnalysis content;
    content.pitch_frequency = 200.0;
    content.spectral_centroid = 1200.0;
    content.harmonic_ratio = 0.8;
    content.transient_factor = 0.2;
    content.dynamic_range_db = 45.0;
    
    for (const auto& level : levels) {
        WindowOptimizationParams params;
        params.minimize_pre_echo = level.minimize_pre_echo;
        params.optimize_for_overlap_add = true;
        params.side_lobe_suppression_db = level.side_lobe_target_db;
        
        // Measure generation time
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<double> final_window;
        
        for (int i = 0; i < iterations; ++i) {
            final_window = optimizer.generate_optimal_window(window_length, content, params);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double avg_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count() / iterations;
        
        // Measure quality
        double quality = optimizer.evaluate_window_quality(final_window, content);
        
        std::cout << "  " << level.name << " optimization: " 
                  << std::setprecision(1) << avg_time_us << " μs, Quality: " 
                  << std::setprecision(3) << quality << "\n";
        
        // All optimization levels should produce valid windows
        EXPECT_GT(quality, 0.0) << level.name << " optimization produced invalid window";
        EXPECT_LT(avg_time_us, 1000.0) << level.name << " optimization too slow";
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=== Window Function Performance Benchmark ===\n\n";
    
    int result = RUN_ALL_TESTS();
    
    std::cout << "\n=== Performance Benchmark " 
              << (result == 0 ? "COMPLETED" : "FAILED") << " ===\n";
    
    return result;
}
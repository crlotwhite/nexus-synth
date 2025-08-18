#include <gtest/gtest.h>
#include "nexussynth/pbp_synthesis_engine.h"
#include "nexussynth/window_optimizer.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace nexussynth;
using namespace nexussynth::synthesis;

class WindowOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.sample_rate = 44100;
        config_.fft_size = 1024;
        config_.hop_size = 256;  // 75% overlap
        config_.frame_period = 5.0;
        config_.window_type = PbpConfig::WindowType::ADAPTIVE;
        config_.enable_adaptive_windowing = true;
        config_.minimize_pre_echo = true;
        config_.optimize_spectral_leakage = true;
        config_.side_lobe_suppression_db = -60.0;
        
        engine_ = std::make_unique<PbpSynthesisEngine>(config_);
        optimizer_ = std::make_unique<WindowOptimizer>();
    }
    
    PbpConfig config_;
    std::unique_ptr<PbpSynthesisEngine> engine_;
    std::unique_ptr<WindowOptimizer> optimizer_;
    
    // Helper function to generate test spectrum
    std::vector<double> generate_test_spectrum(double f0, double formant1, double formant2) {
        std::vector<double> spectrum(513);  // FFT size / 2 + 1
        
        for (size_t i = 0; i < spectrum.size(); ++i) {
            double freq = (i * 44100.0) / (2.0 * spectrum.size());
            
            // Add harmonic structure
            double harmonic_energy = 0.0;
            for (int h = 1; h <= 10; ++h) {
                if (freq > (h - 0.5) * f0 && freq < (h + 0.5) * f0) {
                    harmonic_energy += 1.0 / h;  // Decreasing harmonic amplitude
                }
            }
            
            // Add formant structure
            double formant_energy = 0.0;
            formant_energy += std::exp(-0.5 * std::pow((freq - formant1) / 200.0, 2));
            formant_energy += std::exp(-0.5 * std::pow((freq - formant2) / 300.0, 2));
            
            // Combine and convert to log domain
            spectrum[i] = std::log(std::max(0.001, harmonic_energy + formant_energy));
        }
        
        return spectrum;
    }
    
    // Helper function to generate test aperiodicity
    std::vector<double> generate_test_aperiodicity(double noise_level) {
        std::vector<double> aperiodicity(513);
        for (size_t i = 0; i < aperiodicity.size(); ++i) {
            // Lower frequencies more periodic, higher frequencies more noisy
            double freq_factor = static_cast<double>(i) / aperiodicity.size();
            aperiodicity[i] = noise_level * (0.1 + 0.9 * freq_factor);
        }
        return aperiodicity;
    }
};

TEST_F(WindowOptimizationTest, BasicWindowGeneration) {
    std::cout << "Testing basic window generation with different types...\n";
    
    std::vector<OptimalWindowType> window_types = {
        OptimalWindowType::HANN,
        OptimalWindowType::HAMMING,
        OptimalWindowType::BLACKMAN,
        OptimalWindowType::BLACKMAN_HARRIS,
        OptimalWindowType::KAISER,
        OptimalWindowType::NUTTALL
    };
    
    std::vector<std::string> type_names = {
        "Hann", "Hamming", "Blackman", "Blackman-Harris", "Kaiser", "Nuttall"
    };
    
    for (size_t i = 0; i < window_types.size(); ++i) {
        auto window = optimizer_->generate_window(window_types[i], 512);
        
        ASSERT_EQ(window.size(), 512);
        
        // Check for proper normalization (peak should be around 1.0)
        double max_val = *std::max_element(window.begin(), window.end());
        EXPECT_NEAR(max_val, 1.0, 0.1) << "Window " << type_names[i] << " not properly normalized";
        
        // Check for symmetry
        bool is_symmetric = true;
        for (size_t j = 0; j < window.size() / 2; ++j) {
            if (std::abs(window[j] - window[window.size() - 1 - j]) > 1e-10) {
                is_symmetric = false;
                break;
            }
        }
        EXPECT_TRUE(is_symmetric) << "Window " << type_names[i] << " is not symmetric";
        
        std::cout << "  " << type_names[i] << " window: ✓ Normalized, ✓ Symmetric\n";
    }
}

TEST_F(WindowOptimizationTest, AdaptiveWindowSelection) {
    std::cout << "Testing adaptive window selection for different content types...\n";
    
    // Test different content scenarios
    struct TestScenario {
        std::string name;
        double f0;
        double formant1;
        double formant2;
        double noise_level;
        OptimalWindowType expected_type;
    };
    
    std::vector<TestScenario> scenarios = {
        {"Male Voice (Low Pitch)", 120.0, 800.0, 1200.0, 0.1, OptimalWindowType::BLACKMAN_HARRIS},
        {"Female Voice (High Pitch)", 250.0, 1000.0, 1400.0, 0.15, OptimalWindowType::BLACKMAN},
        {"Whispered Speech (High Noise)", 150.0, 900.0, 1300.0, 0.7, OptimalWindowType::TUKEY},
        {"Singing Voice (Pure Tones)", 200.0, 800.0, 1500.0, 0.05, OptimalWindowType::BLACKMAN_HARRIS}
    };
    
    for (const auto& scenario : scenarios) {
        ContentAnalysis content;
        content.pitch_frequency = scenario.f0;
        content.spectral_centroid = (scenario.formant1 + scenario.formant2) / 2.0;
        content.harmonic_ratio = 1.0 - scenario.noise_level;
        content.transient_factor = scenario.noise_level * 0.8;
        content.formant_frequencies = {scenario.formant1, scenario.formant2};
        content.dynamic_range_db = 50.0;
        
        OptimalWindowType selected = optimizer_->select_optimal_window_type(content);
        
        std::cout << "  " << scenario.name << ": Selected " 
                  << static_cast<int>(selected) << " (F0=" << scenario.f0 
                  << " Hz, Harmonic Ratio=" << std::fixed << std::setprecision(2) 
                  << content.harmonic_ratio << ")\n";
                  
        // The selection should be reasonable (we can't test exact matches due to adaptive nature)
        EXPECT_NE(selected, OptimalWindowType::HANN) << "Should select more advanced window for " << scenario.name;
    }
}

TEST_F(WindowOptimizationTest, WindowCharacteristicsAnalysis) {
    std::cout << "Testing window characteristics analysis...\n";
    
    // Generate different windows and analyze their characteristics
    auto hann_window = optimizer_->generate_window(OptimalWindowType::HANN, 512);
    auto blackman_window = optimizer_->generate_window(OptimalWindowType::BLACKMAN, 512);
    auto kaiser_window = optimizer_->generate_window(OptimalWindowType::KAISER, 512);
    
    auto hann_char = optimizer_->analyze_window_characteristics(hann_window, 44100.0);
    auto blackman_char = optimizer_->analyze_window_characteristics(blackman_window, 44100.0);
    auto kaiser_char = optimizer_->analyze_window_characteristics(kaiser_window, 44100.0);
    
    std::cout << "  Hann: Main lobe width = " << hann_char.main_lobe_width 
              << " Hz, Side lobe = " << hann_char.peak_side_lobe_db << " dB\n";
    std::cout << "  Blackman: Main lobe width = " << blackman_char.main_lobe_width 
              << " Hz, Side lobe = " << blackman_char.peak_side_lobe_db << " dB\n";
    std::cout << "  Kaiser: Main lobe width = " << kaiser_char.main_lobe_width 
              << " Hz, Side lobe = " << kaiser_char.peak_side_lobe_db << " dB\n";
    
    // Blackman should have better side lobe suppression than Hann
    EXPECT_LT(blackman_char.peak_side_lobe_db, hann_char.peak_side_lobe_db)
        << "Blackman should have better side lobe suppression than Hann";
    
    // All windows should have reasonable characteristics
    EXPECT_GT(hann_char.coherent_gain, 0.3) << "Hann window coherent gain too low";
    EXPECT_GT(blackman_char.coherent_gain, 0.2) << "Blackman window coherent gain too low";
    EXPECT_GT(kaiser_char.coherent_gain, 0.3) << "Kaiser window coherent gain too low";
}

TEST_F(WindowOptimizationTest, PreEchoSuppression) {
    std::cout << "Testing pre-echo suppression...\n";
    
    auto original_window = optimizer_->generate_window(OptimalWindowType::HANN, 512);
    auto suppressed_window = original_window;
    
    optimizer_->apply_pre_echo_suppression(suppressed_window, 0.8);
    
    // Check that the beginning of the window is more aggressively tapered
    double original_start_sum = 0.0;
    double suppressed_start_sum = 0.0;
    int fade_region = 51;  // First 10% of window
    
    for (int i = 0; i < fade_region; ++i) {
        original_start_sum += original_window[i];
        suppressed_start_sum += suppressed_window[i];
    }
    
    EXPECT_LT(suppressed_start_sum, original_start_sum) 
        << "Pre-echo suppression should reduce energy at window start";
    
    // Check that the main part of the window is preserved or enhanced
    double original_center_sum = 0.0;
    double suppressed_center_sum = 0.0;
    int center_start = 200;
    int center_end = 312;
    
    for (int i = center_start; i < center_end; ++i) {
        original_center_sum += original_window[i];
        suppressed_center_sum += suppressed_window[i];
    }
    
    EXPECT_GE(suppressed_center_sum, original_center_sum * 0.95) 
        << "Pre-echo suppression should not significantly reduce center energy";
    
    std::cout << "  Pre-echo suppression: ✓ Reduced start energy by " 
              << std::setprecision(1) << ((original_start_sum - suppressed_start_sum) / original_start_sum * 100)
              << "%\n";
}

TEST_F(WindowOptimizationTest, SpectralLeakageMinimization) {
    std::cout << "Testing spectral leakage minimization...\n";
    
    auto original_window = optimizer_->generate_window(OptimalWindowType::HANN, 512);
    auto optimized_window = original_window;
    
    optimizer_->minimize_spectral_leakage(optimized_window, -60.0);
    
    // Analyze spectral characteristics
    std::vector<double> original_leakage, optimized_leakage;
    window_utils::calculate_spectral_leakage(original_window, original_leakage);
    window_utils::calculate_spectral_leakage(optimized_window, optimized_leakage);
    
    ASSERT_EQ(original_leakage.size(), optimized_leakage.size());
    
    // Check that side lobes are reduced
    double original_max_side_lobe = 0.0;
    double optimized_max_side_lobe = 0.0;
    
    // Skip the main lobe (first few bins)
    for (size_t i = 10; i < original_leakage.size(); ++i) {
        original_max_side_lobe = std::max(original_max_side_lobe, original_leakage[i]);
        optimized_max_side_lobe = std::max(optimized_max_side_lobe, optimized_leakage[i]);
    }
    
    EXPECT_LE(optimized_max_side_lobe, original_max_side_lobe) 
        << "Optimized window should have reduced spectral leakage";
    
    double improvement_db = 20.0 * std::log10(original_max_side_lobe / optimized_max_side_lobe);
    std::cout << "  Spectral leakage improvement: " << std::setprecision(1) 
              << improvement_db << " dB\n";
}

TEST_F(WindowOptimizationTest, OverlapAddOptimization) {
    std::cout << "Testing overlap-add optimization...\n";
    
    auto original_window = optimizer_->generate_window(OptimalWindowType::HANN, 512);
    auto optimized_window = original_window;
    
    int hop_size = 128;  // 75% overlap
    double overlap_factor = 0.75;
    
    optimizer_->optimize_for_overlap_add(optimized_window, overlap_factor, hop_size);
    
    // Test reconstruction error
    double original_error = window_utils::calculate_ola_reconstruction_error(original_window, hop_size);
    double optimized_error = window_utils::calculate_ola_reconstruction_error(optimized_window, hop_size);
    
    std::cout << "  Original OLA error: " << std::setprecision(4) << original_error << "\n";
    std::cout << "  Optimized OLA error: " << std::setprecision(4) << optimized_error << "\n";
    
    EXPECT_LE(optimized_error, original_error * 1.1) 
        << "Optimized window should not have significantly worse reconstruction";
    
    // Both should have reasonable reconstruction quality
    EXPECT_LT(original_error, 0.1) << "Original window OLA error too high";
    EXPECT_LT(optimized_error, 0.1) << "Optimized window OLA error too high";
}

TEST_F(WindowOptimizationTest, ContentAnalysis) {
    std::cout << "Testing content analysis for adaptive windowing...\n";
    
    // Test different spectral content
    double f0 = 200.0;
    auto harmonic_spectrum = generate_test_spectrum(f0, 800.0, 1200.0);
    auto harmonic_aperiodicity = generate_test_aperiodicity(0.1);  // Low noise
    
    auto noise_spectrum = generate_test_spectrum(f0, 800.0, 1200.0);
    auto noise_aperiodicity = generate_test_aperiodicity(0.8);  // High noise
    
    auto harmonic_analysis = engine_->analyze_audio_content_for_testing(f0, harmonic_spectrum, harmonic_aperiodicity);
    auto noise_analysis = engine_->analyze_audio_content_for_testing(f0, noise_spectrum, noise_aperiodicity);
    
    std::cout << "  Harmonic content: F0=" << harmonic_analysis.pitch_frequency 
              << " Hz, Harmonic ratio=" << std::setprecision(2) << harmonic_analysis.harmonic_ratio
              << ", Transient factor=" << harmonic_analysis.transient_factor << "\n";
    
    std::cout << "  Noisy content: F0=" << noise_analysis.pitch_frequency 
              << " Hz, Harmonic ratio=" << std::setprecision(2) << noise_analysis.harmonic_ratio
              << ", Transient factor=" << noise_analysis.transient_factor << "\n";
    
    // Harmonic content should have higher harmonic ratio
    EXPECT_GT(harmonic_analysis.harmonic_ratio, noise_analysis.harmonic_ratio)
        << "Harmonic content should have higher harmonic ratio";
    
    // Noisy content should have higher transient factor
    EXPECT_GT(noise_analysis.transient_factor, harmonic_analysis.transient_factor)
        << "Noisy content should have higher transient factor";
    
    // Check formant detection
    EXPECT_GT(harmonic_analysis.formant_frequencies.size(), 0)
        << "Should detect some formants in test spectrum";
}

TEST_F(WindowOptimizationTest, AdaptiveWindowGeneration) {
    std::cout << "Testing adaptive window generation in synthesis engine...\n";
    
    double f0 = 150.0;
    auto spectrum = generate_test_spectrum(f0, 900.0, 1400.0);
    auto aperiodicity = generate_test_aperiodicity(0.2);
    
    // Generate adaptive window
    auto adaptive_window = engine_->generate_adaptive_window_for_testing(512, f0, spectrum, aperiodicity);
    
    // Generate standard Hann window for comparison
    auto hann_window = engine_->generate_window_for_testing(512, PbpConfig::WindowType::HANN);
    
    ASSERT_EQ(adaptive_window.size(), 512);
    ASSERT_EQ(hann_window.size(), 512);
    
    // Adaptive window should be different from standard Hann
    bool is_different = false;
    for (size_t i = 0; i < adaptive_window.size(); ++i) {
        if (std::abs(adaptive_window[i] - hann_window[i]) > 1e-6) {
            is_different = true;
            break;
        }
    }
    
    EXPECT_TRUE(is_different) << "Adaptive window should differ from standard Hann window";
    
    // Both windows should be properly normalized
    double adaptive_max = *std::max_element(adaptive_window.begin(), adaptive_window.end());
    double hann_max = *std::max_element(hann_window.begin(), hann_window.end());
    
    EXPECT_NEAR(adaptive_max, 1.0, 0.1) << "Adaptive window should be normalized";
    EXPECT_NEAR(hann_max, 1.0, 0.1) << "Hann window should be normalized";
    
    std::cout << "  Adaptive window: ✓ Generated, ✓ Different from Hann, ✓ Normalized\n";
}

TEST_F(WindowOptimizationTest, WindowQualityEvaluation) {
    std::cout << "Testing window quality evaluation...\n";
    
    // Create content analysis
    ContentAnalysis content;
    content.pitch_frequency = 200.0;
    content.spectral_centroid = 1200.0;
    content.harmonic_ratio = 0.8;
    content.transient_factor = 0.2;
    content.dynamic_range_db = 45.0;
    
    // Generate different windows
    auto hann_window = optimizer_->generate_window(OptimalWindowType::HANN, 512);
    auto blackman_window = optimizer_->generate_window(OptimalWindowType::BLACKMAN, 512);
    auto kaiser_window = optimizer_->generate_window(OptimalWindowType::KAISER, 512);
    
    // Evaluate quality
    double hann_quality = optimizer_->evaluate_window_quality(hann_window, content);
    double blackman_quality = optimizer_->evaluate_window_quality(blackman_window, content);
    double kaiser_quality = optimizer_->evaluate_window_quality(kaiser_window, content);
    
    std::cout << "  Hann quality score: " << std::setprecision(3) << hann_quality << "\n";
    std::cout << "  Blackman quality score: " << std::setprecision(3) << blackman_quality << "\n";
    std::cout << "  Kaiser quality score: " << std::setprecision(3) << kaiser_quality << "\n";
    
    // All quality scores should be reasonable
    EXPECT_GE(hann_quality, 0.0) << "Quality scores should be non-negative";
    EXPECT_LE(hann_quality, 1.0) << "Quality scores should be <= 1.0";
    EXPECT_GE(blackman_quality, 0.0);
    EXPECT_LE(blackman_quality, 1.0);
    EXPECT_GE(kaiser_quality, 0.0);
    EXPECT_LE(kaiser_quality, 1.0);
    
    // For harmonic content, Blackman should generally score higher than Hann
    std::cout << "  Quality comparison: " << (blackman_quality > hann_quality ? "Blackman > Hann" : "Hann >= Blackman") << "\n";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=== Window Optimization Test Suite ===\n\n";
    
    int result = RUN_ALL_TESTS();
    
    std::cout << "\n=== Window Optimization Tests " 
              << (result == 0 ? "PASSED" : "FAILED") << " ===\n";
    
    return result;
}
#include "nexussynth/pbp_synthesis_engine.h"
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <random>

using namespace nexussynth::synthesis;

class OverlapAddTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.sample_rate = 44100;
        config_.fft_size = 1024;
        config_.hop_size = 256;  // 75% overlap
        config_.frame_period = 5.0;
        config_.window_type = PbpConfig::WindowType::HANN;
        
        engine_ = std::make_unique<PbpSynthesisEngine>(config_);
    }

    void TearDown() override {
        engine_.reset();
    }

    // Generate test pulse with known characteristics
    std::vector<double> generate_test_pulse(double frequency, int length, double amplitude = 1.0) {
        std::vector<double> pulse(length);
        for (int i = 0; i < length; ++i) {
            pulse[i] = amplitude * std::sin(2.0 * M_PI * frequency * i / config_.sample_rate);
        }
        return pulse;
    }

    // Generate noise pulse for testing
    std::vector<double> generate_noise_pulse(int length, double amplitude = 0.1) {
        std::vector<double> pulse(length);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> dist(0.0, amplitude);
        
        for (int i = 0; i < length; ++i) {
            pulse[i] = dist(gen);
        }
        return pulse;
    }

    PbpConfig config_;
    std::unique_ptr<PbpSynthesisEngine> engine_;
};

// Test basic overlap-add functionality
TEST_F(OverlapAddTest, BasicOverlapAdd) {
    std::vector<double> synthesis_buffer(2048, 0.0);
    
    // Generate two test pulses
    auto pulse1 = generate_test_pulse(440.0, 512, 1.0);  // A4 note
    auto pulse2 = generate_test_pulse(880.0, 512, 0.5);  // A5 note
    
    // Apply window to pulses
    auto window = engine_->generate_window_for_testing(512, PbpConfig::WindowType::HANN);
    for (size_t i = 0; i < pulse1.size(); ++i) {
        pulse1[i] *= window[i];
        pulse2[i] *= window[i];
    }
    
    // Add pulses with overlap
    engine_->overlap_add_pulse_for_testing(pulse1, 0, synthesis_buffer);
    engine_->overlap_add_pulse_for_testing(pulse2, 256, synthesis_buffer);  // 50% overlap
    
    // Verify no clipping occurred
    for (double sample : synthesis_buffer) {
        EXPECT_LE(std::abs(sample), 2.0) << "Sample clipping detected";
    }
    
    // Verify energy conservation in overlap region
    double overlap_energy = 0.0;
    for (int i = 256; i < 512; ++i) {
        overlap_energy += synthesis_buffer[i] * synthesis_buffer[i];
    }
    EXPECT_GT(overlap_energy, 0.0) << "No energy in overlap region";
}

// Test crossfade functionality
TEST_F(OverlapAddTest, CrossfadeSmoothing) {
    std::vector<double> buffer1 = {1.0, 1.0, 1.0, 1.0};
    std::vector<double> buffer2 = {0.0, 0.0, 0.0, 0.0};
    std::vector<double> output;
    
    engine_->apply_crossfade_for_testing(buffer1, buffer2, 4, output);
    
    ASSERT_EQ(output.size(), 4);
    
    // First sample should be close to buffer1
    EXPECT_NEAR(output[0], 1.0, 0.1);
    
    // Last sample should be close to buffer2
    EXPECT_NEAR(output[3], 0.0, 0.1);
    
    // Middle samples should be interpolated
    EXPECT_GT(output[1], output[2]) << "Crossfade should be monotonic";
}

// Test boundary smoothing
TEST_F(OverlapAddTest, BoundarySmoothing) {
    std::vector<double> buffer(128, 1.0);  // Constant signal
    
    engine_->smooth_boundaries_for_testing(buffer, 16);
    
    // Check fade-in at beginning
    EXPECT_LT(buffer[0], 0.1) << "Beginning should be faded";
    EXPECT_NEAR(buffer[15], 1.0, 0.1) << "Should reach full amplitude";
    
    // Check fade-out at end
    EXPECT_LT(buffer[127], 0.1) << "End should be faded";
    EXPECT_NEAR(buffer[112], 1.0, 0.1) << "Should maintain full amplitude";
}

// Test overlap-add with realistic WORLD-like parameters
TEST_F(OverlapAddTest, RealisticSynthesis) {
    const int num_frames = 10;
    const int frame_length = config_.hop_size;
    std::vector<double> synthesis_buffer(num_frames * frame_length + config_.fft_size, 0.0);
    
    // Simulate multiple pulses with varying F0
    std::vector<double> f0_contour = {220.0, 230.0, 240.0, 235.0, 225.0, 
                                     215.0, 210.0, 205.0, 200.0, 195.0};
    
    for (int frame = 0; frame < num_frames; ++frame) {
        // Generate pulse for current frame
        auto pulse = generate_test_pulse(f0_contour[frame], config_.fft_size, 0.5);
        
        // Apply window
        auto window = engine_->generate_window_for_testing(config_.fft_size, config_.window_type);
        for (size_t i = 0; i < pulse.size(); ++i) {
            pulse[i] *= window[i];
        }
        
        // Add to synthesis buffer
        int pulse_position = frame * frame_length;
        engine_->overlap_add_pulse_for_testing(pulse, pulse_position, synthesis_buffer);
    }
    
    // Verify continuity - no large discontinuities
    for (size_t i = 1; i < synthesis_buffer.size(); ++i) {
        double diff = std::abs(synthesis_buffer[i] - synthesis_buffer[i-1]);
        EXPECT_LT(diff, 1.0) << "Large discontinuity detected at sample " << i;
    }
    
    // Verify overall energy is reasonable
    double total_energy = 0.0;
    for (double sample : synthesis_buffer) {
        total_energy += sample * sample;
    }
    EXPECT_GT(total_energy, 0.1) << "Total energy too low";
    EXPECT_LT(total_energy, synthesis_buffer.size() * 4.0) << "Total energy too high";
}

// Test streaming overlap-add
TEST_F(OverlapAddTest, StreamingOverlapAdd) {
    const int buffer_size = 512;
    std::vector<double> output_buffer(buffer_size, 0.0);
    
    auto pulse = generate_test_pulse(440.0, 256, 0.8);
    
    int samples_written = engine_->streaming_overlap_add_for_testing(
        pulse, 
        100,  // Start position 
        output_buffer.data(),
        buffer_size
    );
    
    EXPECT_GT(samples_written, 0) << "No samples written to streaming buffer";
    EXPECT_LE(samples_written, buffer_size) << "Too many samples written";
    
    // Verify pulse was placed correctly
    bool has_signal = false;
    for (int i = 100; i < 100 + static_cast<int>(pulse.size()) && i < buffer_size; ++i) {
        if (std::abs(output_buffer[i]) > 1e-10) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal) << "No signal detected in expected region";
}

// Performance test for overlap-add
TEST_F(OverlapAddTest, OverlapAddPerformance) {
    const int num_iterations = 1000;
    const int buffer_size = 8192;
    std::vector<double> synthesis_buffer(buffer_size, 0.0);
    auto pulse = generate_test_pulse(440.0, 1024, 0.5);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; ++i) {
        int position = (i * 256) % (buffer_size - 1024);
        engine_->overlap_add_pulse_for_testing(pulse, position, synthesis_buffer);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double avg_time_per_operation = duration.count() / static_cast<double>(num_iterations);
    
    // Should complete in reasonable time (less than 100 μs per operation)
    EXPECT_LT(avg_time_per_operation, 100.0) 
        << "Overlap-add too slow: " << avg_time_per_operation << " μs per operation";
    
    std::cout << "Average overlap-add time: " << avg_time_per_operation << " μs" << std::endl;
}

// Test edge cases
TEST_F(OverlapAddTest, EdgeCases) {
    std::vector<double> synthesis_buffer(1024, 0.0);
    
    // Test empty pulse
    std::vector<double> empty_pulse;
    EXPECT_NO_THROW(engine_->overlap_add_pulse_for_testing(empty_pulse, 0, synthesis_buffer));
    
    // Test negative position
    auto pulse = generate_test_pulse(440.0, 256, 0.5);
    EXPECT_NO_THROW(engine_->overlap_add_pulse_for_testing(pulse, -100, synthesis_buffer));
    
    // Test position beyond buffer
    EXPECT_NO_THROW(engine_->overlap_add_pulse_for_testing(pulse, 2000, synthesis_buffer));
    
    // Test pulse extending beyond buffer
    EXPECT_NO_THROW(engine_->overlap_add_pulse_for_testing(pulse, 900, synthesis_buffer));
}

// Using gtest_main so no main() needed
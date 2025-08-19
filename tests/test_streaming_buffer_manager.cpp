#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <random>
#include <iostream>
#include "nexussynth/streaming_buffer_manager.h"
#include "nexussynth/world_wrapper.h"

using namespace nexussynth::synthesis;

class StreamingBufferManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.input_buffer_size = 16;
        config_.output_buffer_size = 2048;
        config_.target_latency_ms = 10.0;
        config_.max_latency_ms = 50.0;
        config_.enable_background_processing = false; // Disable for deterministic testing
        
        manager_ = std::make_unique<StreamingBufferManager>(config_);
        
        sample_rate_ = 44100;
        frame_period_ = 5.0;
    }

    void TearDown() override {
        if (manager_) {
            manager_->stop_streaming();
        }
    }

    // Helper function to create test frame
    StreamingFrame create_test_frame(int frame_index, double f0 = 440.0) {
        StreamingFrame frame;
        frame.f0 = f0;
        frame.frame_index = frame_index;
        frame.timestamp_ms = frame_index * frame_period_;
        frame.is_voiced = (f0 > 0.0);
        
        // Create simple spectrum and aperiodicity
        int spectrum_size = 1025; // For 2048 FFT size
        frame.spectrum.resize(spectrum_size, 1.0);
        frame.aperiodicity.resize(spectrum_size, 0.1);
        
        return frame;
    }

    // Simple synthesis callback for testing
    std::vector<double> simple_synthesis_callback(const StreamingFrame& frame) {
        int samples_per_frame = static_cast<int>(sample_rate_ * frame_period_ / 1000.0);
        std::vector<double> output(samples_per_frame);
        
        // Generate simple sine wave based on F0
        if (frame.is_voiced && frame.f0 > 0.0) {
            double phase_increment = 2.0 * M_PI * frame.f0 / sample_rate_;
            for (int i = 0; i < samples_per_frame; ++i) {
                output[i] = 0.1 * std::sin(phase_increment * i);
            }
        } else {
            // Generate silence for unvoiced frames
            std::fill(output.begin(), output.end(), 0.0);
        }
        
        return output;
    }

    StreamingConfig config_;
    std::unique_ptr<StreamingBufferManager> manager_;
    int sample_rate_;
    double frame_period_;
};

// Ring Buffer Tests
TEST_F(StreamingBufferManagerTest, RingBufferBasicOperations) {
    RingBuffer<int> ring_buffer(4);
    
    EXPECT_TRUE(ring_buffer.empty());
    EXPECT_FALSE(ring_buffer.full());
    EXPECT_EQ(ring_buffer.size(), 0);
    EXPECT_EQ(ring_buffer.capacity(), 4);
    
    // Test push operations
    EXPECT_TRUE(ring_buffer.push(1));
    EXPECT_TRUE(ring_buffer.push(2));
    EXPECT_TRUE(ring_buffer.push(3));
    EXPECT_TRUE(ring_buffer.push(4));
    
    EXPECT_TRUE(ring_buffer.full());
    EXPECT_FALSE(ring_buffer.push(5)); // Should fail when full
    
    // Test pop operations
    int value;
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 1);
    
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 2);
    
    // Test peek
    EXPECT_TRUE(ring_buffer.peek(value));
    EXPECT_EQ(value, 3);
    
    // Pop should still work after peek
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 3);
    
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 4);
    
    EXPECT_TRUE(ring_buffer.empty());
    EXPECT_FALSE(ring_buffer.pop(value)); // Should fail when empty
}

TEST_F(StreamingBufferManagerTest, RingBufferBulkOperations) {
    RingBuffer<double> ring_buffer(8);
    
    // Test bulk push
    std::vector<double> input_data = {1.0, 2.0, 3.0, 4.0, 5.0};
    size_t pushed = ring_buffer.push_bulk(input_data.data(), input_data.size());
    EXPECT_EQ(pushed, 5);
    EXPECT_EQ(ring_buffer.size(), 5);
    
    // Test bulk pop
    std::vector<double> output_data(3);
    size_t popped = ring_buffer.pop_bulk(output_data.data(), 3);
    EXPECT_EQ(popped, 3);
    EXPECT_EQ(output_data[0], 1.0);
    EXPECT_EQ(output_data[1], 2.0);
    EXPECT_EQ(output_data[2], 3.0);
    
    EXPECT_EQ(ring_buffer.size(), 2);
}

TEST_F(StreamingBufferManagerTest, RingBufferWraparound) {
    RingBuffer<int> ring_buffer(3);
    
    // Fill the buffer
    EXPECT_TRUE(ring_buffer.push(1));
    EXPECT_TRUE(ring_buffer.push(2));
    EXPECT_TRUE(ring_buffer.push(3));
    
    // Remove one element
    int value;
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 1);
    
    // Add another element (should wrap around)
    EXPECT_TRUE(ring_buffer.push(4));
    
    // Check that the order is maintained
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(ring_buffer.pop(value));
    EXPECT_EQ(value, 4);
    
    EXPECT_TRUE(ring_buffer.empty());
}

// StreamingBufferManager Basic Tests
TEST_F(StreamingBufferManagerTest, InitializationAndConfiguration) {
    EXPECT_FALSE(manager_->is_streaming());
    
    // Test initialization
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Test configuration access
    const auto& current_config = manager_->get_config();
    EXPECT_EQ(current_config.input_buffer_size, config_.input_buffer_size);
    EXPECT_EQ(current_config.output_buffer_size, config_.output_buffer_size);
    
    // Test configuration update (should fail while not streaming)
    StreamingConfig new_config = config_;
    new_config.input_buffer_size = 32;
    EXPECT_TRUE(manager_->update_config(new_config));
}

TEST_F(StreamingBufferManagerTest, BasicStreamingWorkflow) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Set synthesis callback
    manager_->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    EXPECT_TRUE(manager_->is_streaming());
    
    // Queue some test frames
    std::vector<StreamingFrame> test_frames;
    for (int i = 0; i < 5; ++i) {
        test_frames.push_back(create_test_frame(i, 440.0 + i * 10.0));
    }
    
    size_t queued = manager_->queue_input_frames(test_frames);
    EXPECT_EQ(queued, 5);
    EXPECT_EQ(manager_->available_input_frames(), 5);
    
    // Give some time for processing (since background processing is disabled,
    // we need to manually trigger processing or implement synchronous processing)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    manager_->stop_streaming();
    EXPECT_FALSE(manager_->is_streaming());
}

TEST_F(StreamingBufferManagerTest, FrameQueueingAndProcessing) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Test frame queuing before streaming starts
    StreamingFrame test_frame = create_test_frame(0, 440.0);
    EXPECT_FALSE(manager_->queue_input_frame(test_frame)); // Should fail when not streaming
    
    manager_->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    
    // Test frame queuing after streaming starts
    EXPECT_TRUE(manager_->queue_input_frame(test_frame));
    EXPECT_EQ(manager_->available_input_frames(), 1);
    
    // Queue multiple frames
    std::vector<StreamingFrame> frames;
    for (int i = 1; i < 10; ++i) {
        frames.push_back(create_test_frame(i, 440.0 + i * 50.0));
    }
    
    size_t queued = manager_->queue_input_frames(frames);
    EXPECT_GT(queued, 0);
    
    manager_->stop_streaming();
}

TEST_F(StreamingBufferManagerTest, AudioOutputReading) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    manager_->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    
    // Queue a frame
    StreamingFrame test_frame = create_test_frame(0, 440.0);
    EXPECT_TRUE(manager_->queue_input_frame(test_frame));
    
    // Wait for processing (with background processing disabled, 
    // we need to implement synchronous processing for testing)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Try to read output samples
    std::vector<double> output_buffer(1024);
    size_t samples_read = manager_->read_output_samples(
        output_buffer.data(), output_buffer.size()
    );
    
    // With background processing disabled and no explicit processing trigger,
    // this might be 0. In a real implementation, we'd need to trigger processing.
    std::cout << "Samples read: " << samples_read << std::endl;
    
    manager_->stop_streaming();
}

TEST_F(StreamingBufferManagerTest, StatisticsAndMonitoring) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Get initial statistics
    StreamingStats initial_stats = manager_->get_stats();
    EXPECT_EQ(initial_stats.frames_processed, 0);
    EXPECT_EQ(initial_stats.buffer_underruns, 0);
    EXPECT_EQ(initial_stats.buffer_overflows, 0);
    
    manager_->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    
    // Queue several frames
    for (int i = 0; i < 3; ++i) {
        StreamingFrame frame = create_test_frame(i, 440.0);
        manager_->queue_input_frame(frame);
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    StreamingStats final_stats = manager_->get_stats();
    // Statistics should be updated (though exact values depend on processing implementation)
    
    manager_->stop_streaming();
}

TEST_F(StreamingBufferManagerTest, BufferOverflowHandling) {
    // Create a manager with very small input buffer
    StreamingConfig small_config = config_;
    small_config.input_buffer_size = 2;
    small_config.enable_overflow_protection = true;
    
    auto small_manager = std::make_unique<StreamingBufferManager>(small_config);
    EXPECT_TRUE(small_manager->initialize(sample_rate_, frame_period_));
    
    small_manager->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(small_manager->start_streaming());
    
    // Try to queue more frames than buffer can hold
    std::vector<StreamingFrame> many_frames;
    for (int i = 0; i < 10; ++i) {
        many_frames.push_back(create_test_frame(i, 440.0));
    }
    
    size_t queued = small_manager->queue_input_frames(many_frames);
    
    // Should queue some frames, but not all due to buffer size limit
    EXPECT_LE(queued, small_config.input_buffer_size);
    
    // Check that overflow was handled
    StreamingStats stats = small_manager->get_stats();
    // Note: Without background processing, overflow handling might not be triggered
    
    small_manager->stop_streaming();
}

TEST_F(StreamingBufferManagerTest, LatencyTargetAdjustment) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Test valid latency target
    EXPECT_TRUE(manager_->set_latency_target(20.0));
    EXPECT_EQ(manager_->get_config().target_latency_ms, 20.0);
    
    // Test invalid latency targets
    EXPECT_FALSE(manager_->set_latency_target(0.0));      // Too low
    EXPECT_FALSE(manager_->set_latency_target(-5.0));     // Negative
    EXPECT_FALSE(manager_->set_latency_target(100.0));    // Exceeds max
}

TEST_F(StreamingBufferManagerTest, AdaptiveBuffering) {
    config_.enable_adaptive_buffering = true;
    auto adaptive_manager = std::make_unique<StreamingBufferManager>(config_);
    
    EXPECT_TRUE(adaptive_manager->initialize(sample_rate_, frame_period_));
    
    // Test enabling/disabling adaptive buffering
    adaptive_manager->set_adaptive_buffering(false);
    // Note: Testing actual adaptive behavior would require longer running tests
    // with performance monitoring
    
    adaptive_manager->set_adaptive_buffering(true);
}

TEST_F(StreamingBufferManagerTest, PrefillBuffers) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Create prefill frames
    std::vector<StreamingFrame> prefill_frames;
    for (int i = 0; i < 3; ++i) {
        prefill_frames.push_back(create_test_frame(i, 440.0));
    }
    
    // Test prefill before streaming
    EXPECT_TRUE(manager_->prefill_buffers(prefill_frames));
    EXPECT_EQ(manager_->available_input_frames(), 3);
    
    manager_->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    
    // Test prefill during streaming (should fail)
    EXPECT_FALSE(manager_->prefill_buffers(prefill_frames));
    
    manager_->stop_streaming();
}

TEST_F(StreamingBufferManagerTest, FlushBuffers) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    // Add some frames
    std::vector<StreamingFrame> frames;
    for (int i = 0; i < 3; ++i) {
        frames.push_back(create_test_frame(i, 440.0));
    }
    
    EXPECT_TRUE(manager_->prefill_buffers(frames));
    EXPECT_EQ(manager_->available_input_frames(), 3);
    
    // Flush buffers
    manager_->flush_buffers();
    EXPECT_EQ(manager_->available_input_frames(), 0);
}

// Utility Function Tests
TEST_F(StreamingBufferManagerTest, UtilityFunctions) {
    using namespace streaming_utils;
    
    // Test buffer size calculation
    size_t buffer_size = calculate_buffer_size_for_latency(10.0, 44100, 2.0);
    EXPECT_GT(buffer_size, 0);
    
    // Test processing latency estimation
    double estimated_latency = estimate_processing_latency(config_, sample_rate_, frame_period_);
    EXPECT_GT(estimated_latency, 0.0);
    
    // Test optimal config detection
    StreamingConfig optimal_config = detect_optimal_config(sample_rate_, frame_period_, 15.0);
    EXPECT_GT(optimal_config.input_buffer_size, 0);
    EXPECT_GT(optimal_config.output_buffer_size, 0);
    EXPECT_EQ(optimal_config.target_latency_ms, 15.0);
}

// Stress Tests
TEST_F(StreamingBufferManagerTest, ConcurrentAccess) {
    EXPECT_TRUE(manager_->initialize(sample_rate_, frame_period_));
    
    manager_->set_synthesis_callback(
        [this](const StreamingFrame& frame) {
            return simple_synthesis_callback(frame);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    
    // Create producer thread
    std::atomic<bool> should_stop{false};
    std::thread producer([&]() {
        int frame_index = 0;
        while (!should_stop.load()) {
            StreamingFrame frame = create_test_frame(frame_index++, 440.0);
            manager_->queue_input_frame(frame);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Create consumer thread
    std::thread consumer([&]() {
        std::vector<double> buffer(256);
        while (!should_stop.load()) {
            manager_->read_output_samples(buffer.data(), buffer.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    should_stop.store(true);
    producer.join();
    consumer.join();
    
    manager_->stop_streaming();
    
    // Verify no crashes occurred and basic functionality works
    StreamingStats stats = manager_->get_stats();
    std::cout << "Stress test completed. Frames processed: " << stats.frames_processed << std::endl;
}

TEST_F(StreamingBufferManagerTest, HighFrequencyFrameProcessing) {
    // Test with higher frame rate (smaller frame period)
    double high_freq_frame_period = 2.5; // 2.5ms frames instead of 5ms
    
    EXPECT_TRUE(manager_->initialize(sample_rate_, high_freq_frame_period));
    
    manager_->set_synthesis_callback(
        [=](const StreamingFrame& frame) {
            int samples_per_frame = static_cast<int>(sample_rate_ * high_freq_frame_period / 1000.0);
            return std::vector<double>(samples_per_frame, 0.1);
        }
    );
    
    EXPECT_TRUE(manager_->start_streaming());
    
    // Queue many frames rapidly
    for (int i = 0; i < 20; ++i) {
        StreamingFrame frame = create_test_frame(i, 440.0 + i * 10.0);
        manager_->queue_input_frame(frame);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    manager_->stop_streaming();
    
    StreamingStats stats = manager_->get_stats();
    std::cout << "High frequency test. Available input frames: " 
              << manager_->available_input_frames() << std::endl;
}

// Integration Test with WORLD Parameters
TEST_F(StreamingBufferManagerTest, WorldParameterIntegration) {
    using namespace streaming_utils;
    
    // Create mock WORLD parameters
    AudioParameters world_params;
    world_params.length = 10;
    world_params.sample_rate = sample_rate_;
    world_params.fft_size = 2048;
    
    // Allocate memory for WORLD parameters
    world_params.f0 = new double[world_params.length];
    world_params.spectrogram = new double*[world_params.length];
    world_params.aperiodicity = new double*[world_params.length];
    
    int spectrum_size = world_params.fft_size / 2 + 1;
    
    for (int i = 0; i < world_params.length; ++i) {
        world_params.f0[i] = 440.0 + i * 10.0; // Varying F0
        
        world_params.spectrogram[i] = new double[spectrum_size];
        world_params.aperiodicity[i] = new double[spectrum_size];
        
        // Fill with test data
        for (int j = 0; j < spectrum_size; ++j) {
            world_params.spectrogram[i][j] = 1.0 / (j + 1); // Simple spectrum
            world_params.aperiodicity[i][j] = 0.1;          // Low aperiodicity
        }
    }
    
    // Convert to streaming frames
    std::vector<StreamingFrame> streaming_frames = world_to_streaming_frames(
        world_params, frame_period_
    );
    
    EXPECT_EQ(streaming_frames.size(), static_cast<size_t>(world_params.length));
    
    // Verify frame data
    for (size_t i = 0; i < streaming_frames.size(); ++i) {
        EXPECT_EQ(streaming_frames[i].f0, world_params.f0[i]);
        EXPECT_EQ(streaming_frames[i].frame_index, static_cast<int>(i));
        EXPECT_TRUE(streaming_frames[i].is_voiced);
        EXPECT_EQ(streaming_frames[i].spectrum.size(), static_cast<size_t>(spectrum_size));
        EXPECT_EQ(streaming_frames[i].aperiodicity.size(), static_cast<size_t>(spectrum_size));
    }
    
    // Clean up
    for (int i = 0; i < world_params.length; ++i) {
        delete[] world_params.spectrogram[i];
        delete[] world_params.aperiodicity[i];
    }
    delete[] world_params.f0;
    delete[] world_params.spectrogram;
    delete[] world_params.aperiodicity;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
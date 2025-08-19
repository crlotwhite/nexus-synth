#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <chrono>
#include <queue>
#include <Eigen/Core>
#include "world_wrapper.h"

namespace nexussynth {
namespace synthesis {

    /**
     * @brief Real-time streaming buffer configuration
     * 
     * Configuration parameters for real-time audio streaming and buffering
     */
    struct StreamingConfig {
        // Buffer size configuration
        int input_buffer_size = 1024;        // Input parameter buffer size (frames)
        int output_buffer_size = 4096;       // Output audio buffer size (samples)
        int ring_buffer_size = 8192;         // Ring buffer capacity (samples)
        
        // Latency optimization
        double target_latency_ms = 10.0;      // Target processing latency
        double max_latency_ms = 50.0;         // Maximum acceptable latency
        int prefill_frames = 2;               // Number of frames to prefill
        
        // Threading and performance
        bool enable_background_processing = true;  // Use background processing thread
        int processing_thread_priority = 1;        // Processing thread priority (0-10)
        bool enable_adaptive_buffering = true;     // Dynamically adjust buffer sizes
        
        // Quality vs performance trade-offs
        bool enable_underrun_protection = true;    // Prevent audio underruns
        bool enable_overflow_protection = true;    // Prevent buffer overflows
        double cpu_usage_threshold = 0.8;         // CPU usage warning threshold
        
        // Advanced features
        bool enable_jitter_compensation = true;    // Compensate for timing jitter
        bool enable_dropout_detection = true;     // Detect and handle audio dropouts
        int dropout_threshold_samples = 64;       // Samples to consider a dropout
    };

    /**
     * @brief Real-time performance metrics
     * 
     * Statistics and monitoring for streaming performance
     */
    struct StreamingStats {
        // Latency metrics (in milliseconds)
        double current_latency_ms = 0.0;
        double average_latency_ms = 0.0;
        double peak_latency_ms = 0.0;
        
        // Buffer utilization
        double input_buffer_utilization = 0.0;    // 0.0-1.0
        double output_buffer_utilization = 0.0;   // 0.0-1.0
        double ring_buffer_utilization = 0.0;     // 0.0-1.0
        
        // Processing performance
        double processing_time_ms = 0.0;          // Time per processing cycle
        double cpu_usage_percent = 0.0;           // Estimated CPU usage
        int frames_processed = 0;                 // Total frames processed
        
        // Error tracking
        int buffer_underruns = 0;                 // Number of underrun events
        int buffer_overflows = 0;                 // Number of overflow events
        int dropouts_detected = 0;                // Number of dropout events
        
        // Timing statistics
        std::chrono::steady_clock::time_point session_start_time;
        double total_processing_time_ms = 0.0;
        double average_frame_time_ms = 0.0;
    };

    /**
     * @brief Input parameter frame for streaming
     * 
     * Contains all parameters needed for a single synthesis frame
     */
    struct StreamingFrame {
        // Core WORLD parameters
        double f0 = 0.0;                          // Fundamental frequency
        std::vector<double> spectrum;             // Spectral envelope
        std::vector<double> aperiodicity;         // Aperiodicity coefficients
        
        // Timing information
        double timestamp_ms = 0.0;                // Frame timestamp
        int frame_index = 0;                      // Sequential frame number
        
        // Synthesis control
        double amplitude_scale = 1.0;             // Overall amplitude scaling
        double pitch_shift = 1.0;                 // Real-time pitch shifting
        double formant_shift = 1.0;               // Real-time formant shifting
        
        // Quality flags
        bool is_voiced = true;                    // Whether frame is voiced
        bool enable_anti_aliasing = true;         // Enable anti-aliasing for this frame
        
        StreamingFrame() = default;
        
        StreamingFrame(double f0_val, const std::vector<double>& sp, const std::vector<double>& ap)
            : f0(f0_val), spectrum(sp), aperiodicity(ap) {}
    };

    /**
     * @brief Thread-safe ring buffer for audio streaming
     * 
     * Lock-free ring buffer optimized for real-time audio processing
     */
    template<typename T>
    class RingBuffer {
    public:
        explicit RingBuffer(size_t capacity);
        ~RingBuffer() = default;

        // Core operations
        bool push(const T& item);                 // Add item to buffer
        bool pop(T& item);                        // Remove item from buffer
        bool peek(T& item) const;                 // Look at next item without removing
        
        // Bulk operations
        size_t push_bulk(const T* items, size_t count);
        size_t pop_bulk(T* items, size_t count);
        
        // State queries
        size_t size() const;                      // Current number of items
        size_t capacity() const;                  // Maximum capacity
        bool empty() const;                       // Check if empty
        bool full() const;                        // Check if full
        double utilization() const;               // Utilization ratio (0.0-1.0)
        
        // Advanced operations
        void clear();                             // Clear all items
        void reset();                             // Reset to initial state
        size_t available_space() const;           // Available space for new items
        
    private:
        std::vector<T> buffer_;
        std::atomic<size_t> head_{0};            // Write position
        std::atomic<size_t> tail_{0};            // Read position
        size_t capacity_;
        
        // Helper functions
        size_t next_index(size_t index) const;
        size_t distance(size_t from, size_t to) const;
    };

    /**
     * @brief Real-time streaming buffer manager
     * 
     * Manages input parameter streams and output audio buffers for real-time synthesis
     */
    class StreamingBufferManager {
    public:
        explicit StreamingBufferManager(const StreamingConfig& config = StreamingConfig{});
        ~StreamingBufferManager();

        // Lifecycle management
        /**
         * @brief Initialize streaming system
         * 
         * @param sample_rate Target sample rate
         * @param frame_period Frame period in milliseconds
         * @return true if initialization successful
         */
        bool initialize(int sample_rate, double frame_period);

        /**
         * @brief Start real-time streaming
         * 
         * @return true if streaming started successfully
         */
        bool start_streaming();

        /**
         * @brief Stop real-time streaming
         */
        void stop_streaming();

        /**
         * @brief Check if streaming is active
         */
        bool is_streaming() const { return streaming_active_.load(); }

        // Input parameter management
        /**
         * @brief Queue input frame for synthesis
         * 
         * @param frame Input parameter frame
         * @return true if frame was queued successfully
         */
        bool queue_input_frame(const StreamingFrame& frame);

        /**
         * @brief Queue multiple input frames
         * 
         * @param frames Vector of input frames
         * @return Number of frames successfully queued
         */
        size_t queue_input_frames(const std::vector<StreamingFrame>& frames);

        /**
         * @brief Check input frame availability
         * 
         * @return Number of frames available for processing
         */
        size_t available_input_frames() const;

        // Output audio management
        /**
         * @brief Read synthesized audio samples
         * 
         * @param buffer Output buffer for audio samples
         * @param samples_requested Number of samples to read
         * @return Number of samples actually read
         */
        size_t read_output_samples(double* buffer, size_t samples_requested);

        /**
         * @brief Check output sample availability
         * 
         * @return Number of samples available for reading
         */
        size_t available_output_samples() const;

        /**
         * @brief Get current output buffer utilization
         * 
         * @return Utilization ratio (0.0 = empty, 1.0 = full)
         */
        double get_output_utilization() const;

        // Configuration and monitoring
        /**
         * @brief Update streaming configuration
         * 
         * @param config New configuration parameters
         * @return true if configuration updated successfully
         */
        bool update_config(const StreamingConfig& config);

        /**
         * @brief Get current configuration
         */
        const StreamingConfig& get_config() const { return config_; }

        /**
         * @brief Get real-time performance statistics
         */
        StreamingStats get_stats() const;

        /**
         * @brief Reset performance statistics
         */
        void reset_stats();

        // Advanced features
        /**
         * @brief Set synthesis callback function
         * 
         * Callback will be called for each input frame to generate output
         * 
         * @param callback Function to call for synthesis
         */
        void set_synthesis_callback(
            std::function<std::vector<double>(const StreamingFrame&)> callback
        );

        /**
         * @brief Enable/disable adaptive buffering
         * 
         * @param enable Enable adaptive buffer size adjustment
         */
        void set_adaptive_buffering(bool enable);

        /**
         * @brief Set latency target
         * 
         * @param target_ms Target latency in milliseconds
         * @return true if target is achievable
         */
        bool set_latency_target(double target_ms);

        /**
         * @brief Force buffer flush
         * 
         * Flushes all pending audio to output
         */
        void flush_buffers();

        /**
         * @brief Prefill buffers for smooth startup
         * 
         * @param frames Vector of initial frames to prefill
         * @return true if prefill successful
         */
        bool prefill_buffers(const std::vector<StreamingFrame>& frames);

        // Error handling and recovery
        /**
         * @brief Handle buffer underrun
         * 
         * Called when output buffer runs empty
         */
        void handle_underrun();

        /**
         * @brief Handle buffer overflow
         * 
         * Called when input buffer becomes full
         */
        void handle_overflow();

        /**
         * @brief Detect and handle audio dropouts
         * 
         * @return true if dropout was detected and handled
         */
        bool detect_and_handle_dropouts();

    private:
        StreamingConfig config_;
        
        // System state
        std::atomic<bool> initialized_{false};
        std::atomic<bool> streaming_active_{false};
        std::atomic<bool> shutdown_requested_{false};
        
        // Audio parameters
        int sample_rate_ = 44100;
        double frame_period_ms_ = 5.0;
        int samples_per_frame_ = 220;  // Calculated from sample_rate and frame_period
        
        // Ring buffers
        std::unique_ptr<RingBuffer<StreamingFrame>> input_buffer_;
        std::unique_ptr<RingBuffer<double>> output_buffer_;
        
        // Processing thread
        std::unique_ptr<std::thread> processing_thread_;
        std::mutex processing_mutex_;
        std::condition_variable processing_cv_;
        
        // Synthesis callback
        std::function<std::vector<double>(const StreamingFrame&)> synthesis_callback_;
        
        // Performance monitoring
        mutable std::mutex stats_mutex_;
        StreamingStats stats_;
        std::chrono::steady_clock::time_point last_stats_update_;
        
        // Adaptive buffering state
        double current_cpu_usage_ = 0.0;
        std::queue<double> latency_history_;
        std::chrono::steady_clock::time_point last_adaptive_adjustment_;
        
        // Internal methods
        /**
         * @brief Main processing thread function
         */
        void processing_thread_main();
        
        /**
         * @brief Process one cycle of input->synthesis->output
         * 
         * @return true if processing successful
         */
        bool process_cycle();
        
        /**
         * @brief Update performance statistics
         */
        void update_stats();
        
        /**
         * @brief Adjust buffer sizes based on performance
         */
        void adjust_adaptive_buffers();
        
        /**
         * @brief Calculate optimal buffer sizes
         * 
         * @param target_latency_ms Target latency
         * @return Recommended buffer configuration
         */
        StreamingConfig calculate_optimal_buffer_sizes(double target_latency_ms) const;
        
        /**
         * @brief Estimate CPU usage from processing times
         * 
         * @return Estimated CPU usage (0.0-1.0)
         */
        double estimate_cpu_usage() const;
        
        /**
         * @brief Generate silence to fill buffer underruns
         * 
         * @param samples Number of silence samples to generate
         */
        void generate_silence(size_t samples);
        
        /**
         * @brief Validate configuration parameters
         * 
         * @param config Configuration to validate
         * @return true if configuration is valid
         */
        bool validate_config(const StreamingConfig& config) const;
    };

    /**
     * @brief Utility functions for streaming buffer management
     */
    namespace streaming_utils {
        /**
         * @brief Calculate buffer size for target latency
         * 
         * @param target_latency_ms Target latency in milliseconds
         * @param sample_rate Sample rate
         * @param safety_factor Safety factor for buffer size (default: 2.0)
         * @return Recommended buffer size in samples
         */
        size_t calculate_buffer_size_for_latency(
            double target_latency_ms,
            int sample_rate,
            double safety_factor = 2.0
        );

        /**
         * @brief Convert WORLD parameters to streaming frames
         * 
         * @param world_params WORLD AudioParameters
         * @param frame_period Frame period in milliseconds
         * @return Vector of streaming frames
         */
        std::vector<StreamingFrame> world_to_streaming_frames(
            const AudioParameters& world_params,
            double frame_period
        );

        /**
         * @brief Estimate processing latency for given configuration
         * 
         * @param config Streaming configuration
         * @param sample_rate Sample rate
         * @param frame_period Frame period
         * @return Estimated latency in milliseconds
         */
        double estimate_processing_latency(
            const StreamingConfig& config,
            int sample_rate,
            double frame_period
        );

        /**
         * @brief Benchmark streaming performance
         * 
         * @param manager Streaming buffer manager to test
         * @param test_duration_ms Duration of test in milliseconds
         * @return Performance benchmark results
         */
        StreamingStats benchmark_streaming_performance(
            StreamingBufferManager& manager,
            double test_duration_ms
        );

        /**
         * @brief Detect optimal buffer configuration
         * 
         * @param sample_rate Target sample rate
         * @param frame_period Frame period in milliseconds
         * @param target_latency_ms Target latency
         * @return Optimized streaming configuration
         */
        StreamingConfig detect_optimal_config(
            int sample_rate,
            double frame_period,
            double target_latency_ms
        );
    }

} // namespace synthesis
} // namespace nexussynth
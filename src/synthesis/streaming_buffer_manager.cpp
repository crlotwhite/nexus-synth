#include "nexussynth/streaming_buffer_manager.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <iomanip>

namespace nexussynth {
namespace synthesis {

    // Ring Buffer Implementation
    template<typename T>
    RingBuffer<T>::RingBuffer(size_t capacity) 
        : buffer_(capacity + 1), capacity_(capacity + 1) {
        // Add one extra slot to distinguish between full and empty states
    }

    template<typename T>
    bool RingBuffer<T>::push(const T& item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head = next_index(current_head);
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer is full
        }
        
        buffer_[current_head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    template<typename T>
    bool RingBuffer<T>::pop(T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer is empty
        }
        
        item = buffer_[current_tail];
        tail_.store(next_index(current_tail), std::memory_order_release);
        return true;
    }

    template<typename T>
    bool RingBuffer<T>::peek(T& item) const {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer is empty
        }
        
        item = buffer_[current_tail];
        return true;
    }

    template<typename T>
    size_t RingBuffer<T>::push_bulk(const T* items, size_t count) {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!push(items[i])) {
                break;
            }
            ++pushed;
        }
        return pushed;
    }

    template<typename T>
    size_t RingBuffer<T>::pop_bulk(T* items, size_t count) {
        size_t popped = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!pop(items[i])) {
                break;
            }
            ++popped;
        }
        return popped;
    }

    template<typename T>
    size_t RingBuffer<T>::size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return distance(tail, head);
    }

    template<typename T>
    size_t RingBuffer<T>::capacity() const {
        return capacity_ - 1; // Subtract the extra slot used for full/empty distinction
    }

    template<typename T>
    bool RingBuffer<T>::empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    template<typename T>
    bool RingBuffer<T>::full() const {
        size_t current_head = head_.load(std::memory_order_acquire);
        size_t current_tail = tail_.load(std::memory_order_acquire);
        return next_index(current_head) == current_tail;
    }

    template<typename T>
    double RingBuffer<T>::utilization() const {
        return static_cast<double>(size()) / capacity();
    }

    template<typename T>
    void RingBuffer<T>::clear() {
        tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
    }

    template<typename T>
    void RingBuffer<T>::reset() {
        head_.store(0, std::memory_order_release);
        tail_.store(0, std::memory_order_release);
    }

    template<typename T>
    size_t RingBuffer<T>::available_space() const {
        return capacity() - size();
    }

    template<typename T>
    size_t RingBuffer<T>::next_index(size_t index) const {
        return (index + 1) % capacity_;
    }

    template<typename T>
    size_t RingBuffer<T>::distance(size_t from, size_t to) const {
        if (to >= from) {
            return to - from;
        }
        return capacity_ - from + to;
    }

    // Explicit template instantiations
    template class RingBuffer<StreamingFrame>;
    template class RingBuffer<double>;

    // StreamingBufferManager Implementation
    StreamingBufferManager::StreamingBufferManager(const StreamingConfig& config)
        : config_(config) {
        
        // Initialize statistics
        stats_.session_start_time = std::chrono::steady_clock::now();
        last_stats_update_ = stats_.session_start_time;
        last_adaptive_adjustment_ = stats_.session_start_time;
        
        // Validate configuration
        if (!validate_config(config_)) {
            throw std::invalid_argument("Invalid streaming configuration");
        }
    }

    StreamingBufferManager::~StreamingBufferManager() {
        stop_streaming();
    }

    bool StreamingBufferManager::initialize(int sample_rate, double frame_period) {
        if (streaming_active_.load()) {
            std::cerr << "Cannot initialize while streaming is active" << std::endl;
            return false;
        }

        sample_rate_ = sample_rate;
        frame_period_ms_ = frame_period;
        samples_per_frame_ = static_cast<int>(sample_rate * frame_period / 1000.0);

        // Create ring buffers with configured sizes
        try {
            input_buffer_ = std::make_unique<RingBuffer<StreamingFrame>>(config_.input_buffer_size);
            output_buffer_ = std::make_unique<RingBuffer<double>>(config_.output_buffer_size);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create ring buffers: " << e.what() << std::endl;
            return false;
        }

        // Validate buffer sizes against latency requirements
        double estimated_latency = streaming_utils::estimate_processing_latency(
            config_, sample_rate_, frame_period_ms_
        );
        
        if (estimated_latency > config_.max_latency_ms) {
            std::cerr << "Warning: Estimated latency (" << estimated_latency 
                      << "ms) exceeds maximum (" << config_.max_latency_ms << "ms)" << std::endl;
        }

        initialized_.store(true);
        
        std::cout << "Streaming buffer manager initialized:" << std::endl;
        std::cout << "  Sample rate: " << sample_rate_ << " Hz" << std::endl;
        std::cout << "  Frame period: " << frame_period_ms_ << " ms" << std::endl;
        std::cout << "  Samples per frame: " << samples_per_frame_ << std::endl;
        std::cout << "  Input buffer size: " << config_.input_buffer_size << " frames" << std::endl;
        std::cout << "  Output buffer size: " << config_.output_buffer_size << " samples" << std::endl;
        std::cout << "  Estimated latency: " << std::fixed << std::setprecision(2) 
                  << estimated_latency << " ms" << std::endl;

        return true;
    }

    bool StreamingBufferManager::start_streaming() {
        if (!initialized_.load()) {
            std::cerr << "StreamingBufferManager not initialized" << std::endl;
            return false;
        }

        if (streaming_active_.load()) {
            std::cerr << "Streaming already active" << std::endl;
            return false;
        }

        if (!synthesis_callback_) {
            std::cerr << "Synthesis callback not set" << std::endl;
            return false;
        }

        // Reset state
        shutdown_requested_.store(false);
        reset_stats();

        // Start processing thread if background processing is enabled
        if (config_.enable_background_processing) {
            try {
                processing_thread_ = std::make_unique<std::thread>(
                    &StreamingBufferManager::processing_thread_main, this
                );
                
                // Set thread priority if supported by the system
                #ifdef __linux__
                pthread_t native_handle = processing_thread_->native_handle();
                sched_param sch_params;
                sch_params.sched_priority = config_.processing_thread_priority;
                if (pthread_setschedparam(native_handle, SCHED_FIFO, &sch_params) != 0) {
                    std::cerr << "Warning: Failed to set thread priority" << std::endl;
                }
                #endif
                
            } catch (const std::exception& e) {
                std::cerr << "Failed to create processing thread: " << e.what() << std::endl;
                return false;
            }
        }

        streaming_active_.store(true);
        stats_.session_start_time = std::chrono::steady_clock::now();
        
        std::cout << "Real-time streaming started" << std::endl;
        return true;
    }

    void StreamingBufferManager::stop_streaming() {
        if (!streaming_active_.load()) {
            return;
        }

        // Signal shutdown
        shutdown_requested_.store(true);
        streaming_active_.store(false);

        // Wake up processing thread
        processing_cv_.notify_all();

        // Wait for processing thread to finish
        if (processing_thread_ && processing_thread_->joinable()) {
            processing_thread_->join();
            processing_thread_.reset();
        }

        std::cout << "Real-time streaming stopped" << std::endl;
        
        // Print final statistics
        StreamingStats final_stats = get_stats();
        std::cout << "Final streaming statistics:" << std::endl;
        std::cout << "  Frames processed: " << final_stats.frames_processed << std::endl;
        std::cout << "  Average latency: " << std::fixed << std::setprecision(2) 
                  << final_stats.average_latency_ms << " ms" << std::endl;
        std::cout << "  Peak latency: " << std::fixed << std::setprecision(2) 
                  << final_stats.peak_latency_ms << " ms" << std::endl;
        std::cout << "  Buffer underruns: " << final_stats.buffer_underruns << std::endl;
        std::cout << "  Buffer overflows: " << final_stats.buffer_overflows << std::endl;
    }

    bool StreamingBufferManager::queue_input_frame(const StreamingFrame& frame) {
        if (!streaming_active_.load()) {
            return false;
        }

        bool success = input_buffer_->push(frame);
        if (!success && config_.enable_overflow_protection) {
            handle_overflow();
            // Try again after overflow handling
            success = input_buffer_->push(frame);
        }

        return success;
    }

    size_t StreamingBufferManager::queue_input_frames(const std::vector<StreamingFrame>& frames) {
        if (!streaming_active_.load()) {
            return 0;
        }

        size_t queued = 0;
        for (const auto& frame : frames) {
            if (queue_input_frame(frame)) {
                ++queued;
            } else {
                break; // Stop on first failure to maintain frame order
            }
        }

        return queued;
    }

    size_t StreamingBufferManager::available_input_frames() const {
        if (!input_buffer_) {
            return 0;
        }
        return input_buffer_->size();
    }

    size_t StreamingBufferManager::read_output_samples(double* buffer, size_t samples_requested) {
        if (!streaming_active_.load() || !buffer || samples_requested == 0) {
            return 0;
        }

        size_t samples_read = output_buffer_->pop_bulk(buffer, samples_requested);
        
        // Handle underrun if we couldn't provide enough samples
        if (samples_read < samples_requested && config_.enable_underrun_protection) {
            handle_underrun();
            
            // Fill remaining buffer with silence
            size_t remaining = samples_requested - samples_read;
            std::fill(buffer + samples_read, buffer + samples_requested, 0.0);
            samples_read = samples_requested;
        }

        return samples_read;
    }

    size_t StreamingBufferManager::available_output_samples() const {
        if (!output_buffer_) {
            return 0;
        }
        return output_buffer_->size();
    }

    double StreamingBufferManager::get_output_utilization() const {
        if (!output_buffer_) {
            return 0.0;
        }
        return output_buffer_->utilization();
    }

    bool StreamingBufferManager::update_config(const StreamingConfig& config) {
        if (streaming_active_.load()) {
            std::cerr << "Cannot update configuration while streaming is active" << std::endl;
            return false;
        }

        if (!validate_config(config)) {
            std::cerr << "Invalid configuration parameters" << std::endl;
            return false;
        }

        config_ = config;
        return true;
    }

    StreamingStats StreamingBufferManager::get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        StreamingStats current_stats = stats_;
        
        // Update real-time metrics
        if (input_buffer_) {
            current_stats.input_buffer_utilization = input_buffer_->utilization();
        }
        if (output_buffer_) {
            current_stats.output_buffer_utilization = output_buffer_->utilization();
        }
        
        // Calculate average metrics
        if (current_stats.frames_processed > 0) {
            current_stats.average_latency_ms = 
                current_stats.total_processing_time_ms / current_stats.frames_processed;
            current_stats.average_frame_time_ms = 
                current_stats.total_processing_time_ms / current_stats.frames_processed;
        }
        
        current_stats.cpu_usage_percent = current_cpu_usage_;
        
        return current_stats;
    }

    void StreamingBufferManager::reset_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        stats_ = StreamingStats{};
        stats_.session_start_time = std::chrono::steady_clock::now();
        last_stats_update_ = stats_.session_start_time;
        current_cpu_usage_ = 0.0;
        
        // Clear latency history for adaptive buffering
        while (!latency_history_.empty()) {
            latency_history_.pop();
        }
    }

    void StreamingBufferManager::set_synthesis_callback(
        std::function<std::vector<double>(const StreamingFrame&)> callback) {
        synthesis_callback_ = callback;
    }

    void StreamingBufferManager::set_adaptive_buffering(bool enable) {
        config_.enable_adaptive_buffering = enable;
        if (enable) {
            last_adaptive_adjustment_ = std::chrono::steady_clock::now();
        }
    }

    bool StreamingBufferManager::set_latency_target(double target_ms) {
        if (target_ms <= 0.0 || target_ms > config_.max_latency_ms) {
            return false;
        }
        
        config_.target_latency_ms = target_ms;
        
        // Recalculate optimal buffer sizes if adaptive buffering is enabled
        if (config_.enable_adaptive_buffering) {
            adjust_adaptive_buffers();
        }
        
        return true;
    }

    void StreamingBufferManager::flush_buffers() {
        if (input_buffer_) {
            input_buffer_->clear();
        }
        if (output_buffer_) {
            output_buffer_->clear();
        }
    }

    bool StreamingBufferManager::prefill_buffers(const std::vector<StreamingFrame>& frames) {
        if (streaming_active_.load()) {
            std::cerr << "Cannot prefill buffers while streaming is active" << std::endl;
            return false;
        }

        if (!input_buffer_) {
            std::cerr << "Input buffer not initialized" << std::endl;
            return false;
        }

        // Queue prefill frames
        size_t prefilled = 0;
        for (const auto& frame : frames) {
            if (input_buffer_->push(frame)) {
                ++prefilled;
            } else {
                break;
            }
        }

        std::cout << "Prefilled " << prefilled << " frames into input buffer" << std::endl;
        return prefilled > 0;
    }

    void StreamingBufferManager::handle_underrun() {
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            ++stats_.buffer_underruns;
        }
        
        std::cerr << "Warning: Buffer underrun detected" << std::endl;
        
        // Generate silence to fill the gap
        generate_silence(samples_per_frame_);
    }

    void StreamingBufferManager::handle_overflow() {
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            ++stats_.buffer_overflows;
        }
        
        std::cerr << "Warning: Buffer overflow detected" << std::endl;
        
        // Drop oldest frames to make room
        StreamingFrame dummy_frame;
        while (input_buffer_->full() && input_buffer_->pop(dummy_frame)) {
            // Drop frame
        }
    }

    bool StreamingBufferManager::detect_and_handle_dropouts() {
        if (!config_.enable_dropout_detection) {
            return false;
        }
        
        // Check for consecutive silence in output buffer
        // This is a simplified dropout detection - could be enhanced
        size_t available_samples = available_output_samples();
        if (available_samples < config_.dropout_threshold_samples) {
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                ++stats_.dropouts_detected;
            }
            
            std::cerr << "Warning: Audio dropout detected" << std::endl;
            generate_silence(config_.dropout_threshold_samples);
            return true;
        }
        
        return false;
    }

    void StreamingBufferManager::processing_thread_main() {
        std::cout << "Processing thread started" << std::endl;
        
        while (!shutdown_requested_.load()) {
            bool processed = process_cycle();
            
            if (!processed) {
                // No work to do, wait briefly
                std::unique_lock<std::mutex> lock(processing_mutex_);
                processing_cv_.wait_for(lock, std::chrono::microseconds(100));
            }
            
            // Update statistics periodically
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_update_ > std::chrono::milliseconds(100)) {
                update_stats();
                last_stats_update_ = now;
                
                // Perform adaptive adjustments if enabled
                if (config_.enable_adaptive_buffering && 
                    now - last_adaptive_adjustment_ > std::chrono::seconds(1)) {
                    adjust_adaptive_buffers();
                    last_adaptive_adjustment_ = now;
                }
            }
            
            // Check for dropouts
            detect_and_handle_dropouts();
        }
        
        std::cout << "Processing thread finished" << std::endl;
    }

    bool StreamingBufferManager::process_cycle() {
        if (!synthesis_callback_) {
            return false;
        }
        
        // Try to get input frame
        StreamingFrame input_frame;
        if (!input_buffer_->pop(input_frame)) {
            return false; // No input available
        }
        
        auto cycle_start = std::chrono::high_resolution_clock::now();
        
        // Synthesize audio from frame
        std::vector<double> synthesized_audio;
        try {
            synthesized_audio = synthesis_callback_(input_frame);
        } catch (const std::exception& e) {
            std::cerr << "Synthesis callback failed: " << e.what() << std::endl;
            return false;
        }
        
        auto cycle_end = std::chrono::high_resolution_clock::now();
        double cycle_time_ms = std::chrono::duration<double, std::milli>(
            cycle_end - cycle_start
        ).count();
        
        // Push synthesized audio to output buffer
        size_t samples_pushed = output_buffer_->push_bulk(
            synthesized_audio.data(), synthesized_audio.size()
        );
        
        if (samples_pushed < synthesized_audio.size()) {
            std::cerr << "Warning: Output buffer full, dropped " 
                      << (synthesized_audio.size() - samples_pushed) << " samples" << std::endl;
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            ++stats_.frames_processed;
            stats_.processing_time_ms = cycle_time_ms;
            stats_.total_processing_time_ms += cycle_time_ms;
            stats_.current_latency_ms = cycle_time_ms;
            stats_.peak_latency_ms = std::max(stats_.peak_latency_ms, cycle_time_ms);
        }
        
        return true;
    }

    void StreamingBufferManager::update_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        // Update CPU usage estimate
        current_cpu_usage_ = estimate_cpu_usage();
        
        // Update buffer utilization
        if (input_buffer_) {
            stats_.input_buffer_utilization = input_buffer_->utilization();
        }
        if (output_buffer_) {
            stats_.output_buffer_utilization = output_buffer_->utilization();
        }
    }

    void StreamingBufferManager::adjust_adaptive_buffers() {
        if (!config_.enable_adaptive_buffering) {
            return;
        }
        
        // Get current performance metrics
        StreamingStats current_stats = get_stats();
        
        // Add current latency to history
        latency_history_.push(current_stats.current_latency_ms);
        if (latency_history_.size() > 10) {
            latency_history_.pop(); // Keep only recent history
        }
        
        // Calculate average recent latency
        double avg_latency = 0.0;
        std::queue<double> temp_queue = latency_history_;
        while (!temp_queue.empty()) {
            avg_latency += temp_queue.front();
            temp_queue.pop();
        }
        avg_latency /= latency_history_.size();
        
        // Adjust buffers if latency is too high or CPU usage is high
        if (avg_latency > config_.target_latency_ms || current_cpu_usage_ > config_.cpu_usage_threshold) {
            std::cout << "Adaptive buffering: Increasing buffer sizes due to high latency/CPU usage" << std::endl;
            
            // Increase buffer sizes by 25%
            config_.input_buffer_size = static_cast<int>(config_.input_buffer_size * 1.25);
            config_.output_buffer_size = static_cast<int>(config_.output_buffer_size * 1.25);
        } else if (avg_latency < config_.target_latency_ms * 0.5 && current_cpu_usage_ < 0.5) {
            std::cout << "Adaptive buffering: Decreasing buffer sizes due to low latency/CPU usage" << std::endl;
            
            // Decrease buffer sizes by 10% (more conservative)
            config_.input_buffer_size = static_cast<int>(config_.input_buffer_size * 0.9);
            config_.output_buffer_size = static_cast<int>(config_.output_buffer_size * 0.9);
            
            // Ensure minimum buffer sizes
            config_.input_buffer_size = std::max(config_.input_buffer_size, 256);
            config_.output_buffer_size = std::max(config_.output_buffer_size, 1024);
        }
    }

    StreamingConfig StreamingBufferManager::calculate_optimal_buffer_sizes(double target_latency_ms) const {
        StreamingConfig optimal_config = config_;
        
        // Calculate buffer size for target latency
        size_t target_buffer_size = streaming_utils::calculate_buffer_size_for_latency(
            target_latency_ms, sample_rate_, 2.0
        );
        
        optimal_config.output_buffer_size = static_cast<int>(target_buffer_size);
        optimal_config.input_buffer_size = std::max(
            static_cast<int>(target_buffer_size / samples_per_frame_), 
            4  // Minimum 4 frames
        );
        
        return optimal_config;
    }

    double StreamingBufferManager::estimate_cpu_usage() const {
        if (stats_.frames_processed == 0) {
            return 0.0;
        }
        
        // Estimate based on processing time vs real-time
        double real_time_per_frame_ms = frame_period_ms_;
        double avg_processing_time_ms = stats_.total_processing_time_ms / stats_.frames_processed;
        
        return std::min(1.0, avg_processing_time_ms / real_time_per_frame_ms);
    }

    void StreamingBufferManager::generate_silence(size_t samples) {
        std::vector<double> silence(samples, 0.0);
        output_buffer_->push_bulk(silence.data(), samples);
    }

    bool StreamingBufferManager::validate_config(const StreamingConfig& config) const {
        if (config.input_buffer_size <= 0 || config.output_buffer_size <= 0 || 
            config.ring_buffer_size <= 0) {
            return false;
        }
        
        if (config.target_latency_ms <= 0.0 || config.max_latency_ms <= config.target_latency_ms) {
            return false;
        }
        
        if (config.processing_thread_priority < 0 || config.processing_thread_priority > 10) {
            return false;
        }
        
        return true;
    }

    // Utility Functions Implementation
    namespace streaming_utils {
        
        size_t calculate_buffer_size_for_latency(
            double target_latency_ms,
            int sample_rate,
            double safety_factor) {
            
            double samples_for_latency = (target_latency_ms / 1000.0) * sample_rate;
            return static_cast<size_t>(samples_for_latency * safety_factor);
        }

        std::vector<StreamingFrame> world_to_streaming_frames(
            const AudioParameters& world_params,
            double frame_period) {
            
            std::vector<StreamingFrame> frames;
            frames.reserve(world_params.length);
            
            for (int i = 0; i < world_params.length; ++i) {
                StreamingFrame frame;
                frame.f0 = world_params.f0[i];
                
                // Copy spectrum and aperiodicity vectors
                if (i < static_cast<int>(world_params.spectrum.size())) {
                    frame.spectrum = world_params.spectrum[i];
                } else {
                    // Default spectrum if not available
                    int spectrum_size = world_params.fft_size / 2 + 1;
                    frame.spectrum.resize(spectrum_size, 1.0);
                }
                
                if (i < static_cast<int>(world_params.aperiodicity.size())) {
                    frame.aperiodicity = world_params.aperiodicity[i];
                } else {
                    // Default aperiodicity if not available  
                    int spectrum_size = world_params.fft_size / 2 + 1;
                    frame.aperiodicity.resize(spectrum_size, 0.1);
                }
                
                frame.timestamp_ms = i * frame_period;
                frame.frame_index = i;
                frame.is_voiced = (frame.f0 > 0.0);
                
                frames.push_back(frame);
            }
            
            return frames;
        }

        double estimate_processing_latency(
            const StreamingConfig& config,
            int sample_rate,
            double frame_period) {
            
            // Input buffer latency
            double input_latency_ms = config.input_buffer_size * frame_period;
            
            // Output buffer latency
            double output_latency_ms = (static_cast<double>(config.output_buffer_size) / sample_rate) * 1000.0;
            
            // Processing latency (estimated)
            double processing_latency_ms = frame_period * 0.5; // Conservative estimate
            
            return input_latency_ms + output_latency_ms + processing_latency_ms;
        }

        StreamingStats benchmark_streaming_performance(
            StreamingBufferManager& manager,
            double test_duration_ms) {
            
            // This would be implemented to run a performance test
            // For now, return current stats
            return manager.get_stats();
        }

        StreamingConfig detect_optimal_config(
            int sample_rate,
            double frame_period,
            double target_latency_ms) {
            
            StreamingConfig config;
            
            // Calculate optimal buffer sizes
            size_t target_output_samples = calculate_buffer_size_for_latency(
                target_latency_ms, sample_rate, 1.5
            );
            
            config.output_buffer_size = static_cast<int>(target_output_samples);
            config.input_buffer_size = static_cast<int>(target_latency_ms / frame_period) + 2;
            config.ring_buffer_size = config.output_buffer_size * 2;
            
            config.target_latency_ms = target_latency_ms;
            config.max_latency_ms = target_latency_ms * 2.0;
            
            // Enable adaptive features for better performance
            config.enable_adaptive_buffering = true;
            config.enable_background_processing = true;
            config.enable_underrun_protection = true;
            config.enable_overflow_protection = true;
            
            return config;
        }
    }

} // namespace synthesis
} // namespace nexussynth
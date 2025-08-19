#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <future>
#include "conditioning_config.h"
#include "voicebank_scanner.h"

namespace nexussynth {
namespace conditioning {

    /**
     * @brief Batch processing job item
     * 
     * Represents a single work item in the batch processing queue
     */
    struct BatchJob {
        std::string id;                     // Unique job identifier
        std::string input_path;             // Input voice bank path
        std::string output_path;            // Output .nvm file path
        ConditioningConfig config;          // Processing configuration
        std::string voice_bank_name;        // Voice bank display name
        size_t estimated_duration_ms;       // Estimated processing time
        
        // Job metadata
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point started_time;
        std::chrono::system_clock::time_point completed_time;
        
        BatchJob(const std::string& job_id, const std::string& input, 
                const std::string& output, const ConditioningConfig& cfg)
            : id(job_id), input_path(input), output_path(output), config(cfg)
            , estimated_duration_ms(0)
            , created_time(std::chrono::system_clock::now()) {}
    };

    /**
     * @brief Batch processing statistics
     */
    struct BatchProcessingStats {
        size_t total_jobs;                  // Total number of jobs
        size_t completed_jobs;              // Number of completed jobs
        size_t failed_jobs;                 // Number of failed jobs
        size_t active_jobs;                 // Currently processing jobs
        size_t queued_jobs;                 // Jobs waiting in queue
        
        // Performance metrics
        double average_processing_time_ms;   // Average job processing time
        double total_processing_time_ms;     // Total processing time
        size_t total_input_files;            // Total input files processed
        size_t total_output_size_bytes;      // Total output size in bytes
        
        // Resource usage
        double peak_memory_usage_mb;         // Peak memory usage
        double current_memory_usage_mb;      // Current memory usage
        int active_threads;                  // Number of active threads
        
        // Timing
        std::chrono::system_clock::time_point batch_start_time;
        std::chrono::system_clock::time_point estimated_completion_time;
        
        BatchProcessingStats() 
            : total_jobs(0), completed_jobs(0), failed_jobs(0)
            , active_jobs(0), queued_jobs(0)
            , average_processing_time_ms(0.0), total_processing_time_ms(0.0)
            , total_input_files(0), total_output_size_bytes(0)
            , peak_memory_usage_mb(0.0), current_memory_usage_mb(0.0)
            , active_threads(0)
            , batch_start_time(std::chrono::system_clock::now()) {}
    };

    /**
     * @brief Job processing result
     */
    struct JobResult {
        std::string job_id;                 // Job identifier
        bool success;                       // Processing success
        std::string error_message;          // Error details if failed
        std::vector<std::string> warnings;  // Processing warnings
        
        // Processing metrics
        std::chrono::duration<double, std::milli> processing_time;
        size_t input_files_processed;       // Number of input files
        size_t output_file_size_bytes;      // Output file size
        double compression_ratio;           // Size compression ratio
        
        // Quality metrics
        double estimated_quality_score;     // Quality estimation (0-1)
        std::unordered_map<std::string, double> quality_metrics;
        
        JobResult(const std::string& id) 
            : job_id(id), success(false), input_files_processed(0)
            , output_file_size_bytes(0), compression_ratio(0.0)
            , estimated_quality_score(0.0) {}
    };

    /**
     * @brief Batch processing progress callback interface
     */
    class BatchProgressCallback {
    public:
        virtual ~BatchProgressCallback() = default;
        
        // Batch-level events
        virtual void on_batch_started(size_t total_jobs) {}
        virtual void on_batch_completed(const BatchProcessingStats& stats) {}
        virtual void on_batch_progress(const BatchProcessingStats& stats) {}
        virtual void on_batch_paused() {}
        virtual void on_batch_resumed() {}
        virtual void on_batch_cancelled() {}
        
        // Job-level events
        virtual void on_job_started(const BatchJob& job) {}
        virtual void on_job_completed(const BatchJob& job, const JobResult& result) {}
        virtual void on_job_failed(const BatchJob& job, const std::string& error) {}
        virtual void on_job_progress(const BatchJob& job, double progress_percent) {}
        
        // Resource monitoring events
        virtual void on_memory_warning(double current_mb, double limit_mb) {}
        virtual void on_performance_degradation(const std::string& reason) {}
        
        // ETA and time estimation events
        virtual void on_eta_updated(const std::chrono::system_clock::time_point& estimated_completion) {}
    };

    /**
     * @brief Thread pool for batch processing
     * 
     * Manages worker threads and job distribution with dynamic scaling
     */
    class ThreadPool {
    public:
        explicit ThreadPool(size_t num_threads = 0);  // 0 = auto-detect CPU cores
        ~ThreadPool();
        
        // Thread management
        void start();
        void stop();
        void resize(size_t new_size);
        size_t size() const { return num_threads_; }
        
        // Job submission
        template<typename F, typename... Args>
        auto submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
        
        // Status queries
        size_t queued_jobs() const { return job_queue_.size(); }
        size_t active_jobs() const { return active_jobs_.load(); }
        bool is_running() const { return running_.load(); }
        
    private:
        size_t num_threads_;
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> job_queue_;
        
        mutable std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::atomic<bool> running_;
        std::atomic<size_t> active_jobs_;
        
        void worker_thread();
    };

    /**
     * @brief Resource monitor for memory and performance tracking
     */
    class ResourceMonitor {
    public:
        ResourceMonitor();
        ~ResourceMonitor();
        
        // Memory monitoring
        double get_current_memory_usage_mb() const;
        double get_peak_memory_usage_mb() const;
        void reset_peak_memory();
        
        // CPU monitoring
        double get_cpu_usage_percent() const;
        int get_cpu_core_count() const;
        
        // System resources
        double get_available_memory_mb() const;
        double get_disk_space_mb(const std::string& path) const;
        
        // Monitoring control
        void start_monitoring();
        void stop_monitoring();
        void set_memory_limit_mb(double limit);
        bool is_memory_limit_exceeded() const;
        
    private:
        std::atomic<bool> monitoring_active_;
        std::atomic<double> current_memory_mb_;
        std::atomic<double> peak_memory_mb_;
        std::atomic<double> memory_limit_mb_;
        std::thread monitor_thread_;
        mutable std::mutex stats_mutex_;
        
        void monitor_loop();
        double calculate_memory_usage() const;
    };

    /**
     * @brief Main batch processing engine
     * 
     * Coordinates batch processing jobs with progress tracking,
     * resource management, and error handling
     */
    class BatchProcessor {
    public:
        explicit BatchProcessor(const BatchProcessingConfig& config);
        ~BatchProcessor();
        
        // Configuration
        void set_config(const BatchProcessingConfig& config);
        const BatchProcessingConfig& get_config() const { return config_; }
        
        // Callback management
        void set_progress_callback(std::shared_ptr<BatchProgressCallback> callback);
        void remove_progress_callback();
        
        // Job management
        std::string add_job(const std::string& input_path, const std::string& output_path, 
                           const ConditioningConfig& conditioning_config);
        bool remove_job(const std::string& job_id);
        void clear_jobs();
        
        // Batch execution control
        bool start_batch();
        void pause_batch();
        void resume_batch();
        void cancel_batch();
        bool is_running() const { return state_ == ProcessingState::RUNNING; }
        bool is_paused() const { return state_ == ProcessingState::PAUSED; }
        
        // Status and statistics
        BatchProcessingStats get_stats() const;
        std::vector<BatchJob> get_jobs() const;
        std::vector<JobResult> get_results() const;
        
        // Error handling
        std::vector<std::string> get_error_log() const;
        void clear_error_log();
        
        // Resource management
        void set_memory_limit_mb(double limit);
        void set_thread_count(size_t count);
        bool check_system_resources() const;
        
    private:
        enum class ProcessingState {
            IDLE,
            RUNNING,
            PAUSED,
            CANCELLING,
            COMPLETED,
            FAILED
        };
        
        BatchProcessingConfig config_;
        std::unique_ptr<ThreadPool> thread_pool_;
        std::unique_ptr<ResourceMonitor> resource_monitor_;
        std::shared_ptr<BatchProgressCallback> progress_callback_;
        
        // State management
        std::atomic<ProcessingState> state_;
        mutable std::mutex state_mutex_;
        std::condition_variable state_condition_;
        
        // Job management
        std::vector<BatchJob> jobs_;
        std::queue<size_t> job_queue_;
        std::vector<JobResult> results_;
        mutable std::mutex jobs_mutex_;
        
        // Statistics
        BatchProcessingStats stats_;
        mutable std::mutex stats_mutex_;
        
        // Error tracking
        std::vector<std::string> error_log_;
        mutable std::mutex error_mutex_;
        
        // Internal processing
        void submit_next_job();
        void process_job(size_t job_index);
        JobResult process_voice_bank(const BatchJob& job);
        void update_statistics(const JobResult& result);
        void estimate_completion_time();
        
        // Resource management
        bool check_memory_limits() const;
        void handle_resource_warning(const std::string& warning);
        
        // Progress reporting
        void report_batch_progress();
        void report_job_progress(const BatchJob& job, double progress);
        
        // Error handling
        void log_error(const std::string& error);
        void handle_job_failure(const BatchJob& job, const std::string& error);
        
        // Thread synchronization
        void wait_for_state_change();
        void notify_state_change();
    };

    /**
     * @brief Console progress reporter for batch processing
     */
    class ConsoleBatchProgressCallback : public BatchProgressCallback {
    public:
        ConsoleBatchProgressCallback();
        ~ConsoleBatchProgressCallback() = default;
        
        void on_batch_started(size_t total_jobs) override;
        void on_batch_completed(const BatchProcessingStats& stats) override;
        void on_batch_progress(const BatchProcessingStats& stats) override;
        void on_job_started(const BatchJob& job) override;
        void on_job_completed(const BatchJob& job, const JobResult& result) override;
        void on_job_failed(const BatchJob& job, const std::string& error) override;
        void on_eta_updated(const std::chrono::system_clock::time_point& estimated_completion) override;
        
    private:
        std::chrono::steady_clock::time_point last_update_time_;
        void print_progress_bar(size_t current, size_t total, int width = 50);
        std::string format_duration(std::chrono::duration<double> duration);
        std::string format_file_size(size_t bytes);
    };

} // namespace conditioning
} // namespace nexussynth
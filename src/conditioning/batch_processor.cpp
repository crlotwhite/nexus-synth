#include "nexussynth/batch_processor.h"
#include "nexussynth/utau_logger.h"
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#else
    #include <sys/sysinfo.h>
    #include <unistd.h>
#endif

namespace nexussynth {
namespace conditioning {

    // =============================================================================
    // ThreadPool Implementation
    // =============================================================================

    ThreadPool::ThreadPool(size_t num_threads) 
        : num_threads_(num_threads == 0 ? std::thread::hardware_concurrency() : num_threads)
        , running_(false)
        , active_jobs_(0) {
        if (num_threads_ == 0) {
            num_threads_ = 4; // Fallback if hardware_concurrency fails
        }
        LOG_INFO("ThreadPool created with " + std::to_string(num_threads_) + " threads");
    }

    ThreadPool::~ThreadPool() {
        stop();
    }

    void ThreadPool::start() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (running_.load()) {
            return; // Already running
        }
        
        running_.store(true);
        workers_.reserve(num_threads_);
        
        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back(&ThreadPool::worker_thread, this);
        }
        
        LOG_INFO("ThreadPool started with " + std::to_string(num_threads_) + " worker threads");
    }

    void ThreadPool::stop() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            running_.store(false);
        }
        condition_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        
        // Clear remaining jobs
        std::queue<std::function<void()>> empty;
        job_queue_.swap(empty);
        
        LOG_INFO("ThreadPool stopped");
    }

    void ThreadPool::resize(size_t new_size) {
        if (new_size == num_threads_) {
            return;
        }
        
        stop();
        num_threads_ = new_size;
        if (running_.load()) {
            start();
        }
        
        LOG_INFO("ThreadPool resized to " + std::to_string(new_size) + " threads");
    }

    template<typename F, typename... Args>
    auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!running_.load()) {
                throw std::runtime_error("ThreadPool is not running");
            }
            
            job_queue_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        
        return result;
    }

    void ThreadPool::worker_thread() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] { 
                    return !running_.load() || !job_queue_.empty(); 
                });
                
                if (!running_.load() && job_queue_.empty()) {
                    break;
                }
                
                if (!job_queue_.empty()) {
                    task = std::move(job_queue_.front());
                    job_queue_.pop();
                }
            }
            
            if (task) {
                active_jobs_.fetch_add(1);
                try {
                    task();
                } catch (const std::exception& e) {
                    LOG_ERROR("Worker thread exception: " + std::string(e.what()));
                } catch (...) {
                    LOG_ERROR("Worker thread unknown exception");
                }
                active_jobs_.fetch_sub(1);
            }
        }
    }

    // =============================================================================
    // ResourceMonitor Implementation
    // =============================================================================

    ResourceMonitor::ResourceMonitor() 
        : monitoring_active_(false)
        , current_memory_mb_(0.0)
        , peak_memory_mb_(0.0)
        , memory_limit_mb_(0.0) {
    }

    ResourceMonitor::~ResourceMonitor() {
        stop_monitoring();
    }

    double ResourceMonitor::get_current_memory_usage_mb() const {
        return current_memory_mb_.load();
    }

    double ResourceMonitor::get_peak_memory_usage_mb() const {
        return peak_memory_mb_.load();
    }

    void ResourceMonitor::reset_peak_memory() {
        peak_memory_mb_.store(current_memory_mb_.load());
    }

    double ResourceMonitor::get_cpu_usage_percent() const {
        // Platform-specific CPU usage calculation would go here
        // For now, return a placeholder
        return 0.0;
    }

    int ResourceMonitor::get_cpu_core_count() const {
        return static_cast<int>(std::thread::hardware_concurrency());
    }

    double ResourceMonitor::get_available_memory_mb() const {
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        return static_cast<double>(memInfo.ullAvailPhys) / (1024.0 * 1024.0);
#elif defined(__APPLE__)
        vm_size_t page_size;
        vm_statistics64_data_t vm_stat;
        mach_port_t mach_port = mach_host_self();
        mach_msg_type_number_t count = sizeof(vm_stat) / sizeof(natural_t);
        
        if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
            return static_cast<double>(vm_stat.free_count * page_size) / (1024.0 * 1024.0);
        }
        return 1024.0; // Fallback
#else
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) == 0) {
            return static_cast<double>(sys_info.freeram * sys_info.mem_unit) / (1024.0 * 1024.0);
        }
        return 1024.0; // Fallback
#endif
    }

    double ResourceMonitor::get_disk_space_mb(const std::string& path) const {
        try {
            auto space_info = std::filesystem::space(path);
            return static_cast<double>(space_info.available) / (1024.0 * 1024.0);
        } catch (const std::filesystem::filesystem_error&) {
            return 0.0;
        }
    }

    void ResourceMonitor::start_monitoring() {
        if (monitoring_active_.load()) {
            return;
        }
        
        monitoring_active_.store(true);
        monitor_thread_ = std::thread(&ResourceMonitor::monitor_loop, this);
        
        LOG_DEBUG("Resource monitoring started");
    }

    void ResourceMonitor::stop_monitoring() {
        monitoring_active_.store(false);
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
        
        LOG_DEBUG("Resource monitoring stopped");
    }

    void ResourceMonitor::set_memory_limit_mb(double limit) {
        memory_limit_mb_.store(limit);
    }

    bool ResourceMonitor::is_memory_limit_exceeded() const {
        double limit = memory_limit_mb_.load();
        return limit > 0.0 && current_memory_mb_.load() > limit;
    }

    void ResourceMonitor::monitor_loop() {
        while (monitoring_active_.load()) {
            double current_memory = calculate_memory_usage();
            current_memory_mb_.store(current_memory);
            
            double peak = peak_memory_mb_.load();
            if (current_memory > peak) {
                peak_memory_mb_.store(current_memory);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    double ResourceMonitor::calculate_memory_usage() const {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        }
        return 0.0;
#elif defined(__APPLE__)
        struct mach_task_basic_info info;
        mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, 
                     (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
            return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
        }
        return 0.0;
#else
        // Linux: Read from /proc/self/status
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line.substr(7));
                size_t memory_kb;
                if (iss >> memory_kb) {
                    return static_cast<double>(memory_kb) / 1024.0;
                }
            }
        }
        return 0.0;
#endif
    }

    // =============================================================================
    // BatchProcessor Implementation
    // =============================================================================

    BatchProcessor::BatchProcessor(const BatchProcessingConfig& config) 
        : config_(config)
        , thread_pool_(std::make_unique<ThreadPool>(config.num_worker_threads))
        , resource_monitor_(std::make_unique<ResourceMonitor>())
        , state_(ProcessingState::IDLE) {
        
        resource_monitor_->set_memory_limit_mb(static_cast<double>(config_.max_memory_usage_mb));
        
        LOG_INFO("BatchProcessor created with " + std::to_string(config.num_worker_threads) + " threads");
    }

    BatchProcessor::~BatchProcessor() {
        cancel_batch();
    }

    void BatchProcessor::set_config(const BatchProcessingConfig& config) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        config_ = config;
        
        if (thread_pool_) {
            thread_pool_->resize(config.num_worker_threads);
        }
        
        if (resource_monitor_) {
            resource_monitor_->set_memory_limit_mb(static_cast<double>(config.max_memory_usage_mb));
        }
    }

    void BatchProcessor::set_progress_callback(std::shared_ptr<BatchProgressCallback> callback) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        progress_callback_ = callback;
    }

    void BatchProcessor::remove_progress_callback() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        progress_callback_.reset();
    }

    std::string BatchProcessor::add_job(const std::string& input_path, const std::string& output_path, 
                                       const ConditioningConfig& conditioning_config) {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        
        // Generate unique job ID
        std::string job_id = "job_" + std::to_string(jobs_.size()) + "_" + 
                            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Extract voice bank name from path
        std::string voice_bank_name = std::filesystem::path(input_path).filename().string();
        
        BatchJob job(job_id, input_path, output_path, conditioning_config);
        job.voice_bank_name = voice_bank_name;
        
        jobs_.push_back(std::move(job));
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_jobs++;
            stats_.queued_jobs++;
        }
        
        LOG_INFO("Added batch job: " + job_id + " (" + voice_bank_name + ")");
        return job_id;
    }

    bool BatchProcessor::remove_job(const std::string& job_id) {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        
        auto it = std::find_if(jobs_.begin(), jobs_.end(),
            [&job_id](const BatchJob& job) { return job.id == job_id; });
        
        if (it != jobs_.end()) {
            jobs_.erase(it);
            
            // Update statistics
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.total_jobs--;
                if (stats_.queued_jobs > 0) {
                    stats_.queued_jobs--;
                }
            }
            
            LOG_INFO("Removed batch job: " + job_id);
            return true;
        }
        
        return false;
    }

    void BatchProcessor::clear_jobs() {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        jobs_.clear();
        
        // Clear job queue
        std::queue<size_t> empty;
        job_queue_.swap(empty);
        
        // Reset statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_ = BatchProcessingStats();
        }
        
        LOG_INFO("Cleared all batch jobs");
    }

    bool BatchProcessor::start_batch() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_ != ProcessingState::IDLE) {
            LOG_WARN("Cannot start batch: processor is not idle");
            return false;
        }
        
        if (jobs_.empty()) {
            LOG_WARN("Cannot start batch: no jobs queued");
            return false;
        }
        
        // Initialize processing
        state_ = ProcessingState::RUNNING;
        
        // Reset statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.batch_start_time = std::chrono::system_clock::now();
            stats_.active_jobs = 0;
            stats_.completed_jobs = 0;
            stats_.failed_jobs = 0;
        }
        
        // Start thread pool and resource monitoring
        thread_pool_->start();
        resource_monitor_->start_monitoring();
        
        // Queue all jobs
        {
            std::lock_guard<std::mutex> jobs_lock(jobs_mutex_);
            for (size_t i = 0; i < jobs_.size(); ++i) {
                job_queue_.push(i);
            }
        }
        
        // Notify callback
        if (progress_callback_) {
            progress_callback_->on_batch_started(jobs_.size());
        }
        
        // Submit initial jobs to thread pool
        size_t concurrent_jobs = std::min(static_cast<size_t>(config_.batch_size), jobs_.size());
        for (size_t i = 0; i < concurrent_jobs; ++i) {
            submit_next_job();
        }
        
        LOG_INFO("Batch processing started with " + std::to_string(jobs_.size()) + " jobs");
        return true;
    }

    void BatchProcessor::pause_batch() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_ == ProcessingState::RUNNING) {
            state_ = ProcessingState::PAUSED;
            
            if (progress_callback_) {
                progress_callback_->on_batch_paused();
            }
            
            LOG_INFO("Batch processing paused");
        }
    }

    void BatchProcessor::resume_batch() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_ == ProcessingState::PAUSED) {
            state_ = ProcessingState::RUNNING;
            
            if (progress_callback_) {
                progress_callback_->on_batch_resumed();
            }
            
            LOG_INFO("Batch processing resumed");
        }
    }

    void BatchProcessor::cancel_batch() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = ProcessingState::CANCELLING;
        }
        
        // Stop thread pool and resource monitoring
        thread_pool_->stop();
        resource_monitor_->stop_monitoring();
        
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = ProcessingState::IDLE;
        }
        
        if (progress_callback_) {
            progress_callback_->on_batch_cancelled();
        }
        
        LOG_INFO("Batch processing cancelled");
    }

    BatchProcessingStats BatchProcessor::get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        BatchProcessingStats current_stats = stats_;
        current_stats.current_memory_usage_mb = resource_monitor_->get_current_memory_usage_mb();
        current_stats.peak_memory_usage_mb = resource_monitor_->get_peak_memory_usage_mb();
        current_stats.active_threads = static_cast<int>(thread_pool_->active_jobs());
        
        return current_stats;
    }

    std::vector<BatchJob> BatchProcessor::get_jobs() const {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        return jobs_;
    }

    std::vector<JobResult> BatchProcessor::get_results() const {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        return results_;
    }

    std::vector<std::string> BatchProcessor::get_error_log() const {
        std::lock_guard<std::mutex> lock(error_mutex_);
        return error_log_;
    }

    void BatchProcessor::clear_error_log() {
        std::lock_guard<std::mutex> lock(error_mutex_);
        error_log_.clear();
    }

    void BatchProcessor::set_memory_limit_mb(double limit) {
        config_.max_memory_usage_mb = static_cast<size_t>(limit);
        resource_monitor_->set_memory_limit_mb(limit);
    }

    void BatchProcessor::set_thread_count(size_t count) {
        config_.num_worker_threads = static_cast<int>(count);
        thread_pool_->resize(count);
    }

    bool BatchProcessor::check_system_resources() const {
        double available_memory = resource_monitor_->get_available_memory_mb();
        double required_memory = static_cast<double>(config_.max_memory_usage_mb);
        
        return available_memory >= required_memory;
    }

    void BatchProcessor::submit_next_job() {
        size_t job_index;
        bool has_job = false;
        
        {
            std::lock_guard<std::mutex> jobs_lock(jobs_mutex_);
            if (!job_queue_.empty()) {
                job_index = job_queue_.front();
                job_queue_.pop();
                has_job = true;
            }
        }
        
        if (has_job) {
            // Submit job to thread pool
            thread_pool_->submit([this, job_index]() {
                this->process_job(job_index);
            });
        }
    }

    void BatchProcessor::process_job(size_t job_index) {
        BatchJob* job = nullptr;
        
        {
            std::lock_guard<std::mutex> jobs_lock(jobs_mutex_);
            if (job_index < jobs_.size()) {
                job = &jobs_[job_index];
                job->started_time = std::chrono::system_clock::now();
            }
        }
        
        if (!job) {
            LOG_ERROR("Invalid job index: " + std::to_string(job_index));
            return;
        }
        
        // Wait if paused
        while (state_.load() == ProcessingState::PAUSED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Check if cancelled
        if (state_.load() == ProcessingState::CANCELLING) {
            return;
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.active_jobs++;
            stats_.queued_jobs--;
        }
        
        // Notify callback
        if (progress_callback_) {
            progress_callback_->on_job_started(*job);
        }
        
        LOG_INFO("Processing job: " + job->id + " (" + job->voice_bank_name + ")");
        
        // Process the voice bank
        JobResult result = process_voice_bank(*job);
        
        // Update job completion time
        job->completed_time = std::chrono::system_clock::now();
        
        // Store result
        {
            std::lock_guard<std::mutex> jobs_lock(jobs_mutex_);
            results_.push_back(result);
        }
        
        // Update statistics
        update_statistics(result);
        
        // Notify callback
        if (progress_callback_) {
            if (result.success) {
                progress_callback_->on_job_completed(*job, result);
            } else {
                progress_callback_->on_job_failed(*job, result.error_message);
            }
        }
        
        // Check if we should submit another job
        if (state_.load() == ProcessingState::RUNNING) {
            submit_next_job();
        }
        
        // Check if batch is complete
        bool batch_complete = false;
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.active_jobs--;
            batch_complete = (stats_.completed_jobs + stats_.failed_jobs) >= stats_.total_jobs && 
                           stats_.active_jobs == 0;
        }
        
        if (batch_complete) {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            state_ = ProcessingState::COMPLETED;
            
            if (progress_callback_) {
                progress_callback_->on_batch_completed(get_stats());
            }
            
            LOG_INFO("Batch processing completed");
        }
    }

    JobResult BatchProcessor::process_voice_bank(const BatchJob& job) {
        JobResult result(job.id);
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // TODO: Implement actual voice bank processing
            // For now, simulate processing
            std::this_thread::sleep_for(std::chrono::milliseconds(100 + (rand() % 500)));
            
            // Simulate success/failure based on random chance (90% success rate)
            result.success = (rand() % 100) < 90;
            
            if (result.success) {
                result.input_files_processed = 10 + (rand() % 50);
                result.output_file_size_bytes = 1024 * 1024 + (rand() % (5 * 1024 * 1024));
                result.compression_ratio = 0.3 + (rand() % 40) / 100.0;
                result.estimated_quality_score = 0.7 + (rand() % 30) / 100.0;
            } else {
                result.error_message = "Simulated processing error";
            }
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = "Exception during processing: " + std::string(e.what());
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.processing_time = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end_time - start_time);
        
        return result;
    }

    void BatchProcessor::update_statistics(const JobResult& result) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (result.success) {
            stats_.completed_jobs++;
            stats_.total_input_files += result.input_files_processed;
            stats_.total_output_size_bytes += result.output_file_size_bytes;
            
            // Update average processing time
            double total_time = stats_.total_processing_time_ms + result.processing_time.count();
            stats_.average_processing_time_ms = total_time / stats_.completed_jobs;
            stats_.total_processing_time_ms = total_time;
        } else {
            stats_.failed_jobs++;
            log_error("Job failed: " + result.job_id + " - " + result.error_message);
        }
        
        estimate_completion_time();
        report_batch_progress();
    }

    void BatchProcessor::estimate_completion_time() {
        if (stats_.completed_jobs == 0) {
            return;
        }
        
        size_t remaining_jobs = stats_.total_jobs - stats_.completed_jobs - stats_.failed_jobs;
        if (remaining_jobs == 0) {
            return;
        }
        
        double estimated_remaining_ms = remaining_jobs * stats_.average_processing_time_ms;
        auto estimated_completion = std::chrono::system_clock::now() + 
                                  std::chrono::milliseconds(static_cast<long long>(estimated_remaining_ms));
        
        stats_.estimated_completion_time = estimated_completion;
        
        if (progress_callback_) {
            progress_callback_->on_eta_updated(estimated_completion);
        }
    }

    void BatchProcessor::report_batch_progress() {
        if (progress_callback_) {
            progress_callback_->on_batch_progress(stats_);
        }
    }

    void BatchProcessor::log_error(const std::string& error) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        error_log_.push_back(error);
        LOG_ERROR(error);
    }

    // =============================================================================
    // ConsoleBatchProgressCallback Implementation
    // =============================================================================

    ConsoleBatchProgressCallback::ConsoleBatchProgressCallback() 
        : last_update_time_(std::chrono::steady_clock::now()) {
    }

    void ConsoleBatchProgressCallback::on_batch_started(size_t total_jobs) {
        std::cout << "\n🚀 Starting batch processing of " << total_jobs << " voice banks...\n" << std::endl;
    }

    void ConsoleBatchProgressCallback::on_batch_completed(const BatchProcessingStats& stats) {
        std::cout << "\n✅ Batch processing completed!\n";
        std::cout << "   Total jobs: " << stats.total_jobs << "\n";
        std::cout << "   Completed: " << stats.completed_jobs << "\n";
        std::cout << "   Failed: " << stats.failed_jobs << "\n";
        std::cout << "   Success rate: " << std::fixed << std::setprecision(1) 
                  << (100.0 * stats.completed_jobs / stats.total_jobs) << "%\n";
        std::cout << "   Total output size: " << format_file_size(stats.total_output_size_bytes) << "\n";
        std::cout << "   Average processing time: " << std::fixed << std::setprecision(1)
                  << stats.average_processing_time_ms << "ms\n" << std::endl;
    }

    void ConsoleBatchProgressCallback::on_batch_progress(const BatchProcessingStats& stats) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time_).count() < 500) {
            return; // Limit update frequency
        }
        last_update_time_ = now;
        
        size_t completed = stats.completed_jobs + stats.failed_jobs;
        print_progress_bar(completed, stats.total_jobs);
        
        std::cout << " [" << completed << "/" << stats.total_jobs << "] ";
        std::cout << "Active: " << stats.active_jobs << " ";
        std::cout << "Memory: " << std::fixed << std::setprecision(1) << stats.current_memory_usage_mb << "MB";
        
        if (stats.average_processing_time_ms > 0) {
            size_t remaining = stats.total_jobs - completed;
            auto eta_duration = std::chrono::milliseconds(
                static_cast<long long>(remaining * stats.average_processing_time_ms));
            std::cout << " ETA: " << format_duration(eta_duration);
        }
        
        std::cout << "\r" << std::flush;
    }

    void ConsoleBatchProgressCallback::on_job_started(const BatchJob& job) {
        // Could add verbose job-level logging here
    }

    void ConsoleBatchProgressCallback::on_job_completed(const BatchJob& job, const JobResult& result) {
        // Could add verbose completion logging here
    }

    void ConsoleBatchProgressCallback::on_job_failed(const BatchJob& job, const std::string& error) {
        std::cout << "\n❌ Job failed: " << job.voice_bank_name << " - " << error << std::endl;
    }

    void ConsoleBatchProgressCallback::on_eta_updated(const std::chrono::system_clock::time_point& estimated_completion) {
        // ETA updates are handled in on_batch_progress
    }

    void ConsoleBatchProgressCallback::print_progress_bar(size_t current, size_t total, int width) {
        if (total == 0) return;
        
        float progress = static_cast<float>(current) / static_cast<float>(total);
        int pos = static_cast<int>(width * progress);
        
        std::cout << "[";
        for (int i = 0; i < width; ++i) {
            if (i < pos) std::cout << "█";
            else if (i == pos) std::cout << "▏";
            else std::cout << " ";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0f) << "%";
    }

    std::string ConsoleBatchProgressCallback::format_duration(std::chrono::duration<double> duration) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(seconds);
        auto hours = std::chrono::duration_cast<std::chrono::hours>(minutes);
        
        seconds -= minutes;
        minutes -= hours;
        
        std::ostringstream oss;
        if (hours.count() > 0) {
            oss << hours.count() << "h";
        }
        if (minutes.count() > 0) {
            oss << minutes.count() << "m";
        }
        oss << seconds.count() << "s";
        
        return oss.str();
    }

    std::string ConsoleBatchProgressCallback::format_file_size(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unit_index < 4) {
            size /= 1024.0;
            unit_index++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
        return oss.str();
    }

} // namespace conditioning
} // namespace nexussynth
#include "nexussynth/performance_profiler.h"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>

#ifdef __linux__
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#elif _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#endif

namespace nexussynth {
namespace synthesis {

PerformanceProfiler::PerformanceProfiler(const ProfilingConfig& config)
    : config_(config) {
    current_metrics_.session_start_time = std::chrono::steady_clock::now();
    peak_metrics_.session_start_time = current_metrics_.session_start_time;
    accumulated_metrics_.session_start_time = current_metrics_.session_start_time;
    last_quality_measurement_ = current_metrics_.session_start_time;
    last_alert_check_ = current_metrics_.session_start_time;
}

PerformanceProfiler::~PerformanceProfiler() {
    stop_profiling();
}

bool PerformanceProfiler::start_profiling() {
    if (profiling_active_.load()) {
        return true; // Already running
    }
    
    // Reset statistics
    reset_statistics();
    
    // Start monitoring thread
    shutdown_requested_.store(false);
    try {
        monitoring_thread_ = std::make_unique<std::thread>(
            &PerformanceProfiler::monitoring_thread_main, this
        );
        profiling_active_.store(true);
        
        std::cout << "Performance profiling started" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start profiling thread: " << e.what() << std::endl;
        return false;
    }
}

void PerformanceProfiler::stop_profiling() {
    if (!profiling_active_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    profiling_active_.store(false);
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
        monitoring_thread_.reset();
    }
    
    std::cout << "Performance profiling stopped" << std::endl;
    
    // Print final summary
    auto final_metrics = get_current_metrics();
    std::cout << "Final Performance Summary:" << std::endl;
    std::cout << "  Total frames processed: " << final_metrics.total_frames_processed << std::endl;
    std::cout << "  Average FPS: " << std::fixed << std::setprecision(2) 
              << final_metrics.processing_fps << std::endl;
    std::cout << "  Real-time factor: " << std::fixed << std::setprecision(3) 
              << final_metrics.real_time_factor << std::endl;
    std::cout << "  Peak CPU usage: " << std::fixed << std::setprecision(1) 
              << final_metrics.peak_cpu_usage_percent << "%" << std::endl;
}

void PerformanceProfiler::begin_frame_measurement() {
    frame_start_time_ = std::chrono::high_resolution_clock::now();
    frame_timing_active_ = true;
}

void PerformanceProfiler::end_frame_measurement() {
    if (!frame_timing_active_) {
        return;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double frame_time_ms = std::chrono::duration<double, std::milli>(
        end_time - frame_start_time_
    ).count();
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Update current metrics
    current_metrics_.average_frame_time_ms = frame_time_ms;
    current_metrics_.peak_frame_time_ms = std::max(
        current_metrics_.peak_frame_time_ms, frame_time_ms
    );
    current_metrics_.total_processing_time_ms += frame_time_ms;
    
    // Update peak metrics
    peak_metrics_.peak_frame_time_ms = std::max(
        peak_metrics_.peak_frame_time_ms, frame_time_ms
    );
    
    // Add to history
    add_to_history(frame_time_history_, frame_time_ms);
    
    frame_timing_active_ = false;
}

void PerformanceProfiler::record_frame_processed(size_t frame_count) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    current_metrics_.total_frames_processed += frame_count;
    accumulated_metrics_.total_frames_processed += frame_count;
}

void PerformanceProfiler::record_frame_dropped(size_t drop_count) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    current_metrics_.frames_dropped += drop_count;
    accumulated_metrics_.frames_dropped += drop_count;
}

void PerformanceProfiler::record_quality_metrics(const NexusSynth::QualityMetrics& quality_metrics) {
    if (!config_.enable_quality_tracking) {
        return;
    }
    
    // Calculate composite quality score
    double quality_score = 0.0;
    if (quality_metrics.is_valid()) {
        // Normalize MCD (lower is better, typical range 2-10)
        double mcd_score = std::max(0.0, 1.0 - (quality_metrics.mcd_score / 10.0));
        
        // Normalize F0 RMSE (lower is better, typical range 5-50 Hz)
        double f0_score = std::max(0.0, 1.0 - (quality_metrics.f0_rmse / 50.0));
        
        // Spectral correlation (higher is better, range 0-1)
        double spectral_score = std::max(0.0, quality_metrics.spectral_correlation);
        
        // Weighted composite score
        quality_score = 0.4 * mcd_score + 0.3 * f0_score + 0.3 * spectral_score;
    }
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    current_metrics_.synthesis_quality_score = quality_score;
    peak_metrics_.synthesis_quality_score = std::max(
        peak_metrics_.synthesis_quality_score, quality_score
    );
    
    add_to_history(quality_score_history_, quality_score);
    last_quality_measurement_ = std::chrono::steady_clock::now();
}

void PerformanceProfiler::set_reference_audio(const std::vector<double>& reference_audio, double sample_rate) {
    reference_audio_ = reference_audio;
    reference_sample_rate_ = sample_rate;
}

void PerformanceProfiler::update_buffer_statistics(double input_utilization, double output_utilization,
                                                  size_t underruns, size_t overflows) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    current_metrics_.input_buffer_utilization = input_utilization;
    current_metrics_.output_buffer_utilization = output_utilization;
    current_metrics_.buffer_underruns += underruns;
    current_metrics_.buffer_overflows += overflows;
    
    // Update accumulated metrics
    accumulated_metrics_.buffer_underruns += underruns;
    accumulated_metrics_.buffer_overflows += overflows;
}

void PerformanceProfiler::record_synthesis_latency(double latency_ms) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    current_metrics_.latency_ms = latency_ms;
}

PerformanceMetrics PerformanceProfiler::get_current_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return current_metrics_;
}

PerformanceMetrics PerformanceProfiler::get_average_metrics() const {
    return calculate_average_metrics();
}

PerformanceMetrics PerformanceProfiler::get_peak_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return peak_metrics_;
}

PerformanceReport PerformanceProfiler::generate_report() const {
    PerformanceReport report;
    
    report.current_metrics = get_current_metrics();
    report.average_metrics = get_average_metrics();
    report.peak_metrics = get_peak_metrics();
    
    // Copy historical data
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        
        std::queue<double> temp_frame_history = frame_time_history_;
        while (!temp_frame_history.empty()) {
            report.frame_time_history.push_back(temp_frame_history.front());
            temp_frame_history.pop();
        }
        
        std::queue<double> temp_cpu_history = cpu_usage_history_;
        while (!temp_cpu_history.empty()) {
            report.cpu_usage_history.push_back(temp_cpu_history.front());
            temp_cpu_history.pop();
        }
        
        std::queue<double> temp_quality_history = quality_score_history_;
        while (!temp_quality_history.empty()) {
            report.quality_score_history.push_back(temp_quality_history.front());
            temp_quality_history.pop();
        }
    }
    
    // Generate timestamp
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    report.report_timestamp = oss.str();
    
    // System information
    report.system_info = get_system_info();
    
    // Analysis
    auto bottlenecks = analyze_bottlenecks();
    for (const auto& bottleneck : bottlenecks) {
        report.bottleneck_analysis[bottleneck] = 1.0; // Simplified scoring
    }
    
    report.performance_alerts = get_performance_alerts();
    report.optimization_suggestions = get_optimization_suggestions();
    
    return report;
}

std::vector<std::string> PerformanceProfiler::get_performance_alerts() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return active_alerts_;
}

std::vector<std::string> PerformanceProfiler::get_optimization_suggestions() const {
    return generate_optimization_suggestions();
}

bool PerformanceProfiler::update_config(const ProfilingConfig& new_config) {
    if (profiling_active_.load()) {
        std::cerr << "Cannot update config while profiling is active" << std::endl;
        return false;
    }
    
    config_ = new_config;
    return true;
}

void PerformanceProfiler::reset_statistics() {
    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    std::lock_guard<std::mutex> history_lock(history_mutex_);
    
    auto start_time = std::chrono::steady_clock::now();
    
    current_metrics_ = PerformanceMetrics{};
    current_metrics_.session_start_time = start_time;
    
    peak_metrics_ = PerformanceMetrics{};
    peak_metrics_.session_start_time = start_time;
    
    accumulated_metrics_ = PerformanceMetrics{};
    accumulated_metrics_.session_start_time = start_time;
    
    // Clear history
    while (!frame_time_history_.empty()) frame_time_history_.pop();
    while (!cpu_usage_history_.empty()) cpu_usage_history_.pop();
    while (!quality_score_history_.empty()) quality_score_history_.pop();
    while (!metrics_history_.empty()) metrics_history_.pop();
    
    active_alerts_.clear();
    
    last_quality_measurement_ = start_time;
    last_alert_check_ = start_time;
}

void PerformanceProfiler::monitoring_thread_main() {
    std::cout << "Performance monitoring thread started" << std::endl;
    
    auto last_update = std::chrono::steady_clock::now();
    
    while (!shutdown_requested_.load()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double, std::milli>(now - last_update).count();
        
        if (elapsed >= config_.sampling_interval_ms) {
            update_system_metrics();
            update_derived_metrics();
            check_performance_alerts();
            
            // Add current metrics to history
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                add_to_metrics_history(current_metrics_);
            }
            
            last_update = now;
        }
        
        // Sleep for a short interval to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Performance monitoring thread finished" << std::endl;
}

void PerformanceProfiler::update_system_metrics() {
    if (config_.enable_cpu_monitoring) {
        double cpu_usage = get_cpu_usage();
        
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_metrics_.cpu_usage_percent = cpu_usage;
        peak_metrics_.peak_cpu_usage_percent = std::max(
            peak_metrics_.peak_cpu_usage_percent, cpu_usage
        );
        
        add_to_history(cpu_usage_history_, cpu_usage);
    }
    
    if (config_.enable_memory_monitoring) {
        size_t memory_usage = get_memory_usage_mb();
        
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_metrics_.memory_usage_mb = memory_usage;
        peak_metrics_.peak_memory_mb = std::max(
            peak_metrics_.peak_memory_mb, memory_usage
        );
    }
}

void PerformanceProfiler::update_derived_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Calculate FPS
    double session_duration = current_metrics_.get_session_duration_seconds();
    if (session_duration > 0.0 && current_metrics_.total_frames_processed > 0) {
        current_metrics_.processing_fps = 
            current_metrics_.total_frames_processed / session_duration;
    }
    
    // Calculate real-time factor
    if (current_metrics_.total_processing_time_ms > 0.0 && session_duration > 0.0) {
        double processing_time_s = current_metrics_.total_processing_time_ms / 1000.0;
        current_metrics_.real_time_factor = processing_time_s / session_duration;
    }
    
    // Update average frame time from history
    if (!frame_time_history_.empty()) {
        current_metrics_.average_frame_time_ms = calculate_average(frame_time_history_);
    }
}

void PerformanceProfiler::check_performance_alerts() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_alert_check_).count();
    
    if (elapsed < 1.0) { // Check alerts only once per second
        return;
    }
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    active_alerts_.clear();
    
    // CPU usage alert
    if (current_metrics_.cpu_usage_percent > config_.cpu_usage_alert_threshold) {
        active_alerts_.push_back(
            "HIGH_CPU_USAGE: " + std::to_string(current_metrics_.cpu_usage_percent) + "%"
        );
    }
    
    // Latency alert
    if (current_metrics_.latency_ms > config_.latency_alert_threshold_ms) {
        active_alerts_.push_back(
            "HIGH_LATENCY: " + std::to_string(current_metrics_.latency_ms) + "ms"
        );
    }
    
    // Buffer utilization alerts
    if (current_metrics_.input_buffer_utilization > config_.buffer_utilization_alert_threshold) {
        active_alerts_.push_back(
            "INPUT_BUFFER_HIGH: " + std::to_string(current_metrics_.input_buffer_utilization * 100) + "%"
        );
    }
    
    if (current_metrics_.output_buffer_utilization > config_.buffer_utilization_alert_threshold) {
        active_alerts_.push_back(
            "OUTPUT_BUFFER_HIGH: " + std::to_string(current_metrics_.output_buffer_utilization * 100) + "%"
        );
    }
    
    // Buffer underrun/overflow alerts
    if (current_metrics_.buffer_underruns > 0) {
        active_alerts_.push_back(
            "BUFFER_UNDERRUNS: " + std::to_string(current_metrics_.buffer_underruns)
        );
    }
    
    if (current_metrics_.buffer_overflows > 0) {
        active_alerts_.push_back(
            "BUFFER_OVERFLOWS: " + std::to_string(current_metrics_.buffer_overflows)
        );
    }
    
    last_alert_check_ = now;
}

std::vector<std::string> PerformanceProfiler::analyze_bottlenecks() const {
    std::vector<std::string> bottlenecks;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // High CPU usage
    if (current_metrics_.cpu_usage_percent > 70.0) {
        bottlenecks.push_back("CPU_INTENSIVE_PROCESSING");
    }
    
    // High real-time factor
    if (current_metrics_.real_time_factor > 0.8) {
        bottlenecks.push_back("PROCESSING_TOO_SLOW");
    }
    
    // High buffer utilization
    if (current_metrics_.input_buffer_utilization > 0.8 || 
        current_metrics_.output_buffer_utilization > 0.8) {
        bottlenecks.push_back("BUFFER_SATURATION");
    }
    
    // High latency
    if (current_metrics_.latency_ms > 30.0) {
        bottlenecks.push_back("HIGH_SYNTHESIS_LATENCY");
    }
    
    // Many dropped frames
    if (current_metrics_.total_frames_processed > 0) {
        double drop_rate = static_cast<double>(current_metrics_.frames_dropped) / 
                          current_metrics_.total_frames_processed;
        if (drop_rate > 0.05) { // 5% drop rate
            bottlenecks.push_back("HIGH_FRAME_DROP_RATE");
        }
    }
    
    return bottlenecks;
}

std::vector<std::string> PerformanceProfiler::generate_optimization_suggestions() const {
    std::vector<std::string> suggestions;
    
    auto bottlenecks = analyze_bottlenecks();
    
    for (const auto& bottleneck : bottlenecks) {
        if (bottleneck == "CPU_INTENSIVE_PROCESSING") {
            suggestions.push_back("Consider using multithreading or SIMD optimizations");
            suggestions.push_back("Reduce FFT size if quality permits");
        } else if (bottleneck == "PROCESSING_TOO_SLOW") {
            suggestions.push_back("Optimize synthesis algorithm complexity");
            suggestions.push_back("Use faster FFT implementation (FFTW, Intel MKL)");
        } else if (bottleneck == "BUFFER_SATURATION") {
            suggestions.push_back("Increase buffer sizes");
            suggestions.push_back("Enable adaptive buffering");
        } else if (bottleneck == "HIGH_SYNTHESIS_LATENCY") {
            suggestions.push_back("Reduce buffer sizes");
            suggestions.push_back("Optimize processing pipeline");
        } else if (bottleneck == "HIGH_FRAME_DROP_RATE") {
            suggestions.push_back("Increase thread priority");
            suggestions.push_back("Reduce concurrent system load");
        }
    }
    
    return suggestions;
}

double PerformanceProfiler::get_cpu_usage() const {
#ifdef __linux__
    static long long last_total_time = 0;
    static long long last_idle_time = 0;
    
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return 0.0;
    }
    
    std::string line;
    std::getline(file, line);
    file.close();
    
    std::istringstream ss(line);
    std::string cpu_label;
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    
    ss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    
    long long total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    long long idle_time = idle + iowait;
    
    if (last_total_time == 0) {
        last_total_time = total_time;
        last_idle_time = idle_time;
        return 0.0;
    }
    
    long long total_diff = total_time - last_total_time;
    long long idle_diff = idle_time - last_idle_time;
    
    last_total_time = total_time;
    last_idle_time = idle_time;
    
    if (total_diff == 0) {
        return 0.0;
    }
    
    return 100.0 * (1.0 - static_cast<double>(idle_diff) / total_diff);
#elif _WIN32
    // Windows implementation would go here
    return 0.0;
#else
    return 0.0;
#endif
}

size_t PerformanceProfiler::get_memory_usage_mb() const {
#ifdef __linux__
    std::ifstream file("/proc/self/status");
    if (!file.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream ss(line);
            std::string label;
            size_t memory_kb;
            ss >> label >> memory_kb;
            return memory_kb / 1024; // Convert to MB
        }
    }
    file.close();
    return 0;
#elif _WIN32
    // Windows implementation would go here
    return 0;
#else
    return 0;
#endif
}

std::string PerformanceProfiler::get_system_info() const {
    std::ostringstream info;
    
#ifdef __linux__
    // Get CPU info
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.substr(0, 10) == "model name") {
                info << "CPU: " << line.substr(line.find(':') + 2) << std::endl;
                break;
            }
        }
        cpuinfo.close();
    }
    
    // Get memory info
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.substr(0, 8) == "MemTotal") {
                std::istringstream ss(line);
                std::string label;
                size_t memory_kb;
                ss >> label >> memory_kb;
                info << "Memory: " << (memory_kb / 1024 / 1024) << " GB" << std::endl;
                break;
            }
        }
        meminfo.close();
    }
#endif
    
    info << "Profiling Config:" << std::endl;
    info << "  Sampling interval: " << config_.sampling_interval_ms << "ms" << std::endl;
    info << "  History buffer size: " << config_.history_buffer_size << std::endl;
    
    return info.str();
}

void PerformanceProfiler::add_to_history(std::queue<double>& history, double value) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    history.push(value);
    while (history.size() > config_.history_buffer_size) {
        history.pop();
    }
}

void PerformanceProfiler::add_to_metrics_history(const PerformanceMetrics& metrics) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    metrics_history_.push(metrics);
    while (metrics_history_.size() > config_.history_buffer_size) {
        metrics_history_.pop();
    }
}

double PerformanceProfiler::calculate_average(const std::queue<double>& history) const {
    if (history.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    std::queue<double> temp = history;
    while (!temp.empty()) {
        sum += temp.front();
        temp.pop();
    }
    
    return sum / history.size();
}

double PerformanceProfiler::calculate_peak(const std::queue<double>& history) const {
    if (history.empty()) {
        return 0.0;
    }
    
    double peak = -std::numeric_limits<double>::infinity();
    std::queue<double> temp = history;
    while (!temp.empty()) {
        peak = std::max(peak, temp.front());
        temp.pop();
    }
    
    return peak;
}

PerformanceMetrics PerformanceProfiler::calculate_average_metrics() const {
    std::lock_guard<std::mutex> history_lock(history_mutex_);
    
    if (metrics_history_.empty()) {
        return PerformanceMetrics{};
    }
    
    PerformanceMetrics avg_metrics{};
    size_t count = 0;
    
    std::queue<PerformanceMetrics> temp = metrics_history_;
    while (!temp.empty()) {
        const auto& metrics = temp.front();
        
        avg_metrics.processing_fps += metrics.processing_fps;
        avg_metrics.real_time_factor += metrics.real_time_factor;
        avg_metrics.average_frame_time_ms += metrics.average_frame_time_ms;
        avg_metrics.cpu_usage_percent += metrics.cpu_usage_percent;
        avg_metrics.memory_usage_mb += metrics.memory_usage_mb;
        avg_metrics.synthesis_quality_score += metrics.synthesis_quality_score;
        avg_metrics.latency_ms += metrics.latency_ms;
        
        temp.pop();
        count++;
    }
    
    if (count > 0) {
        avg_metrics.processing_fps /= count;
        avg_metrics.real_time_factor /= count;
        avg_metrics.average_frame_time_ms /= count;
        avg_metrics.cpu_usage_percent /= count;
        avg_metrics.memory_usage_mb /= count;
        avg_metrics.synthesis_quality_score /= count;
        avg_metrics.latency_ms /= count;
    }
    
    return avg_metrics;
}

// PerformanceReport methods
void PerformanceReport::save_to_json(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }
    
    file << "{\n";
    file << "  \"timestamp\": \"" << report_timestamp << "\",\n";
    file << "  \"system_info\": \"" << system_info << "\",\n";
    file << "  \"current_metrics\": {\n";
    file << "    \"processing_fps\": " << current_metrics.processing_fps << ",\n";
    file << "    \"real_time_factor\": " << current_metrics.real_time_factor << ",\n";
    file << "    \"average_frame_time_ms\": " << current_metrics.average_frame_time_ms << ",\n";
    file << "    \"cpu_usage_percent\": " << current_metrics.cpu_usage_percent << ",\n";
    file << "    \"memory_usage_mb\": " << current_metrics.memory_usage_mb << ",\n";
    file << "    \"synthesis_quality_score\": " << current_metrics.synthesis_quality_score << ",\n";
    file << "    \"latency_ms\": " << current_metrics.latency_ms << ",\n";
    file << "    \"total_frames_processed\": " << current_metrics.total_frames_processed << "\n";
    file << "  },\n";
    file << "  \"peak_metrics\": {\n";
    file << "    \"peak_frame_time_ms\": " << peak_metrics.peak_frame_time_ms << ",\n";
    file << "    \"peak_cpu_usage_percent\": " << peak_metrics.peak_cpu_usage_percent << ",\n";
    file << "    \"peak_memory_mb\": " << peak_metrics.peak_memory_mb << "\n";
    file << "  },\n";
    file << "  \"performance_alerts\": [";
    for (size_t i = 0; i < performance_alerts.size(); ++i) {
        file << "\"" << performance_alerts[i] << "\"";
        if (i < performance_alerts.size() - 1) file << ",";
    }
    file << "],\n";
    file << "  \"optimization_suggestions\": [";
    for (size_t i = 0; i < optimization_suggestions.size(); ++i) {
        file << "\"" << optimization_suggestions[i] << "\"";
        if (i < optimization_suggestions.size() - 1) file << ",";
    }
    file << "]\n";
    file << "}\n";
    
    file.close();
}

void PerformanceReport::save_to_csv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " + filepath);
    }
    
    // Write header
    file << "metric,current,average,peak\n";
    
    // Write metrics
    file << "processing_fps," << current_metrics.processing_fps << "," 
          << average_metrics.processing_fps << ",N/A\n";
    file << "real_time_factor," << current_metrics.real_time_factor << "," 
          << average_metrics.real_time_factor << ",N/A\n";
    file << "frame_time_ms," << current_metrics.average_frame_time_ms << "," 
          << average_metrics.average_frame_time_ms << "," << peak_metrics.peak_frame_time_ms << "\n";
    file << "cpu_usage_percent," << current_metrics.cpu_usage_percent << "," 
          << average_metrics.cpu_usage_percent << "," << peak_metrics.peak_cpu_usage_percent << "\n";
    file << "memory_usage_mb," << current_metrics.memory_usage_mb << "," 
          << average_metrics.memory_usage_mb << "," << peak_metrics.peak_memory_mb << "\n";
    file << "quality_score," << current_metrics.synthesis_quality_score << "," 
          << average_metrics.synthesis_quality_score << "," << peak_metrics.synthesis_quality_score << "\n";
    file << "latency_ms," << current_metrics.latency_ms << "," 
          << average_metrics.latency_ms << ",N/A\n";
    file << "frames_processed," << current_metrics.total_frames_processed << ",N/A,N/A\n";
    file << "frames_dropped," << current_metrics.frames_dropped << ",N/A,N/A\n";
    
    file.close();
}

std::string PerformanceReport::generate_summary() const {
    std::ostringstream summary;
    
    summary << "=== Performance Report Summary ===\n";
    summary << "Generated: " << report_timestamp << "\n\n";
    
    summary << "Real-time Performance:\n";
    summary << "  Processing FPS: " << std::fixed << std::setprecision(2) 
            << current_metrics.processing_fps << "\n";
    summary << "  Real-time factor: " << std::fixed << std::setprecision(3) 
            << current_metrics.real_time_factor << "\n";
    summary << "  Average latency: " << std::fixed << std::setprecision(2) 
            << current_metrics.latency_ms << " ms\n\n";
    
    summary << "Resource Usage:\n";
    summary << "  CPU usage: " << std::fixed << std::setprecision(1) 
            << current_metrics.cpu_usage_percent << "% (peak: " 
            << peak_metrics.peak_cpu_usage_percent << "%)\n";
    summary << "  Memory usage: " << current_metrics.memory_usage_mb 
            << " MB (peak: " << peak_metrics.peak_memory_mb << " MB)\n\n";
    
    summary << "Quality Metrics:\n";
    summary << "  Synthesis quality score: " << std::fixed << std::setprecision(3) 
            << current_metrics.synthesis_quality_score << "\n\n";
    
    summary << "Frame Statistics:\n";
    summary << "  Total frames processed: " << current_metrics.total_frames_processed << "\n";
    summary << "  Frames dropped: " << current_metrics.frames_dropped << "\n";
    summary << "  Buffer underruns: " << current_metrics.buffer_underruns << "\n";
    summary << "  Buffer overflows: " << current_metrics.buffer_overflows << "\n\n";
    
    if (!performance_alerts.empty()) {
        summary << "Performance Alerts:\n";
        for (const auto& alert : performance_alerts) {
            summary << "  • " << alert << "\n";
        }
        summary << "\n";
    }
    
    if (!optimization_suggestions.empty()) {
        summary << "Optimization Suggestions:\n";
        for (const auto& suggestion : optimization_suggestions) {
            summary << "  • " << suggestion << "\n";
        }
    }
    
    return summary.str();
}

// Static benchmark method
PerformanceReport PerformanceProfiler::run_synthesis_benchmark(
    const std::string& test_audio_file,
    const std::string& reference_audio_file,
    int duration_seconds) {
    
    PerformanceProfiler profiler;
    profiler.start_profiling();
    
    // TODO: Implement actual synthesis benchmark
    // This would involve:
    // 1. Loading test audio file
    // 2. Running synthesis for specified duration
    // 3. Measuring performance metrics
    // 4. Comparing with reference audio
    
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    
    auto report = profiler.generate_report();
    profiler.stop_profiling();
    
    return report;
}

// Utility functions
namespace PerformanceUtils {

double get_system_cpu_usage() {
    PerformanceProfiler profiler;
    return profiler.get_cpu_usage();
}

size_t get_process_memory_usage_mb() {
    PerformanceProfiler profiler;
    return profiler.get_memory_usage_mb();
}

std::string get_hardware_info() {
    PerformanceProfiler profiler;
    return profiler.get_system_info();
}

ComparisonResult compare_performance_reports(
    const PerformanceReport& baseline,
    const PerformanceReport& comparison) {
    
    ComparisonResult result;
    
    // Calculate performance improvement factor
    if (baseline.current_metrics.processing_fps > 0) {
        result.performance_improvement_factor = 
            comparison.current_metrics.processing_fps / baseline.current_metrics.processing_fps;
    } else {
        result.performance_improvement_factor = 1.0;
    }
    
    // Calculate quality difference
    result.quality_difference = 
        comparison.current_metrics.synthesis_quality_score - 
        baseline.current_metrics.synthesis_quality_score;
    
    // Generate recommendation
    if (result.performance_improvement_factor > 1.1 && result.quality_difference >= -0.05) {
        result.recommendation = "UPGRADE_RECOMMENDED: Better performance with minimal quality loss";
    } else if (result.quality_difference > 0.1) {
        result.recommendation = "QUALITY_IMPROVEMENT: Significantly better quality";
    } else if (result.performance_improvement_factor < 0.9) {
        result.recommendation = "PERFORMANCE_REGRESSION: Consider reverting changes";
    } else {
        result.recommendation = "MIXED_RESULTS: Evaluate based on priorities";
    }
    
    return result;
}

PerformanceReport run_automated_stress_test(int duration_seconds, int concurrent_threads) {
    // TODO: Implement automated stress test
    // This would create multiple concurrent synthesis operations
    // and measure system behavior under load
    
    PerformanceProfiler profiler;
    profiler.start_profiling();
    
    // Simulate stress test
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    
    auto report = profiler.generate_report();
    profiler.stop_profiling();
    
    return report;
}

void export_performance_data_to_csv(
    const std::vector<PerformanceReport>& reports,
    const std::string& output_file) {
    
    std::ofstream file(output_file);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + output_file);
    }
    
    // Write header
    file << "timestamp,processing_fps,real_time_factor,cpu_usage,memory_usage,quality_score,latency_ms\n";
    
    // Write data
    for (const auto& report : reports) {
        file << report.report_timestamp << ","
             << report.current_metrics.processing_fps << ","
             << report.current_metrics.real_time_factor << ","
             << report.current_metrics.cpu_usage_percent << ","
             << report.current_metrics.memory_usage_mb << ","
             << report.current_metrics.synthesis_quality_score << ","
             << report.current_metrics.latency_ms << "\n";
    }
    
    file.close();
}

void generate_performance_visualization_data(
    const PerformanceReport& report,
    const std::string& output_dir) {
    
    // Export frame time history
    std::ofstream frame_time_file(output_dir + "/frame_time_history.csv");
    if (frame_time_file.is_open()) {
        frame_time_file << "frame_index,frame_time_ms\n";
        for (size_t i = 0; i < report.frame_time_history.size(); ++i) {
            frame_time_file << i << "," << report.frame_time_history[i] << "\n";
        }
        frame_time_file.close();
    }
    
    // Export CPU usage history
    std::ofstream cpu_file(output_dir + "/cpu_usage_history.csv");
    if (cpu_file.is_open()) {
        cpu_file << "sample_index,cpu_usage_percent\n";
        for (size_t i = 0; i < report.cpu_usage_history.size(); ++i) {
            cpu_file << i << "," << report.cpu_usage_history[i] << "\n";
        }
        cpu_file.close();
    }
    
    // Export quality score history
    std::ofstream quality_file(output_dir + "/quality_history.csv");
    if (quality_file.is_open()) {
        quality_file << "sample_index,quality_score\n";
        for (size_t i = 0; i < report.quality_score_history.size(); ++i) {
            quality_file << i << "," << report.quality_score_history[i] << "\n";
        }
        quality_file.close();
    }
}

} // namespace PerformanceUtils

} // namespace synthesis
} // namespace nexussynth
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include "quality_metrics.h"

namespace nexussynth {
namespace synthesis {

/**
 * @brief Real-time performance metrics for synthesis operations
 */
struct PerformanceMetrics {
    // Timing metrics
    double processing_fps = 0.0;              // Frames processed per second
    double real_time_factor = 0.0;            // Processing time / real time
    double average_frame_time_ms = 0.0;       // Average time per frame
    double peak_frame_time_ms = 0.0;          // Maximum frame processing time
    double total_processing_time_ms = 0.0;    // Total accumulated processing time
    
    // Resource usage
    double cpu_usage_percent = 0.0;           // Current CPU usage
    double peak_cpu_usage_percent = 0.0;      // Peak CPU usage observed
    size_t memory_usage_mb = 0;               // Current memory usage
    size_t peak_memory_mb = 0;                // Peak memory usage
    
    // Buffer statistics
    double input_buffer_utilization = 0.0;   // Input buffer usage (0.0-1.0)
    double output_buffer_utilization = 0.0;  // Output buffer usage (0.0-1.0)
    size_t buffer_underruns = 0;              // Number of buffer underruns
    size_t buffer_overflows = 0;              // Number of buffer overflows
    
    // Quality vs Performance
    double synthesis_quality_score = 0.0;    // Composite quality metric
    double latency_ms = 0.0;                  // Current synthesis latency
    
    // Frame processing statistics
    size_t total_frames_processed = 0;        // Total frames processed
    size_t frames_dropped = 0;                // Frames dropped due to performance
    
    std::chrono::steady_clock::time_point session_start_time;
    
    bool is_valid() const {
        return total_frames_processed > 0;
    }
    
    double get_session_duration_seconds() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - session_start_time
        );
        return duration.count() / 1000.0;
    }
};

/**
 * @brief Performance profiling configuration
 */
struct ProfilingConfig {
    bool enable_cpu_monitoring = true;        // Monitor CPU usage
    bool enable_memory_monitoring = true;     // Monitor memory usage
    bool enable_quality_tracking = true;      // Track synthesis quality
    bool enable_real_time_analysis = true;    // Real-time performance analysis
    
    double sampling_interval_ms = 100.0;      // Metrics sampling interval
    size_t history_buffer_size = 1000;        // Number of historical samples
    double quality_measurement_interval_s = 5.0;  // Quality measurement frequency
    
    // Performance thresholds for alerts
    double cpu_usage_alert_threshold = 80.0;
    double latency_alert_threshold_ms = 50.0;
    double buffer_utilization_alert_threshold = 0.9;
    
    ProfilingConfig() = default;
};

/**
 * @brief Detailed performance analysis report
 */
struct PerformanceReport {
    PerformanceMetrics current_metrics;
    PerformanceMetrics average_metrics;
    PerformanceMetrics peak_metrics;
    
    std::vector<double> frame_time_history;
    std::vector<double> cpu_usage_history;
    std::vector<double> quality_score_history;
    
    std::map<std::string, double> bottleneck_analysis;
    std::vector<std::string> performance_alerts;
    std::vector<std::string> optimization_suggestions;
    
    std::string report_timestamp;
    std::string system_info;
    
    void save_to_json(const std::string& filepath) const;
    void save_to_csv(const std::string& filepath) const;
    bool load_from_json(const std::string& filepath);
    
    // Generate human-readable performance summary
    std::string generate_summary() const;
};

/**
 * @brief Real-time performance profiler for synthesis engines
 */
class PerformanceProfiler {
public:
    explicit PerformanceProfiler(const ProfilingConfig& config = ProfilingConfig{});
    ~PerformanceProfiler();
    
    // Profiling control
    bool start_profiling();
    void stop_profiling();
    bool is_profiling() const { return profiling_active_.load(); }
    
    // Frame-level performance tracking
    void begin_frame_measurement();
    void end_frame_measurement();
    void record_frame_processed(size_t frame_count = 1);
    void record_frame_dropped(size_t drop_count = 1);
    
    // Quality measurement integration
    void record_quality_metrics(const NexusSynth::QualityMetrics& quality_metrics);
    void set_reference_audio(const std::vector<double>& reference_audio, double sample_rate);
    
    // Resource monitoring
    void update_buffer_statistics(double input_utilization, double output_utilization,
                                size_t underruns = 0, size_t overflows = 0);
    void record_synthesis_latency(double latency_ms);
    
    // Real-time metrics access
    PerformanceMetrics get_current_metrics() const;
    PerformanceMetrics get_average_metrics() const;
    PerformanceMetrics get_peak_metrics() const;
    
    // Analysis and reporting
    PerformanceReport generate_report() const;
    std::vector<std::string> get_performance_alerts() const;
    std::vector<std::string> get_optimization_suggestions() const;
    
    // Configuration
    bool update_config(const ProfilingConfig& new_config);
    const ProfilingConfig& get_config() const { return config_; }
    
    // Reset statistics
    void reset_statistics();
    
    // Benchmark utilities
    static PerformanceReport run_synthesis_benchmark(
        const std::string& test_audio_file,
        const std::string& reference_audio_file,
        int duration_seconds = 30
    );

private:
    ProfilingConfig config_;
    
    // Threading and synchronization
    std::atomic<bool> profiling_active_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::unique_ptr<std::thread> monitoring_thread_;
    mutable std::mutex metrics_mutex_;
    mutable std::mutex history_mutex_;
    
    // Metrics storage
    PerformanceMetrics current_metrics_;
    PerformanceMetrics peak_metrics_;
    PerformanceMetrics accumulated_metrics_;
    
    // Historical data
    std::queue<double> frame_time_history_;
    std::queue<double> cpu_usage_history_;
    std::queue<double> quality_score_history_;
    std::queue<PerformanceMetrics> metrics_history_;
    
    // Frame timing measurement
    std::chrono::high_resolution_clock::time_point frame_start_time_;
    bool frame_timing_active_ = false;
    
    // Quality tracking
    std::vector<double> reference_audio_;
    double reference_sample_rate_ = 44100.0;
    std::chrono::steady_clock::time_point last_quality_measurement_;
    
    // Performance analysis
    std::vector<std::string> active_alerts_;
    std::chrono::steady_clock::time_point last_alert_check_;
    
    // Internal methods
    void monitoring_thread_main();
    void update_system_metrics();
    void update_derived_metrics();
    void check_performance_alerts();
    std::vector<std::string> analyze_bottlenecks() const;
    std::vector<std::string> generate_optimization_suggestions() const;
    
public:
    // System metrics collection (accessible for utility functions)
    double get_cpu_usage() const;
    size_t get_memory_usage_mb() const;
    std::string get_system_info() const;

private:
    
    // History management
    void add_to_history(std::queue<double>& history, double value);
    void add_to_metrics_history(const PerformanceMetrics& metrics);
    
    // Statistics calculation
    double calculate_average(const std::queue<double>& history) const;
    double calculate_peak(const std::queue<double>& history) const;
    PerformanceMetrics calculate_average_metrics() const;
};

/**
 * @brief Utility functions for performance analysis
 */
namespace PerformanceUtils {
    // System resource monitoring
    double get_system_cpu_usage();
    size_t get_process_memory_usage_mb();
    std::string get_hardware_info();
    
    // Performance comparison
    struct ComparisonResult {
        double performance_improvement_factor;
        double quality_difference;
        std::string recommendation;
    };
    
    ComparisonResult compare_performance_reports(
        const PerformanceReport& baseline,
        const PerformanceReport& comparison
    );
    
    // Automated performance testing
    PerformanceReport run_automated_stress_test(
        int duration_seconds = 60,
        int concurrent_threads = 1
    );
    
    // Quality-performance tradeoff analysis
    struct TradeoffPoint {
        double quality_score;
        double performance_score;
        std::map<std::string, double> config_parameters;
    };
    
    std::vector<TradeoffPoint> analyze_quality_performance_tradeoff(
        const std::vector<std::string>& test_files,
        const std::vector<std::map<std::string, double>>& config_variations
    );
    
    // Export utilities
    void export_performance_data_to_csv(
        const std::vector<PerformanceReport>& reports,
        const std::string& output_file
    );
    
    void generate_performance_visualization_data(
        const PerformanceReport& report,
        const std::string& output_dir
    );
}

} // namespace synthesis
} // namespace nexussynth
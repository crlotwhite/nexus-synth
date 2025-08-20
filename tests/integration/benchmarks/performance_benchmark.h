#pragma once

#include "../utils/performance_monitor.h"
#include "../utils/quality_analyzer.h"
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <future>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Comprehensive performance benchmark result
     */
    struct BenchmarkResult {
        std::string benchmark_name;
        std::string test_scenario;
        
        // Timing metrics
        std::chrono::nanoseconds min_execution_time{0};
        std::chrono::nanoseconds max_execution_time{0};
        std::chrono::nanoseconds avg_execution_time{0};
        std::chrono::nanoseconds median_execution_time{0};
        std::chrono::nanoseconds std_dev_execution_time{0};
        
        // Memory metrics
        size_t min_memory_usage = 0;
        size_t max_memory_usage = 0;
        size_t avg_memory_usage = 0;
        size_t peak_allocation = 0;
        size_t total_allocations = 0;
        
        // Quality metrics
        double formant_preservation_score = 0.0;
        double pitch_accuracy_score = 0.0;
        double spectral_fidelity_score = 0.0;
        double overall_quality_score = 0.0;
        
        // Threading metrics
        double cpu_efficiency_score = 0.0;
        size_t optimal_thread_count = 1;
        double scalability_factor = 1.0;
        
        // Statistical data
        std::vector<std::chrono::nanoseconds> raw_timings;
        std::vector<size_t> raw_memory_samples;
        std::vector<double> raw_quality_scores;
        
        bool benchmark_successful = false;
        std::string error_message;
        
        // Comparison data (vs baseline/reference)
        double performance_improvement_percent = 0.0;
        double quality_improvement_percent = 0.0;
        std::string baseline_system_name;
    };

    /**
     * @brief Benchmark configuration settings
     */
    struct BenchmarkConfig {
        // Test parameters
        int warmup_iterations = 5;
        int measurement_iterations = 50;
        int concurrent_threads = std::thread::hardware_concurrency();
        
        // Quality thresholds
        double min_formant_preservation = 0.85;
        double min_pitch_accuracy = 0.90;
        double min_spectral_fidelity = 0.80;
        
        // Performance thresholds
        std::chrono::milliseconds max_single_phoneme_time{500};
        std::chrono::seconds max_full_song_time{30};
        size_t max_memory_usage_mb = 512;
        
        // Test scenarios
        std::vector<std::string> test_voice_banks;
        std::vector<std::string> test_audio_files;
        std::vector<double> pitch_shift_ratios = {0.5, 0.8, 1.0, 1.2, 2.0};
        std::vector<int> note_lengths_ms = {100, 500, 1000, 2000};
        
        // Statistical settings
        double confidence_level = 0.95;
        bool enable_outlier_detection = true;
        bool collect_detailed_metrics = true;
    };

    /**
     * @brief Micro-benchmark timer for high-precision measurements
     */
    class MicroBenchmarkTimer {
    public:
        MicroBenchmarkTimer();
        ~MicroBenchmarkTimer();

        void start();
        std::chrono::nanoseconds stop();
        std::chrono::nanoseconds elapsed() const;
        
        // Statistical operations
        static std::chrono::nanoseconds calculate_median(std::vector<std::chrono::nanoseconds>& timings);
        static std::chrono::nanoseconds calculate_std_dev(const std::vector<std::chrono::nanoseconds>& timings,
                                                         std::chrono::nanoseconds mean);
        static void remove_outliers(std::vector<std::chrono::nanoseconds>& timings, double std_dev_threshold = 2.0);

    private:
        std::chrono::high_resolution_clock::time_point start_time_;
        bool timing_active_ = false;
    };

    /**
     * @brief Memory profiler for detailed allocation tracking
     */
    class MemoryProfiler {
    public:
        MemoryProfiler();
        ~MemoryProfiler();

        void start_profiling();
        void stop_profiling();
        
        size_t get_current_usage() const;
        size_t get_peak_usage() const;
        size_t get_total_allocations() const;
        size_t get_allocation_count() const;
        
        void reset_counters();

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    /**
     * @brief Formant preservation analyzer
     */
    class FormantAnalyzer {
    public:
        FormantAnalyzer();
        ~FormantAnalyzer();

        // Analyze formant preservation between original and synthesized audio
        double calculate_formant_preservation(const std::vector<float>& original_audio,
                                            const std::vector<float>& synthesized_audio,
                                            double sample_rate = 44100.0);
        
        // Extract formant frequencies from audio
        std::vector<std::vector<double>> extract_formants(const std::vector<float>& audio,
                                                        double sample_rate = 44100.0);
        
        // Calculate deviation between formant tracks
        double calculate_formant_deviation(const std::vector<std::vector<double>>& formants1,
                                         const std::vector<std::vector<double>>& formants2);

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    /**
     * @brief Multi-threaded performance testing framework
     */
    class ConcurrentBenchmark {
    public:
        ConcurrentBenchmark(const BenchmarkConfig& config);
        ~ConcurrentBenchmark();

        // Run benchmark with varying thread counts
        std::vector<BenchmarkResult> run_scalability_test(
            std::function<void()> benchmark_function,
            const std::string& test_name);
        
        // Find optimal thread count for given workload
        size_t find_optimal_thread_count(std::function<void()> benchmark_function);
        
        // Measure thread efficiency and contention
        double calculate_cpu_efficiency(const std::vector<BenchmarkResult>& results);

    private:
        BenchmarkConfig config_;
        std::vector<std::thread> worker_threads_;
    };

    /**
     * @brief Main performance benchmark framework
     */
    class PerformanceBenchmarkFramework {
    public:
        PerformanceBenchmarkFramework();
        explicit PerformanceBenchmarkFramework(const BenchmarkConfig& config);
        ~PerformanceBenchmarkFramework();

        // Configuration
        void set_config(const BenchmarkConfig& config);
        const BenchmarkConfig& get_config() const;

        // Core benchmarking methods
        BenchmarkResult run_single_benchmark(
            std::function<void()> benchmark_function,
            const std::string& benchmark_name,
            const std::string& test_scenario = "");

        std::vector<BenchmarkResult> run_benchmark_suite(
            const std::vector<std::pair<std::function<void()>, std::string>>& benchmarks);

        // Specialized benchmark types
        BenchmarkResult benchmark_phoneme_synthesis(const std::string& phoneme_data,
                                                   const std::string& voice_bank);
        
        BenchmarkResult benchmark_pitch_shift_performance(const std::string& audio_file,
                                                         double pitch_ratio);
        
        BenchmarkResult benchmark_voice_bank_loading(const std::string& voice_bank_path);
        
        BenchmarkResult benchmark_concurrent_synthesis(const std::vector<std::string>& synthesis_tasks,
                                                      int thread_count);

        // Quality-focused benchmarks
        BenchmarkResult benchmark_formant_preservation(const std::string& original_audio,
                                                      const std::string& synthesized_audio);
        
        BenchmarkResult benchmark_pitch_accuracy(const std::vector<double>& target_pitches,
                                                const std::string& synthesized_audio);

        // Comparison benchmarks
        std::vector<BenchmarkResult> compare_against_baseline(
            const std::string& baseline_system,
            const std::vector<std::string>& test_cases);

        // Data collection and analysis
        void save_results(const std::vector<BenchmarkResult>& results,
                         const std::string& output_path);
        
        void generate_performance_report(const std::vector<BenchmarkResult>& results,
                                       const std::string& report_path);
        
        void generate_comparison_report(const std::vector<BenchmarkResult>& current_results,
                                      const std::vector<BenchmarkResult>& baseline_results,
                                      const std::string& report_path);

        // Statistical analysis
        BenchmarkResult calculate_aggregate_statistics(const std::vector<BenchmarkResult>& results);
        bool detect_performance_regression(const BenchmarkResult& current,
                                         const BenchmarkResult& baseline,
                                         double threshold_percent = 10.0);

    private:
        BenchmarkConfig config_;
        std::unique_ptr<MicroBenchmarkTimer> timer_;
        std::unique_ptr<MemoryProfiler> memory_profiler_;
        std::unique_ptr<FormantAnalyzer> formant_analyzer_;
        std::unique_ptr<ConcurrentBenchmark> concurrent_benchmark_;
        std::unique_ptr<QualityAnalyzer> quality_analyzer_;
        
        // Internal helper methods
        void warmup_system();
        std::vector<std::chrono::nanoseconds> collect_timing_samples(
            std::function<void()> benchmark_function, int iterations);
        std::vector<size_t> collect_memory_samples(
            std::function<void()> benchmark_function, int iterations);
        void apply_statistical_analysis(BenchmarkResult& result);
        bool validate_benchmark_result(const BenchmarkResult& result) const;
    };

} // namespace integration_test
} // namespace nexussynth
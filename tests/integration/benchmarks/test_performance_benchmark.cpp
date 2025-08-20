#include <gtest/gtest.h>
#include "performance_benchmark.h"
#include "benchmark_data_collector.h"
#include "../utils/test_data_manager.h"
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace nexussynth {
namespace integration_test {

class PerformanceBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string test_data_dir = std::filesystem::current_path() / "test_data";
        ASSERT_TRUE(test_data_manager_.initialize(test_data_dir));
        ASSERT_TRUE(test_data_manager_.setup_test_environment());
        
        // Set up benchmark framework with test configuration
        config_.measurement_iterations = 10; // Reduced for faster testing
        config_.warmup_iterations = 2;
        config_.enable_outlier_detection = true;
        
        framework_ = std::make_unique<PerformanceBenchmarkFramework>(config_);
        data_collector_ = std::make_unique<BenchmarkDataCollector>();
    }
    
    void TearDown() override {
        test_data_manager_.cleanup_test_environment();
    }
    
    TestDataManager test_data_manager_;
    BenchmarkConfig config_;
    std::unique_ptr<PerformanceBenchmarkFramework> framework_;
    std::unique_ptr<BenchmarkDataCollector> data_collector_;
    
    // Mock benchmark functions
    static void fast_benchmark() {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    static void slow_benchmark() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    static void memory_intensive_benchmark() {
        std::vector<float> buffer(10000, 0.5f);
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = static_cast<float>(i) * 0.001f;
        }
    }
    
    static void cpu_intensive_benchmark() {
        volatile double result = 0.0;
        for (int i = 0; i < 50000; ++i) {
            result += std::sin(i * 0.001);
        }
    }
};

TEST_F(PerformanceBenchmarkTest, MicroBenchmarkTimerAccuracy) {
    // Test high-precision timing accuracy
    
    MicroBenchmarkTimer timer;
    
    // Test basic start/stop functionality
    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto elapsed = timer.stop();
    
    EXPECT_GT(elapsed.count(), 9000000); // At least 9ms (accounting for system variance)
    EXPECT_LT(elapsed.count(), 20000000); // Less than 20ms (reasonable upper bound)
}

TEST_F(PerformanceBenchmarkTest, MicroBenchmarkTimerStatistics) {
    // Test statistical functions
    
    std::vector<std::chrono::nanoseconds> timings = {
        std::chrono::nanoseconds(100),
        std::chrono::nanoseconds(200),
        std::chrono::nanoseconds(150),
        std::chrono::nanoseconds(300), // Potential outlier
        std::chrono::nanoseconds(120),
        std::chrono::nanoseconds(110),
        std::chrono::nanoseconds(180)
    };
    
    // Test median calculation
    auto median = MicroBenchmarkTimer::calculate_median(timings);
    EXPECT_EQ(median.count(), 150); // Middle value after sorting
    
    // Test standard deviation calculation
    auto mean = std::chrono::nanoseconds(160); // Approximate mean
    auto std_dev = MicroBenchmarkTimer::calculate_std_dev(timings, mean);
    EXPECT_GT(std_dev.count(), 0);
    
    // Test outlier removal
    size_t original_size = timings.size();
    MicroBenchmarkTimer::remove_outliers(timings, 1.5); // Aggressive outlier threshold
    EXPECT_LE(timings.size(), original_size); // Size should stay same or decrease
}

TEST_F(PerformanceBenchmarkTest, MemoryProfilerFunctionality) {
    // Test memory profiling capabilities
    
    MemoryProfiler profiler;
    
    size_t initial_memory = profiler.get_current_usage();
    EXPECT_GE(initial_memory, 0);
    
    profiler.start_profiling();
    
    // Allocate some memory
    std::vector<std::vector<float>> buffers;
    for (int i = 0; i < 5; ++i) {
        buffers.emplace_back(1000, static_cast<float>(i));
    }
    
    profiler.stop_profiling();
    
    size_t peak_usage = profiler.get_peak_usage();
    EXPECT_GE(peak_usage, initial_memory);
    
    size_t current_usage = profiler.get_current_usage();
    EXPECT_GE(current_usage, 0);
}

TEST_F(PerformanceBenchmarkTest, FormantAnalyzerBasicFunctionality) {
    // Test formant analysis capabilities
    
    FormantAnalyzer analyzer;
    
    // Create mock audio data
    std::vector<float> original_audio(44100, 0.0f); // 1 second at 44.1kHz
    std::vector<float> synthesized_audio(44100, 0.0f);
    
    // Add some signal content
    for (size_t i = 0; i < original_audio.size(); ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        original_audio[i] = 0.5f * std::sin(2 * M_PI * 440.0f * t); // 440 Hz tone
        synthesized_audio[i] = 0.5f * std::sin(2 * M_PI * 442.0f * t); // Slightly off-tune
    }
    
    // Test formant extraction
    auto formants = analyzer.extract_formants(original_audio, 44100.0);
    EXPECT_EQ(formants.size(), 3); // Should extract F1, F2, F3
    EXPECT_GT(formants[0].size(), 0); // Should have time frames
    
    // Test formant preservation calculation
    double preservation_score = analyzer.calculate_formant_preservation(
        original_audio, synthesized_audio, 44100.0);
    
    EXPECT_GE(preservation_score, 0.0);
    EXPECT_LE(preservation_score, 1.0);
    
    // Test with identical audio (should have high preservation)
    double identical_preservation = analyzer.calculate_formant_preservation(
        original_audio, original_audio, 44100.0);
    EXPECT_GT(identical_preservation, 0.95); // Should be near perfect
}

TEST_F(PerformanceBenchmarkTest, SingleBenchmarkExecution) {
    // Test single benchmark execution
    
    BenchmarkResult result = framework_->run_single_benchmark(
        fast_benchmark, "fast_test", "unit_test");
    
    EXPECT_TRUE(result.benchmark_successful) << "Error: " << result.error_message;
    EXPECT_EQ(result.benchmark_name, "fast_test");
    EXPECT_EQ(result.test_scenario, "unit_test");
    
    // Check timing results
    EXPECT_GT(result.avg_execution_time.count(), 0);
    EXPECT_GE(result.min_execution_time.count(), 0);
    EXPECT_GE(result.max_execution_time, result.min_execution_time);
    EXPECT_GE(result.avg_execution_time, result.min_execution_time);
    EXPECT_LE(result.avg_execution_time, result.max_execution_time);
    
    // Check that we have raw timing data
    EXPECT_EQ(result.raw_timings.size(), config_.measurement_iterations);
    
    // Check memory results
    EXPECT_GE(result.avg_memory_usage, 0);
    EXPECT_GE(result.peak_allocation, 0);
}

TEST_F(PerformanceBenchmarkTest, BenchmarkSuiteExecution) {
    // Test benchmark suite execution
    
    std::vector<std::pair<std::function<void()>, std::string>> benchmarks = {
        {fast_benchmark, "fast_test"},
        {memory_intensive_benchmark, "memory_test"},
        {cpu_intensive_benchmark, "cpu_test"}
    };
    
    auto results = framework_->run_benchmark_suite(benchmarks);
    
    EXPECT_EQ(results.size(), 3);
    
    for (const auto& result : results) {
        EXPECT_TRUE(result.benchmark_successful) << "Benchmark failed: " << result.benchmark_name;
        EXPECT_GT(result.avg_execution_time.count(), 0);
        EXPECT_EQ(result.raw_timings.size(), config_.measurement_iterations);
    }
    
    // Verify different benchmarks have different characteristics
    bool found_fast = false, found_memory = false, found_cpu = false;
    for (const auto& result : results) {
        if (result.benchmark_name == "fast_test") found_fast = true;
        if (result.benchmark_name == "memory_test") found_memory = true;
        if (result.benchmark_name == "cpu_test") found_cpu = true;
    }
    
    EXPECT_TRUE(found_fast && found_memory && found_cpu);
}

TEST_F(PerformanceBenchmarkTest, ConcurrentBenchmarkScalability) {
    // Test concurrent benchmark functionality
    
    ConcurrentBenchmark concurrent_benchmark(config_);
    
    auto scalability_results = concurrent_benchmark.run_scalability_test(
        cpu_intensive_benchmark, "scalability_test");
    
    EXPECT_GE(scalability_results.size(), 1); // At least single-threaded result
    
    for (const auto& result : scalability_results) {
        EXPECT_TRUE(result.benchmark_successful);
        EXPECT_GT(result.avg_execution_time.count(), 0);
        EXPECT_GE(result.optimal_thread_count, 1);
    }
    
    // Test optimal thread count detection
    size_t optimal_threads = concurrent_benchmark.find_optimal_thread_count(cpu_intensive_benchmark);
    EXPECT_GE(optimal_threads, 1);
    EXPECT_LE(optimal_threads, std::thread::hardware_concurrency());
}

TEST_F(PerformanceBenchmarkTest, BenchmarkDataCollectorSerialization) {
    // Test data collection and serialization
    
    // Run some benchmarks to get results
    std::vector<BenchmarkResult> results;
    results.push_back(framework_->run_single_benchmark(fast_benchmark, "test1", "scenario1"));
    results.push_back(framework_->run_single_benchmark(slow_benchmark, "test2", "scenario2"));
    
    // Test JSON serialization
    std::string temp_dir = std::filesystem::temp_directory_path();
    std::string json_path = temp_dir + "/test_results.json";
    
    EXPECT_TRUE(data_collector_->serialize_to_json(results, json_path));
    EXPECT_TRUE(std::filesystem::exists(json_path));
    
    // Verify JSON content
    std::ifstream json_file(json_path);
    ASSERT_TRUE(json_file.is_open());
    
    std::string json_content((std::istreambuf_iterator<char>(json_file)),
                            std::istreambuf_iterator<char>());
    
    EXPECT_NE(json_content.find("\"benchmark_name\": \"test1\""), std::string::npos);
    EXPECT_NE(json_content.find("\"benchmark_name\": \"test2\""), std::string::npos);
    EXPECT_NE(json_content.find("\"avg_execution_time_ns\""), std::string::npos);
    
    // Test CSV serialization
    std::string csv_path = temp_dir + "/test_results.csv";
    
    EXPECT_TRUE(data_collector_->serialize_to_csv(results, csv_path));
    EXPECT_TRUE(std::filesystem::exists(csv_path));
    
    // Verify CSV content
    std::ifstream csv_file(csv_path);
    ASSERT_TRUE(csv_file.is_open());
    
    std::string csv_header;
    std::getline(csv_file, csv_header);
    
    EXPECT_NE(csv_header.find("benchmark_name"), std::string::npos);
    EXPECT_NE(csv_header.find("avg_execution_time_ns"), std::string::npos);
    EXPECT_NE(csv_header.find("avg_memory_usage"), std::string::npos);
    
    // Clean up
    std::filesystem::remove(json_path);
    std::filesystem::remove(csv_path);
}

TEST_F(PerformanceBenchmarkTest, BenchmarkConfigurationValidation) {
    // Test benchmark configuration validation
    
    BenchmarkConfig invalid_config;
    invalid_config.measurement_iterations = 0; // Invalid
    invalid_config.warmup_iterations = -1; // Invalid
    
    // Framework should handle invalid configuration gracefully
    PerformanceBenchmarkFramework test_framework(invalid_config);
    
    auto result = test_framework.run_single_benchmark(fast_benchmark, "config_test");
    
    // Should either succeed with corrected config or fail gracefully
    if (!result.benchmark_successful) {
        EXPECT_FALSE(result.error_message.empty());
    }
}

TEST_F(PerformanceBenchmarkTest, OutlierDetectionFunctionality) {
    // Test outlier detection in benchmark results
    
    // Create a benchmark that occasionally takes much longer (simulating outliers)
    auto outlier_benchmark = []() {
        static int call_count = 0;
        call_count++;
        
        if (call_count % 5 == 0) {
            // Every 5th call takes much longer (outlier)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    };
    
    BenchmarkConfig outlier_config = config_;
    outlier_config.measurement_iterations = 20; // More iterations to catch outliers
    outlier_config.enable_outlier_detection = true;
    
    PerformanceBenchmarkFramework outlier_framework(outlier_config);
    auto result = outlier_framework.run_single_benchmark(outlier_benchmark, "outlier_test");
    
    EXPECT_TRUE(result.benchmark_successful);
    
    // With outlier detection, the number of raw timings might be less than iterations
    // due to outlier removal
    EXPECT_LE(result.raw_timings.size(), outlier_config.measurement_iterations);
    EXPECT_GT(result.raw_timings.size(), 0);
}

TEST_F(PerformanceBenchmarkTest, PerformanceRegressionDetection) {
    // Test performance regression detection
    
    // Create baseline results
    std::vector<BenchmarkResult> baseline_results;
    baseline_results.push_back(framework_->run_single_benchmark(fast_benchmark, "regression_test"));
    
    // Simulate a current result that's slower (regression)
    auto slow_result = framework_->run_single_benchmark(slow_benchmark, "regression_test");
    
    // Create historical data for regression detection
    std::vector<HistoricalBenchmarkData> historical_data;
    HistoricalBenchmarkData baseline_data = data_collector_->create_historical_entry(baseline_results);
    historical_data.push_back(baseline_data);
    
    // Test regression detection
    auto regression_alerts = data_collector_->detect_regressions(
        slow_result, historical_data, 50.0); // 50% regression threshold
    
    // Should detect regression if slow_benchmark is significantly slower than fast_benchmark
    if (slow_result.avg_execution_time > baseline_results[0].avg_execution_time * 1.5) {
        EXPECT_GT(regression_alerts.size(), 0);
        
        if (!regression_alerts.empty()) {
            EXPECT_EQ(regression_alerts[0].benchmark_name, "regression_test");
            EXPECT_GT(regression_alerts[0].regression_percent, 0);
        }
    }
}

TEST_F(PerformanceBenchmarkTest, SystemInfoCollection) {
    // Test system information collection
    
    EXPECT_TRUE(data_collector_->collect_system_info());
    EXPECT_TRUE(data_collector_->collect_build_info());
    
    std::string timestamp = data_collector_->get_current_timestamp();
    EXPECT_FALSE(timestamp.empty());
    EXPECT_NE(timestamp.find("-"), std::string::npos); // Should contain date separators
    
    std::string git_hash = data_collector_->get_git_commit_hash();
    EXPECT_FALSE(git_hash.empty());
    // Git hash should be "unknown" or a valid hash (alphanumeric)
    EXPECT_TRUE(git_hash == "unknown" || 
                std::all_of(git_hash.begin(), git_hash.end(), 
                           [](char c) { return std::isalnum(c); }));
}

TEST_F(PerformanceBenchmarkTest, MemoryLeakDetection) {
    // Test that benchmark framework doesn't leak memory
    
    MemoryProfiler memory_monitor;
    memory_monitor.start_profiling();
    
    size_t initial_memory = memory_monitor.get_current_usage();
    
    // Run multiple benchmarks
    for (int i = 0; i < 5; ++i) {
        auto result = framework_->run_single_benchmark(memory_intensive_benchmark, 
                                                      "leak_test_" + std::to_string(i));
        EXPECT_TRUE(result.benchmark_successful);
    }
    
    // Force garbage collection (if applicable)
    // Note: C++ doesn't have GC, but we can try to release any temporary objects
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    size_t final_memory = memory_monitor.get_current_usage();
    memory_monitor.stop_profiling();
    
    // Memory usage shouldn't grow significantly beyond reasonable bounds
    // Allow for some growth due to caching and system variance
    double memory_growth_ratio = static_cast<double>(final_memory) / initial_memory;
    EXPECT_LT(memory_growth_ratio, 2.0) << "Potential memory leak detected";
}

TEST_F(PerformanceBenchmarkTest, ThreadSafetyValidation) {
    // Test thread safety of benchmark framework
    
    std::vector<std::thread> threads;
    std::vector<BenchmarkResult> results(4);
    std::atomic<int> completed_benchmarks{0};
    
    // Run benchmarks concurrently from multiple threads
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &results, &completed_benchmarks, i]() {
            // Each thread uses its own framework instance for safety
            BenchmarkConfig thread_config = config_;
            thread_config.measurement_iterations = 5; // Reduced for faster execution
            PerformanceBenchmarkFramework thread_framework(thread_config);
            
            results[i] = thread_framework.run_single_benchmark(
                fast_benchmark, "thread_test_" + std::to_string(i));
            
            completed_benchmarks++;
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_benchmarks.load(), 4);
    
    // Verify all benchmarks completed successfully
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(results[i].benchmark_successful) 
            << "Thread " << i << " benchmark failed: " << results[i].error_message;
        EXPECT_EQ(results[i].benchmark_name, "thread_test_" + std::to_string(i));
    }
}

} // namespace integration_test
} // namespace nexussynth
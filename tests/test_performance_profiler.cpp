#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cmath>
#include "nexussynth/performance_profiler.h"
#include "nexussynth/quality_metrics.h"

using namespace nexussynth::synthesis;

class PerformanceProfilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.sampling_interval_ms = 50.0;  // Fast sampling for tests
        config_.history_buffer_size = 100;
        profiler_ = std::make_unique<PerformanceProfiler>(config_);
    }
    
    void TearDown() override {
        if (profiler_ && profiler_->is_profiling()) {
            profiler_->stop_profiling();
        }
    }
    
    ProfilingConfig config_;
    std::unique_ptr<PerformanceProfiler> profiler_;
};

// Basic profiler lifecycle tests
TEST_F(PerformanceProfilerTest, StartStopProfiling) {
    EXPECT_FALSE(profiler_->is_profiling());
    
    EXPECT_TRUE(profiler_->start_profiling());
    EXPECT_TRUE(profiler_->is_profiling());
    
    // Allow some time for monitoring thread to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    profiler_->stop_profiling();
    EXPECT_FALSE(profiler_->is_profiling());
}

TEST_F(PerformanceProfilerTest, DoubleStartProfiling) {
    EXPECT_TRUE(profiler_->start_profiling());
    EXPECT_TRUE(profiler_->is_profiling());
    
    // Second start should return true but not create new thread
    EXPECT_TRUE(profiler_->start_profiling());
    EXPECT_TRUE(profiler_->is_profiling());
    
    profiler_->stop_profiling();
}

// Frame measurement tests
TEST_F(PerformanceProfilerTest, FrameMeasurement) {
    profiler_->start_profiling();
    
    // Simulate frame processing
    profiler_->begin_frame_measurement();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    profiler_->end_frame_measurement();
    profiler_->record_frame_processed(1);
    
    // Allow metrics to update
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_GT(metrics.average_frame_time_ms, 5.0);  // Should be around 10ms
    EXPECT_LT(metrics.average_frame_time_ms, 20.0); // But allow some variance
    EXPECT_EQ(metrics.total_frames_processed, 1);
    
    profiler_->stop_profiling();
}

TEST_F(PerformanceProfilerTest, MultipleFrameMeasurements) {
    profiler_->start_profiling();
    
    const int num_frames = 10;
    for (int i = 0; i < num_frames; ++i) {
        profiler_->begin_frame_measurement();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        profiler_->end_frame_measurement();
        profiler_->record_frame_processed(1);
    }
    
    // Allow metrics to update
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_EQ(metrics.total_frames_processed, num_frames);
    EXPECT_GT(metrics.processing_fps, 0.0);
    EXPECT_GT(metrics.average_frame_time_ms, 0.0);
    
    profiler_->stop_profiling();
}

// Quality metrics integration tests
TEST_F(PerformanceProfilerTest, QualityMetricsIntegration) {
    profiler_->start_profiling();
    
    // Create sample quality metrics
    NexusSynth::QualityMetrics quality_metrics;
    quality_metrics.mcd_score = 5.5;        // Moderate MCD
    quality_metrics.f0_rmse = 20.0;         // Moderate F0 error
    quality_metrics.spectral_correlation = 0.85;  // Good correlation
    quality_metrics.total_frames = 100;
    quality_metrics.valid_frames = 95;
    
    profiler_->record_quality_metrics(quality_metrics);
    
    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_GT(metrics.synthesis_quality_score, 0.0);
    EXPECT_LT(metrics.synthesis_quality_score, 1.0);
    
    profiler_->stop_profiling();
}

// Buffer statistics tests
TEST_F(PerformanceProfilerTest, BufferStatistics) {
    profiler_->start_profiling();
    
    // Simulate buffer usage
    profiler_->update_buffer_statistics(0.7, 0.8, 2, 1);
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_DOUBLE_EQ(metrics.input_buffer_utilization, 0.7);
    EXPECT_DOUBLE_EQ(metrics.output_buffer_utilization, 0.8);
    EXPECT_EQ(metrics.buffer_underruns, 2);
    EXPECT_EQ(metrics.buffer_overflows, 1);
    
    profiler_->stop_profiling();
}

// Latency measurement tests
TEST_F(PerformanceProfilerTest, LatencyMeasurement) {
    profiler_->start_profiling();
    
    const double test_latency = 25.5;
    profiler_->record_synthesis_latency(test_latency);
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_DOUBLE_EQ(metrics.latency_ms, test_latency);
    
    profiler_->stop_profiling();
}

// Performance alerts tests
TEST_F(PerformanceProfilerTest, PerformanceAlerts) {
    // Configure aggressive alert thresholds
    config_.cpu_usage_alert_threshold = 1.0;  // Very low threshold
    config_.latency_alert_threshold_ms = 10.0;
    config_.buffer_utilization_alert_threshold = 0.5;
    
    profiler_ = std::make_unique<PerformanceProfiler>(config_);
    profiler_->start_profiling();
    
    // Trigger alerts
    profiler_->record_synthesis_latency(20.0);  // Above threshold
    profiler_->update_buffer_statistics(0.6, 0.7, 0, 0);  // Above threshold
    
    // Allow time for alert checking
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    
    auto alerts = profiler_->get_performance_alerts();
    EXPECT_GT(alerts.size(), 0);
    
    // Check for specific alert types
    bool found_latency_alert = false;
    bool found_buffer_alert = false;
    
    for (const auto& alert : alerts) {
        if (alert.find("HIGH_LATENCY") != std::string::npos) {
            found_latency_alert = true;
        }
        if (alert.find("BUFFER_HIGH") != std::string::npos) {
            found_buffer_alert = true;
        }
    }
    
    EXPECT_TRUE(found_latency_alert);
    EXPECT_TRUE(found_buffer_alert);
    
    profiler_->stop_profiling();
}

// Configuration update tests
TEST_F(PerformanceProfilerTest, ConfigurationUpdate) {
    ProfilingConfig new_config = config_;
    new_config.sampling_interval_ms = 200.0;
    new_config.enable_cpu_monitoring = false;
    
    // Should succeed when not profiling
    EXPECT_TRUE(profiler_->update_config(new_config));
    
    profiler_->start_profiling();
    
    // Should fail when profiling
    EXPECT_FALSE(profiler_->update_config(new_config));
    
    profiler_->stop_profiling();
}

// Statistics reset tests
TEST_F(PerformanceProfilerTest, StatisticsReset) {
    profiler_->start_profiling();
    
    // Generate some statistics
    profiler_->record_frame_processed(10);
    profiler_->record_frame_dropped(2);
    profiler_->record_synthesis_latency(15.0);
    
    auto metrics_before = profiler_->get_current_metrics();
    EXPECT_EQ(metrics_before.total_frames_processed, 10);
    EXPECT_EQ(metrics_before.frames_dropped, 2);
    
    profiler_->stop_profiling();
    profiler_->reset_statistics();
    profiler_->start_profiling();
    
    auto metrics_after = profiler_->get_current_metrics();
    EXPECT_EQ(metrics_after.total_frames_processed, 0);
    EXPECT_EQ(metrics_after.frames_dropped, 0);
    
    profiler_->stop_profiling();
}

// Report generation tests
TEST_F(PerformanceProfilerTest, ReportGeneration) {
    profiler_->start_profiling();
    
    // Generate some data
    for (int i = 0; i < 5; ++i) {
        profiler_->begin_frame_measurement();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        profiler_->end_frame_measurement();
        profiler_->record_frame_processed(1);
    }
    
    // Allow time for metrics to accumulate
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    auto report = profiler_->generate_report();
    
    EXPECT_GT(report.current_metrics.total_frames_processed, 0);
    EXPECT_FALSE(report.report_timestamp.empty());
    EXPECT_FALSE(report.system_info.empty());
    
    // Test summary generation
    std::string summary = report.generate_summary();
    EXPECT_FALSE(summary.empty());
    EXPECT_NE(summary.find("Performance Report Summary"), std::string::npos);
    
    profiler_->stop_profiling();
}

// Peak metrics tracking tests
TEST_F(PerformanceProfilerTest, PeakMetricsTracking) {
    profiler_->start_profiling();
    
    // Simulate varying frame times
    std::vector<int> frame_times_ms = {5, 15, 8, 25, 10, 30, 7};
    
    for (int frame_time : frame_times_ms) {
        profiler_->begin_frame_measurement();
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_time));
        profiler_->end_frame_measurement();
        profiler_->record_frame_processed(1);
    }
    
    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto peak_metrics = profiler_->get_peak_metrics();
    
    // Peak should be around the maximum frame time (30ms)
    EXPECT_GE(peak_metrics.peak_frame_time_ms, 25.0);
    
    profiler_->stop_profiling();
}

// Real-time factor calculation tests
TEST_F(PerformanceProfilerTest, RealTimeFactorCalculation) {
    profiler_->start_profiling();
    
    const int num_frames = 20;
    const int frame_time_ms = 10;
    
    // Simulate consistent frame processing
    for (int i = 0; i < num_frames; ++i) {
        profiler_->begin_frame_measurement();
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_time_ms));
        profiler_->end_frame_measurement();
        profiler_->record_frame_processed(1);
        
        // Small delay between frames to simulate real processing
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    
    // Allow time for metrics calculation
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    auto metrics = profiler_->get_current_metrics();
    
    // Real-time factor should be reasonable (processing time / real time)
    EXPECT_GT(metrics.real_time_factor, 0.0);
    EXPECT_LT(metrics.real_time_factor, 2.0);  // Should not exceed 2x real-time
    
    profiler_->stop_profiling();
}

// Optimization suggestions tests
TEST_F(PerformanceProfilerTest, OptimizationSuggestions) {
    profiler_->start_profiling();
    
    // Simulate poor performance scenarios
    profiler_->record_frame_dropped(50);
    profiler_->record_frame_processed(100);  // 50% drop rate
    profiler_->record_synthesis_latency(100.0);  // High latency
    profiler_->update_buffer_statistics(0.95, 0.95, 0, 0);  // High buffer usage
    
    // Allow time for analysis
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto suggestions = profiler_->get_optimization_suggestions();
    EXPECT_GT(suggestions.size(), 0);
    
    // Check for relevant suggestions
    bool found_buffer_suggestion = false;
    bool found_latency_suggestion = false;
    
    for (const auto& suggestion : suggestions) {
        if (suggestion.find("buffer") != std::string::npos) {
            found_buffer_suggestion = true;
        }
        if (suggestion.find("latency") != std::string::npos || 
            suggestion.find("pipeline") != std::string::npos) {
            found_latency_suggestion = true;
        }
    }
    
    EXPECT_TRUE(found_buffer_suggestion || found_latency_suggestion);
    
    profiler_->stop_profiling();
}

// File I/O tests for reports
TEST_F(PerformanceProfilerTest, ReportFileIO) {
    profiler_->start_profiling();
    
    // Generate some test data
    profiler_->record_frame_processed(100);
    profiler_->record_synthesis_latency(15.0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto report = profiler_->generate_report();
    
    // Test JSON export
    const std::string json_file = "/tmp/test_performance_report.json";
    EXPECT_NO_THROW(report.save_to_json(json_file));
    
    // Test CSV export
    const std::string csv_file = "/tmp/test_performance_report.csv";
    EXPECT_NO_THROW(report.save_to_csv(csv_file));
    
    profiler_->stop_profiling();
}

// Stress test simulation
TEST_F(PerformanceProfilerTest, StressTestSimulation) {
    profiler_->start_profiling();
    
    const int stress_duration_ms = 500;
    const int concurrent_operations = 4;
    
    // Simulate multiple concurrent processing threads
    std::vector<std::thread> stress_threads;
    
    for (int i = 0; i < concurrent_operations; ++i) {
        stress_threads.emplace_back([this, stress_duration_ms]() {
            auto start_time = std::chrono::steady_clock::now();
            int frame_count = 0;
            
            while (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count() < stress_duration_ms) {
                
                profiler_->begin_frame_measurement();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                profiler_->end_frame_measurement();
                profiler_->record_frame_processed(1);
                
                frame_count++;
                
                // Occasionally simulate dropped frames
                if (frame_count % 20 == 0) {
                    profiler_->record_frame_dropped(1);
                }
            }
        });
    }
    
    // Wait for all stress threads to complete
    for (auto& thread : stress_threads) {
        thread.join();
    }
    
    // Allow final metrics collection
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_GT(metrics.total_frames_processed, 0);
    EXPECT_GT(metrics.processing_fps, 0.0);
    
    auto report = profiler_->generate_report();
    
    // Under stress, we should have some performance data
    EXPECT_FALSE(report.frame_time_history.empty());
    EXPECT_GT(report.current_metrics.total_frames_processed, 100);  // Should process many frames
    
    profiler_->stop_profiling();
}

// Test quality-performance tradeoff scenarios
TEST_F(PerformanceProfilerTest, QualityPerformanceTradeoff) {
    profiler_->start_profiling();
    
    // Scenario 1: High quality, slow performance
    NexusSynth::QualityMetrics high_quality;
    high_quality.mcd_score = 3.0;  // Excellent MCD
    high_quality.f0_rmse = 10.0;   // Low F0 error
    high_quality.spectral_correlation = 0.95;  // Excellent correlation
    high_quality.total_frames = 100;
    high_quality.valid_frames = 100;
    
    profiler_->record_quality_metrics(high_quality);
    
    // Simulate slow processing
    profiler_->begin_frame_measurement();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    profiler_->end_frame_measurement();
    profiler_->record_frame_processed(1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto high_quality_metrics = profiler_->get_current_metrics();
    
    profiler_->reset_statistics();
    
    // Scenario 2: Lower quality, fast performance
    NexusSynth::QualityMetrics fast_quality;
    fast_quality.mcd_score = 6.0;  // Moderate MCD
    fast_quality.f0_rmse = 25.0;   // Higher F0 error
    fast_quality.spectral_correlation = 0.8;  // Good correlation
    fast_quality.total_frames = 100;
    fast_quality.valid_frames = 100;
    
    profiler_->record_quality_metrics(fast_quality);
    
    // Simulate fast processing
    profiler_->begin_frame_measurement();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    profiler_->end_frame_measurement();
    profiler_->record_frame_processed(1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto fast_metrics = profiler_->get_current_metrics();
    
    // Verify tradeoff: higher quality should have longer processing time
    EXPECT_GT(high_quality_metrics.synthesis_quality_score, fast_metrics.synthesis_quality_score);
    EXPECT_GT(high_quality_metrics.average_frame_time_ms, fast_metrics.average_frame_time_ms);
    
    profiler_->stop_profiling();
}

// Test edge cases and error handling
TEST_F(PerformanceProfilerTest, EdgeCases) {
    // Test with invalid configuration
    ProfilingConfig invalid_config;
    invalid_config.sampling_interval_ms = -1.0;  // Invalid
    invalid_config.history_buffer_size = 0;      // Invalid
    
    PerformanceProfiler invalid_profiler(invalid_config);
    
    // Should still start (profiler handles invalid config gracefully)
    EXPECT_TRUE(invalid_profiler.start_profiling());
    invalid_profiler.stop_profiling();
    
    // Test frame measurement without starting
    profiler_->begin_frame_measurement();
    profiler_->end_frame_measurement();
    
    auto metrics = profiler_->get_current_metrics();
    // Should have zero metrics when not profiling
    EXPECT_EQ(metrics.total_frames_processed, 0);
    
    // Test multiple stop calls
    profiler_->stop_profiling();
    profiler_->stop_profiling();  // Should not crash
}

// Integration test with different profiling modes
TEST_F(PerformanceProfilerTest, ProfilingModes) {
    // Test with CPU monitoring disabled
    config_.enable_cpu_monitoring = false;
    config_.enable_memory_monitoring = false;
    config_.enable_quality_tracking = false;
    
    profiler_ = std::make_unique<PerformanceProfiler>(config_);
    profiler_->start_profiling();
    
    // Generate some activity
    profiler_->record_frame_processed(10);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto metrics = profiler_->get_current_metrics();
    
    // Basic metrics should still work
    EXPECT_EQ(metrics.total_frames_processed, 10);
    
    // Resource metrics should be zero/default when disabled
    EXPECT_EQ(metrics.cpu_usage_percent, 0.0);
    EXPECT_EQ(metrics.memory_usage_mb, 0);
    
    profiler_->stop_profiling();
}

// Test concurrent access safety
TEST_F(PerformanceProfilerTest, ConcurrentAccess) {
    profiler_->start_profiling();
    
    const int num_threads = 4;
    const int operations_per_thread = 50;
    std::vector<std::thread> threads;
    
    // Multiple threads recording metrics simultaneously
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, operations_per_thread]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                profiler_->begin_frame_measurement();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                profiler_->end_frame_measurement();
                profiler_->record_frame_processed(1);
                
                if (i % 10 == 0) {
                    profiler_->record_synthesis_latency(static_cast<double>(i));
                    profiler_->update_buffer_statistics(0.5, 0.6, 0, 0);
                }
                
                // Occasionally read metrics
                if (i % 20 == 0) {
                    auto metrics = profiler_->get_current_metrics();
                    (void)metrics;  // Suppress unused variable warning
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto final_metrics = profiler_->get_current_metrics();
    
    // Should have processed all frames from all threads
    EXPECT_EQ(final_metrics.total_frames_processed, num_threads * operations_per_thread);
    
    profiler_->stop_profiling();
}

// Performance benchmark test
TEST_F(PerformanceProfilerTest, ProfilingOverhead) {
    const int num_operations = 1000;
    
    // Measure overhead without profiling
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        // Simulate minimal work
        volatile int dummy = i * 2;
        (void)dummy;
    }
    
    auto baseline_time = std::chrono::high_resolution_clock::now() - start_time;
    
    // Measure with profiling enabled
    profiler_->start_profiling();
    
    start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        profiler_->begin_frame_measurement();
        volatile int dummy = i * 2;
        (void)dummy;
        profiler_->end_frame_measurement();
        profiler_->record_frame_processed(1);
    }
    
    auto profiled_time = std::chrono::high_resolution_clock::now() - start_time;
    
    profiler_->stop_profiling();
    
    // Calculate overhead
    double overhead_ratio = static_cast<double>(profiled_time.count()) / baseline_time.count();
    
    std::cout << "Profiling overhead ratio: " << overhead_ratio << std::endl;
    
    // Profiling overhead should be reasonable (less than 5x slower)
    EXPECT_LT(overhead_ratio, 5.0);
    
    auto metrics = profiler_->get_current_metrics();
    EXPECT_EQ(metrics.total_frames_processed, num_operations);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
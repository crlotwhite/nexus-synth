#pragma once

#include <string>
#include <chrono>
#include <memory>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Performance measurement result
     */
    struct PerformanceMetrics {
        std::chrono::milliseconds execution_time{0};
        size_t peak_memory_bytes = 0;
        size_t current_memory_bytes = 0;
        double cpu_usage_percent = 0.0;
        bool measurement_successful = false;
        std::string error_message;
    };

    /**
     * @brief RAII performance monitor for integration tests
     */
    class PerformanceMonitor {
    public:
        PerformanceMonitor();
        ~PerformanceMonitor();

        void start_monitoring();
        PerformanceMetrics stop_monitoring();
        
        // Static utility methods
        static size_t get_current_memory_usage();
        static double get_current_cpu_usage();

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    /**
     * @brief Performance threshold checker
     */
    struct PerformanceThreshold {
        std::chrono::milliseconds max_execution_time{30000};
        size_t max_memory_bytes = 1024 * 1024 * 1024; // 1GB
        double max_cpu_percent = 80.0;
    };

    bool meets_performance_threshold(const PerformanceMetrics& metrics, 
                                   const PerformanceThreshold& threshold);

} // namespace integration_test  
} // namespace nexussynth
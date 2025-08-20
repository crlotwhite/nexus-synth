#include "performance_monitor.h"
#include <chrono>

#if defined(__APPLE__)
    #include <mach/mach.h>
    #include <sys/resource.h>
#elif defined(__linux__)
    #include <sys/resource.h>
    #include <fstream>
#elif defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
#endif

namespace nexussynth {
namespace integration_test {

class PerformanceMonitor::Impl {
public:
    std::chrono::steady_clock::time_point start_time;
    size_t initial_memory = 0;
    size_t peak_memory = 0;
    bool monitoring = false;
};

PerformanceMonitor::PerformanceMonitor() 
    : pImpl(std::make_unique<Impl>()) {
}

PerformanceMonitor::~PerformanceMonitor() = default;

void PerformanceMonitor::start_monitoring() {
    pImpl->start_time = std::chrono::steady_clock::now();
    pImpl->initial_memory = get_current_memory_usage();
    pImpl->peak_memory = pImpl->initial_memory;
    pImpl->monitoring = true;
}

PerformanceMetrics PerformanceMonitor::stop_monitoring() {
    PerformanceMetrics metrics;
    
    if (!pImpl->monitoring) {
        metrics.error_message = "Monitoring was not started";
        return metrics;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    metrics.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - pImpl->start_time);
    
    metrics.current_memory_bytes = get_current_memory_usage();
    metrics.peak_memory_bytes = std::max(pImpl->peak_memory, metrics.current_memory_bytes);
    metrics.cpu_usage_percent = get_current_cpu_usage();
    metrics.measurement_successful = true;
    
    pImpl->monitoring = false;
    return metrics;
}

size_t PerformanceMonitor::get_current_memory_usage() {
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, 
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return info.resident_size;
    }
#elif defined(__linux__)
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            size_t memory_kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %zu kB", &memory_kb);
            return memory_kb * 1024; // Convert to bytes
        }
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#endif
    return 0;
}

double PerformanceMonitor::get_current_cpu_usage() {
    // Simplified CPU usage measurement
    // In a production system, this would need more sophisticated measurement
    return 0.0; // Placeholder
}

bool meets_performance_threshold(const PerformanceMetrics& metrics,
                               const PerformanceThreshold& threshold) {
    if (!metrics.measurement_successful) {
        return false;
    }
    
    if (metrics.execution_time > threshold.max_execution_time) {
        return false;
    }
    
    if (metrics.peak_memory_bytes > threshold.max_memory_bytes) {
        return false;
    }
    
    if (metrics.cpu_usage_percent > threshold.max_cpu_percent) {
        return false;
    }
    
    return true;
}

} // namespace integration_test
} // namespace nexussynth
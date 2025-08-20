#include "benchmark_data_collector.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <regex>

#if defined(__APPLE__)
    #include <sys/utsname.h>
    #include <sys/sysctl.h>
#elif defined(__linux__)
    #include <sys/utsname.h>
    #include <fstream>
#elif defined(_WIN32)
    #include <windows.h>
    #include <shlobj.h>
#endif

namespace nexussynth {
namespace integration_test {

// ===== FileBenchmarkDatabase Implementation =====

FileBenchmarkDatabase::FileBenchmarkDatabase() = default;

FileBenchmarkDatabase::~FileBenchmarkDatabase() {
    close();
}

bool FileBenchmarkDatabase::initialize(const std::string& base_directory) {
    base_directory_ = base_directory;
    initialized_ = create_tables();
    return initialized_;
}

bool FileBenchmarkDatabase::store_results(const HistoricalBenchmarkData& data) {
    if (!initialized_) {
        return false;
    }
    
    try {
        // Store in JSON format with timestamp-based filename
        std::string filename = generate_filename(data.timestamp, SerializationFormat::JSON);
        std::string full_path = base_directory_ + "/" + filename;
        
        std::ofstream file(full_path);
        if (!file.is_open()) {
            return false;
        }
        
        // Write JSON data (simplified implementation)
        file << "{\n";
        file << "  \"timestamp\": \"" << data.timestamp << "\",\n";
        file << "  \"git_commit_hash\": \"" << data.git_commit_hash << "\",\n";
        file << "  \"system_info\": \"" << data.system_info << "\",\n";
        file << "  \"build_configuration\": \"" << data.build_configuration << "\",\n";
        file << "  \"test_environment\": \"" << data.test_environment << "\",\n";
        file << "  \"compiler_version\": \"" << data.compiler_version << "\",\n";
        file << "  \"optimization_level\": \"" << data.optimization_level << "\",\n";
        file << "  \"results\": [\n";
        
        for (size_t i = 0; i < data.results.size(); ++i) {
            const auto& result = data.results[i];
            file << "    {\n";
            file << "      \"benchmark_name\": \"" << result.benchmark_name << "\",\n";
            file << "      \"test_scenario\": \"" << result.test_scenario << "\",\n";
            file << "      \"avg_execution_time_ns\": " << result.avg_execution_time.count() << ",\n";
            file << "      \"min_execution_time_ns\": " << result.min_execution_time.count() << ",\n";
            file << "      \"max_execution_time_ns\": " << result.max_execution_time.count() << ",\n";
            file << "      \"avg_memory_usage\": " << result.avg_memory_usage << ",\n";
            file << "      \"peak_memory_usage\": " << result.peak_allocation << ",\n";
            file << "      \"formant_preservation_score\": " << result.formant_preservation_score << ",\n";
            file << "      \"overall_quality_score\": " << result.overall_quality_score << ",\n";
            file << "      \"benchmark_successful\": " << (result.benchmark_successful ? "true" : "false");
            if (!result.error_message.empty()) {
                file << ",\n      \"error_message\": \"" << result.error_message << "\"";
            }
            file << "\n    }";
            if (i < data.results.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<HistoricalBenchmarkData> FileBenchmarkDatabase::query_results(
    const std::string& query_filter, const std::string& time_range) {
    
    std::vector<HistoricalBenchmarkData> results;
    
    if (!initialized_) {
        return results;
    }
    
    try {
        // Scan directory for JSON files
        for (const auto& entry : std::filesystem::directory_iterator(base_directory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                // Simple implementation: load all files and filter later
                // In production, would implement proper querying
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    HistoricalBenchmarkData data;
                    
                    // Simplified JSON parsing (in production, use proper JSON library)
                    std::string line;
                    while (std::getline(file, line)) {
                        if (line.find("\"timestamp\":") != std::string::npos) {
                            size_t start = line.find("\"", line.find(":")) + 1;
                            size_t end = line.find("\"", start);
                            if (end != std::string::npos) {
                                data.timestamp = line.substr(start, end - start);
                            }
                        }
                        // Add more parsing as needed...
                    }
                    
                    // Apply basic filtering
                    if (query_filter.empty() || 
                        data.timestamp.find(query_filter) != std::string::npos) {
                        results.push_back(data);
                    }
                }
            }
        }
        
    } catch (const std::exception&) {
        // Return empty results on error
    }
    
    return results;
}

bool FileBenchmarkDatabase::create_tables() {
    return ensure_directory_exists(base_directory_);
}

void FileBenchmarkDatabase::close() {
    initialized_ = false;
}

std::string FileBenchmarkDatabase::generate_filename(const std::string& timestamp, 
                                                   SerializationFormat format) const {
    std::string cleaned_timestamp = timestamp;
    std::replace(cleaned_timestamp.begin(), cleaned_timestamp.end(), ':', '-');
    std::replace(cleaned_timestamp.begin(), cleaned_timestamp.end(), ' ', '_');
    
    std::string extension;
    switch (format) {
        case SerializationFormat::JSON: extension = ".json"; break;
        case SerializationFormat::CSV: extension = ".csv"; break;
        case SerializationFormat::XML: extension = ".xml"; break;
        case SerializationFormat::BINARY: extension = ".bin"; break;
    }
    
    return "benchmark_" + cleaned_timestamp + extension;
}

bool FileBenchmarkDatabase::ensure_directory_exists(const std::string& directory) const {
    try {
        return std::filesystem::create_directories(directory) || 
               std::filesystem::exists(directory);
    } catch (const std::exception&) {
        return false;
    }
}

// ===== BenchmarkDataCollector Implementation =====

BenchmarkDataCollector::BenchmarkDataCollector() 
    : database_(std::make_unique<FileBenchmarkDatabase>()),
      output_directory_("./benchmark_results") {
    
    // Enable common formats by default
    enabled_formats_.insert(SerializationFormat::JSON);
    enabled_formats_.insert(SerializationFormat::CSV);
    
    collect_system_info();
    collect_build_info();
}

BenchmarkDataCollector::BenchmarkDataCollector(std::unique_ptr<BenchmarkDatabase> database)
    : database_(std::move(database)),
      output_directory_("./benchmark_results") {
    
    enabled_formats_.insert(SerializationFormat::JSON);
    enabled_formats_.insert(SerializationFormat::CSV);
    
    collect_system_info();
    collect_build_info();
}

BenchmarkDataCollector::~BenchmarkDataCollector() = default;

void BenchmarkDataCollector::set_output_directory(const std::string& directory) {
    output_directory_ = directory;
    
    // Reinitialize database with new directory
    if (database_) {
        database_->initialize(directory);
    }
}

void BenchmarkDataCollector::set_database(std::unique_ptr<BenchmarkDatabase> database) {
    database_ = std::move(database);
}

void BenchmarkDataCollector::enable_format(SerializationFormat format, bool enabled) {
    if (enabled) {
        enabled_formats_.insert(format);
    } else {
        enabled_formats_.erase(format);
    }
}

bool BenchmarkDataCollector::collect_system_info() {
    system_info_.clear();
    
    try {
#if defined(__APPLE__)
        struct utsname sys_info;
        if (uname(&sys_info) == 0) {
            system_info_["os_name"] = sys_info.sysname;
            system_info_["os_version"] = sys_info.release;
            system_info_["architecture"] = sys_info.machine;
            system_info_["hostname"] = sys_info.nodename;
        }
        
        // Get CPU information
        size_t size = sizeof(int);
        int cpu_count;
        if (sysctlbyname("hw.ncpu", &cpu_count, &size, nullptr, 0) == 0) {
            system_info_["cpu_count"] = std::to_string(cpu_count);
        }
        
        uint64_t memory_size;
        size = sizeof(uint64_t);
        if (sysctlbyname("hw.memsize", &memory_size, &size, nullptr, 0) == 0) {
            system_info_["total_memory"] = std::to_string(memory_size);
        }
        
#elif defined(__linux__)
        struct utsname sys_info;
        if (uname(&sys_info) == 0) {
            system_info_["os_name"] = sys_info.sysname;
            system_info_["os_version"] = sys_info.release;
            system_info_["architecture"] = sys_info.machine;
            system_info_["hostname"] = sys_info.nodename;
        }
        
        // Read CPU info
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("processor") != std::string::npos) {
                // Count processors (simplified)
                static int cpu_count = 0;
                cpu_count++;
                system_info_["cpu_count"] = std::to_string(cpu_count);
            }
        }
        
#elif defined(_WIN32)
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        system_info_["cpu_count"] = std::to_string(sys_info.dwNumberOfProcessors);
        system_info_["architecture"] = std::to_string(sys_info.wProcessorArchitecture);
        
        MEMORYSTATUSEX mem_info;
        mem_info.dwLength = sizeof(mem_info);
        if (GlobalMemoryStatusEx(&mem_info)) {
            system_info_["total_memory"] = std::to_string(mem_info.ullTotalPhys);
        }
#endif

        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool BenchmarkDataCollector::collect_build_info() {
    build_info_.clear();
    
    try {
        // Compiler information
#ifdef __GNUC__
        build_info_["compiler"] = "GCC";
        build_info_["compiler_version"] = std::to_string(__GNUC__) + "." + 
                                        std::to_string(__GNUC_MINOR__) + "." + 
                                        std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(__clang__)
        build_info_["compiler"] = "Clang";
        build_info_["compiler_version"] = std::to_string(__clang_major__) + "." + 
                                        std::to_string(__clang_minor__) + "." + 
                                        std::to_string(__clang_patchlevel__);
#elif defined(_MSC_VER)
        build_info_["compiler"] = "MSVC";
        build_info_["compiler_version"] = std::to_string(_MSC_VER);
#endif

        // Build configuration
#ifdef DEBUG
        build_info_["build_type"] = "Debug";
#else
        build_info_["build_type"] = "Release";
#endif

        // C++ standard
        build_info_["cpp_standard"] = std::to_string(__cplusplus);
        
        // Timestamp
        build_info_["build_timestamp"] = __DATE__ " " __TIME__;
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string BenchmarkDataCollector::get_current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

std::string BenchmarkDataCollector::get_git_commit_hash() const {
    // Simple implementation - in production, would use libgit2 or similar
    try {
        std::string command = "git rev-parse --short HEAD 2>/dev/null";
        
#if defined(_WIN32)
        FILE* pipe = _popen(command.c_str(), "r");
#else
        FILE* pipe = popen(command.c_str(), "r");
#endif
        
        if (!pipe) {
            return "unknown";
        }
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
#if defined(_WIN32)
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        
        // Remove trailing newline
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        
        return result.empty() ? "unknown" : result;
        
    } catch (const std::exception&) {
        return "unknown";
    }
}

bool BenchmarkDataCollector::save_results(const std::vector<BenchmarkResult>& results,
                                        const std::string& output_path) {
    
    std::string base_path = output_path.empty() ? 
        output_directory_ + "/benchmark_" + get_current_timestamp() : output_path;
    
    // Replace spaces and colons in filename
    std::string cleaned_path = base_path;
    std::replace(cleaned_path.begin(), cleaned_path.end(), ':', '-');
    std::replace(cleaned_path.begin(), cleaned_path.end(), ' ', '_');
    
    bool success = true;
    
    // Save in enabled formats
    if (enabled_formats_.count(SerializationFormat::JSON)) {
        success &= serialize_to_json(results, cleaned_path + ".json");
    }
    
    if (enabled_formats_.count(SerializationFormat::CSV)) {
        success &= serialize_to_csv(results, cleaned_path + ".csv");
    }
    
    // Store in database if available
    if (database_) {
        auto historical_data = create_historical_entry(results);
        success &= database_->store_results(historical_data);
    }
    
    return success;
}

bool BenchmarkDataCollector::serialize_to_json(const std::vector<BenchmarkResult>& results,
                                             const std::string& output_path) {
    try {
        std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
        
        std::ofstream file(output_path);
        if (!file.is_open()) {
            return false;
        }
        
        file << std::fixed << std::setprecision(6);
        file << "{\n";
        file << "  \"metadata\": {\n";
        file << "    \"timestamp\": \"" << get_current_timestamp() << "\",\n";
        file << "    \"git_commit\": \"" << get_git_commit_hash() << "\",\n";
        file << "    \"system_info\": {\n";
        
        auto it = system_info_.begin();
        for (const auto& [key, value] : system_info_) {
            file << "      \"" << key << "\": \"" << value << "\"";
            if (++it != system_info_.end()) file << ",";
            file << "\n";
            --it;
            ++it;
        }
        
        file << "    },\n";
        file << "    \"build_info\": {\n";
        
        it = build_info_.begin();
        for (const auto& [key, value] : build_info_) {
            file << "      \"" << key << "\": \"" << value << "\"";
            if (++it != build_info_.end()) file << ",";
            file << "\n";
            --it;
            ++it;
        }
        
        file << "    }\n";
        file << "  },\n";
        file << "  \"results\": [\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& result = results[i];
            
            file << "    {\n";
            file << "      \"benchmark_name\": \"" << result.benchmark_name << "\",\n";
            file << "      \"test_scenario\": \"" << result.test_scenario << "\",\n";
            file << "      \"timing\": {\n";
            file << "        \"avg_execution_time_ns\": " << result.avg_execution_time.count() << ",\n";
            file << "        \"min_execution_time_ns\": " << result.min_execution_time.count() << ",\n";
            file << "        \"max_execution_time_ns\": " << result.max_execution_time.count() << ",\n";
            file << "        \"median_execution_time_ns\": " << result.median_execution_time.count() << ",\n";
            file << "        \"std_dev_execution_time_ns\": " << result.std_dev_execution_time.count() << "\n";
            file << "      },\n";
            file << "      \"memory\": {\n";
            file << "        \"avg_memory_usage\": " << result.avg_memory_usage << ",\n";
            file << "        \"min_memory_usage\": " << result.min_memory_usage << ",\n";
            file << "        \"max_memory_usage\": " << result.max_memory_usage << ",\n";
            file << "        \"peak_allocation\": " << result.peak_allocation << "\n";
            file << "      },\n";
            file << "      \"quality\": {\n";
            file << "        \"formant_preservation_score\": " << result.formant_preservation_score << ",\n";
            file << "        \"pitch_accuracy_score\": " << result.pitch_accuracy_score << ",\n";
            file << "        \"spectral_fidelity_score\": " << result.spectral_fidelity_score << ",\n";
            file << "        \"overall_quality_score\": " << result.overall_quality_score << "\n";
            file << "      },\n";
            file << "      \"threading\": {\n";
            file << "        \"optimal_thread_count\": " << result.optimal_thread_count << ",\n";
            file << "        \"cpu_efficiency_score\": " << result.cpu_efficiency_score << ",\n";
            file << "        \"scalability_factor\": " << result.scalability_factor << "\n";
            file << "      },\n";
            file << "      \"benchmark_successful\": " << (result.benchmark_successful ? "true" : "false");
            
            if (!result.error_message.empty()) {
                file << ",\n      \"error_message\": \"" << result.error_message << "\"";
            }
            
            file << "\n    }";
            if (i < results.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool BenchmarkDataCollector::serialize_to_csv(const std::vector<BenchmarkResult>& results,
                                            const std::string& output_path) {
    try {
        std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
        
        std::ofstream file(output_path);
        if (!file.is_open()) {
            return false;
        }
        
        // Write CSV header
        file << "benchmark_name,test_scenario,";
        file << "avg_execution_time_ns,min_execution_time_ns,max_execution_time_ns,";
        file << "median_execution_time_ns,std_dev_execution_time_ns,";
        file << "avg_memory_usage,min_memory_usage,max_memory_usage,peak_allocation,";
        file << "formant_preservation_score,pitch_accuracy_score,spectral_fidelity_score,";
        file << "overall_quality_score,optimal_thread_count,cpu_efficiency_score,";
        file << "scalability_factor,benchmark_successful,error_message\n";
        
        // Write data rows
        for (const auto& result : results) {
            file << escape_csv_field(result.benchmark_name) << ",";
            file << escape_csv_field(result.test_scenario) << ",";
            file << result.avg_execution_time.count() << ",";
            file << result.min_execution_time.count() << ",";
            file << result.max_execution_time.count() << ",";
            file << result.median_execution_time.count() << ",";
            file << result.std_dev_execution_time.count() << ",";
            file << result.avg_memory_usage << ",";
            file << result.min_memory_usage << ",";
            file << result.max_memory_usage << ",";
            file << result.peak_allocation << ",";
            file << result.formant_preservation_score << ",";
            file << result.pitch_accuracy_score << ",";
            file << result.spectral_fidelity_score << ",";
            file << result.overall_quality_score << ",";
            file << result.optimal_thread_count << ",";
            file << result.cpu_efficiency_score << ",";
            file << result.scalability_factor << ",";
            file << (result.benchmark_successful ? "true" : "false") << ",";
            file << escape_csv_field(result.error_message) << "\n";
        }
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

HistoricalBenchmarkData BenchmarkDataCollector::create_historical_entry(
    const std::vector<BenchmarkResult>& results) const {
    
    HistoricalBenchmarkData data;
    data.timestamp = get_current_timestamp();
    data.git_commit_hash = get_git_commit_hash();
    data.results = results;
    
    // Populate system info
    std::stringstream system_info_str;
    for (const auto& [key, value] : system_info_) {
        system_info_str << key << "=" << value << ";";
    }
    data.system_info = system_info_str.str();
    
    // Populate build info
    std::stringstream build_info_str;
    for (const auto& [key, value] : build_info_) {
        build_info_str << key << "=" << value << ";";
    }
    data.build_configuration = build_info_str.str();
    
    // Set metadata
    data.test_environment = "integration_test";
    data.compiler_version = build_info_.count("compiler") ? 
        build_info_.at("compiler") + " " + build_info_.at("compiler_version") : "unknown";
    data.optimization_level = build_info_.count("build_type") ? 
        build_info_.at("build_type") : "unknown";
    
    return data;
}

std::string BenchmarkDataCollector::escape_csv_field(const std::string& field) const {
    if (field.find(',') != std::string::npos || 
        field.find('"') != std::string::npos || 
        field.find('\n') != std::string::npos) {
        
        std::string escaped = "\"";
        for (char c : field) {
            if (c == '"') {
                escaped += "\"\"";  // Escape quotes by doubling
            } else {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
    }
    return field;
}

std::vector<BenchmarkDataCollector::RegressionAlert> BenchmarkDataCollector::detect_regressions(
    const BenchmarkResult& current_result,
    const std::vector<HistoricalBenchmarkData>& baseline_data,
    double regression_threshold_percent) {
    
    std::vector<RegressionAlert> alerts;
    
    // Find baseline data for the same benchmark
    for (const auto& historical_data : baseline_data) {
        for (const auto& baseline_result : historical_data.results) {
            if (baseline_result.benchmark_name == current_result.benchmark_name &&
                baseline_result.benchmark_successful && current_result.benchmark_successful) {
                
                // Check execution time regression
                double time_regression_percent = 
                    (static_cast<double>(current_result.avg_execution_time.count()) - 
                     static_cast<double>(baseline_result.avg_execution_time.count())) * 100.0 /
                    static_cast<double>(baseline_result.avg_execution_time.count());
                
                if (time_regression_percent > regression_threshold_percent) {
                    RegressionAlert alert;
                    alert.benchmark_name = current_result.benchmark_name;
                    alert.metric_name = "avg_execution_time";
                    alert.current_value = static_cast<double>(current_result.avg_execution_time.count());
                    alert.baseline_value = static_cast<double>(baseline_result.avg_execution_time.count());
                    alert.regression_percent = time_regression_percent;
                    alert.timestamp = get_current_timestamp();
                    alert.git_commit = get_git_commit_hash();
                    
                    if (time_regression_percent > 50.0) {
                        alert.alert_severity = "critical";
                    } else if (time_regression_percent > 25.0) {
                        alert.alert_severity = "high";
                    } else if (time_regression_percent > 15.0) {
                        alert.alert_severity = "medium";
                    } else {
                        alert.alert_severity = "low";
                    }
                    
                    alerts.push_back(alert);
                }
                
                break; // Found matching baseline, no need to check others
            }
        }
    }
    
    return alerts;
}

} // namespace integration_test
} // namespace nexussynth
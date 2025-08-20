#pragma once

#include "performance_benchmark.h"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Data serialization formats for benchmark results
     */
    enum class SerializationFormat {
        JSON,
        CSV,
        XML,
        BINARY
    };

    /**
     * @brief Historical benchmark data for trend analysis
     */
    struct HistoricalBenchmarkData {
        std::string timestamp;
        std::string git_commit_hash;
        std::string system_info;
        std::string build_configuration;
        std::vector<BenchmarkResult> results;
        
        // Metadata
        std::string test_environment;
        std::string compiler_version;
        std::string optimization_level;
        std::map<std::string, std::string> system_specifications;
    };

    /**
     * @brief Benchmark database interface for persistent storage
     */
    class BenchmarkDatabase {
    public:
        virtual ~BenchmarkDatabase() = default;
        
        virtual bool initialize(const std::string& connection_string) = 0;
        virtual bool store_results(const HistoricalBenchmarkData& data) = 0;
        virtual std::vector<HistoricalBenchmarkData> query_results(
            const std::string& query_filter, 
            const std::string& time_range = "") = 0;
        virtual bool create_tables() = 0;
        virtual void close() = 0;
    };

    /**
     * @brief File-based benchmark database implementation
     */
    class FileBenchmarkDatabase : public BenchmarkDatabase {
    public:
        FileBenchmarkDatabase();
        ~FileBenchmarkDatabase() override;
        
        bool initialize(const std::string& base_directory) override;
        bool store_results(const HistoricalBenchmarkData& data) override;
        std::vector<HistoricalBenchmarkData> query_results(
            const std::string& query_filter, 
            const std::string& time_range = "") override;
        bool create_tables() override;
        void close() override;

    private:
        std::string base_directory_;
        bool initialized_ = false;
        
        std::string generate_filename(const std::string& timestamp, 
                                    SerializationFormat format) const;
        bool ensure_directory_exists(const std::string& directory) const;
    };

    /**
     * @brief Comprehensive benchmark data collector and analyzer
     */
    class BenchmarkDataCollector {
    public:
        BenchmarkDataCollector();
        explicit BenchmarkDataCollector(std::unique_ptr<BenchmarkDatabase> database);
        ~BenchmarkDataCollector();

        // Configuration
        void set_output_directory(const std::string& directory);
        void set_database(std::unique_ptr<BenchmarkDatabase> database);
        void enable_format(SerializationFormat format, bool enabled = true);

        // Data collection
        bool collect_system_info();
        bool collect_build_info();
        std::string get_current_timestamp() const;
        std::string get_git_commit_hash() const;

        // Storage operations
        bool save_results(const std::vector<BenchmarkResult>& results,
                         const std::string& output_path = "");
        bool save_historical_data(const HistoricalBenchmarkData& data);
        
        // Serialization
        bool serialize_to_json(const std::vector<BenchmarkResult>& results,
                              const std::string& output_path);
        bool serialize_to_csv(const std::vector<BenchmarkResult>& results,
                             const std::string& output_path);
        bool serialize_historical_to_json(const HistoricalBenchmarkData& data,
                                         const std::string& output_path);

        // Deserialization  
        std::vector<BenchmarkResult> deserialize_from_json(const std::string& input_path);
        HistoricalBenchmarkData deserialize_historical_from_json(const std::string& input_path);

        // Query and analysis
        std::vector<HistoricalBenchmarkData> query_historical_data(
            const std::string& benchmark_name_filter = "",
            const std::string& time_range = "",
            int max_results = 100);

        HistoricalBenchmarkData create_historical_entry(
            const std::vector<BenchmarkResult>& results) const;

        // Trend analysis
        struct TrendAnalysis {
            std::string benchmark_name;
            double performance_trend_percent; // Positive = improving, negative = degrading
            double quality_trend_percent;
            bool statistically_significant;
            size_t data_points_analyzed;
            std::string confidence_interval;
        };

        std::vector<TrendAnalysis> analyze_performance_trends(
            const std::vector<HistoricalBenchmarkData>& historical_data,
            const std::string& benchmark_filter = "");

        // Regression detection
        struct RegressionAlert {
            std::string benchmark_name;
            std::string metric_name;
            double current_value;
            double baseline_value;
            double regression_percent;
            std::string alert_severity; // "low", "medium", "high", "critical"
            std::string timestamp;
            std::string git_commit;
        };

        std::vector<RegressionAlert> detect_regressions(
            const BenchmarkResult& current_result,
            const std::vector<HistoricalBenchmarkData>& baseline_data,
            double regression_threshold_percent = 10.0);

        // Export utilities
        bool export_to_spreadsheet(const std::vector<BenchmarkResult>& results,
                                  const std::string& output_path);
        bool export_comparison_chart(const std::vector<BenchmarkResult>& current_results,
                                   const std::vector<BenchmarkResult>& baseline_results,
                                   const std::string& output_path);

    private:
        std::unique_ptr<BenchmarkDatabase> database_;
        std::string output_directory_;
        std::set<SerializationFormat> enabled_formats_;
        
        std::map<std::string, std::string> system_info_;
        std::map<std::string, std::string> build_info_;
        
        // Helper methods
        std::string format_duration(std::chrono::nanoseconds duration) const;
        std::string format_memory(size_t bytes) const;
        std::string escape_csv_field(const std::string& field) const;
        bool write_json_field(std::ofstream& file, const std::string& key, 
                             const std::string& value, bool is_last = false) const;
        bool write_json_array_start(std::ofstream& file, const std::string& key) const;
        bool write_json_array_end(std::ofstream& file, bool is_last = false) const;
        
        double calculate_linear_trend(const std::vector<double>& values) const;
        double calculate_correlation(const std::vector<double>& x, 
                                   const std::vector<double>& y) const;
        std::pair<double, double> calculate_confidence_interval(
            const std::vector<double>& data, double confidence_level = 0.95) const;
    };

} // namespace integration_test  
} // namespace nexussynth
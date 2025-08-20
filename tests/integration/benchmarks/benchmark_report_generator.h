#pragma once

#include "performance_benchmark.h"
#include "benchmark_data_collector.h"
#include <string>
#include <vector>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Performance report configuration
     */
    struct ReportConfig {
        std::string title = "NexusSynth Performance Benchmark Report";
        std::string subtitle = "Comprehensive Performance Analysis";
        bool include_charts = true;
        bool include_detailed_tables = true;
        bool include_statistical_analysis = true;
        bool include_trend_analysis = true;
        bool include_comparison_charts = true;
        std::string template_path = "";
        std::string output_format = "html"; // "html", "pdf", "markdown"
    };

    /**
     * @brief Chart configuration for visualizations
     */
    struct ChartConfig {
        std::string chart_type; // "bar", "line", "scatter", "box_plot"
        std::string title;
        std::string x_axis_label;
        std::string y_axis_label;
        std::vector<std::string> data_series;
        std::string color_scheme = "default";
        bool enable_interactive = true;
        int width = 800;
        int height = 400;
    };

    /**
     * @brief Comprehensive benchmark report generator
     */
    class BenchmarkReportGenerator {
    public:
        BenchmarkReportGenerator();
        explicit BenchmarkReportGenerator(const ReportConfig& config);
        ~BenchmarkReportGenerator();

        // Configuration
        void set_config(const ReportConfig& config);
        void set_template_path(const std::string& template_path);

        // HTML Report Generation
        bool generate_html_report(const std::vector<BenchmarkResult>& results,
                                 const std::string& output_path);
        
        bool generate_comparison_html_report(
            const std::vector<BenchmarkResult>& current_results,
            const std::vector<BenchmarkResult>& baseline_results,
            const std::string& output_path,
            const std::string& comparison_title = "Performance Comparison");

        bool generate_trend_analysis_report(
            const std::vector<HistoricalBenchmarkData>& historical_data,
            const std::string& output_path);

        // PDF Report Generation (requires external dependency)
        bool generate_pdf_report(const std::vector<BenchmarkResult>& results,
                                const std::string& output_path);

        // Markdown Report Generation
        bool generate_markdown_report(const std::vector<BenchmarkResult>& results,
                                    const std::string& output_path);

        // Chart Generation
        std::string generate_performance_chart(const std::vector<BenchmarkResult>& results,
                                             const ChartConfig& config);
        
        std::string generate_memory_usage_chart(const std::vector<BenchmarkResult>& results);
        std::string generate_quality_metrics_chart(const std::vector<BenchmarkResult>& results);
        std::string generate_scalability_chart(const std::vector<BenchmarkResult>& results);
        std::string generate_comparison_chart(const std::vector<BenchmarkResult>& current,
                                            const std::vector<BenchmarkResult>& baseline);

        // Table Generation
        std::string generate_summary_table(const std::vector<BenchmarkResult>& results);
        std::string generate_detailed_timing_table(const std::vector<BenchmarkResult>& results);
        std::string generate_memory_analysis_table(const std::vector<BenchmarkResult>& results);
        std::string generate_quality_analysis_table(const std::vector<BenchmarkResult>& results);
        std::string generate_regression_analysis_table(
            const std::vector<BenchmarkDataCollector::RegressionAlert>& alerts);

        // Statistical Analysis Sections
        std::string generate_statistical_summary(const std::vector<BenchmarkResult>& results);
        std::string generate_outlier_analysis(const std::vector<BenchmarkResult>& results);
        std::string generate_confidence_intervals(const std::vector<BenchmarkResult>& results);

        // Executive Summary Generation
        std::string generate_executive_summary(const std::vector<BenchmarkResult>& results,
                                             const std::vector<BenchmarkResult>& baseline = {});

        // Utility Methods
        bool save_standalone_chart(const std::string& chart_html,
                                  const std::string& output_path);
        
        std::string get_performance_recommendation(const std::vector<BenchmarkResult>& results);
        std::string format_benchmark_duration(std::chrono::nanoseconds duration);
        std::string format_memory_size(size_t bytes);
        std::string get_performance_grade(double score);

    private:
        ReportConfig config_;
        
        // HTML generation helpers
        std::string generate_html_header(const std::string& title);
        std::string generate_html_footer();
        std::string generate_css_styles();
        std::string generate_javascript_libraries();
        
        // Chart generation helpers (using Chart.js or similar)
        std::string generate_chart_js_data(const std::vector<BenchmarkResult>& results,
                                         const std::string& metric_name);
        std::string generate_chart_configuration(const ChartConfig& config);
        std::string wrap_chart_in_canvas(const std::string& chart_id, const std::string& chart_script);
        
        // Table generation helpers
        std::string generate_table_header(const std::vector<std::string>& headers);
        std::string generate_table_row(const std::vector<std::string>& cells, bool is_header = false);
        std::string wrap_table(const std::string& table_content, const std::string& table_id = "");
        
        // Statistical helpers
        double calculate_performance_score(const BenchmarkResult& result);
        std::string classify_performance_level(double score);
        std::vector<BenchmarkResult> identify_outliers(const std::vector<BenchmarkResult>& results);
        
        // Color and styling helpers
        std::string get_color_for_metric(const std::string& metric_name, double value);
        std::string get_status_color(bool successful);
        std::string get_trend_arrow(double change_percent);
        
        // Data transformation helpers
        std::vector<std::pair<std::string, double>> extract_metric_series(
            const std::vector<BenchmarkResult>& results, const std::string& metric_name);
        std::map<std::string, std::vector<double>> group_results_by_benchmark(
            const std::vector<BenchmarkResult>& results);
    };

    /**
     * @brief Performance dashboard generator for continuous monitoring
     */
    class PerformanceDashboard {
    public:
        PerformanceDashboard();
        ~PerformanceDashboard();

        // Dashboard configuration
        void set_refresh_interval(int seconds);
        void set_data_retention_days(int days);
        void add_benchmark_filter(const std::string& benchmark_name);

        // Dashboard generation
        bool generate_live_dashboard(const std::string& output_path,
                                   const std::string& data_source_path = "");
        
        bool update_dashboard_data(const std::vector<BenchmarkResult>& new_results);
        
        // Real-time monitoring
        bool start_monitoring(const std::string& benchmark_directory);
        void stop_monitoring();
        bool is_monitoring() const;

        // Alert configuration
        void set_performance_threshold(const std::string& benchmark_name,
                                     const std::string& metric_name,
                                     double threshold_value,
                                     const std::string& comparison_operator = ">");

        void set_regression_alert_threshold(double percentage);
        
        std::vector<std::string> check_alerts(const std::vector<BenchmarkResult>& results);

    private:
        int refresh_interval_seconds_ = 30;
        int data_retention_days_ = 30;
        bool monitoring_active_ = false;
        
        std::vector<std::string> benchmark_filters_;
        std::map<std::string, std::pair<std::string, std::pair<double, std::string>>> performance_thresholds_;
        double regression_alert_threshold_ = 10.0; // 10% regression threshold
        
        std::string generate_dashboard_html();
        std::string generate_dashboard_javascript();
        bool update_dashboard_json_data(const std::vector<BenchmarkResult>& results,
                                       const std::string& json_path);
    };

} // namespace integration_test
} // namespace nexussynth
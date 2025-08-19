#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "performance_profiler.h"
#include "quality_metrics.h"
#include "pbp_synthesis_engine.h"
#include "streaming_buffer_manager.h"

namespace nexussynth {
namespace synthesis {

/**
 * @brief Configuration for synthesis quality validation
 */
struct ValidationConfig {
    // Performance profiling settings
    ProfilingConfig profiling_config;
    
    // Quality measurement settings
    bool enable_mcd_measurement = true;
    bool enable_f0_rmse_measurement = true;
    bool enable_spectral_correlation = true;
    bool enable_real_time_validation = true;
    
    // Validation thresholds
    double acceptable_mcd_threshold = 6.0;
    double acceptable_f0_rmse_threshold = 25.0;
    double acceptable_spectral_correlation_threshold = 0.75;
    double acceptable_real_time_factor_threshold = 1.0;
    
    // Test parameters
    double validation_duration_seconds = 30.0;
    std::string reference_audio_dir;
    std::string test_audio_dir;
    
    ValidationConfig() = default;
};

/**
 * @brief Comprehensive validation results
 */
struct ValidationResults {
    // Performance metrics
    PerformanceReport performance_report;
    
    // Quality metrics
    NexusSynth::ValidationReport quality_report;
    
    // Integrated analysis
    double overall_score = 0.0;
    std::string quality_assessment;
    std::string performance_assessment;
    std::string overall_recommendation;
    
    std::vector<std::string> issues_found;
    std::vector<std::string> improvement_suggestions;
    
    bool validation_passed = false;
    std::string validation_timestamp;
    
    void save_comprehensive_report(const std::string& output_dir) const;
    std::string generate_executive_summary() const;
};

/**
 * @brief Comprehensive synthesis quality and performance validator
 */
class SynthesisQualityValidator {
public:
    explicit SynthesisQualityValidator(const ValidationConfig& config = ValidationConfig{});
    ~SynthesisQualityValidator();
    
    // Primary validation methods
    ValidationResults validate_synthesis_engine(
        PbpSynthesisEngine& engine,
        const std::vector<AudioParameters>& test_cases
    );
    
    ValidationResults validate_streaming_performance(
        StreamingBufferManager& buffer_manager,
        PbpSynthesisEngine& engine,
        const std::vector<AudioParameters>& test_cases
    );
    
    ValidationResults run_comprehensive_validation(
        PbpSynthesisEngine& engine,
        StreamingBufferManager& buffer_manager,
        const std::string& test_data_dir
    );
    
    // Real-time validation for live monitoring
    bool start_continuous_validation(
        PbpSynthesisEngine& engine,
        StreamingBufferManager& buffer_manager
    );
    
    void stop_continuous_validation();
    ValidationResults get_continuous_validation_results() const;
    
    // Specialized validation tests
    ValidationResults validate_latency_performance(
        StreamingBufferManager& buffer_manager,
        const std::vector<double>& target_latencies_ms
    );
    
    ValidationResults validate_quality_consistency(
        PbpSynthesisEngine& engine,
        const AudioParameters& reference_params,
        int iterations = 100
    );
    
    ValidationResults validate_stress_performance(
        PbpSynthesisEngine& engine,
        StreamingBufferManager& buffer_manager,
        int concurrent_streams = 4,
        double duration_seconds = 60.0
    );
    
    // Comparative analysis
    ValidationResults compare_against_baseline(
        const ValidationResults& current_results,
        const std::string& baseline_report_file
    );
    
    // Configuration and utilities
    bool update_config(const ValidationConfig& new_config);
    const ValidationConfig& get_config() const { return config_; }
    
    // Static utility methods
    static ValidationResults load_validation_results(const std::string& filepath);
    static bool save_validation_results(const ValidationResults& results, const std::string& filepath);
    
    // Automated test suite generation
    static std::vector<AudioParameters> generate_test_cases(
        const std::string& voice_bank_dir,
        int max_test_cases = 50
    );

private:
    ValidationConfig config_;
    
    // Internal validation components
    std::unique_ptr<PerformanceProfiler> profiler_;
    std::unique_ptr<NexusSynth::QualityEvaluator> quality_evaluator_;
    
    // Continuous validation state
    std::atomic<bool> continuous_validation_active_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::unique_ptr<std::thread> validation_thread_;
    mutable std::mutex validation_mutex_;
    ValidationResults continuous_results_;
    
    // Reference data for quality comparison
    std::vector<AudioParameters> reference_audio_params_;
    std::vector<std::vector<double>> reference_audio_samples_;
    
    // Internal helper methods
    ValidationResults create_validation_results() const;
    void initialize_reference_data(const std::string& reference_dir);
    
    // Quality validation helpers
    NexusSynth::QualityMetrics measure_synthesis_quality(
        PbpSynthesisEngine& engine,
        const AudioParameters& test_params,
        const AudioParameters& reference_params
    );
    
    // Performance validation helpers
    PerformanceMetrics measure_synthesis_performance(
        PbpSynthesisEngine& engine,
        const std::vector<AudioParameters>& test_cases
    );
    
    PerformanceMetrics measure_streaming_performance(
        StreamingBufferManager& buffer_manager,
        PbpSynthesisEngine& engine,
        const std::vector<AudioParameters>& test_cases
    );
    
    // Analysis and scoring
    double calculate_overall_score(
        const PerformanceReport& performance_report,
        const NexusSynth::ValidationReport& quality_report
    ) const;
    
    std::string assess_quality_level(const NexusSynth::ValidationReport& quality_report) const;
    std::string assess_performance_level(const PerformanceReport& performance_report) const;
    
    std::vector<std::string> identify_issues(
        const PerformanceReport& performance_report,
        const NexusSynth::ValidationReport& quality_report
    ) const;
    
    std::vector<std::string> generate_improvement_suggestions(
        const PerformanceReport& performance_report,
        const NexusSynth::ValidationReport& quality_report
    ) const;
    
    // Continuous validation thread
    void continuous_validation_thread_main(
        PbpSynthesisEngine& engine,
        StreamingBufferManager& buffer_manager
    );
    
    // Test case management
    AudioParameters create_test_case(
        double f0_hz,
        double duration_seconds,
        const std::string& phoneme_sequence = "a"
    ) const;
    
    std::vector<AudioParameters> create_stress_test_cases() const;
    std::vector<AudioParameters> create_quality_test_cases() const;
    std::vector<AudioParameters> create_latency_test_cases() const;
    
    // File I/O helpers
    bool save_audio_parameters(const AudioParameters& params, const std::string& filepath) const;
    AudioParameters load_audio_parameters(const std::string& filepath) const;
    
    // Validation criteria checking
    bool meets_quality_criteria(const NexusSynth::QualityMetrics& metrics) const;
    bool meets_performance_criteria(const PerformanceMetrics& metrics) const;
    bool meets_overall_criteria(const ValidationResults& results) const;
};

/**
 * @brief Utility functions for validation and testing
 */
namespace ValidationUtils {
    // Test data generation
    std::vector<AudioParameters> generate_voice_range_test_cases(
        double min_f0_hz = 80.0,
        double max_f0_hz = 400.0,
        int num_cases = 20
    );
    
    std::vector<AudioParameters> generate_phoneme_diversity_test_cases(
        const std::vector<std::string>& phonemes,
        double base_f0_hz = 150.0
    );
    
    std::vector<AudioParameters> generate_duration_test_cases(
        double min_duration_s = 0.1,
        double max_duration_s = 5.0,
        int num_cases = 15
    );
    
    // Statistical analysis
    struct ValidationStatistics {
        double mean_quality_score;
        double std_quality_score;
        double mean_performance_score;
        double std_performance_score;
        double correlation_quality_performance;
        int total_tests;
        int passed_tests;
        double success_rate;
    };
    
    ValidationStatistics analyze_validation_batch(
        const std::vector<ValidationResults>& results
    );
    
    // Report generation
    void generate_validation_dashboard(
        const std::vector<ValidationResults>& results,
        const std::string& output_html_file
    );
    
    void generate_trend_analysis(
        const std::vector<ValidationResults>& historical_results,
        const std::string& output_csv_file
    );
    
    // Configuration presets
    ValidationConfig create_development_config();
    ValidationConfig create_production_config();
    ValidationConfig create_benchmark_config();
    ValidationConfig create_continuous_integration_config();
    
    // Automated testing workflows
    class AutomatedTestSuite {
    public:
        explicit AutomatedTestSuite(const ValidationConfig& config);
        
        void add_test_case(const std::string& name, const AudioParameters& params);
        void add_test_directory(const std::string& test_dir);
        
        std::vector<ValidationResults> run_full_test_suite(
            PbpSynthesisEngine& engine,
            StreamingBufferManager& buffer_manager
        );
        
        bool run_regression_test(
            PbpSynthesisEngine& engine,
            StreamingBufferManager& buffer_manager,
            const std::string& baseline_results_dir
        );
        
        void generate_test_report(
            const std::vector<ValidationResults>& results,
            const std::string& output_dir
        );
        
    private:
        ValidationConfig config_;
        std::vector<std::pair<std::string, AudioParameters>> test_cases_;
    };
}

} // namespace synthesis
} // namespace nexussynth
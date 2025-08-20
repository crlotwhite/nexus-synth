#pragma once

#include "../utils/audio_comparator.h"
#include "../utils/quality_analyzer.h"
#include <vector>
#include <string>
#include <chrono>
#include <memory>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Advanced audio quality metrics for A/B testing
     */
    struct AdvancedQualityMetrics {
        bool measurement_successful = false;
        
        // Basic metrics from AudioComparator
        double snr_db = 0.0;
        double similarity_score = 0.0;
        double spectral_similarity = 0.0;
        
        // Advanced metrics for A/B comparison
        double mel_cepstral_distortion = 0.0;  // MCD
        double f0_rmse = 0.0;                  // Fundamental frequency error
        double spectral_distortion = 0.0;     // Spectral envelope distortion
        double formant_deviation = 0.0;       // Formant preservation metric
        double phase_coherence = 0.0;         // Phase relationship quality
        double roughness_score = 0.0;         // Perceptual roughness measure
        double brightness_score = 0.0;        // High-frequency energy preservation
        
        // Statistical measures
        double mean_square_error = 0.0;
        double peak_signal_noise_ratio = 0.0;
        double structural_similarity_index = 0.0;
        
        std::string error_message;
    };

    /**
     * @brief Result of A/B comparison between two resamplers
     */
    struct ABComparisonResult {
        bool comparison_successful = false;
        
        // Compared systems
        std::string system_a_name;
        std::string system_b_name;
        
        // Individual quality metrics
        AdvancedQualityMetrics system_a_metrics;
        AdvancedQualityMetrics system_b_metrics;
        
        // Comparative analysis
        double overall_quality_difference = 0.0;  // Positive = A better, Negative = B better
        double statistical_significance = 0.0;    // p-value
        std::string winner;                        // "A", "B", or "tie"
        
        // Per-metric comparisons
        struct MetricComparison {
            std::string metric_name;
            double system_a_value;
            double system_b_value;
            double difference;
            double confidence_interval;
            bool statistically_significant;
        };
        std::vector<MetricComparison> metric_comparisons;
        
        // Performance metrics
        std::chrono::milliseconds system_a_render_time;
        std::chrono::milliseconds system_b_render_time;
        size_t system_a_memory_usage;
        size_t system_b_memory_usage;
        
        std::string detailed_report;
        std::string error_message;
    };

    /**
     * @brief Configuration for A/B comparison tests
     */
    struct ABComparisonConfig {
        struct SystemConfig {
            std::string name;
            std::string executable_path;
            std::vector<std::string> command_args;
            std::string output_format;
        };
        
        SystemConfig system_a;
        SystemConfig system_b;
        
        // Test parameters
        std::vector<std::string> test_voice_banks;
        std::vector<std::string> test_scenarios;
        int repetitions_per_test = 5;
        double significance_threshold = 0.05;
        
        // Quality thresholds
        double min_acceptable_snr = 20.0;
        double max_acceptable_mcd = 10.0;
        double max_acceptable_f0_rmse = 50.0;
    };

    /**
     * @brief A/B comparison framework for objective quality assessment
     */
    class ABComparator {
    public:
        ABComparator();
        ~ABComparator();

        // Configuration
        bool load_config(const std::string& config_file);
        void set_config(const ABComparisonConfig& config);

        // Single test comparison
        ABComparisonResult compare_single_test(
            const std::string& test_input,
            const std::string& reference_audio = "");

        // Batch comparison
        std::vector<ABComparisonResult> compare_batch(
            const std::vector<std::string>& test_inputs,
            const std::string& output_report_path = "");

        // Advanced metrics calculation
        AdvancedQualityMetrics calculate_advanced_metrics(
            const std::string& audio_file,
            const std::string& reference_file = "");

        // Statistical analysis
        bool perform_statistical_analysis(
            const std::vector<ABComparisonResult>& results,
            std::string& analysis_report);

        // Report generation
        bool generate_html_report(
            const std::vector<ABComparisonResult>& results,
            const std::string& output_path);
        
        bool generate_csv_report(
            const std::vector<ABComparisonResult>& results,
            const std::string& output_path);

    private:
        std::unique_ptr<AudioComparator> audio_comparator_;
        std::unique_ptr<QualityAnalyzer> quality_analyzer_;
        ABComparisonConfig config_;
        
        // Advanced metric calculation helpers
        double calculate_mel_cepstral_distortion(const std::vector<float>& audio1, 
                                                const std::vector<float>& audio2);
        double calculate_f0_rmse(const std::vector<float>& audio1, 
                                const std::vector<float>& audio2);
        double calculate_spectral_distortion(const std::vector<float>& audio1, 
                                           const std::vector<float>& audio2);
        double calculate_formant_deviation(const std::vector<float>& audio1, 
                                         const std::vector<float>& audio2);
        
        // Statistical helpers
        double calculate_t_test(const std::vector<double>& group1, 
                               const std::vector<double>& group2);
        std::pair<double, double> calculate_confidence_interval(
            const std::vector<double>& data, double confidence_level = 0.95);

        // System execution helpers
        bool execute_resampler(const ABComparisonConfig::SystemConfig& system, 
                              const std::string& input_file,
                              const std::string& output_file);
    };

} // namespace integration_test
} // namespace nexussynth
#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Result of synthesis quality analysis
     */
    struct QualityAnalysisResult {
        bool analysis_successful = false;
        double overall_score = 0.0;
        double similarity_score = 0.0;
        double snr_db = 0.0;
        double spectral_similarity = 0.0;
        double duration_accuracy = 0.0;
        double frequency_response_score = 0.0;
        double distortion_score = 0.0;
        std::string error_message;
    };

    /**
     * @brief Result of conversion quality validation
     */
    struct ConversionQualityResult {
        bool validation_successful = false;
        double overall_quality_score = 0.0;
        double file_integrity_score = 0.0;
        double model_completeness_score = 0.0;
        double compression_efficiency = 0.0;
        size_t phoneme_count = 0;
        size_t model_count = 0;
        std::string error_message;
    };

    /**
     * @brief Comprehensive quality report
     */
    struct QualityReport {
        std::chrono::system_clock::time_point timestamp;
        size_t total_tests = 0;
        size_t passed_tests = 0;
        double overall_pass_rate = 0.0;
        
        // Synthesis metrics
        double synthesis_pass_rate = 0.0;
        double average_synthesis_score = 0.0;
        
        // Conversion metrics  
        double conversion_pass_rate = 0.0;
        double average_conversion_score = 0.0;
        
        // Performance metrics
        double average_synthesis_time_ms = 0.0;
        double average_conversion_time_ms = 0.0;
        
        // Recommendations
        std::vector<std::string> recommendations;
    };

    /**
     * @brief Quality analyzer for integration test results
     */
    class QualityAnalyzer {
    public:
        QualityAnalyzer();
        ~QualityAnalyzer();

        // Analysis methods
        bool analyze_synthesis_quality(const std::string& output_file,
                                     const std::string& reference_file,
                                     QualityAnalysisResult& result);

        bool validate_conversion_output(const std::string& nvm_file,
                                      ConversionQualityResult& result);

        // Report generation
        QualityReport generate_quality_report(
            const std::vector<QualityAnalysisResult>& synthesis_results,
            const std::vector<ConversionQualityResult>& conversion_results);

    private:
        // Helper methods would go here
    };

} // namespace integration_test
} // namespace nexussynth
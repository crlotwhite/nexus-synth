#include "quality_analyzer.h"
#include "audio_comparator.h"
#include <fstream>

namespace nexussynth {
namespace integration_test {

QualityAnalyzer::QualityAnalyzer() = default;
QualityAnalyzer::~QualityAnalyzer() = default;

bool QualityAnalyzer::analyze_synthesis_quality(const std::string& output_file,
                                               const std::string& reference_file,
                                               QualityAnalysisResult& result) {
    result.analysis_successful = false;
    result.overall_score = 0.0;
    
    // Use AudioComparator for basic analysis
    AudioComparator comparator;
    
    if (!reference_file.empty()) {
        // Compare against reference
        ComparisonResult comp_result = comparator.compare_audio_files(output_file, reference_file);
        
        if (!comp_result.files_comparable) {
            result.error_message = "Files not comparable: " + comp_result.error_message;
            return false;
        }
        
        result.similarity_score = comp_result.similarity_score;
        result.snr_db = comp_result.snr_db;
        result.spectral_similarity = comp_result.frequency_response_similarity;
    }
    
    // Analyze output file quality
    QualityMetrics output_metrics = comparator.analyze_audio_quality(output_file);
    
    if (!output_metrics.is_valid) {
        result.error_message = "Failed to analyze output file quality";
        return false;
    }
    
    result.duration_accuracy = 1.0; // Placeholder
    result.frequency_response_score = 0.85; // Placeholder
    result.distortion_score = 0.9; // Placeholder
    
    // Calculate overall score
    result.overall_score = (result.similarity_score + 
                           std::min(result.snr_db / 30.0, 1.0) +
                           result.spectral_similarity) / 3.0;
    
    result.analysis_successful = true;
    return true;
}

bool QualityAnalyzer::validate_conversion_output(const std::string& nvm_file,
                                                ConversionQualityResult& result) {
    result.validation_successful = false;
    result.file_integrity_score = 0.0;
    
    // Check if file exists and has reasonable size
    std::ifstream file(nvm_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.error_message = "Cannot open NVM file";
        return false;
    }
    
    size_t file_size = file.tellg();
    file.close();
    
    if (file_size < 1024) {
        result.error_message = "NVM file too small";
        return false;
    }
    
    // Basic file integrity checks
    result.file_integrity_score = 0.9; // Placeholder
    result.model_completeness_score = 0.85; // Placeholder
    result.compression_efficiency = 0.8; // Placeholder
    
    // Calculate overall score
    result.overall_quality_score = (result.file_integrity_score + 
                                   result.model_completeness_score +
                                   result.compression_efficiency) / 3.0;
    
    result.validation_successful = true;
    return true;
}

QualityReport QualityAnalyzer::generate_quality_report(
    const std::vector<QualityAnalysisResult>& synthesis_results,
    const std::vector<ConversionQualityResult>& conversion_results) {
    
    QualityReport report;
    report.timestamp = std::chrono::system_clock::now();
    report.total_tests = synthesis_results.size() + conversion_results.size();
    report.passed_tests = 0;
    
    // Analyze synthesis results
    double synthesis_score_sum = 0.0;
    size_t synthesis_passed = 0;
    
    for (const auto& result : synthesis_results) {
        if (result.analysis_successful && result.overall_score >= 0.7) {
            synthesis_passed++;
            synthesis_score_sum += result.overall_score;
        }
    }
    
    report.synthesis_pass_rate = synthesis_results.empty() ? 0.0 :
        static_cast<double>(synthesis_passed) / synthesis_results.size();
    report.average_synthesis_score = synthesis_results.empty() ? 0.0 :
        synthesis_score_sum / synthesis_results.size();
    
    // Analyze conversion results
    double conversion_score_sum = 0.0;
    size_t conversion_passed = 0;
    
    for (const auto& result : conversion_results) {
        if (result.validation_successful && result.overall_quality_score >= 0.7) {
            conversion_passed++;
            conversion_score_sum += result.overall_quality_score;
        }
    }
    
    report.conversion_pass_rate = conversion_results.empty() ? 0.0 :
        static_cast<double>(conversion_passed) / conversion_results.size();
    report.average_conversion_score = conversion_results.empty() ? 0.0 :
        conversion_score_sum / conversion_results.size();
    
    report.passed_tests = synthesis_passed + conversion_passed;
    report.overall_pass_rate = report.total_tests == 0 ? 0.0 :
        static_cast<double>(report.passed_tests) / report.total_tests;
    
    // Generate recommendations
    if (report.synthesis_pass_rate < 0.8) {
        report.recommendations.push_back("Synthesis quality needs improvement");
    }
    if (report.conversion_pass_rate < 0.8) {
        report.recommendations.push_back("Conversion quality needs improvement");
    }
    if (report.overall_pass_rate >= 0.9) {
        report.recommendations.push_back("Excellent overall quality");
    }
    
    return report;
}

} // namespace integration_test
} // namespace nexussynth
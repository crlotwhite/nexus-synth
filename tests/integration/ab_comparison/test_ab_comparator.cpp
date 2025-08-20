#include <gtest/gtest.h>
#include "ab_comparator.h"
#include "../utils/test_data_manager.h"
#include <filesystem>
#include <fstream>

namespace nexussynth {
namespace integration_test {

class ABComparatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string test_data_dir = std::filesystem::current_path() / "test_data";
        ASSERT_TRUE(test_data_manager_.initialize(test_data_dir));
        ASSERT_TRUE(test_data_manager_.setup_test_environment());
        
        comparator_ = std::make_unique<ABComparator>();
        
        // Set up basic configuration
        ABComparisonConfig config;
        config.system_a.name = "NexusSynth";
        config.system_a.executable_path = "nexussynth";
        config.system_a.command_args = {"synthesize", "--input", "{INPUT}", "--output", "{OUTPUT}"};
        
        config.system_b.name = "TestResampler";
        config.system_b.executable_path = "test_resampler";
        config.system_b.command_args = {"{INPUT}", "{OUTPUT}"};
        
        config.repetitions_per_test = 3;
        config.significance_threshold = 0.05;
        
        comparator_->set_config(config);
    }
    
    void TearDown() override {
        test_data_manager_.cleanup_test_environment();
    }
    
    TestDataManager test_data_manager_;
    std::unique_ptr<ABComparator> comparator_;
};

TEST_F(ABComparatorTest, ConfigurationLoading) {
    // Test configuration loading functionality
    
    // Create a temporary config file
    std::string config_path = test_data_manager_.create_temp_file(".json");
    std::ofstream config_file(config_path);
    
    // Write minimal JSON config (simplified for testing)
    config_file << R"({
        "system_a": {"name": "SystemA"},
        "system_b": {"name": "SystemB"},
        "test_parameters": {"repetitions_per_test": 10}
    })";
    config_file.close();
    
    ABComparator test_comparator;
    EXPECT_TRUE(test_comparator.load_config(config_path));
    
    // Test with non-existent file
    EXPECT_FALSE(test_comparator.load_config("non_existent_file.json"));
}

TEST_F(ABComparatorTest, AdvancedMetricsCalculation) {
    // Test advanced audio quality metrics calculation
    
    std::string test_audio = test_data_manager_.create_temp_file(".wav");
    std::string reference_audio = test_data_manager_.create_temp_file(".wav");
    
    AdvancedQualityMetrics metrics = comparator_->calculate_advanced_metrics(
        test_audio, reference_audio);
    
    EXPECT_TRUE(metrics.measurement_successful);
    EXPECT_GE(metrics.snr_db, 0.0);
    EXPECT_GE(metrics.similarity_score, 0.0);
    EXPECT_LE(metrics.similarity_score, 1.0);
    EXPECT_GE(metrics.mel_cepstral_distortion, 0.0);
    EXPECT_GE(metrics.f0_rmse, 0.0);
    EXPECT_GE(metrics.spectral_distortion, 0.0);
    EXPECT_GE(metrics.formant_deviation, 0.0);
}

TEST_F(ABComparatorTest, SingleComparisonExecution) {
    // Test single A/B comparison execution
    
    std::string test_input = test_data_manager_.create_temp_file(".wav");
    std::string reference_audio = test_data_manager_.create_temp_file(".wav");
    
    ABComparisonResult result = comparator_->compare_single_test(test_input, reference_audio);
    
    EXPECT_TRUE(result.comparison_successful) << "Comparison should succeed: " << result.error_message;
    EXPECT_FALSE(result.system_a_name.empty());
    EXPECT_FALSE(result.system_b_name.empty());
    EXPECT_TRUE(result.system_a_metrics.measurement_successful);
    EXPECT_TRUE(result.system_b_metrics.measurement_successful);
    EXPECT_GT(result.system_a_render_time.count(), 0);
    EXPECT_GT(result.system_b_render_time.count(), 0);
    EXPECT_FALSE(result.detailed_report.empty());
    
    // Winner should be one of the systems or "tie"
    EXPECT_TRUE(result.winner == result.system_a_name || 
                result.winner == result.system_b_name || 
                result.winner == "tie");
}

TEST_F(ABComparatorTest, BatchComparisonExecution) {
    // Test batch A/B comparison execution
    
    std::vector<std::string> test_inputs;
    for (int i = 0; i < 3; ++i) {
        test_inputs.push_back(test_data_manager_.create_temp_file(".wav"));
    }
    
    std::string temp_dir = std::filesystem::temp_directory_path();
    std::string output_path = temp_dir + "/batch_reports";
    std::vector<ABComparisonResult> results = comparator_->compare_batch(test_inputs, output_path);
    
    // Should have results for each input * repetitions
    EXPECT_GE(results.size(), test_inputs.size());
    
    // All results should be successful
    for (const auto& result : results) {
        EXPECT_TRUE(result.comparison_successful) << "Result error: " << result.error_message;
    }
    
    // Check if report files were generated
    EXPECT_TRUE(std::filesystem::exists(output_path + ".html"));
    EXPECT_TRUE(std::filesystem::exists(output_path + ".csv"));
}

TEST_F(ABComparatorTest, StatisticalAnalysis) {
    // Test statistical analysis functionality
    
    // Create mock comparison results
    std::vector<ABComparisonResult> results;
    
    for (int i = 0; i < 10; ++i) {
        ABComparisonResult result;
        result.comparison_successful = true;
        result.system_a_name = "SystemA";
        result.system_b_name = "SystemB";
        
        // Create varied metrics for meaningful statistical analysis
        result.system_a_metrics.snr_db = 20.0 + (i % 3);
        result.system_a_metrics.similarity_score = 0.8 + (i % 5) * 0.02;
        result.system_b_metrics.snr_db = 18.0 + (i % 4);
        result.system_b_metrics.similarity_score = 0.7 + (i % 6) * 0.03;
        
        result.winner = (result.system_a_metrics.similarity_score > 
                        result.system_b_metrics.similarity_score) ? "SystemA" : "SystemB";
        
        results.push_back(result);
    }
    
    std::string analysis_report;
    EXPECT_TRUE(comparator_->perform_statistical_analysis(results, analysis_report));
    EXPECT_FALSE(analysis_report.empty());
    
    // Check that report contains expected sections
    EXPECT_NE(analysis_report.find("Statistical Analysis Report"), std::string::npos);
    EXPECT_NE(analysis_report.find("Total Tests"), std::string::npos);
    EXPECT_NE(analysis_report.find("SNR Analysis"), std::string::npos);
    EXPECT_NE(analysis_report.find("Similarity Analysis"), std::string::npos);
    EXPECT_NE(analysis_report.find("Conclusions"), std::string::npos);
}

TEST_F(ABComparatorTest, ReportGeneration) {
    // Test HTML and CSV report generation
    
    // Create sample results
    std::vector<ABComparisonResult> results;
    for (int i = 0; i < 5; ++i) {
        ABComparisonResult result;
        result.comparison_successful = true;
        result.system_a_name = "SystemA";
        result.system_b_name = "SystemB";
        result.system_a_metrics.snr_db = 20.0 + i;
        result.system_b_metrics.snr_db = 18.0 + i;
        result.system_a_render_time = std::chrono::milliseconds(100 + i * 10);
        result.system_b_render_time = std::chrono::milliseconds(120 + i * 10);
        result.winner = (i % 2 == 0) ? "SystemA" : "SystemB";
        results.push_back(result);
    }
    
    std::string temp_dir = std::filesystem::temp_directory_path();
    std::string html_path = temp_dir + "/test_report.html";
    std::string csv_path = temp_dir + "/test_report.csv";
    
    // Test HTML report generation
    EXPECT_TRUE(comparator_->generate_html_report(results, html_path));
    EXPECT_TRUE(std::filesystem::exists(html_path));
    
    // Verify HTML content
    std::ifstream html_file(html_path);
    std::string html_content((std::istreambuf_iterator<char>(html_file)),
                            std::istreambuf_iterator<char>());
    EXPECT_NE(html_content.find("A/B Comparison Report"), std::string::npos);
    EXPECT_NE(html_content.find("SystemA"), std::string::npos);
    EXPECT_NE(html_content.find("SystemB"), std::string::npos);
    
    // Test CSV report generation
    EXPECT_TRUE(comparator_->generate_csv_report(results, csv_path));
    EXPECT_TRUE(std::filesystem::exists(csv_path));
    
    // Verify CSV content
    std::ifstream csv_file(csv_path);
    std::string csv_content((std::istreambuf_iterator<char>(csv_file)),
                           std::istreambuf_iterator<char>());
    EXPECT_NE(csv_content.find("Test,SystemA_SNR,SystemB_SNR"), std::string::npos);
    EXPECT_NE(csv_content.find(",20,18,"), std::string::npos); // First test data
}

TEST_F(ABComparatorTest, QualityMetricsEdgeCases) {
    // Test advanced metrics with edge cases
    
    // Test with empty audio vectors
    std::vector<float> empty_audio;
    std::vector<float> normal_audio(1000, 0.5f);
    
    AdvancedQualityMetrics metrics_empty = comparator_->calculate_advanced_metrics(
        test_data_manager_.create_temp_file(".wav"), "");
    
    // Should handle empty/missing files gracefully
    EXPECT_FALSE(metrics_empty.measurement_successful);
    
    // Test with identical audio (should have high similarity)
    std::string identical_audio = test_data_manager_.create_temp_file(".wav");
    AdvancedQualityMetrics metrics_identical = comparator_->calculate_advanced_metrics(
        identical_audio, identical_audio);
    
    EXPECT_TRUE(metrics_identical.measurement_successful);
    // Identical files should have perfect or near-perfect similarity
    EXPECT_GT(metrics_identical.similarity_score, 0.95);
    EXPECT_LT(metrics_identical.mel_cepstral_distortion, 1.0); // Should be low for identical files
}

TEST_F(ABComparatorTest, ConfigurationValidation) {
    // Test configuration validation and error handling
    
    ABComparisonConfig invalid_config;
    // Leave required fields empty
    invalid_config.system_a.name = "";
    invalid_config.system_b.name = "";
    
    comparator_->set_config(invalid_config);
    
    std::string test_input = test_data_manager_.create_temp_file(".wav");
    ABComparisonResult result = comparator_->compare_single_test(test_input);
    
    // Should handle invalid configuration gracefully
    EXPECT_FALSE(result.comparison_successful);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ABComparatorTest, PerformanceMetricsValidation) {
    // Test that performance metrics are captured correctly
    
    std::string test_input = test_data_manager_.create_temp_file(".wav");
    ABComparisonResult result = comparator_->compare_single_test(test_input);
    
    if (result.comparison_successful) {
        // Render times should be positive
        EXPECT_GT(result.system_a_render_time.count(), 0);
        EXPECT_GT(result.system_b_render_time.count(), 0);
        
        // Memory usage should be recorded (even if placeholder values)
        EXPECT_GE(result.system_a_memory_usage, 0);
        EXPECT_GE(result.system_b_memory_usage, 0);
        
        // Performance difference should be calculable
        auto time_difference = result.system_a_render_time - result.system_b_render_time;
        EXPECT_NE(time_difference.count(), 0); // Should have some difference due to randomization
    }
}

} // namespace integration_test
} // namespace nexussynth
#include <gtest/gtest.h>
#include "nexussynth/validation_system.h"
#include "nexussynth/nvm_format.h"
#include <filesystem>
#include <fstream>
#include <memory>

using namespace nexussynth::validation;

/**
 * @brief Test suite for ValidationEngine quality checking system
 * 
 * Tests the comprehensive validation system including parameter validation,
 * model consistency checking, and error reporting functionality
 */
class ValidationSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<ValidationEngine>();
        
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "nexussynth_validation_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    void create_test_utau_voicebank(const std::string& path) {
        std::filesystem::create_directories(path);
        
        // Create basic oto.ini
        std::ofstream oto_file(path + "/oto.ini");
        oto_file << "a.wav=a,100,200,300,40,80\n";
        oto_file << "ka.wav=ka,50,150,250,35,70\n";
        oto_file.close();
        
        // Create dummy audio files
        std::ofstream audio1(path + "/a.wav", std::ios::binary);
        audio1 << "RIFF____WAVEfmt ________________data____"; // Minimal WAV header
        audio1.close();
        
        std::ofstream audio2(path + "/ka.wav", std::ios::binary);
        audio2 << "RIFF____WAVEfmt ________________data____";
        audio2.close();
        
        // Create character.txt
        std::ofstream char_file(path + "/character.txt");
        char_file << "name=TestVoice\nauthor=Test\n";
        char_file.close();
    }
    
    std::unique_ptr<ValidationEngine> engine_;
    std::filesystem::path test_dir_;
};

TEST_F(ValidationSystemTest, ValidationEngineInitialization) {
    ASSERT_TRUE(engine_ != nullptr);
    
    // Check default validation rules
    const auto& rules = engine_->get_validation_rules();
    EXPECT_GT(rules.min_f0_hz, 0);
    EXPECT_GT(rules.max_f0_hz, rules.min_f0_hz);
    EXPECT_GE(rules.min_hmm_states, 3);
    EXPECT_LE(rules.min_hmm_states, rules.max_hmm_states);
}

TEST_F(ValidationSystemTest, ParameterValidationRulesConfiguration) {
    ParameterValidationRules custom_rules;
    custom_rules.min_f0_hz = 60.0;
    custom_rules.max_f0_hz = 500.0;
    custom_rules.min_hmm_states = 5;
    custom_rules.max_hmm_states = 8;
    
    engine_->set_validation_rules(custom_rules);
    
    const auto& retrieved_rules = engine_->get_validation_rules();
    EXPECT_EQ(retrieved_rules.min_f0_hz, 60.0);
    EXPECT_EQ(retrieved_rules.max_f0_hz, 500.0);
    EXPECT_EQ(retrieved_rules.min_hmm_states, 5);
    EXPECT_EQ(retrieved_rules.max_hmm_states, 8);
}

TEST_F(ValidationSystemTest, UTAUVoicebankStructureValidation) {
    // Test with valid UTAU voicebank
    std::string valid_voicebank = test_dir_ / "valid_utau";
    create_test_utau_voicebank(valid_voicebank);
    
    auto report = engine_->validate_utau_voicebank(valid_voicebank);
    
    EXPECT_TRUE(report.is_usable);
    EXPECT_EQ(report.critical_count, 0);
    EXPECT_GE(report.info_count, 0); // May have info messages about missing readme
    
    // Test with non-existent directory
    std::string nonexistent = test_dir_ / "nonexistent";
    auto report2 = engine_->validate_utau_voicebank(nonexistent);
    
    EXPECT_FALSE(report2.is_valid);
    EXPECT_FALSE(report2.is_usable);
    EXPECT_GT(report2.critical_count, 0);
    
    // Check for specific error about missing directory
    bool found_missing_dir_error = false;
    for (const auto& issue : report2.issues) {
        if (issue.id == "VOICEBANK_NOT_FOUND") {
            found_missing_dir_error = true;
            EXPECT_EQ(issue.severity, ValidationSeverity::CRITICAL);
            EXPECT_EQ(issue.category, ValidationCategory::FILE_STRUCTURE);
            break;
        }
    }
    EXPECT_TRUE(found_missing_dir_error);
}

TEST_F(ValidationSystemTest, UTAUVoicebankMissingFiles) {
    std::string incomplete_voicebank = test_dir_ / "incomplete_utau";
    std::filesystem::create_directories(incomplete_voicebank);
    
    // Create directory but no oto.ini
    auto report = engine_->validate_utau_voicebank(incomplete_voicebank);
    
    EXPECT_FALSE(report.is_valid);
    EXPECT_FALSE(report.is_usable);
    EXPECT_GT(report.critical_count, 0);
    
    // Check for missing oto.ini error
    bool found_missing_oto = false;
    for (const auto& issue : report.issues) {
        if (issue.id == "MISSING_OTO_INI") {
            found_missing_oto = true;
            EXPECT_EQ(issue.severity, ValidationSeverity::CRITICAL);
            break;
        }
    }
    EXPECT_TRUE(found_missing_oto);
}

TEST_F(ValidationSystemTest, ValidationIssueStructure) {
    ValidationIssue issue("TEST_ID", ValidationSeverity::WARNING, 
                         ValidationCategory::PARAMETER_RANGE, "Test issue");
    
    EXPECT_EQ(issue.id, "TEST_ID");
    EXPECT_EQ(issue.severity, ValidationSeverity::WARNING);
    EXPECT_EQ(issue.category, ValidationCategory::PARAMETER_RANGE);
    EXPECT_EQ(issue.title, "Test issue");
    
    // Test optional fields
    issue.suggestion = "Fix this issue";
    issue.model_name = "test_model";
    issue.phoneme = "a";
    issue.metadata["key"] = "value";
    
    EXPECT_TRUE(issue.suggestion.has_value());
    EXPECT_EQ(issue.suggestion.value(), "Fix this issue");
    EXPECT_TRUE(issue.model_name.has_value());
    EXPECT_EQ(issue.model_name.value(), "test_model");
    EXPECT_TRUE(issue.phoneme.has_value());
    EXPECT_EQ(issue.phoneme.value(), "a");
    EXPECT_EQ(issue.metadata["key"], "value");
}

TEST_F(ValidationSystemTest, PhonemeAnalysisBasic) {
    // Create a mock NVM file for testing (this would need proper NVM implementation)
    // For now, test the phoneme analysis structure
    
    PhonemeAnalysis analysis;
    analysis.required_phonemes = {"a", "i", "u", "e", "o", "ka", "ki", "ku"};
    analysis.found_phonemes = {"a", "i", "u", "e", "ka", "ki"};
    analysis.total_required = analysis.required_phonemes.size();
    analysis.total_found = analysis.found_phonemes.size();
    
    // Calculate missing phonemes
    std::set_difference(analysis.required_phonemes.begin(), analysis.required_phonemes.end(),
                       analysis.found_phonemes.begin(), analysis.found_phonemes.end(),
                       std::inserter(analysis.missing_phonemes, analysis.missing_phonemes.begin()));
    
    analysis.total_missing = analysis.missing_phonemes.size();
    analysis.coverage_percentage = 100.0 * (analysis.total_required - analysis.total_missing) / analysis.total_required;
    
    EXPECT_EQ(analysis.total_missing, 2); // "o", "ku" should be missing
    EXPECT_NEAR(analysis.coverage_percentage, 75.0, 0.1); // 6/8 = 75%
    EXPECT_TRUE(analysis.missing_phonemes.count("o") > 0);
    EXPECT_TRUE(analysis.missing_phonemes.count("ku") > 0);
}

TEST_F(ValidationSystemTest, ValidationReportStructure) {
    ValidationReport report;
    
    EXPECT_FALSE(report.is_valid);
    EXPECT_FALSE(report.is_usable);
    EXPECT_EQ(report.total_issues, 0);
    EXPECT_EQ(report.info_count, 0);
    EXPECT_EQ(report.warning_count, 0);
    EXPECT_EQ(report.error_count, 0);
    EXPECT_EQ(report.critical_count, 0);
    EXPECT_EQ(report.quality_metrics.overall_score, 0.0);
    
    // Add some test issues
    ValidationIssue warning("WARN1", ValidationSeverity::WARNING, 
                           ValidationCategory::PHONEME_COVERAGE, "Warning");
    ValidationIssue error("ERR1", ValidationSeverity::ERROR,
                         ValidationCategory::PARAMETER_RANGE, "Error");
    
    report.issues.push_back(warning);
    report.issues.push_back(error);
    
    // Update counts (this would normally be done by ValidationEngine)
    report.warning_count = 1;
    report.error_count = 1;
    report.total_issues = 2;
    report.is_usable = (report.critical_count == 0);
    report.is_valid = (report.critical_count == 0 && report.error_count == 0);
    
    EXPECT_EQ(report.total_issues, 2);
    EXPECT_EQ(report.warning_count, 1);
    EXPECT_EQ(report.error_count, 1);
    EXPECT_FALSE(report.is_valid); // Has errors
    EXPECT_TRUE(report.is_usable);  // No critical errors
}

TEST_F(ValidationSystemTest, ConsoleProgressCallback) {
    auto callback = std::make_shared<ConsoleValidationProgressCallback>(false);
    engine_->set_progress_callback(callback);
    
    // Test that callback is set without errors
    // (More comprehensive testing would require capturing console output)
    EXPECT_NO_THROW(callback->on_validation_started("test.nvm"));
    EXPECT_NO_THROW(callback->on_validation_progress(1, 5, "Testing"));
    
    ValidationReport dummy_report;
    dummy_report.is_valid = true;
    dummy_report.total_issues = 0;
    dummy_report.quality_metrics.overall_score = 0.95;
    
    EXPECT_NO_THROW(callback->on_validation_completed(dummy_report));
    
    ValidationIssue test_issue("TEST", ValidationSeverity::INFO, 
                              ValidationCategory::FILE_STRUCTURE, "Test");
    EXPECT_NO_THROW(callback->on_issue_found(test_issue));
    EXPECT_NO_THROW(callback->on_critical_error("Test error"));
}

TEST_F(ValidationSystemTest, ValidationUtilities) {
    using namespace validation_utils;
    
    // Test phoneme set retrieval
    auto japanese_phonemes = get_japanese_phoneme_set();
    auto english_phonemes = get_english_phoneme_set();
    auto basic_utau_phonemes = get_basic_utau_phoneme_set();
    
    EXPECT_FALSE(japanese_phonemes.empty());
    EXPECT_FALSE(english_phonemes.empty());
    EXPECT_FALSE(basic_utau_phonemes.empty());
    
    // Check some expected phonemes
    EXPECT_TRUE(japanese_phonemes.count("a") > 0);
    EXPECT_TRUE(japanese_phonemes.count("ka") > 0);
    EXPECT_TRUE(english_phonemes.count("AA") > 0);
    EXPECT_TRUE(basic_utau_phonemes.count("a") > 0);
    
    // Test format detection
    std::string test_dir_str = test_dir_.string();
    std::string format = detect_file_format(test_dir_str);
    EXPECT_EQ(format, "directory");
    
    // Test with UTAU voicebank
    std::string utau_path = test_dir_ / "format_test_utau";
    create_test_utau_voicebank(utau_path);
    
    std::string utau_format = detect_file_format(utau_path);
    EXPECT_EQ(utau_format, "utau");
    EXPECT_TRUE(is_utau_voicebank(utau_path));
}

// Main test runner
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
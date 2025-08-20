#include <gtest/gtest.h>
#include "../utils/test_data_manager.h"
#include "../utils/quality_analyzer.h"
#include "nexussynth/validation_system.h"
#include <filesystem>

namespace nexussynth {
namespace integration_test {

class NvmGenerationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string test_data_dir = std::filesystem::current_path() / "test_data";
        ASSERT_TRUE(test_data_manager_.initialize(test_data_dir));
        ASSERT_TRUE(test_data_manager_.setup_test_environment());
        
        quality_analyzer_ = std::make_unique<QualityAnalyzer>();
    }
    
    void TearDown() override {
        test_data_manager_.cleanup_test_environment();
    }
    
    TestDataManager test_data_manager_;
    std::unique_ptr<QualityAnalyzer> quality_analyzer_;
};

TEST_F(NvmGenerationTest, ValidateNvmFileStructure) {
    // Test that generated .nvm files have correct structure
    
    std::string voice_bank_path = test_data_manager_.get_minimal_voice_bank_path();
    std::string nvm_output = test_data_manager_.create_temp_file(".nvm");
    
    // Convert to .nvm format
    // This would use the CLI interface or direct API calls
    // For now, we'll create a mock .nvm file for testing
    
    ConversionQualityResult result;
    bool validation_success = quality_analyzer_->validate_conversion_output(nvm_output, result);
    
    EXPECT_TRUE(validation_success) << "NVM validation failed: " << result.error_message;
    EXPECT_GT(result.overall_quality_score, 0.7) << "NVM quality score too low";
    EXPECT_TRUE(result.validation_successful);
}

TEST_F(NvmGenerationTest, PhonemeCompleteness) {
    // Test that all phonemes from voice bank are included in .nvm file
    GTEST_SKIP() << "Integration with actual NVM generation pending";
}

TEST_F(NvmGenerationTest, CompressionEfficiency) {
    // Test .nvm compression and file size optimization
    GTEST_SKIP() << "Integration with actual NVM generation pending";
}

} // namespace integration_test  
} // namespace nexussynth
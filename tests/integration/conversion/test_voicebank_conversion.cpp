#include <gtest/gtest.h>
#include "../utils/test_data_manager.h"
#include "nexussynth/cli_interface.h"
#include "nexussynth/conditioning_config.h"
#include "nexussynth/validation_system.h"
#include <filesystem>
#include <chrono>

namespace nexussynth {
namespace integration_test {

class VoicebankConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data manager
        std::string test_data_dir = std::filesystem::current_path() / "test_data";
        ASSERT_TRUE(test_data_manager_.initialize(test_data_dir));
        ASSERT_TRUE(test_data_manager_.setup_test_environment());
        
        // Initialize CLI interface
        cli_interface_ = std::make_unique<cli::CliInterface>();
    }
    
    void TearDown() override {
        test_data_manager_.cleanup_test_environment();
    }
    
    TestDataManager test_data_manager_;
    std::unique_ptr<cli::CliInterface> cli_interface_;
};

TEST_F(VoicebankConversionTest, ConvertMinimalVoiceBank) {
    // Test basic voice bank conversion functionality
    
    // Get the minimal test voice bank
    std::string voice_bank_path = test_data_manager_.get_minimal_voice_bank_path();
    ASSERT_TRUE(test_data_manager_.file_exists(voice_bank_path + "/oto.ini"));
    
    // Create output path for .nvm file
    std::string output_path = test_data_manager_.create_temp_file(".nvm");
    
    // Prepare CLI arguments for conversion
    std::vector<std::string> args = {
        "nexussynth",
        "convert",
        voice_bank_path,
        "-o", output_path,
        "--preset", "fast", // Use fast preset for testing
        "--verbose"
    };
    
    // Convert arguments to argc/argv format
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    // Record start time for performance measurement
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Execute conversion
    cli::CliResult result = cli_interface_->run(static_cast<int>(argv.size()), argv.data());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto conversion_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Verify conversion succeeded
    EXPECT_TRUE(result.success) << "Conversion failed: " << result.message;
    EXPECT_EQ(result.exit_code, 0) << "Non-zero exit code: " << result.exit_code;
    
    // Verify output file was created
    EXPECT_TRUE(test_data_manager_.file_exists(output_path)) 
        << "Output .nvm file was not created: " << output_path;
    
    // Verify file is not empty
    size_t file_size = test_data_manager_.get_file_size(output_path);
    EXPECT_GT(file_size, 0u) << "Output file is empty";
    
    // Performance check - conversion should complete in reasonable time
    EXPECT_LT(conversion_time.count(), 30000) // 30 seconds max
        << "Conversion took too long: " << conversion_time.count() << "ms";
    
    // Log results for analysis
    std::cout << "Conversion completed in " << conversion_time.count() << "ms" << std::endl;
    std::cout << "Output file size: " << file_size << " bytes" << std::endl;
}

TEST_F(VoicebankConversionTest, ValidateConvertedFile) {
    // Test that converted .nvm files pass validation
    
    std::string voice_bank_path = test_data_manager_.get_minimal_voice_bank_path();
    std::string output_path = test_data_manager_.create_temp_file(".nvm");
    
    // Convert voice bank first
    std::vector<std::string> convert_args = {
        "nexussynth", "convert", voice_bank_path, 
        "-o", output_path, "--preset", "default"
    };
    
    std::vector<char*> convert_argv;
    for (auto& arg : convert_args) {
        convert_argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    cli::CliResult convert_result = cli_interface_->run(
        static_cast<int>(convert_argv.size()), convert_argv.data());
    
    ASSERT_TRUE(convert_result.success) << "Initial conversion failed";
    
    // Now validate the converted file
    std::vector<std::string> validate_args = {
        "nexussynth", "validate", output_path, "--verbose"
    };
    
    std::vector<char*> validate_argv;
    for (auto& arg : validate_args) {
        validate_argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    cli::CliResult validate_result = cli_interface_->run(
        static_cast<int>(validate_argv.size()), validate_argv.data());
    
    // Validation should succeed
    EXPECT_TRUE(validate_result.success) << "Validation failed: " << validate_result.message;
    
    // Should not have critical errors
    EXPECT_EQ(validate_result.exit_code, 0) 
        << "Validation found critical errors, exit code: " << validate_result.exit_code;
}

TEST_F(VoicebankConversionTest, ConversionWithDifferentPresets) {
    // Test conversion with different quality presets
    
    std::string voice_bank_path = test_data_manager_.get_minimal_voice_bank_path();
    std::vector<std::string> presets = {"fast", "default", "quality"};
    
    for (const auto& preset : presets) {
        SCOPED_TRACE("Testing preset: " + preset);
        
        std::string output_path = test_data_manager_.create_temp_file(".nvm");
        
        std::vector<std::string> args = {
            "nexussynth", "convert", voice_bank_path,
            "-o", output_path, "--preset", preset
        };
        
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        cli::CliResult result = cli_interface_->run(static_cast<int>(argv.size()), argv.data());
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        EXPECT_TRUE(result.success) << "Conversion failed with preset " << preset;
        EXPECT_TRUE(test_data_manager_.file_exists(output_path)) 
            << "Output file not created with preset " << preset;
        
        size_t file_size = test_data_manager_.get_file_size(output_path);
        EXPECT_GT(file_size, 0u) << "Empty output file with preset " << preset;
        
        // Quality preset should produce larger files (generally)
        if (preset == "quality") {
            // Allow more time for quality preset
            EXPECT_LT(duration.count(), 60000) << "Quality preset took too long";
        } else if (preset == "fast") {
            // Fast preset should complete quickly
            EXPECT_LT(duration.count(), 15000) << "Fast preset took too long";
        }
        
        std::cout << "Preset " << preset << ": " << duration.count() 
                  << "ms, " << file_size << " bytes" << std::endl;
    }
}

TEST_F(VoicebankConversionTest, ConversionErrorHandling) {
    // Test error handling for invalid inputs
    
    // Test with non-existent voice bank
    std::string invalid_path = "/nonexistent/voicebank";
    std::string output_path = test_data_manager_.create_temp_file(".nvm");
    
    std::vector<std::string> args = {
        "nexussynth", "convert", invalid_path, "-o", output_path
    };
    
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    cli::CliResult result = cli_interface_->run(static_cast<int>(argv.size()), argv.data());
    
    // Should fail gracefully
    EXPECT_FALSE(result.success) << "Should fail with non-existent input";
    EXPECT_NE(result.exit_code, 0) << "Should return non-zero exit code for error";
    EXPECT_FALSE(result.message.empty()) << "Should provide error message";
    
    // Output file should not be created
    EXPECT_FALSE(test_data_manager_.file_exists(output_path))
        << "Should not create output file on error";
}

TEST_F(VoicebankConversionTest, BatchConversionTest) {
    // Test batch conversion of multiple voice banks
    
    // Create a temporary directory with multiple voice banks
    std::string batch_dir = test_data_manager_.create_temp_directory("batch_test");
    std::string output_dir = test_data_manager_.create_temp_directory("batch_output");
    
    // Copy minimal voice bank to create multiple test banks
    std::string source_vb = test_data_manager_.get_minimal_voice_bank_path();
    std::vector<std::string> voice_bank_names = {"vb1", "vb2", "vb3"};
    
    for (const auto& name : voice_bank_names) {
        std::string target_path = batch_dir + "/" + name;
        std::filesystem::create_directories(target_path);
        
        // Copy files
        for (const auto& entry : std::filesystem::directory_iterator(source_vb)) {
            std::filesystem::copy_file(entry.path(), target_path + "/" + entry.path().filename().string());
        }
    }
    
    // Run batch conversion
    std::vector<std::string> args = {
        "nexussynth", "batch", batch_dir, "-o", output_dir, 
        "--preset", "fast", "--recursive"
    };
    
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    cli::CliResult result = cli_interface_->run(static_cast<int>(argv.size()), argv.data());
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_TRUE(result.success) << "Batch conversion failed: " << result.message;
    
    // Verify all output files were created
    for (const auto& name : voice_bank_names) {
        std::string expected_output = output_dir + "/" + name + ".nvm";
        EXPECT_TRUE(test_data_manager_.file_exists(expected_output))
            << "Missing output for " << name;
        
        EXPECT_GT(test_data_manager_.get_file_size(expected_output), 0u)
            << "Empty output file for " << name;
    }
    
    // Batch conversion should be reasonably fast
    EXPECT_LT(duration.count(), 90000) // 90 seconds for 3 voice banks
        << "Batch conversion took too long: " << duration.count() << "ms";
    
    std::cout << "Batch conversion of " << voice_bank_names.size() 
              << " voice banks completed in " << duration.count() << "ms" << std::endl;
}

TEST_F(VoicebankConversionTest, ConfigurationCustomization) {
    // Test conversion with custom configuration parameters
    
    std::string voice_bank_path = test_data_manager_.get_minimal_voice_bank_path();
    std::string output_path = test_data_manager_.create_temp_file(".nvm");
    
    // Create custom configuration file
    std::string config_path = test_data_manager_.create_temp_file(".json");
    std::ofstream config_file(config_path);
    config_file << R"({
        "world_config": {
            "frame_period": 10.0,
            "f0_method": "harvest",
            "fft_size": 1024
        },
        "model_training": {
            "hmm_states": 3,
            "gaussians_per_state": 4,
            "training_iterations": 25
        },
        "output_config": {
            "compression_enabled": false,
            "checksum_enabled": true
        }
    })";
    config_file.close();
    
    std::vector<std::string> args = {
        "nexussynth", "convert", voice_bank_path,
        "-o", output_path, "--config", config_path
    };
    
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    cli::CliResult result = cli_interface_->run(static_cast<int>(argv.size()), argv.data());
    
    EXPECT_TRUE(result.success) << "Custom config conversion failed: " << result.message;
    EXPECT_TRUE(test_data_manager_.file_exists(output_path));
    
    // File should exist and be valid
    size_t file_size = test_data_manager_.get_file_size(output_path);
    EXPECT_GT(file_size, 0u) << "Custom config produced empty file";
}

} // namespace integration_test
} // namespace nexussynth
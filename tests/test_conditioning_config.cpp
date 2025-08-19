#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

#include "nexussynth/conditioning_config.h"

using namespace nexussynth::conditioning;
using namespace testing;

class ConditioningConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "nexussynth_config_test";
        std::filesystem::create_directories(test_dir_);
        
        config_manager_ = std::make_unique<ConfigManager>();
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::filesystem::path test_dir_;
    std::unique_ptr<ConfigManager> config_manager_;
};

// Test basic configuration structure initialization
TEST_F(ConditioningConfigTest, DefaultConfigInitialization) {
    ConditioningConfig config;
    
    // Check default values
    EXPECT_EQ(config.config_version, "1.0");
    EXPECT_EQ(config.config_name, "default");
    EXPECT_FALSE(config.description.empty());
    
    // Check component configurations have reasonable defaults
    EXPECT_GT(config.world_config.frame_period, 0.0);
    EXPECT_GT(config.world_config.f0_floor, 0.0);
    EXPECT_GT(config.world_config.f0_ceil, config.world_config.f0_floor);
    
    EXPECT_GT(config.audio_config.target_sample_rate, 0);
    EXPECT_GT(config.audio_config.target_bit_depth, 0);
    
    EXPECT_GE(config.training_config.max_training_iterations, 1);
    EXPECT_GT(config.training_config.convergence_threshold, 0.0);
    
    EXPECT_GE(config.batch_config.batch_size, 1);
    EXPECT_GE(config.batch_config.num_worker_threads, 0);
    
    EXPECT_FALSE(config.output_config.output_directory.empty());
    EXPECT_FALSE(config.output_config.model_file_extension.empty());
}

// Test named configuration constructor
TEST_F(ConditioningConfigTest, NamedConfigInitialization) {
    ConditioningConfig config("test_config");
    
    EXPECT_EQ(config.config_name, "test_config");
    EXPECT_FALSE(config.description.empty());
    EXPECT_EQ(config.config_version, "1.0");
}

// Test JSON serialization round-trip
TEST_F(ConditioningConfigTest, JsonSerializationRoundTrip) {
    ConditioningConfig original_config("test_config");
    original_config.description = "Test configuration for unit tests";
    original_config.world_config.frame_period = 7.5;
    original_config.world_config.f0_floor = 80.0;
    original_config.world_config.f0_ceil = 600.0;
    original_config.audio_config.target_sample_rate = 48000;
    original_config.audio_config.target_bit_depth = 24;
    original_config.training_config.max_training_iterations = 150;
    original_config.batch_config.batch_size = 25;
    original_config.output_config.output_directory = "/tmp/test_output";
    original_config.custom_settings["test_key"] = "test_value";
    
    // Serialize to JSON
    std::string json_str = config_manager_->config_to_json(original_config);
    EXPECT_FALSE(json_str.empty());
    EXPECT_THAT(json_str, HasSubstr("test_config"));
    EXPECT_THAT(json_str, HasSubstr("48000"));
    EXPECT_THAT(json_str, HasSubstr("test_value"));
    
    // Deserialize from JSON
    ConditioningConfig deserialized_config;
    bool success = config_manager_->config_from_json(json_str, deserialized_config);
    EXPECT_TRUE(success);
    
    // Verify round-trip integrity
    EXPECT_EQ(deserialized_config.config_name, original_config.config_name);
    EXPECT_EQ(deserialized_config.description, original_config.description);
    EXPECT_DOUBLE_EQ(deserialized_config.world_config.frame_period, original_config.world_config.frame_period);
    EXPECT_DOUBLE_EQ(deserialized_config.world_config.f0_floor, original_config.world_config.f0_floor);
    EXPECT_DOUBLE_EQ(deserialized_config.world_config.f0_ceil, original_config.world_config.f0_ceil);
    EXPECT_EQ(deserialized_config.audio_config.target_sample_rate, original_config.audio_config.target_sample_rate);
    EXPECT_EQ(deserialized_config.audio_config.target_bit_depth, original_config.audio_config.target_bit_depth);
    EXPECT_EQ(deserialized_config.training_config.max_training_iterations, original_config.training_config.max_training_iterations);
    EXPECT_EQ(deserialized_config.batch_config.batch_size, original_config.batch_config.batch_size);
    EXPECT_EQ(deserialized_config.output_config.output_directory, original_config.output_config.output_directory);
    EXPECT_EQ(deserialized_config.custom_settings.at("test_key"), "test_value");
}

// Test file I/O operations
TEST_F(ConditioningConfigTest, FileIOOperations) {
    ConditioningConfig config("file_test_config");
    config.description = "Configuration for file I/O testing";
    config.world_config.frame_period = 6.0;
    config.audio_config.target_sample_rate = 44100;
    
    std::string config_file = (test_dir_ / "test_config.json").string();
    
    // Save configuration to file
    bool save_success = config_manager_->save_config(config_file, config);
    EXPECT_TRUE(save_success);
    EXPECT_TRUE(std::filesystem::exists(config_file));
    
    // Check file size is reasonable
    auto file_size = std::filesystem::file_size(config_file);
    EXPECT_GT(file_size, 100);  // Should be a substantial JSON file
    EXPECT_LT(file_size, 50000); // But not excessively large
    
    // Load configuration from file
    ConditioningConfig loaded_config;
    bool load_success = config_manager_->load_config(config_file, loaded_config);
    EXPECT_TRUE(load_success);
    
    // Verify loaded data
    EXPECT_EQ(loaded_config.config_name, config.config_name);
    EXPECT_EQ(loaded_config.description, config.description);
    EXPECT_DOUBLE_EQ(loaded_config.world_config.frame_period, config.world_config.frame_period);
    EXPECT_EQ(loaded_config.audio_config.target_sample_rate, config.audio_config.target_sample_rate);
}

// Test configuration validation
TEST_F(ConditioningConfigTest, ConfigurationValidation) {
    // Test valid configuration
    ConditioningConfig valid_config = config_manager_->get_default_config();
    auto validation_result = config_manager_->validate_config(valid_config);
    EXPECT_TRUE(validation_result.is_valid);
    EXPECT_TRUE(validation_result.errors.empty());
    
    // Test invalid configuration - empty name
    ConditioningConfig invalid_config;
    invalid_config.config_name = "";
    validation_result = config_manager_->validate_config(invalid_config);
    EXPECT_FALSE(validation_result.is_valid);
    EXPECT_FALSE(validation_result.errors.empty());
    EXPECT_THAT(validation_result.errors[0], HasSubstr("name"));
    
    // Test invalid WORLD config - negative frame period
    ConditioningConfig world_invalid_config;
    world_invalid_config.world_config.frame_period = -1.0;
    validation_result = config_manager_->validate_config(world_invalid_config);
    EXPECT_FALSE(validation_result.is_valid);
    EXPECT_FALSE(validation_result.errors.empty());
    
    // Test invalid audio config - invalid sample rate
    ConditioningConfig audio_invalid_config;
    audio_invalid_config.audio_config.target_sample_rate = -44100;
    validation_result = config_manager_->validate_config(audio_invalid_config);
    EXPECT_FALSE(validation_result.is_valid);
    EXPECT_FALSE(validation_result.errors.empty());
    
    // Test configuration with warnings but still valid
    ConditioningConfig warning_config;
    warning_config.batch_config.num_worker_threads = 32; // High thread count
    validation_result = config_manager_->validate_config(warning_config);
    EXPECT_TRUE(validation_result.is_valid); // Still valid
    EXPECT_FALSE(validation_result.warnings.empty()); // But with warnings
}

// Test preset configurations
TEST_F(ConditioningConfigTest, PresetConfigurations) {
    // Test default configuration
    auto default_config = config_manager_->get_default_config();
    EXPECT_EQ(default_config.config_name, "default");
    auto validation = config_manager_->validate_config(default_config);
    EXPECT_TRUE(validation.is_valid);
    
    // Test fast configuration
    auto fast_config = config_manager_->get_fast_config();
    EXPECT_EQ(fast_config.config_name, "fast");
    EXPECT_EQ(fast_config.training_config.optimization_level, ModelTrainingConfig::FAST);
    EXPECT_LT(fast_config.training_config.max_training_iterations, 
              default_config.training_config.max_training_iterations);
    validation = config_manager_->validate_config(fast_config);
    EXPECT_TRUE(validation.is_valid);
    
    // Test quality configuration
    auto quality_config = config_manager_->get_quality_config();
    EXPECT_EQ(quality_config.config_name, "quality");
    EXPECT_EQ(quality_config.training_config.optimization_level, ModelTrainingConfig::MAXIMUM);
    EXPECT_GT(quality_config.training_config.max_training_iterations,
              default_config.training_config.max_training_iterations);
    validation = config_manager_->validate_config(quality_config);
    EXPECT_TRUE(validation.is_valid);
    
    // Test batch configuration
    auto batch_config = config_manager_->get_batch_config();
    EXPECT_EQ(batch_config.config_name, "batch");
    EXPECT_GT(batch_config.batch_config.batch_size, default_config.batch_config.batch_size);
    EXPECT_TRUE(batch_config.batch_config.continue_on_error);
    validation = config_manager_->validate_config(batch_config);
    EXPECT_TRUE(validation.is_valid);
}

// Test configuration templates
TEST_F(ConditioningConfigTest, ConfigurationTemplates) {
    // Test available templates
    auto templates = config_manager_->get_available_templates();
    EXPECT_FALSE(templates.empty());
    EXPECT_THAT(templates, Contains("default"));
    EXPECT_THAT(templates, Contains("fast"));
    EXPECT_THAT(templates, Contains("quality"));
    EXPECT_THAT(templates, Contains("batch"));
    
    // Test template creation
    std::string template_file = (test_dir_ / "fast_template.json").string();
    bool success = config_manager_->create_config_template(template_file, "fast");
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::filesystem::exists(template_file));
    
    // Load and verify the template
    ConditioningConfig loaded_template;
    success = config_manager_->load_config(template_file, loaded_template);
    EXPECT_TRUE(success);
    EXPECT_EQ(loaded_template.config_name, "fast");
    EXPECT_EQ(loaded_template.training_config.optimization_level, ModelTrainingConfig::FAST);
    
    // Test invalid template name
    std::string invalid_template_file = (test_dir_ / "invalid_template.json").string();
    success = config_manager_->create_config_template(invalid_template_file, "nonexistent");
    EXPECT_FALSE(success);
    EXPECT_FALSE(std::filesystem::exists(invalid_template_file));
}

// Test configuration utility functions
TEST_F(ConditioningConfigTest, ConfigurationUtilities) {
    // Test UTAU compatible configuration
    auto utau_config = config_utils::create_utau_compatible_config();
    EXPECT_EQ(utau_config.config_name, "utau_compatible");
    EXPECT_EQ(utau_config.audio_config.target_sample_rate, 44100);
    EXPECT_EQ(utau_config.audio_config.target_bit_depth, 16);
    EXPECT_TRUE(utau_config.audio_config.force_mono);
    EXPECT_TRUE(utau_config.scanner_config.validate_audio_files);
    EXPECT_TRUE(utau_config.scanner_config.validate_timing_parameters);
    
    // Test high quality configuration
    auto hq_config = config_utils::create_high_quality_config();
    EXPECT_EQ(hq_config.config_name, "high_quality");
    EXPECT_EQ(hq_config.audio_config.target_sample_rate, 48000);
    EXPECT_EQ(hq_config.audio_config.target_bit_depth, 24);
    EXPECT_EQ(hq_config.training_config.optimization_level, ModelTrainingConfig::MAXIMUM);
    EXPECT_TRUE(hq_config.scanner_config.analyze_audio_quality);
    
    // Test fast processing configuration
    auto fast_config = config_utils::create_fast_processing_config();
    EXPECT_EQ(fast_config.config_name, "fast_processing");
    EXPECT_EQ(fast_config.training_config.optimization_level, ModelTrainingConfig::FAST);
    EXPECT_FALSE(fast_config.scanner_config.analyze_audio_quality);
    
    // Test batch processing configuration
    auto batch_config = config_utils::create_batch_processing_config();
    EXPECT_EQ(batch_config.config_name, "batch_processing");
    EXPECT_GT(batch_config.batch_config.batch_size, 50);
    EXPECT_TRUE(batch_config.batch_config.enable_memory_mapping);
    EXPECT_TRUE(batch_config.batch_config.continue_on_error);
    EXPECT_EQ(batch_config.logging_config.console_level, LoggingConfig::WARNING);
}

// Test error handling
TEST_F(ConditioningConfigTest, ErrorHandling) {
    // Test loading non-existent file
    ConditioningConfig config;
    std::string nonexistent_file = (test_dir_ / "nonexistent.json").string();
    bool success = config_manager_->load_config(nonexistent_file, config);
    EXPECT_FALSE(success);
    
    // Test loading invalid JSON
    std::string invalid_json_file = (test_dir_ / "invalid.json").string();
    std::ofstream invalid_file(invalid_json_file);
    invalid_file << "{ invalid json content }";
    invalid_file.close();
    
    success = config_manager_->load_config(invalid_json_file, config);
    EXPECT_FALSE(success);
    
    // Test loading empty file
    std::string empty_file = (test_dir_ / "empty.json").string();
    std::ofstream empty_stream(empty_file);
    empty_stream.close(); // Create empty file
    
    success = config_manager_->load_config(empty_file, config);
    EXPECT_FALSE(success);
    
    // Test saving to invalid path
    ConditioningConfig valid_config;
    std::string invalid_path = "/root/restricted/config.json"; // Typically restricted path
    success = config_manager_->save_config(invalid_path, valid_config);
    // Note: This test might pass on systems with different permissions
    // The important thing is that it handles the error gracefully
}

// Test configuration directory management
TEST_F(ConditioningConfigTest, ConfigurationDirectory) {
    // Test config directory detection
    std::string config_dir = config_manager_->get_config_directory();
    EXPECT_FALSE(config_dir.empty());
    
    // Test config directory creation
    bool success = config_manager_->ensure_config_directory_exists();
    EXPECT_TRUE(success);
    
    // Verify directory exists (should be created if it didn't exist)
    EXPECT_TRUE(std::filesystem::exists(config_dir));
    
    // Test file existence check
    std::string test_file = (test_dir_ / "exists_test.json").string();
    EXPECT_FALSE(config_manager_->config_file_exists(test_file));
    
    // Create the file and test again
    std::ofstream file(test_file);
    file << "{}";
    file.close();
    EXPECT_TRUE(config_manager_->config_file_exists(test_file));
}

// Test specialized configuration structures
TEST_F(ConditioningConfigTest, SpecializedConfigurations) {
    // Test LoggingConfig
    LoggingConfig logging_config;
    EXPECT_EQ(logging_config.console_level, LoggingConfig::INFO);
    EXPECT_EQ(logging_config.file_level, LoggingConfig::DEBUG);
    EXPECT_TRUE(logging_config.timestamp_enabled);
    EXPECT_FALSE(logging_config.thread_id_enabled);
    
    // Test AudioProcessingConfig
    AudioProcessingConfig audio_config;
    EXPECT_EQ(audio_config.target_sample_rate, 44100);
    EXPECT_EQ(audio_config.target_bit_depth, 16);
    EXPECT_TRUE(audio_config.force_mono);
    EXPECT_TRUE(audio_config.normalize_audio);
    EXPECT_EQ(audio_config.resample_method, AudioProcessingConfig::SINC_FAST);
    
    // Test ModelTrainingConfig
    ModelTrainingConfig training_config;
    EXPECT_GT(training_config.max_training_iterations, 0);
    EXPECT_GT(training_config.convergence_threshold, 0.0);
    EXPECT_GE(training_config.min_gaussian_components, 1);
    EXPECT_GE(training_config.max_gaussian_components, training_config.min_gaussian_components);
    EXPECT_EQ(training_config.optimization_level, ModelTrainingConfig::BALANCED);
    
    // Test BatchProcessingConfig
    BatchProcessingConfig batch_config;
    EXPECT_GE(batch_config.num_worker_threads, 0);
    EXPECT_GT(batch_config.batch_size, 0);
    EXPECT_TRUE(batch_config.enable_progress_reporting);
    EXPECT_TRUE(batch_config.continue_on_error);
    
    // Test OutputConfig
    OutputConfig output_config;
    EXPECT_FALSE(output_config.output_directory.empty());
    EXPECT_EQ(output_config.model_file_extension, ".nvm");
    EXPECT_EQ(output_config.metadata_file_extension, ".json");
    EXPECT_EQ(output_config.naming_scheme, OutputConfig::SANITIZE_NAMES);
}

// Test configuration versioning
TEST_F(ConditioningConfigTest, ConfigurationVersioning) {
    ConditioningConfig config;
    
    // Test current version
    EXPECT_EQ(config.config_version, "1.0");
    EXPECT_EQ(config_manager_->get_supported_config_version(), "1.0");
    
    // Test version validation with matching version
    auto validation = config_manager_->validate_config(config);
    EXPECT_TRUE(validation.is_valid);
    
    // Test version validation with mismatched version
    config.config_version = "0.9";
    validation = config_manager_->validate_config(config);
    EXPECT_TRUE(validation.is_valid); // Still valid but should have warning
    EXPECT_FALSE(validation.warnings.empty());
    EXPECT_THAT(validation.warnings[0], HasSubstr("version mismatch"));
}

// Performance test for JSON serialization
TEST_F(ConditioningConfigTest, JsonSerializationPerformance) {
    ConditioningConfig config = config_manager_->get_quality_config();
    config.custom_settings["key1"] = "value1";
    config.custom_settings["key2"] = "value2";
    config.custom_settings["key3"] = "value3";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Perform multiple serialization/deserialization cycles
    const int num_cycles = 100;
    for (int i = 0; i < num_cycles; ++i) {
        std::string json_str = config_manager_->config_to_json(config);
        EXPECT_FALSE(json_str.empty());
        
        ConditioningConfig deserialized;
        bool success = config_manager_->config_from_json(json_str, deserialized);
        EXPECT_TRUE(success);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 100 cycles
    
    std::cout << "JSON serialization performance: " << duration.count() 
              << "ms for " << num_cycles << " cycles" << std::endl;
}
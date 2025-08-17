#include <gtest/gtest.h>
#include "nexussynth/label_file_generator.h"
#include "nexussynth/context_feature_extractor.h"
#include <fstream>
#include <filesystem>

using namespace nexussynth::context;

class LabelFileGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_labels";
        std::filesystem::create_directories(test_dir_);
        
        // Create sample context features
        createSampleContextFeatures();
        createSampleHMMFeatures();
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    
    void createSampleContextFeatures() {
        // Create 3 sample context features representing a simple phoneme sequence
        context_features_.resize(3);
        
        // Feature 1: "ka"
        context_features_[0].current_timing.phoneme = "ka";
        context_features_[0].current_timing.start_time_ms = 0.0;
        context_features_[0].current_timing.end_time_ms = 150.0;
        context_features_[0].current_timing.duration_ms = 150.0;
        context_features_[0].current_timing.is_valid = true;
        context_features_[0].current_timing.timing_confidence = 1.0;
        context_features_[0].frame_index = 0;
        context_features_[0].frame_time_ms = 0.0;
        
        // Add MIDI information
        context_features_[0].current_midi.note_number = 60; // C4
        context_features_[0].current_midi.velocity = 100;
        context_features_[0].current_midi.frequency_hz = 261.63;
        
        // Add phoneme context (simplified - normally would be classified features)
        context_features_[0].phoneme_context.resize(ContextFeatures::CONTEXT_WINDOW_SIZE);
        context_features_[0].position_context.resize(ContextFeatures::CONTEXT_WINDOW_SIZE);
        
        // Feature 2: "sa"
        context_features_[1].current_timing.phoneme = "sa";
        context_features_[1].current_timing.start_time_ms = 150.0;
        context_features_[1].current_timing.end_time_ms = 300.0;
        context_features_[1].current_timing.duration_ms = 150.0;
        context_features_[1].current_timing.is_valid = true;
        context_features_[1].current_timing.timing_confidence = 1.0;
        context_features_[1].frame_index = 1;
        context_features_[1].frame_time_ms = 150.0;
        
        context_features_[1].current_midi.note_number = 62; // D4
        context_features_[1].current_midi.velocity = 100;
        context_features_[1].current_midi.frequency_hz = 293.66;
        
        context_features_[1].phoneme_context.resize(ContextFeatures::CONTEXT_WINDOW_SIZE);
        context_features_[1].position_context.resize(ContextFeatures::CONTEXT_WINDOW_SIZE);
        
        // Feature 3: "ki"
        context_features_[2].current_timing.phoneme = "ki";
        context_features_[2].current_timing.start_time_ms = 300.0;
        context_features_[2].current_timing.end_time_ms = 450.0;
        context_features_[2].current_timing.duration_ms = 150.0;
        context_features_[2].current_timing.is_valid = true;
        context_features_[2].current_timing.timing_confidence = 1.0;
        context_features_[2].frame_index = 2;
        context_features_[2].frame_time_ms = 300.0;
        
        context_features_[2].current_midi.note_number = 64; // E4
        context_features_[2].current_midi.velocity = 100;
        context_features_[2].current_midi.frequency_hz = 329.63;
        
        context_features_[2].phoneme_context.resize(ContextFeatures::CONTEXT_WINDOW_SIZE);
        context_features_[2].position_context.resize(ContextFeatures::CONTEXT_WINDOW_SIZE);
    }
    
    void createSampleHMMFeatures() {
        // Create corresponding HMM context features
        hmm_features_.resize(3);
        timing_info_.resize(3);
        
        // HMM Feature 1: "ka"
        hmm_features_[0].current_phoneme = "ka";
        hmm_features_[0].left_phoneme = "sil";
        hmm_features_[0].right_phoneme = "sa";
        hmm_features_[0].left_left_phoneme = "sil";
        hmm_features_[0].right_right_phoneme = "ki";
        hmm_features_[0].position_in_syllable = 1;
        hmm_features_[0].syllable_length = 1;
        hmm_features_[0].position_in_word = 1;
        hmm_features_[0].word_length = 3;
        hmm_features_[0].pitch_cents = 0.0; // C4 reference
        hmm_features_[0].note_duration_ms = 150.0;
        
        timing_info_[0].phoneme = "ka";
        timing_info_[0].start_time_ms = 0.0;
        timing_info_[0].end_time_ms = 150.0;
        timing_info_[0].duration_ms = 150.0;
        timing_info_[0].is_valid = true;
        timing_info_[0].timing_confidence = 1.0;
        
        // HMM Feature 2: "sa"
        hmm_features_[1].current_phoneme = "sa";
        hmm_features_[1].left_phoneme = "ka";
        hmm_features_[1].right_phoneme = "ki";
        hmm_features_[1].left_left_phoneme = "sil";
        hmm_features_[1].right_right_phoneme = "sil";
        hmm_features_[1].position_in_syllable = 1;
        hmm_features_[1].syllable_length = 1;
        hmm_features_[1].position_in_word = 2;
        hmm_features_[1].word_length = 3;
        hmm_features_[1].pitch_cents = 200.0; // D4 (roughly)
        hmm_features_[1].note_duration_ms = 150.0;
        
        timing_info_[1].phoneme = "sa";
        timing_info_[1].start_time_ms = 150.0;
        timing_info_[1].end_time_ms = 300.0;
        timing_info_[1].duration_ms = 150.0;
        timing_info_[1].is_valid = true;
        timing_info_[1].timing_confidence = 1.0;
        
        // HMM Feature 3: "ki"
        hmm_features_[2].current_phoneme = "ki";
        hmm_features_[2].left_phoneme = "sa";
        hmm_features_[2].right_phoneme = "sil";
        hmm_features_[2].left_left_phoneme = "ka";
        hmm_features_[2].right_right_phoneme = "sil";
        hmm_features_[2].position_in_syllable = 1;
        hmm_features_[2].syllable_length = 1;
        hmm_features_[2].position_in_word = 3;
        hmm_features_[2].word_length = 3;
        hmm_features_[2].pitch_cents = 400.0; // E4 (roughly)
        hmm_features_[2].note_duration_ms = 150.0;
        
        timing_info_[2].phoneme = "ki";
        timing_info_[2].start_time_ms = 300.0;
        timing_info_[2].end_time_ms = 450.0;
        timing_info_[2].duration_ms = 150.0;
        timing_info_[2].is_valid = true;
        timing_info_[2].timing_confidence = 1.0;
    }
    
    std::string test_dir_;
    std::vector<ContextFeatures> context_features_;
    std::vector<nexussynth::hmm::ContextFeatureVector> hmm_features_;
    std::vector<PhonemeTimingInfo> timing_info_;
};

TEST_F(LabelFileGeneratorTest, BasicGeneration) {
    LabelFileGenerator generator;
    std::string output_file = test_dir_ + "/test_basic.lab";
    
    bool success = generator.generateFromHMMFeatures(hmm_features_, timing_info_, output_file);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::filesystem::exists(output_file));
    
    // Verify file content
    std::ifstream file(output_file);
    EXPECT_TRUE(file.is_open());
    
    std::string line;
    int line_count = 0;
    while (std::getline(file, line)) {
        EXPECT_FALSE(line.empty());
        line_count++;
    }
    
    EXPECT_EQ(line_count, 3); // Should have 3 label entries
}

TEST_F(LabelFileGeneratorTest, ValidationTest) {
    LabelFileGenerator generator;
    std::string output_file = test_dir_ + "/test_validation.lab";
    
    // Generate file
    bool success = generator.generateFromHMMFeatures(hmm_features_, timing_info_, output_file);
    EXPECT_TRUE(success);
    
    // Validate the generated file
    auto validation_result = generator.validateLabelFile(output_file);
    EXPECT_TRUE(validation_result.is_valid);
    EXPECT_EQ(validation_result.total_entries, 3);
    EXPECT_NEAR(validation_result.total_duration_ms, 450.0, 1.0);
    EXPECT_TRUE(validation_result.errors.empty());
}

TEST_F(LabelFileGeneratorTest, ReadWriteConsistency) {
    LabelFileGenerator generator;
    std::string output_file = test_dir_ + "/test_consistency.lab";
    
    // Generate file
    bool success = generator.generateFromHMMFeatures(hmm_features_, timing_info_, output_file);
    EXPECT_TRUE(success);
    
    // Read back the file
    auto entries_opt = generator.readLabelFile(output_file);
    EXPECT_TRUE(entries_opt.has_value());
    
    const auto& entries = *entries_opt;
    EXPECT_EQ(entries.size(), 3);
    
    // Check timing consistency
    EXPECT_NEAR(entries[0].start_time_ms, 0.0, 1.0);
    EXPECT_NEAR(entries[0].end_time_ms, 150.0, 1.0);
    EXPECT_NEAR(entries[1].start_time_ms, 150.0, 1.0);
    EXPECT_NEAR(entries[1].end_time_ms, 300.0, 1.0);
    EXPECT_NEAR(entries[2].start_time_ms, 300.0, 1.0);
    EXPECT_NEAR(entries[2].end_time_ms, 450.0, 1.0);
    
    // Check that labels are not empty and contain HTS format markers
    for (const auto& entry : entries) {
        EXPECT_FALSE(entry.hts_label.empty());
        EXPECT_NE(entry.hts_label.find("/"), std::string::npos); // Should contain HTS format markers
    }
}

TEST_F(LabelFileGeneratorTest, ConfigurationTest) {
    LabelFileGenerator::GenerationConfig config;
    config.include_timing = false;
    config.validate_timing = false;
    config.time_format = "seconds";
    
    LabelFileGenerator generator(config);
    std::string output_file = test_dir_ + "/test_config.lab";
    
    bool success = generator.generateFromHMMFeatures(hmm_features_, timing_info_, output_file);
    EXPECT_TRUE(success);
    
    // Read and verify format
    std::ifstream file(output_file);
    std::string line;
    std::getline(file, line);
    
    // Without timing, line should start with label content, not numbers
    EXPECT_FALSE(std::isdigit(line[0]));
}

TEST_F(LabelFileGeneratorTest, FileStatistics) {
    LabelFileGenerator generator;
    std::string output_file = test_dir_ + "/test_stats.lab";
    
    generator.generateFromHMMFeatures(hmm_features_, timing_info_, output_file);
    
    auto stats = generator.analyzeLabFile(output_file);
    EXPECT_EQ(stats.total_entries, 3);
    EXPECT_NEAR(stats.total_duration_ms, 450.0, 1.0);
    EXPECT_NEAR(stats.avg_phoneme_duration_ms, 150.0, 1.0);
    EXPECT_EQ(stats.min_phoneme_duration_ms, 150.0);
    EXPECT_EQ(stats.max_phoneme_duration_ms, 150.0);
    EXPECT_GE(stats.unique_phonemes.size(), 1); // Should detect at least some phonemes
}

TEST_F(LabelFileGeneratorTest, ErrorHandling) {
    LabelFileGenerator generator;
    
    // Test with empty features
    std::vector<nexussynth::hmm::ContextFeatureVector> empty_features;
    std::vector<PhonemeTimingInfo> empty_timing;
    
    std::string output_file = test_dir_ + "/test_empty.lab";
    bool success = generator.generateFromHMMFeatures(empty_features, empty_timing, output_file);
    EXPECT_TRUE(success); // Should succeed but create empty file
    
    auto validation_result = generator.validateLabelFile(output_file);
    EXPECT_FALSE(validation_result.errors.empty() || !validation_result.warnings.empty()); // Should have warnings about empty file
}

TEST_F(LabelFileGeneratorTest, LabelUtilsQualityAssessment) {
    LabelFileGenerator generator;
    std::string output_file = test_dir_ + "/test_quality.lab";
    
    generator.generateFromHMMFeatures(hmm_features_, timing_info_, output_file);
    
    auto quality = label_utils::assessQuality(output_file);
    EXPECT_GT(quality.timing_accuracy, 0.5);  // Should have reasonable timing
    EXPECT_GT(quality.label_consistency, 0.5); // Should have consistent labels
    EXPECT_GT(quality.overall_quality, 0.5);   // Should have decent overall quality
}

// Performance test for large datasets
TEST_F(LabelFileGeneratorTest, DISABLED_PerformanceTest) {
    // Create larger dataset for performance testing
    std::vector<nexussynth::hmm::ContextFeatureVector> large_features(1000);
    std::vector<PhonemeTimingInfo> large_timing(1000);
    
    // Initialize with dummy data
    for (size_t i = 0; i < 1000; ++i) {
        large_features[i].current_phoneme = "a";
        large_features[i].note_duration_ms = 100.0;
        
        large_timing[i].phoneme = "a";
        large_timing[i].start_time_ms = i * 100.0;
        large_timing[i].end_time_ms = (i + 1) * 100.0;
        large_timing[i].duration_ms = 100.0;
        large_timing[i].is_valid = true;
        large_timing[i].timing_confidence = 1.0;
    }
    
    LabelFileGenerator generator;
    std::string output_file = test_dir_ + "/test_large.lab";
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = generator.generateFromHMMFeatures(large_features, large_timing, output_file);
    auto end = std::chrono::high_resolution_clock::now();
    
    EXPECT_TRUE(success);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Generated 1000 labels in " << duration.count() << "ms" << std::endl;
    
    // Should complete in reasonable time (less than 1 second for 1000 entries)
    EXPECT_LT(duration.count(), 1000);
}
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include "nexussynth/utau_flag_converter.h"

using namespace nexussynth::utau;

class UtauFlagConverterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up default converter with standard configuration
        default_config_ = ConversionConfig{};
        default_config_.voice_type = VoiceType::UNKNOWN;
        default_config_.enable_cross_flag_interaction = true;
        default_config_.enable_safety_limiting = true;
        
        converter_ = std::make_unique<UtauFlagConverter>(default_config_);
    }
    
    void TearDown() override {
        converter_.reset();
    }
    
    // Helper functions for testing
    FlagValues create_flags(int g = 0, int t = 0, int bre = 0, int bri = 0) {
        FlagValues flags;
        flags.g = g;
        flags.t = t;
        flags.bre = bre;
        flags.bri = bri;
        return flags;
    }
    
    bool is_approximately_equal(double a, double b, double tolerance = 0.01) {
        return std::abs(a - b) <= tolerance;
    }
    
    void validate_params_in_range(const NexusSynthParams& params) {
        EXPECT_TRUE(params.is_valid()) << "Parameters should be within valid ranges";
        EXPECT_GT(params.formant_shift_factor, 0.1) << "Formant shift factor too low";
        EXPECT_LT(params.formant_shift_factor, 3.0) << "Formant shift factor too high";
        EXPECT_GE(params.breathiness_level, 0.0) << "Breathiness level below minimum";
        EXPECT_LE(params.breathiness_level, 1.0) << "Breathiness level above maximum";
        EXPECT_GE(params.tension_factor, -1.0) << "Tension factor below minimum";
        EXPECT_LE(params.tension_factor, 1.0) << "Tension factor above maximum";
    }

protected:
    ConversionConfig default_config_;
    std::unique_ptr<UtauFlagConverter> converter_;
};

// Test basic flag conversion functionality
TEST_F(UtauFlagConverterTest, DefaultConstructor) {
    UtauFlagConverter default_converter;
    auto flags = create_flags();
    auto params = default_converter.convert(flags);
    
    // Default flags should produce neutral parameters
    EXPECT_DOUBLE_EQ(params.formant_shift_factor, 1.0);
    EXPECT_DOUBLE_EQ(params.brightness_gain, 1.0);
    EXPECT_DOUBLE_EQ(params.breathiness_level, 0.0);
    EXPECT_DOUBLE_EQ(params.tension_factor, 0.0);
}

TEST_F(UtauFlagConverterTest, SingleFlagConversion_G) {
    // Test positive g flag
    auto flags_pos = create_flags(50, 0, 0, 0);
    auto params_pos = converter_->convert(flags_pos);
    validate_params_in_range(params_pos);
    EXPECT_GT(params_pos.formant_shift_factor, 1.0) << "Positive g flag should increase formant shift";
    
    // Test negative g flag
    auto flags_neg = create_flags(-50, 0, 0, 0);
    auto params_neg = converter_->convert(flags_neg);
    validate_params_in_range(params_neg);
    EXPECT_LT(params_neg.formant_shift_factor, 1.0) << "Negative g flag should decrease formant shift";
    
    // Test symmetry (approximately)
    double pos_deviation = std::abs(params_pos.formant_shift_factor - 1.0);
    double neg_deviation = std::abs(params_neg.formant_shift_factor - 1.0);
    EXPECT_TRUE(is_approximately_equal(pos_deviation, neg_deviation, 0.1)) 
        << "G flag response should be approximately symmetric";
}

TEST_F(UtauFlagConverterTest, SingleFlagConversion_T) {
    // Test positive t flag
    auto flags_pos = create_flags(0, 60, 0, 0);
    auto params_pos = converter_->convert(flags_pos);
    validate_params_in_range(params_pos);
    EXPECT_GT(params_pos.tension_factor, 0.0) << "Positive t flag should increase tension";
    
    // Test negative t flag
    auto flags_neg = create_flags(0, -60, 0, 0);
    auto params_neg = converter_->convert(flags_neg);
    validate_params_in_range(params_neg);
    EXPECT_LT(params_neg.tension_factor, 0.0) << "Negative t flag should decrease tension";
}

TEST_F(UtauFlagConverterTest, SingleFlagConversion_Bre) {
    // Test breathiness flag
    auto flags = create_flags(0, 0, 70, 0);
    auto params = converter_->convert(flags);
    validate_params_in_range(params);
    EXPECT_GT(params.breathiness_level, 0.0) << "Bre flag should increase breathiness";
    EXPECT_LT(params.breathiness_level, 1.0) << "Breathiness should not exceed maximum";
    
    // Test zero breathiness
    auto flags_zero = create_flags(0, 0, 0, 0);
    auto params_zero = converter_->convert(flags_zero);
    EXPECT_EQ(params_zero.breathiness_level, 0.0) << "Zero bre flag should produce zero breathiness";
}

TEST_F(UtauFlagConverterTest, SingleFlagConversion_Bri) {
    // Test positive brightness
    auto flags_pos = create_flags(0, 0, 0, 40);
    auto params_pos = converter_->convert(flags_pos);
    validate_params_in_range(params_pos);
    EXPECT_GT(params_pos.brightness_gain, 1.0) << "Positive bri flag should increase brightness";
    
    // Test negative brightness
    auto flags_neg = create_flags(0, 0, 0, -40);
    auto params_neg = converter_->convert(flags_neg);
    validate_params_in_range(params_neg);
    EXPECT_LT(params_neg.brightness_gain, 1.0) << "Negative bri flag should decrease brightness";
}

TEST_F(UtauFlagConverterTest, ExtremeFlagValues) {
    // Test maximum positive values
    auto flags_max = create_flags(100, 100, 100, 100);
    auto params_max = converter_->convert(flags_max);
    validate_params_in_range(params_max);
    
    // Test maximum negative values (where applicable)
    auto flags_min = create_flags(-100, -100, 0, -100);  // bre can't be negative
    auto params_min = converter_->convert(flags_min);
    validate_params_in_range(params_min);
    
    // Ensure extreme values are handled safely
    EXPECT_LT(params_max.formant_shift_factor, 3.0) << "Extreme g flag should be limited";
    EXPECT_GT(params_min.formant_shift_factor, 0.1) << "Extreme negative g flag should be limited";
}

TEST_F(UtauFlagConverterTest, FlagInteractions) {
    // Enable flag interactions for this test
    ConversionConfig interaction_config = default_config_;
    interaction_config.enable_cross_flag_interaction = true;
    UtauFlagConverter interaction_converter(interaction_config);
    
    // Test g+t interaction (should increase harmonic emphasis)
    auto flags_gt = create_flags(40, 40, 0, 0);
    auto params_gt = interaction_converter.convert(flags_gt);
    validate_params_in_range(params_gt);
    EXPECT_GT(params_gt.harmonic_emphasis, 0.0) << "G+T combination should increase harmonic emphasis";
    
    // Test bre+t interaction (conflicting characteristics)
    auto flags_bret = create_flags(0, 50, 60, 0);
    auto params_bret = interaction_converter.convert(flags_bret);
    validate_params_in_range(params_bret);
    
    // Compare with non-interacting conversion
    ConversionConfig no_interaction_config = default_config_;
    no_interaction_config.enable_cross_flag_interaction = false;
    UtauFlagConverter no_interaction_converter(no_interaction_config);
    auto params_no_interaction = no_interaction_converter.convert(flags_bret);
    
    // Interacting version should have modified values
    EXPECT_NE(params_bret.breathiness_level, params_no_interaction.breathiness_level) 
        << "Flag interaction should modify breathiness";
}

TEST_F(UtauFlagConverterTest, VoiceTypeAdjustments) {
    auto test_flags = create_flags(50, 30, 20, 40);
    
    // Test male voice adjustments
    auto params_male = converter_->convert_with_context(test_flags, VoiceType::MALE_ADULT, 120.0);
    validate_params_in_range(params_male);
    
    // Test female voice adjustments
    auto params_female = converter_->convert_with_context(test_flags, VoiceType::FEMALE_ADULT, 250.0);
    validate_params_in_range(params_female);
    
    // Test child voice adjustments
    auto params_child = converter_->convert_with_context(test_flags, VoiceType::CHILD, 350.0);
    validate_params_in_range(params_child);
    
    // Voice types should produce different results
    EXPECT_NE(params_male.formant_shift_factor, params_female.formant_shift_factor)
        << "Male and female voice types should handle formant shifts differently";
    
    // Child voice should be more conservative
    EXPECT_LT(std::abs(params_child.formant_shift_factor - 1.0), 
              std::abs(params_male.formant_shift_factor - 1.0))
        << "Child voice should have more conservative formant shifts";
}

TEST_F(UtauFlagConverterTest, VoiceTypeDetection) {
    // Test male voice detection
    auto male_type = UtauFlagConverter::detect_voice_type(110.0, 1200.0, 0.8);
    EXPECT_EQ(male_type, VoiceType::MALE_ADULT);
    
    // Test female voice detection
    auto female_type = UtauFlagConverter::detect_voice_type(220.0, 2000.0, 0.7);
    EXPECT_EQ(female_type, VoiceType::FEMALE_ADULT);
    
    // Test child voice detection
    auto child_type = UtauFlagConverter::detect_voice_type(400.0, 3500.0, 0.8);
    EXPECT_EQ(child_type, VoiceType::CHILD);
    
    // Test whisper voice detection
    auto whisper_type = UtauFlagConverter::detect_voice_type(180.0, 2200.0, 0.2);
    EXPECT_EQ(whisper_type, VoiceType::WHISPER);
}

TEST_F(UtauFlagConverterTest, SafetyLimiting) {
    ConversionConfig unsafe_config = default_config_;
    unsafe_config.enable_safety_limiting = false;
    unsafe_config.max_formant_shift = 5.0;  // Very high limit
    unsafe_config.max_brightness_change = 10.0;
    UtauFlagConverter unsafe_converter(unsafe_config);
    
    ConversionConfig safe_config = default_config_;
    safe_config.enable_safety_limiting = true;
    safe_config.max_formant_shift = 1.8;  // Conservative limit
    safe_config.max_brightness_change = 2.5;
    UtauFlagConverter safe_converter(safe_config);
    
    auto extreme_flags = create_flags(100, 100, 100, 100);
    
    auto unsafe_params = unsafe_converter.convert(extreme_flags);
    auto safe_params = safe_converter.convert(extreme_flags);
    
    // Safe converter should limit extreme values
    EXPECT_LT(safe_params.formant_shift_factor, unsafe_params.formant_shift_factor)
        << "Safety limiting should reduce extreme formant shifts";
    EXPECT_LT(safe_params.brightness_gain, unsafe_params.brightness_gain)
        << "Safety limiting should reduce extreme brightness changes";
}

TEST_F(UtauFlagConverterTest, ParameterInterpolation) {
    auto from_flags = create_flags(0, 0, 0, 0);
    auto to_flags = create_flags(50, 30, 40, 20);
    
    // Test interpolation at various points
    auto params_0 = converter_->interpolate_conversion(from_flags, to_flags, 0.0);
    auto params_50 = converter_->interpolate_conversion(from_flags, to_flags, 0.5);
    auto params_100 = converter_->interpolate_conversion(from_flags, to_flags, 1.0);
    
    // Verify interpolation bounds
    EXPECT_DOUBLE_EQ(params_0.formant_shift_factor, 1.0);  // Should match from_flags (neutral)
    
    // Middle interpolation should be between start and end
    EXPECT_GT(params_50.formant_shift_factor, params_0.formant_shift_factor);
    EXPECT_LT(params_50.formant_shift_factor, params_100.formant_shift_factor);
    
    // Test edge cases
    auto params_negative = converter_->interpolate_conversion(from_flags, to_flags, -0.1);
    auto params_over = converter_->interpolate_conversion(from_flags, to_flags, 1.1);
    
    // Should clamp to valid range
    EXPECT_DOUBLE_EQ(params_negative.formant_shift_factor, params_0.formant_shift_factor);
    EXPECT_DOUBLE_EQ(params_over.formant_shift_factor, params_100.formant_shift_factor);
}

TEST_F(UtauFlagConverterTest, ConversionAnalysis) {
    auto flags = create_flags(40, -30, 50, 20);
    auto params = converter_->convert(flags);
    auto analysis = converter_->analyze_conversion(flags, params);
    
    // Basic analysis checks
    EXPECT_GE(analysis.conversion_fidelity, 0.0) << "Conversion fidelity should be non-negative";
    EXPECT_LE(analysis.conversion_fidelity, 1.0) << "Conversion fidelity should not exceed 1.0";
    EXPECT_GE(analysis.parameter_stability, 0.0) << "Parameter stability should be non-negative";
    EXPECT_LE(analysis.parameter_stability, 1.0) << "Parameter stability should not exceed 1.0";
    
    // Check flag contributions are recorded
    EXPECT_EQ(analysis.flag_contributions.size(), 4) << "Should have contributions for all 4 flags";
    EXPECT_GT(analysis.flag_contributions.at("g"), 0.0) << "G flag should have positive contribution";
    EXPECT_GT(analysis.flag_contributions.at("bre"), 0.0) << "Bre flag should have positive contribution";
}

TEST_F(UtauFlagConverterTest, ConversionReport) {
    auto flags = create_flags(30, -20, 40, 15);
    auto params = converter_->convert(flags);
    auto report = converter_->generate_conversion_report(flags, params);
    
    // Report should contain key information
    EXPECT_TRUE(report.find("Input Flags") != std::string::npos) << "Report should contain input flags section";
    EXPECT_TRUE(report.find("Converted Parameters") != std::string::npos) << "Report should contain converted parameters section";
    EXPECT_TRUE(report.find("g: 30") != std::string::npos) << "Report should show g flag value";
    EXPECT_TRUE(report.find("t: -20") != std::string::npos) << "Report should show t flag value";
    EXPECT_TRUE(report.find("Formant Shift") != std::string::npos) << "Report should show formant shift parameter";
}

TEST_F(UtauFlagConverterTest, CustomFlagSupport) {
    FlagValues flags;
    flags.g = 20;
    flags.t = 0;
    flags.bre = 0;
    flags.bri = 0;
    flags.custom_flags["vel"] = 120;  // Custom velocity flag
    flags.custom_flags["dyn"] = -10;  // Custom dynamics flag
    
    auto params = converter_->convert(flags);
    validate_params_in_range(params);
    
    // Custom flags shouldn't break conversion
    EXPECT_GT(params.formant_shift_factor, 1.0) << "Standard g flag should still work with custom flags present";
}

TEST_F(UtauFlagConverterTest, ConversionTests) {
    auto test_results = converter_->run_conversion_tests();
    
    EXPECT_GT(test_results.size(), 0) << "Should run multiple conversion tests";
    
    // All tests should have valid analysis results
    for (const auto& result : test_results) {
        EXPECT_GE(result.conversion_fidelity, 0.0) << "All test results should have valid fidelity scores";
        EXPECT_LE(result.conversion_fidelity, 1.0) << "All test results should have valid fidelity scores";
        EXPECT_GE(result.parameter_stability, 0.0) << "All test results should have valid stability scores";
        EXPECT_LE(result.parameter_stability, 1.0) << "All test results should have valid stability scores";
    }
}

TEST_F(UtauFlagConverterTest, PbpConfigApplication) {
    auto flags = create_flags(30, 40, 60, 20);
    auto params = converter_->convert(flags);
    
    synthesis::PbpConfig config;
    params.apply_to_pbp_config(config);
    
    // Verify that parameters are applied to config
    // High breathiness should enable Gaussian windowing
    if (params.breathiness_level > 0.3) {
        EXPECT_EQ(config.window_type, synthesis::PbpConfig::WindowType::GAUSSIAN) 
            << "High breathiness should enable Gaussian windowing";
        EXPECT_TRUE(config.enable_adaptive_windowing) 
            << "High breathiness should enable adaptive windowing";
    }
    
    // High tension should enable pre-echo minimization
    if (params.tension_factor > 0.5) {
        EXPECT_TRUE(config.minimize_pre_echo) 
            << "High tension should enable pre-echo minimization";
    }
}

// Performance and utility tests
class UtauFlagConverterUtilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand(12345);  // Fixed seed for reproducible tests
    }
};

TEST_F(UtauFlagConverterUtilityTest, VoiceTypeConfigCreation) {
    // Test config creation for different voice types
    auto male_config = FlagConversionUtils::create_voice_type_config(VoiceType::MALE_ADULT);
    EXPECT_EQ(male_config.voice_type, VoiceType::MALE_ADULT);
    EXPECT_LT(male_config.g_sensitivity, 1.0) << "Male voices should be less sensitive to g flag";
    
    auto female_config = FlagConversionUtils::create_voice_type_config(VoiceType::FEMALE_ADULT);
    EXPECT_EQ(female_config.voice_type, VoiceType::FEMALE_ADULT);
    EXPECT_GT(female_config.g_sensitivity, 1.0) << "Female voices should be more sensitive to g flag";
    
    auto child_config = FlagConversionUtils::create_voice_type_config(VoiceType::CHILD);
    EXPECT_EQ(child_config.voice_type, VoiceType::CHILD);
    EXPECT_TRUE(child_config.preserve_naturalness) << "Child voice config should preserve naturalness";
    EXPECT_TRUE(child_config.enable_safety_limiting) << "Child voice config should enable safety limiting";
}

TEST_F(UtauFlagConverterUtilityTest, PerformanceBenchmark) {
    auto benchmark = FlagConversionUtils::benchmark_conversion_performance(100);
    
    EXPECT_GT(benchmark.conversions_per_second, 100.0) 
        << "Conversion should be fast enough for real-time use";
    EXPECT_LT(benchmark.average_conversion_time_us, 10000.0) 
        << "Average conversion time should be reasonable";
    EXPECT_GT(benchmark.memory_usage_bytes, 0) 
        << "Memory usage should be tracked";
}

TEST_F(UtauFlagConverterUtilityTest, CompatibilityValidation) {
    UtauFlagConverter reference_converter;
    UtauFlagConverter test_converter;
    
    std::vector<FlagValues> test_cases = {
        {{20, 0, 0, 0}},
        {{0, 30, 0, 0}},
        {{0, 0, 40, 0}},
        {{0, 0, 0, 25}},
        {{10, 15, 20, 5}}
    };
    
    bool compatible = FlagConversionUtils::validate_conversion_compatibility(
        reference_converter, test_cases);
    
    EXPECT_TRUE(compatible) << "Converter should be compatible with itself";
}

// Edge case and error handling tests
class UtauFlagConverterEdgeCaseTest : public ::testing::Test {};

TEST_F(UtauFlagConverterEdgeCaseTest, InvalidParameterHandling) {
    ConversionConfig config;
    config.max_formant_shift = -1.0;  // Invalid negative limit
    config.max_brightness_change = 0.0;  // Invalid zero limit
    
    // Constructor should handle invalid config gracefully
    EXPECT_NO_THROW(UtauFlagConverter converter(config));
}

TEST_F(UtauFlagConverterEdgeCaseTest, LargeFlagValues) {
    UtauFlagConverter converter;
    
    // Test with values beyond normal UTAU range
    FlagValues large_flags;
    large_flags.g = 500;    // Way beyond ±100 range
    large_flags.t = -300;
    large_flags.bre = 200;
    large_flags.bri = 1000;
    
    auto params = converter.convert(large_flags);
    
    // Should still produce valid parameters
    EXPECT_TRUE(params.is_valid()) << "Converter should handle large flag values gracefully";
}

TEST_F(UtauFlagConverterEdgeCaseTest, ZeroBaseFrequency) {
    UtauFlagConverter converter;
    auto flags = create_flags(50, 30, 0, 0);
    
    // Test with zero base frequency
    EXPECT_NO_THROW(converter.convert_with_context(flags, VoiceType::UNKNOWN, 0.0));
    
    // Test with negative base frequency
    EXPECT_NO_THROW(converter.convert_with_context(flags, VoiceType::UNKNOWN, -100.0));
}

// Helper function for edge case tests
FlagValues create_flags(int g = 0, int t = 0, int bre = 0, int bri = 0) {
    FlagValues flags;
    flags.g = g;
    flags.t = t;
    flags.bre = bre;
    flags.bri = bri;
    return flags;
}
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include "nexussynth/utau_argument_parser.h"

using namespace nexussynth::utau;

class UtauArgumentParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "nexussynth_test";
        std::filesystem::create_directories(test_dir_);
        
        // Create test WAV files
        test_input_wav_ = test_dir_ / "input.wav";
        test_output_wav_ = test_dir_ / "output.wav";
        
        create_dummy_wav_file(test_input_wav_);
        
        parser_.set_debug_mode(false);
        parser_.set_strict_validation(false); // For basic tests
    }
    
    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir_);
    }
    
    void create_dummy_wav_file(const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary);
        
        // Write minimal WAV header (44 bytes)
        const unsigned char wav_header[] = {
            'R', 'I', 'F', 'F',  // ChunkID
            0x24, 0x00, 0x00, 0x00,  // ChunkSize (36 bytes)
            'W', 'A', 'V', 'E',  // Format
            'f', 'm', 't', ' ',  // Subchunk1ID
            0x10, 0x00, 0x00, 0x00,  // Subchunk1Size (16)
            0x01, 0x00,  // AudioFormat (PCM)
            0x01, 0x00,  // NumChannels (1)
            0x44, 0xAC, 0x00, 0x00,  // SampleRate (44100)
            0x88, 0x58, 0x01, 0x00,  // ByteRate
            0x02, 0x00,  // BlockAlign
            0x10, 0x00,  // BitsPerSample (16)
            'd', 'a', 't', 'a',  // Subchunk2ID
            0x00, 0x00, 0x00, 0x00   // Subchunk2Size (0)
        };
        
        file.write(reinterpret_cast<const char*>(wav_header), sizeof(wav_header));
        file.close();
    }
    
    std::vector<std::string> create_basic_args(const std::string& additional_flags = "") {
        return {
            "resampler.exe",
            test_input_wav_.string(),
            test_output_wav_.string(),
            "0",      // pitch
            "100",    // velocity
            additional_flags  // flags
        };
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path test_input_wav_;
    std::filesystem::path test_output_wav_;
    UtauArgumentParser parser_;
};

// Basic argument parsing tests
TEST_F(UtauArgumentParserTest, BasicArgumentParsing) {
    auto args = create_basic_args();
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.input_path, test_input_wav_);
    EXPECT_EQ(result.output_path, test_output_wav_);
    EXPECT_EQ(result.pitch, 0);
    EXPECT_EQ(result.velocity, 100);
    EXPECT_EQ(result.flags_string, "");
    
    // Check defaults for optional parameters
    EXPECT_EQ(result.offset, 0);
    EXPECT_EQ(result.length, 0);
    EXPECT_EQ(result.consonant, 0);
    EXPECT_EQ(result.cutoff, 0);
    EXPECT_EQ(result.volume, 0);
    EXPECT_EQ(result.start, 0);
    EXPECT_EQ(result.end, 100);
}

TEST_F(UtauArgumentParserTest, MinimumArgumentCount) {
    // Test with insufficient arguments
    std::vector<std::string> insufficient_args = {"resampler.exe", "input.wav"};
    auto result = parser_.parse(insufficient_args);
    
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(UtauArgumentParserTest, AllArgumentsParsing) {
    std::vector<std::string> complete_args = {
        "resampler.exe",
        test_input_wav_.string(),
        test_output_wav_.string(),
        "200",     // pitch
        "150",     // velocity
        "g+50t-20", // flags
        "1000",    // offset
        "44100",   // length
        "2205",    // consonant
        "-500",    // cutoff
        "3",       // volume
        "10",      // start
        "90"       // end
    };
    
    auto result = parser_.parse(complete_args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.pitch, 200);
    EXPECT_EQ(result.velocity, 150);
    EXPECT_EQ(result.offset, 1000);
    EXPECT_EQ(result.length, 44100);
    EXPECT_EQ(result.consonant, 2205);
    EXPECT_EQ(result.cutoff, -500);
    EXPECT_EQ(result.volume, 3);
    EXPECT_EQ(result.start, 10);
    EXPECT_EQ(result.end, 90);
}

// Flag parsing tests
TEST_F(UtauArgumentParserTest, BasicFlagParsing) {
    auto args = create_basic_args("g+50");
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.flag_values.g, 50);
    EXPECT_EQ(result.flag_values.t, 0);
    EXPECT_EQ(result.flag_values.bre, 0);
    EXPECT_EQ(result.flag_values.bri, 0);
}

TEST_F(UtauArgumentParserTest, MultipleFlagParsing) {
    auto args = create_basic_args("g+30t-15bre20bri-10");
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.flag_values.g, 30);
    EXPECT_EQ(result.flag_values.t, -15);
    EXPECT_EQ(result.flag_values.bre, 20);
    EXPECT_EQ(result.flag_values.bri, -10);
}

TEST_F(UtauArgumentParserTest, FlagRangeClampingG) {
    auto args = create_basic_args("g+150");  // Over maximum
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.flag_values.g, 100);  // Should be clamped to max
}

TEST_F(UtauArgumentParserTest, FlagRangeClampingT) {
    auto args = create_basic_args("t-150");  // Under minimum
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.flag_values.t, -100);  // Should be clamped to min
}

TEST_F(UtauArgumentParserTest, FlagRangeClampingBre) {
    auto args = create_basic_args("bre150");  // Over maximum for bre
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.flag_values.bre, 100);  // Should be clamped to max
}

TEST_F(UtauArgumentParserTest, CustomFlagParsing) {
    auto args = create_basic_args("g+50custom123");
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.flag_values.g, 50);
    EXPECT_EQ(result.flag_values.custom_flags["custom"], 123);
}

// Static utility function tests
TEST_F(UtauArgumentParserTest, StaticFlagParsingUtility) {
    auto flags = UtauArgumentParser::parse_flags("g+50t-20bre30");
    
    EXPECT_EQ(flags.g, 50);
    EXPECT_EQ(flags.t, -20);
    EXPECT_EQ(flags.bre, 30);
    EXPECT_EQ(flags.bri, 0);
}

TEST_F(UtauArgumentParserTest, FlagValidationUtility) {
    EXPECT_TRUE(UtauArgumentParser::is_valid_flag_format("g+50"));
    EXPECT_TRUE(UtauArgumentParser::is_valid_flag_format("g+50t-20"));
    EXPECT_TRUE(UtauArgumentParser::is_valid_flag_format(""));
    EXPECT_FALSE(UtauArgumentParser::is_valid_flag_format("invalid"));
    EXPECT_FALSE(UtauArgumentParser::is_valid_flag_format("g+"));
}

// Path handling tests
TEST_F(UtauArgumentParserTest, PathNormalization) {
    auto normalized = UtauArgumentParser::normalize_path("./test/../input.wav");
    EXPECT_TRUE(normalized.is_absolute());
}

TEST_F(UtauArgumentParserTest, WAVPathValidation) {
    EXPECT_TRUE(UtauArgumentParser::is_valid_wav_path("test.wav"));
    EXPECT_TRUE(UtauArgumentParser::is_valid_wav_path("TEST.WAV"));
    EXPECT_FALSE(UtauArgumentParser::is_valid_wav_path("test.mp3"));
    EXPECT_FALSE(UtauArgumentParser::is_valid_wav_path("test"));
}

// Encoding tests
TEST_F(UtauArgumentParserTest, UTF8Conversion) {
    std::string test_string = "hello world";
    auto converted = UtauArgumentParser::convert_to_utf8(test_string);
    EXPECT_EQ(converted, test_string);  // Should be unchanged for ASCII
}

TEST_F(UtauArgumentParserTest, WideStringConversion) {
    std::string test_string = "hello";
    auto wide = UtauArgumentParser::convert_to_wide(test_string);
    auto back = UtauArgumentParser::convert_from_wide(wide);
    EXPECT_EQ(back, test_string);
}

// Parameter validation tests
TEST_F(UtauArgumentParserTest, PitchRangeValidation) {
    parser_.set_strict_validation(true);
    
    // Test extreme pitch values
    auto args = create_basic_args();
    args[3] = "5000";  // Very high pitch
    auto result = parser_.parse(args);
    
    EXPECT_FALSE(result.is_valid);  // Should fail strict validation
}

TEST_F(UtauArgumentParserTest, VelocityRangeValidation) {
    parser_.set_strict_validation(true);
    
    auto args = create_basic_args();
    args[4] = "0";  // Invalid velocity
    auto result = parser_.parse(args);
    
    EXPECT_FALSE(result.is_valid);
}

// Error handling tests
TEST_F(UtauArgumentParserTest, NonExistentInputFile) {
    parser_.set_strict_validation(true);
    
    auto args = create_basic_args();
    args[1] = "/nonexistent/path/input.wav";
    auto result = parser_.parse(args);
    
    EXPECT_FALSE(result.is_valid);
}

TEST_F(UtauArgumentParserTest, InvalidArgumentFormat) {
    auto args = create_basic_args();
    args[3] = "invalid_pitch";  // Non-numeric pitch
    auto result = parser_.parse(args);
    
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.error_message.empty());
}

// Edge case tests
TEST_F(UtauArgumentParserTest, EmptyFlagsString) {
    auto args = create_basic_args("");
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.flags_string.empty());
    EXPECT_EQ(result.flag_values.g, 0);
}

TEST_F(UtauArgumentParserTest, ZeroPitchAndVelocity) {
    auto args = create_basic_args();
    args[3] = "0";   // Zero pitch
    args[4] = "1";   // Minimum velocity
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.pitch, 0);
    EXPECT_EQ(result.velocity, 1);
}

TEST_F(UtauArgumentParserTest, LargeParameterValues) {
    auto args = create_basic_args();
    
    // Extend with large optional parameters
    while (args.size() < 13) {
        args.push_back("0");
    }
    
    args[6] = "1000000";  // Large offset
    args[7] = "4410000";  // Large length (100 seconds at 44.1kHz)
    
    auto result = parser_.parse(args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.offset, 1000000);
    EXPECT_EQ(result.length, 4410000);
}

// Performance and stress tests
TEST_F(UtauArgumentParserTest, ManyFlagsParsing) {
    std::string many_flags;
    for (int i = 0; i < 100; ++i) {
        many_flags += "flag" + std::to_string(i) + "1";
    }
    
    auto flags = UtauArgumentParser::parse_flags(many_flags);
    EXPECT_EQ(flags.custom_flags.size(), 100);
}

TEST_F(UtauArgumentParserTest, LongPathHandling) {
    // Create a very long but valid path
    std::string long_filename(200, 'a');
    long_filename += ".wav";
    
    auto long_path = test_dir_ / long_filename;
    create_dummy_wav_file(long_path);
    
    auto args = create_basic_args();
    args[1] = long_path.string();
    
    auto result = parser_.parse(args);
    EXPECT_TRUE(result.is_valid);
}

// Compatibility tests
TEST_F(UtauArgumentParserTest, MoreSamplerCompatibilityFormat) {
    // Test the exact format that moresampler expects
    std::vector<std::string> moresampler_args = {
        "resampler.exe",
        test_input_wav_.string(),
        test_output_wav_.string(),
        "100",     // pitch (1 semitone up)
        "100",     // velocity
        "g+30t-10", // flags
        "0",       // offset
        "44100",   // length (1 second)
        "4410",    // consonant (0.1 second)
        "0",       // cutoff
        "0",       // volume
        "0",       // start
        "100"      // end
    };
    
    auto result = parser_.parse(moresampler_args);
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.pitch, 100);
    EXPECT_EQ(result.flag_values.g, 30);
    EXPECT_EQ(result.flag_values.t, -10);
    EXPECT_EQ(result.length, 44100);
    EXPECT_EQ(result.consonant, 4410);
}

// Utility tests
TEST_F(UtauArgumentParserTest, DebugInfoOutput) {
    parser_.set_debug_mode(true);
    auto args = create_basic_args("g+50");
    
    // This test mainly ensures debug output doesn't crash
    testing::internal::CaptureStdout();
    auto result = parser_.parse(args);
    std::string output = testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(result.is_valid);
    EXPECT_FALSE(output.empty());  // Should have debug output
}

TEST_F(UtauArgumentParserTest, UsageStringGeneration) {
    ResamplerArgs args;
    auto usage = args.get_usage_string();
    
    EXPECT_FALSE(usage.empty());
    EXPECT_NE(usage.find("input.wav"), std::string::npos);
    EXPECT_NE(usage.find("output.wav"), std::string::npos);
    EXPECT_NE(usage.find("pitch"), std::string::npos);
}

TEST_F(UtauArgumentParserTest, FlagValuesToString) {
    FlagValues flags;
    flags.g = 50;
    flags.t = -20;
    flags.bre = 30;
    
    auto str = flags.to_string();
    EXPECT_NE(str.find("g50"), std::string::npos);
    EXPECT_NE(str.find("t-20"), std::string::npos);
    EXPECT_NE(str.find("bre30"), std::string::npos);
}

// Integration tests with actual scenarios
class UtauScenarioTest : public UtauArgumentParserTest {
protected:
    struct TestScenario {
        std::string name;
        std::vector<std::string> args;
        bool should_succeed;
        std::string description;
    };
    
    std::vector<TestScenario> getCommonScenarios() {
        return {
            {
                "TypicalUTAUCall",
                {
                    "resampler.exe",
                    test_input_wav_.string(),
                    test_output_wav_.string(),
                    "0", "100", "", "0", "0", "0", "0", "0", "0", "100"
                },
                true,
                "Standard UTAU resampler call with all default values"
            },
            {
                "PitchBending",
                {
                    "resampler.exe",
                    test_input_wav_.string(),
                    test_output_wav_.string(),
                    "200", "100", "g+20", "0", "22050"
                },
                true,
                "2 semitones up with slight growl, half-second output"
            },
            {
                "ComplexFlags",
                {
                    "resampler.exe",
                    test_input_wav_.string(),
                    test_output_wav_.string(),
                    "-100", "80", "g-30t+40bre15bri+25"
                },
                true,
                "1 semitone down, slower, with complex voice characteristics"
            },
            {
                "MaximalParameters",
                {
                    "resampler.exe",
                    test_input_wav_.string(),
                    test_output_wav_.string(),
                    "1200", "200", "g+100t-100bre100bri-100",
                    "44100", "176400", "8820", "-22050", "6", "25", "75"
                },
                true,
                "Extreme parameter values within valid ranges"
            }
        };
    }
};

TEST_F(UtauScenarioTest, CommonUsageScenarios) {
    auto scenarios = getCommonScenarios();
    
    for (const auto& scenario : scenarios) {
        SCOPED_TRACE("Testing scenario: " + scenario.name);
        
        auto result = parser_.parse(scenario.args);
        
        if (scenario.should_succeed) {
            EXPECT_TRUE(result.is_valid) << "Scenario '" << scenario.name 
                                        << "' should succeed: " << scenario.description;
            if (!result.is_valid) {
                std::cout << "Error: " << result.error_message << std::endl;
            }
        } else {
            EXPECT_FALSE(result.is_valid) << "Scenario '" << scenario.name 
                                         << "' should fail: " << scenario.description;
        }
    }
}

// Performance benchmark test
TEST_F(UtauArgumentParserTest, ParsingPerformanceBenchmark) {
    auto args = create_basic_args("g+50t-20bre30bri+10");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        auto result = parser_.parse(args);
        EXPECT_TRUE(result.is_valid);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avg_time_us = static_cast<double>(duration.count()) / iterations;
    
    std::cout << "Average parsing time: " << avg_time_us << " microseconds" << std::endl;
    
    // Parsing should be reasonably fast (less than 100 microseconds per call)
    EXPECT_LT(avg_time_us, 100.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
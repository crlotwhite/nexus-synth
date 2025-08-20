#include <gtest/gtest.h>
#include "../utils/test_data_manager.h"
#include "nexussynth/cli_interface.h"
#include <filesystem>
#include <chrono>
#include <fstream>

namespace nexussynth {
namespace integration_test {

class EndToEndSynthesisTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data manager
        std::string test_data_dir = std::filesystem::current_path() / "test_data";
        ASSERT_TRUE(test_data_manager_.initialize(test_data_dir));
        ASSERT_TRUE(test_data_manager_.setup_test_environment());
        
        // Load test scenarios
        ASSERT_TRUE(test_data_manager_.load_test_scenarios("test_scenarios.json"));
        
        // Initialize CLI interface
        cli_interface_ = std::make_unique<cli::CliInterface>();
        
        // Prepare .nvm file for synthesis tests
        setup_test_voice_model();
    }
    
    void TearDown() override {
        test_data_manager_.cleanup_test_environment();
    }
    
    void setup_test_voice_model() {
        // Convert minimal voice bank to .nvm format for synthesis tests
        std::string voice_bank_path = test_data_manager_.get_minimal_voice_bank_path();
        nvm_model_path_ = test_data_manager_.create_temp_file(".nvm");
        
        std::vector<std::string> convert_args = {
            "nexussynth", "convert", voice_bank_path,
            "-o", nvm_model_path_, "--preset", "fast"
        };
        
        std::vector<char*> convert_argv;
        for (auto& arg : convert_args) {
            convert_argv.push_back(const_cast<char*>(arg.c_str()));
        }
        
        cli::CliResult result = cli_interface_->run(
            static_cast<int>(convert_argv.size()), convert_argv.data());
        
        ASSERT_TRUE(result.success) << "Failed to prepare test voice model";
        ASSERT_TRUE(test_data_manager_.file_exists(nvm_model_path_));
    }
    
    // Helper to simulate UTAU resampler call
    cli::CliResult synthesize_utau_style(const std::string& output_wav,
                                       const std::string& input_wav,
                                       const std::string& phoneme,
                                       double pitch = 0.0,
                                       const std::string& flags = "") {
        
        std::vector<std::string> args = {
            "nexussynth", // Executable name
            output_wav,   // Output WAV file
            input_wav,    // Input WAV file (from voice model)
            phoneme,      // Phoneme to synthesize
            std::to_string(static_cast<int>(pitch * 100)) // Pitch in cents
        };
        
        // Add flags if specified
        if (!flags.empty()) {
            args.push_back(flags);
        }
        
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        
        return cli_interface_->run(static_cast<int>(argv.size()), argv.data());
    }
    
    TestDataManager test_data_manager_;
    std::unique_ptr<cli::CliInterface> cli_interface_;
    std::string nvm_model_path_;
};

TEST_F(EndToEndSynthesisTest, BasicPhonemeGeneration) {
    // Test basic single phoneme synthesis
    
    auto scenarios = test_data_manager_.get_test_scenarios();
    ASSERT_FALSE(scenarios.empty()) << "No test scenarios loaded";
    
    // Use the basic synthesis scenario
    auto basic_scenario = test_data_manager_.get_scenario("basic_synthesis");
    ASSERT_FALSE(basic_scenario.id.empty()) << "Basic synthesis scenario not found";
    
    std::string output_wav = test_data_manager_.create_temp_file(".wav");
    
    // Synthesize single phoneme "a"
    auto start_time = std::chrono::high_resolution_clock::now();
    
    cli::CliResult result = synthesize_utau_style(
        output_wav,
        nvm_model_path_,
        "a",
        0.0  // No pitch shift
    );
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto synthesis_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Verify synthesis succeeded
    EXPECT_TRUE(result.success) << "Synthesis failed: " << result.message;
    EXPECT_EQ(result.exit_code, 0) << "Non-zero exit code: " << result.exit_code;
    
    // Verify output file was created
    EXPECT_TRUE(test_data_manager_.file_exists(output_wav))
        << "Output WAV file was not created: " << output_wav;
    
    // Verify file has reasonable size
    size_t file_size = test_data_manager_.get_file_size(output_wav);
    EXPECT_GT(file_size, 44u) << "Output file too small (less than WAV header)";
    EXPECT_LT(file_size, 10u * 1024 * 1024) << "Output file suspiciously large";
    
    // Performance check - synthesis should be fast
    EXPECT_LT(synthesis_time.count(), 5000) // 5 seconds max for single phoneme
        << "Synthesis took too long: " << synthesis_time.count() << "ms";
    
    std::cout << "Single phoneme synthesis completed in " << synthesis_time.count() << "ms" << std::endl;
    std::cout << "Output file size: " << file_size << " bytes" << std::endl;
}

TEST_F(EndToEndSynthesisTest, PitchShiftingSynthesis) {
    // Test synthesis with various pitch shifts
    
    std::vector<double> pitch_shifts = {-12.0, -6.0, 0.0, 6.0, 12.0}; // semitones
    
    for (double pitch : pitch_shifts) {
        SCOPED_TRACE("Testing pitch shift: " + std::to_string(pitch) + " semitones");
        
        std::string output_wav = test_data_manager_.create_temp_file(".wav");
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        cli::CliResult result = synthesize_utau_style(
            output_wav,
            nvm_model_path_,
            "a",
            pitch
        );
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        EXPECT_TRUE(result.success) << "Pitch shift synthesis failed at " << pitch << " semitones";
        EXPECT_TRUE(test_data_manager_.file_exists(output_wav));
        
        size_t file_size = test_data_manager_.get_file_size(output_wav);
        EXPECT_GT(file_size, 44u) << "Output file too small for pitch " << pitch;
        
        // Large pitch shifts might take longer
        int max_time = (std::abs(pitch) > 6.0) ? 10000 : 5000;
        EXPECT_LT(duration.count(), max_time) 
            << "Pitch shift took too long at " << pitch << " semitones";
        
        std::cout << "Pitch " << pitch << " semitones: " << duration.count() 
                  << "ms, " << file_size << " bytes" << std::endl;
    }
}

TEST_F(EndToEndSynthesisTest, MultiplePhonemeSequence) {
    // Test synthesis of phoneme sequences
    
    std::vector<std::string> phoneme_sequences = {
        "a i u e o",         // Basic vowels
        "ka ki ku ke ko",    // CV sequence
        "sa si su se so ta ti tu te to"  // Longer sequence
    };
    
    for (const auto& sequence : phoneme_sequences) {
        SCOPED_TRACE("Testing phoneme sequence: " + sequence);
        
        std::string output_wav = test_data_manager_.create_temp_file(".wav");
        
        // Create input file with phoneme sequence
        std::string input_file = test_data_manager_.create_temp_file(".txt");
        std::ofstream input(input_file);
        input << sequence << std::endl;
        input.close();
        
        // Synthesize using CLI batch mode
        std::vector<std::string> args = {
            "nexussynth", "synthesize",
            "--model", nvm_model_path_,
            "--input", input_file,
            "--output", output_wav
        };
        
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        cli::CliResult result = cli_interface_->run(static_cast<int>(argv.size()), argv.data());
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        EXPECT_TRUE(result.success) << "Multi-phoneme synthesis failed for: " << sequence;
        EXPECT_TRUE(test_data_manager_.file_exists(output_wav));
        
        size_t file_size = test_data_manager_.get_file_size(output_wav);
        EXPECT_GT(file_size, 44u) << "Output file too small";
        
        // Longer sequences should produce larger files and take more time
        size_t phoneme_count = std::count(sequence.begin(), sequence.end(), ' ') + 1;
        EXPECT_LT(duration.count(), phoneme_count * 2000) // 2 seconds per phoneme max
            << "Multi-phoneme synthesis took too long";
        
        std::cout << "Sequence \"" << sequence << "\": " << duration.count() 
                  << "ms, " << file_size << " bytes" << std::endl;
    }
}

TEST_F(EndToEndSynthesisTest, UtauFlagCompatibility) {
    // Test UTAU flag processing (g, t, v, etc.)
    
    struct FlagTest {
        std::string flag;
        std::string description;
        int max_time_ms;
    };
    
    std::vector<FlagTest> flag_tests = {
        {"g-10", "Gender factor -10", 5000},
        {"g+5", "Gender factor +5", 5000},
        {"t+20", "Tempo +20%", 5000},
        {"t-15", "Tempo -15%", 5000},
        {"v100", "Volume 100%", 5000},
        {"v50", "Volume 50%", 5000},
        {"bre30", "Breathiness 30%", 7000},
        {"bri20", "Brightness 20%", 7000}
    };
    
    for (const auto& test : flag_tests) {
        SCOPED_TRACE("Testing UTAU flag: " + test.flag + " - " + test.description);
        
        std::string output_wav = test_data_manager_.create_temp_file(".wav");
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        cli::CliResult result = synthesize_utau_style(
            output_wav,
            nvm_model_path_,
            "a",
            0.0,      // No pitch shift
            test.flag // UTAU flag
        );
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        EXPECT_TRUE(result.success) << "Flag synthesis failed for: " << test.flag;
        EXPECT_TRUE(test_data_manager_.file_exists(output_wav));
        
        size_t file_size = test_data_manager_.get_file_size(output_wav);
        EXPECT_GT(file_size, 44u) << "Output file too small for flag: " << test.flag;
        
        EXPECT_LT(duration.count(), test.max_time_ms)
            << "Flag processing took too long for: " << test.flag;
        
        std::cout << "Flag " << test.flag << ": " << duration.count() 
                  << "ms, " << file_size << " bytes" << std::endl;
    }
}

TEST_F(EndToEndSynthesisTest, ErrorHandlingAndRecovery) {
    // Test error handling for invalid inputs
    
    // Test with invalid phoneme
    std::string output_wav = test_data_manager_.create_temp_file(".wav");
    
    cli::CliResult result = synthesize_utau_style(
        output_wav,
        nvm_model_path_,
        "invalid_phoneme",
        0.0
    );
    
    // Should handle gracefully
    if (!result.success) {
        EXPECT_NE(result.exit_code, 0) << "Should return error code for invalid phoneme";
        EXPECT_FALSE(result.message.empty()) << "Should provide error message";
    }
    
    // Test with invalid pitch range
    std::string output_wav2 = test_data_manager_.create_temp_file(".wav");
    
    cli::CliResult result2 = synthesize_utau_style(
        output_wav2,
        nvm_model_path_,
        "a",
        100.0  // Extreme pitch shift
    );
    
    // Should either succeed with clamping or fail gracefully
    if (!result2.success) {
        EXPECT_NE(result2.exit_code, 0);
        EXPECT_FALSE(result2.message.empty());
    } else {
        // If it succeeds, should produce valid output
        EXPECT_TRUE(test_data_manager_.file_exists(output_wav2));
    }
}

TEST_F(EndToEndSynthesisTest, PerformanceBenchmark) {
    // Benchmark synthesis performance
    
    const int num_iterations = 10;
    const std::string test_phoneme = "a";
    
    std::vector<double> synthesis_times;
    std::vector<size_t> output_sizes;
    
    for (int i = 0; i < num_iterations; ++i) {
        std::string output_wav = test_data_manager_.create_temp_file(".wav");
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        cli::CliResult result = synthesize_utau_style(
            output_wav,
            nvm_model_path_,
            test_phoneme,
            0.0
        );
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        ASSERT_TRUE(result.success) << "Benchmark iteration " << i << " failed";
        
        synthesis_times.push_back(duration.count() / 1000.0); // Convert to milliseconds
        output_sizes.push_back(test_data_manager_.get_file_size(output_wav));
    }
    
    // Calculate statistics
    double total_time = 0.0;
    size_t total_size = 0;
    for (size_t i = 0; i < synthesis_times.size(); ++i) {
        total_time += synthesis_times[i];
        total_size += output_sizes[i];
    }
    
    double avg_time = total_time / num_iterations;
    double avg_size = static_cast<double>(total_size) / num_iterations;
    
    // Find min/max
    auto time_minmax = std::minmax_element(synthesis_times.begin(), synthesis_times.end());
    auto size_minmax = std::minmax_element(output_sizes.begin(), output_sizes.end());
    
    // Performance expectations
    EXPECT_LT(avg_time, 3000.0) << "Average synthesis time too high: " << avg_time << "ms";
    EXPECT_GT(avg_size, 44.0) << "Average output size too small";
    EXPECT_LT(*size_minmax.second - *size_minmax.first, avg_size * 0.1) 
        << "Output size too variable between runs";
    
    // Report results
    std::cout << "\nPerformance Benchmark Results (" << num_iterations << " iterations):" << std::endl;
    std::cout << "Average synthesis time: " << avg_time << "ms" << std::endl;
    std::cout << "Min/Max synthesis time: " << *time_minmax.first << "/" << *time_minmax.second << "ms" << std::endl;
    std::cout << "Average output size: " << avg_size << " bytes" << std::endl;
    std::cout << "Min/Max output size: " << *size_minmax.first << "/" << *size_minmax.second << " bytes" << std::endl;
}

} // namespace integration_test
} // namespace nexussynth
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <regex>
#include <filesystem>
#include <climits>
#include "utau_logger.h"
#include "utau_error_handler.h"

namespace nexussynth {
namespace utau {

/**
 * @brief UTAU resampler exit codes following standard conventions
 */
enum class ResamplerError {
    SUCCESS = 0,
    GENERAL_ERROR = 1,
    FILE_NOT_FOUND = 2,
    INVALID_WAV_FORMAT = 3,
    OUT_OF_MEMORY = 4,
    INVALID_PARAMETERS = 5,
    UNSUPPORTED_SAMPLE_RATE = 6,
    PROCESSING_ERROR = 7
};

/**
 * @brief Parsed UTAU flag values with validated ranges
 */
struct FlagValues {
    int g = 0;      ///< Gender/growl factor (-100 to 100)
    int t = 0;      ///< Tension factor (-100 to 100)
    int bre = 0;    ///< Breathiness (0 to 100)
    int bri = 0;    ///< Brightness (-100 to 100)
    
    // Additional flags that might be present
    std::map<std::string, int> custom_flags;
    
    bool is_valid() const {
        return (g >= -100 && g <= 100) &&
               (t >= -100 && t <= 100) &&
               (bre >= 0 && bre <= 100) &&
               (bri >= -100 && bri <= 100);
    }
    
    std::string to_string() const {
        std::string result;
        if (g != 0) result += "g" + std::to_string(g);
        if (t != 0) result += "t" + std::to_string(t);
        if (bre != 0) result += "bre" + std::to_string(bre);
        if (bri != 0) result += "bri" + std::to_string(bri);
        
        for (const auto& [flag, value] : custom_flags) {
            result += flag + std::to_string(value);
        }
        
        return result;
    }
};

/**
 * @brief Complete UTAU resampler command line arguments
 * 
 * Standard format: resampler.exe input.wav output.wav pitch velocity flags offset length consonant cutoff volume start end
 */
struct ResamplerArgs {
    // Required arguments
    std::filesystem::path input_path;        ///< Input WAV file path
    std::filesystem::path output_path;       ///< Output WAV file path
    int pitch = 0;                          ///< Pitch change (100 = 1 semitone)
    int velocity = 100;                     ///< Velocity/speed (100 = normal)
    std::string flags_string;               ///< Raw flags string
    
    // Optional arguments with defaults
    int offset = 0;                         ///< Start offset (samples)
    int length = 0;                         ///< Output length (samples, 0 = auto)
    int consonant = 0;                      ///< Consonant length (samples)
    int cutoff = 0;                         ///< End cutoff (positive = absolute, negative = relative)
    int volume = 0;                         ///< Volume adjustment (dB, 0 = no change)
    int start = 0;                          ///< Start position (0-100%)
    int end = 100;                          ///< End position (0-100%)
    
    // Parsed flag values
    FlagValues flag_values;
    
    // Metadata
    bool is_valid = false;
    std::string error_message;
    
    // Validation methods
    bool validate_paths() const;
    bool validate_ranges() const;
    bool validate_audio_parameters() const;
    
    // Utility methods
    std::string get_usage_string() const;
    void print_debug_info() const;
};

/**
 * @brief UTAU argument parser with encoding and platform support
 */
class UtauArgumentParser {
public:
    UtauArgumentParser();
    ~UtauArgumentParser();
    
    // Main parsing interface
    ResamplerArgs parse(int argc, char* argv[]);
    ResamplerArgs parse(int argc, wchar_t* argv[]);
    ResamplerArgs parse(const std::vector<std::string>& args);
    
    // Encoding conversion utilities
    static std::string convert_to_utf8(const std::string& input);
    static std::wstring convert_to_wide(const std::string& input);
    static std::string convert_from_wide(const std::wstring& input);
    
    // Path utilities for Windows compatibility
    static std::filesystem::path normalize_path(const std::string& path);
    static std::filesystem::path normalize_path(const std::wstring& path);
    static bool is_valid_wav_path(const std::filesystem::path& path);
    
    // Flag parsing utilities
    static FlagValues parse_flags(const std::string& flags_string);
    static bool is_valid_flag_format(const std::string& flags);
    
    // Error handling
    static void report_error(ResamplerError error, const std::string& details);
    static std::string get_error_description(ResamplerError error);
    
    // Configuration
    void set_strict_validation(bool strict) { strict_validation_ = strict; }
    void set_debug_mode(bool debug) { debug_mode_ = debug; }
    void set_log_file(const std::string& log_path);

private:
    bool strict_validation_ = true;
    bool debug_mode_ = false;
    std::string log_file_path_;
    
    // Internal parsing helpers
    ResamplerArgs parse_internal(const std::vector<std::string>& args);
    bool validate_argument_count(size_t count);
    
    // Path processing
    std::filesystem::path process_path_argument(const std::string& path_str);
    bool check_file_access(const std::filesystem::path& path, bool must_exist);
    
    // Numeric argument parsing
    std::optional<int> parse_integer_argument(const std::string& arg, 
                                            const std::string& param_name,
                                            int min_val = INT_MIN, 
                                            int max_val = INT_MAX);
    
    // Flag processing
    FlagValues process_flags_argument(const std::string& flags);
    bool parse_single_flag(const std::string& flag_match, FlagValues& values);
    
    // Validation helpers
    bool validate_pitch_range(int pitch);
    bool validate_velocity_range(int velocity);
    bool validate_percentage_range(int value);
    
    // Encoding detection and conversion
    bool is_shift_jis_encoded(const std::string& str);
    std::string shift_jis_to_utf8(const std::string& shift_jis_str);
    
    // Error reporting
    void log_debug(const std::string& message);
    void log_error(const std::string& message);
    
    // Platform-specific utilities
#ifdef _WIN32
    std::string get_windows_error_string(DWORD error_code);
    bool set_console_utf8_mode();
#endif
};

/**
 * @brief Utility functions for UTAU interface compatibility
 */
namespace UtauUtils {
    // Command line building for testing
    std::vector<std::string> build_command_line(const ResamplerArgs& args);
    
    // Compatibility testing
    bool test_moresampler_compatibility(const ResamplerArgs& args);
    
    // File format validation
    bool is_valid_wav_format(const std::filesystem::path& wav_path);
    bool get_wav_info(const std::filesystem::path& wav_path, 
                     int& sample_rate, int& channels, int& bit_depth);
    
    // Flag generation for testing
    std::vector<std::string> generate_test_flag_combinations();
    
    // Benchmarking utilities
    struct ParsingBenchmark {
        double parsing_time_ms;
        size_t memory_usage_bytes;
        bool success;
    };
    
    ParsingBenchmark benchmark_parsing(const std::vector<std::string>& test_cases);
}

} // namespace utau
} // namespace nexussynth
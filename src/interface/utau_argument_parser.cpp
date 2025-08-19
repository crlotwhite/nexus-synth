#include "nexussynth/utau_argument_parser.h"
#include "nexussynth/utau_logger.h"
#include "nexussynth/utau_error_handler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace nexussynth {
namespace utau {

// ResamplerArgs validation methods
bool ResamplerArgs::validate_paths() const {
    // Input file must exist
    if (!std::filesystem::exists(input_path)) {
        return false;
    }
    
    // Output directory must exist (file can be created)
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        return false;
    }
    
    // Check file extensions
    auto input_ext = input_path.extension().string();
    auto output_ext = output_path.extension().string();
    std::transform(input_ext.begin(), input_ext.end(), input_ext.begin(), ::tolower);
    std::transform(output_ext.begin(), output_ext.end(), output_ext.begin(), ::tolower);
    
    return (input_ext == ".wav") && (output_ext == ".wav");
}

bool ResamplerArgs::validate_ranges() const {
    return (velocity > 0 && velocity <= 1000) &&
           (start >= 0 && start <= 100) &&
           (end >= 0 && end <= 100) &&
           (start < end) &&
           flag_values.is_valid();
}

bool ResamplerArgs::validate_audio_parameters() const {
    // Basic sanity checks
    if (length < 0 || consonant < 0) return false;
    if (offset < 0) return false;
    if (abs(cutoff) > 48000 * 10) return false; // Max 10 seconds cutoff
    if (abs(volume) > 60) return false; // Max ±60dB
    
    return true;
}

std::string ResamplerArgs::get_usage_string() const {
    return "Usage: resampler.exe <input.wav> <output.wav> <pitch> <velocity> <flags> [offset] [length] [consonant] [cutoff] [volume] [start] [end]\n"
           "  input.wav  : Input WAV file path\n"
           "  output.wav : Output WAV file path\n"
           "  pitch      : Pitch change (100 = 1 semitone)\n"
           "  velocity   : Speed/velocity (100 = normal)\n"
           "  flags      : Voice flags (g±N, t±N, breN, bri±N)\n"
           "  offset     : Start offset in samples (optional)\n"
           "  length     : Output length in samples (optional, 0 = auto)\n"
           "  consonant  : Consonant length in samples (optional)\n"
           "  cutoff     : End cutoff (optional, ±samples)\n"
           "  volume     : Volume adjustment in dB (optional)\n"
           "  start      : Start position 0-100% (optional)\n"
           "  end        : End position 0-100% (optional)";
}

void ResamplerArgs::print_debug_info() const {
    std::cout << "=== UTAU Resampler Arguments ===" << std::endl;
    std::cout << "Input:     " << input_path << std::endl;
    std::cout << "Output:    " << output_path << std::endl;
    std::cout << "Pitch:     " << pitch << std::endl;
    std::cout << "Velocity:  " << velocity << std::endl;
    std::cout << "Flags:     " << flags_string << std::endl;
    std::cout << "  - g:     " << flag_values.g << std::endl;
    std::cout << "  - t:     " << flag_values.t << std::endl;
    std::cout << "  - bre:   " << flag_values.bre << std::endl;
    std::cout << "  - bri:   " << flag_values.bri << std::endl;
    std::cout << "Offset:    " << offset << std::endl;
    std::cout << "Length:    " << length << std::endl;
    std::cout << "Consonant: " << consonant << std::endl;
    std::cout << "Cutoff:    " << cutoff << std::endl;
    std::cout << "Volume:    " << volume << std::endl;
    std::cout << "Start:     " << start << "%" << std::endl;
    std::cout << "End:       " << end << "%" << std::endl;
    std::cout << "Valid:     " << (is_valid ? "YES" : "NO") << std::endl;
    if (!error_message.empty()) {
        std::cout << "Error:     " << error_message << std::endl;
    }
    std::cout << "===============================" << std::endl;
}

// UtauArgumentParser implementation
UtauArgumentParser::UtauArgumentParser() {
#ifdef _WIN32
    set_console_utf8_mode();
#endif
    
    // Initialize logging system with UTAU-appropriate defaults
    if (!LoggingUtils::initialize_utau_logging()) {
        std::cerr << "Warning: Failed to initialize logging system" << std::endl;
    }
}

UtauArgumentParser::~UtauArgumentParser() = default;

ResamplerArgs UtauArgumentParser::parse(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc);
    
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(convert_to_utf8(argv[i]));
    }
    
    return parse_internal(args);
}

ResamplerArgs UtauArgumentParser::parse(int argc, wchar_t* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc);
    
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(convert_from_wide(argv[i]));
    }
    
    return parse_internal(args);
}

ResamplerArgs UtauArgumentParser::parse(const std::vector<std::string>& args) {
    return parse_internal(args);
}

ResamplerArgs UtauArgumentParser::parse_internal(const std::vector<std::string>& args) {
    ResamplerArgs result;
    
    log_debug("Starting argument parsing with " + std::to_string(args.size()) + " arguments");
    
    // Validate minimum argument count
    if (!validate_argument_count(args.size())) {
        result.error_message = "Insufficient arguments. Minimum 5 required.";
        result.is_valid = false;
        log_error(result.error_message);
        return result;
    }
    
    try {
        // Parse required arguments (1-5)
        if (args.size() > 1) {
            result.input_path = process_path_argument(args[1]);
        }
        
        if (args.size() > 2) {
            result.output_path = process_path_argument(args[2]);
        }
        
        if (args.size() > 3) {
            auto pitch_opt = parse_integer_argument(args[3], "pitch", -4800, 4800);
            if (pitch_opt) {
                result.pitch = *pitch_opt;
            } else {
                UTAU_THROW_ERROR(UtauErrorCode::INVALID_PARAMETERS, "Invalid pitch value: " + args[3]);
            }
        }
        
        if (args.size() > 4) {
            auto velocity_opt = parse_integer_argument(args[4], "velocity", 1, 1000);
            if (velocity_opt) {
                result.velocity = *velocity_opt;
            } else {
                UTAU_THROW_ERROR(UtauErrorCode::INVALID_PARAMETERS, "Invalid velocity value: " + args[4]);
            }
        }
        
        if (args.size() > 5) {
            result.flags_string = args[5];
            result.flag_values = process_flags_argument(args[5]);
        }
        
        // Parse optional arguments (6-12)
        if (args.size() > 6) {
            auto offset_opt = parse_integer_argument(args[6], "offset", 0);
            result.offset = offset_opt.value_or(0);
        }
        
        if (args.size() > 7) {
            auto length_opt = parse_integer_argument(args[7], "length", 0);
            result.length = length_opt.value_or(0);
        }
        
        if (args.size() > 8) {
            auto consonant_opt = parse_integer_argument(args[8], "consonant", 0);
            result.consonant = consonant_opt.value_or(0);
        }
        
        if (args.size() > 9) {
            auto cutoff_opt = parse_integer_argument(args[9], "cutoff");
            result.cutoff = cutoff_opt.value_or(0);
        }
        
        if (args.size() > 10) {
            auto volume_opt = parse_integer_argument(args[10], "volume", -60, 60);
            result.volume = volume_opt.value_or(0);
        }
        
        if (args.size() > 11) {
            auto start_opt = parse_integer_argument(args[11], "start", 0, 100);
            result.start = start_opt.value_or(0);
        }
        
        if (args.size() > 12) {
            auto end_opt = parse_integer_argument(args[12], "end", 0, 100);
            result.end = end_opt.value_or(100);
        }
        
        // Validate all components
        if (strict_validation_) {
            if (!result.validate_paths()) {
                UTAU_THROW_ERROR(UtauErrorCode::FILE_NOT_FOUND, "Invalid file paths or formats");
            }
            
            if (!result.validate_ranges()) {
                UTAU_THROW_ERROR(UtauErrorCode::PARAMETER_OUT_OF_RANGE, "Parameter values out of valid range");
            }
            
            if (!result.validate_audio_parameters()) {
                UTAU_THROW_ERROR(UtauErrorCode::INVALID_WAV_FORMAT, "Invalid audio processing parameters");
            }
        }
        
        result.is_valid = true;
        log_debug("Successfully parsed all arguments");
        
    } catch (const UtauException& e) {
        result.error_message = e.what();
        result.is_valid = false;
        log_error("UTAU parsing failed: " + result.error_message);
        
        // Report through error handler for proper categorization
        UtauErrorHandler::instance().report_error(e.get_error_code(), e.what());
    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.is_valid = false;
        log_error("Parsing failed: " + result.error_message);
        
        // Convert generic exception to UTAU error
        UtauErrorHandler::instance().report_exception(e, "Argument parsing");
    }
    
    if (debug_mode_) {
        result.print_debug_info();
    }
    
    return result;
}

bool UtauArgumentParser::validate_argument_count(size_t count) {
    // Minimum: program name + 4 required args = 5
    // Maximum: program name + 11 args = 12
    return count >= 5 && count <= 13;
}

std::filesystem::path UtauArgumentParser::process_path_argument(const std::string& path_str) {
    if (path_str.empty()) {
        UTAU_THROW_ERROR(UtauErrorCode::INVALID_PARAMETERS, "Empty path provided");
    }
    
    // Convert and normalize the path
    auto normalized = normalize_path(path_str);
    
    log_debug("Processed path: " + path_str + " -> " + normalized.string());
    
    return normalized;
}

bool UtauArgumentParser::check_file_access(const std::filesystem::path& path, bool must_exist) {
    if (must_exist && !std::filesystem::exists(path)) {
        return false;
    }
    
    // Check if we can read/write to the location
    try {
        if (must_exist) {
            std::ifstream test(path, std::ios::binary);
            return test.good();
        } else {
            // Check if parent directory exists and is writable
            auto parent = path.parent_path();
            return parent.empty() || std::filesystem::exists(parent);
        }
    } catch (...) {
        return false;
    }
}

std::optional<int> UtauArgumentParser::parse_integer_argument(
    const std::string& arg, 
    const std::string& param_name,
    int min_val, 
    int max_val) {
    
    if (arg.empty()) {
        return std::nullopt;
    }
    
    try {
        size_t end_pos;
        int value = std::stoi(arg, &end_pos);
        
        // Check if entire string was consumed
        if (end_pos != arg.length()) {
            log_error("Invalid " + param_name + " format: " + arg);
            return std::nullopt;
        }
        
        // Check range
        if (value < min_val || value > max_val) {
            log_error(param_name + " value " + std::to_string(value) + 
                     " out of range [" + std::to_string(min_val) + 
                     ", " + std::to_string(max_val) + "]");
            return std::nullopt;
        }
        
        return value;
        
    } catch (const std::exception& e) {
        log_error("Failed to parse " + param_name + ": " + arg + " (" + e.what() + ")");
        return std::nullopt;
    }
}

FlagValues UtauArgumentParser::process_flags_argument(const std::string& flags) {
    return parse_flags(flags);
}

bool UtauArgumentParser::parse_single_flag(const std::string& flag_match, FlagValues& values) {
    std::regex flag_regex(R"(([a-zA-Z]+)([+-]?\d+))");
    std::smatch match;
    
    if (!std::regex_match(flag_match, match, flag_regex)) {
        return false;
    }
    
    std::string flag_name = match[1].str();
    int flag_value = std::stoi(match[2].str());
    
    // Convert to lowercase for comparison
    std::transform(flag_name.begin(), flag_name.end(), flag_name.begin(), ::tolower);
    
    if (flag_name == "g") {
        values.g = std::clamp(flag_value, -100, 100);
    } else if (flag_name == "t") {
        values.t = std::clamp(flag_value, -100, 100);
    } else if (flag_name == "bre") {
        values.bre = std::clamp(flag_value, 0, 100);
    } else if (flag_name == "bri") {
        values.bri = std::clamp(flag_value, -100, 100);
    } else {
        // Store custom flags
        values.custom_flags[flag_name] = flag_value;
    }
    
    return true;
}

// Static utility methods
std::string UtauArgumentParser::convert_to_utf8(const std::string& input) {
    // Simple heuristic to check if already UTF-8
    for (size_t i = 0; i < input.length(); ++i) {
        unsigned char byte = static_cast<unsigned char>(input[i]);
        
        // Shift-JIS first byte ranges
        if ((byte >= 0x81 && byte <= 0x9F) || (byte >= 0xE0 && byte <= 0xFC)) {
            // Might be Shift-JIS, attempt conversion
            UtauArgumentParser parser;
            return parser.shift_jis_to_utf8(input);
        }
    }
    
    // Assume already UTF-8
    return input;
}

std::wstring UtauArgumentParser::convert_to_wide(const std::string& input) {
#ifdef _WIN32
    if (input.empty()) return L"";
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (size_needed <= 0) return L"";
    
    std::wstring result(size_needed - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, &result[0], size_needed);
    return result;
#else
    // Simple ASCII-only fallback for non-Windows platforms
    std::wstring result;
    result.reserve(input.size());
    for (char c : input) {
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    }
    return result;
#endif
}

std::string UtauArgumentParser::convert_from_wide(const std::wstring& input) {
#ifdef _WIN32
    if (input.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return "";
    
    std::string result(size_needed - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, &result[0], size_needed, nullptr, nullptr);
    return result;
#else
    // Simple ASCII-only fallback for non-Windows platforms
    std::string result;
    result.reserve(input.size());
    for (wchar_t c : input) {
        if (c <= 127) { // ASCII range
            result.push_back(static_cast<char>(c));
        } else {
            result.push_back('?'); // Placeholder for non-ASCII
        }
    }
    return result;
#endif
}

std::filesystem::path UtauArgumentParser::normalize_path(const std::string& path) {
    std::filesystem::path result(path);
    
    try {
        // Convert to absolute path if relative
        if (result.is_relative()) {
            result = std::filesystem::absolute(result);
        }
        
        // Normalize the path (resolve . and ..)
        result = std::filesystem::weakly_canonical(result);
        
    } catch (const std::filesystem::filesystem_error& e) {
        // If normalization fails, return the original path
        result = std::filesystem::path(path);
    }
    
    return result;
}

std::filesystem::path UtauArgumentParser::normalize_path(const std::wstring& path) {
    return normalize_path(convert_from_wide(path));
}

bool UtauArgumentParser::is_valid_wav_path(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".wav";
}

FlagValues UtauArgumentParser::parse_flags(const std::string& flags_string) {
    FlagValues values;
    
    if (flags_string.empty()) {
        return values;
    }
    
    // Regular expression to match flag patterns
    std::regex flag_regex(R"(([a-zA-Z]+)([+-]?\d+))");
    std::sregex_iterator iter(flags_string.begin(), flags_string.end(), flag_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        std::string flag_name = match[1].str();
        int flag_value = std::stoi(match[2].str());
        
        // Convert to lowercase for comparison
        std::transform(flag_name.begin(), flag_name.end(), flag_name.begin(), ::tolower);
        
        if (flag_name == "g") {
            values.g = std::clamp(flag_value, -100, 100);
        } else if (flag_name == "t") {
            values.t = std::clamp(flag_value, -100, 100);
        } else if (flag_name == "bre") {
            values.bre = std::clamp(flag_value, 0, 100);
        } else if (flag_name == "bri") {
            values.bri = std::clamp(flag_value, -100, 100);
        } else {
            // Store custom flags
            values.custom_flags[flag_name] = flag_value;
        }
    }
    
    return values;
}

bool UtauArgumentParser::is_valid_flag_format(const std::string& flags) {
    if (flags.empty()) return true;
    
    std::regex valid_flag_regex(R"(^([a-zA-Z]+[+-]?\d+)*$)");
    return std::regex_match(flags, valid_flag_regex);
}

void UtauArgumentParser::report_error(ResamplerError error, const std::string& details) {
    // Convert legacy ResamplerError to new UtauErrorCode
    UtauErrorCode utau_code = static_cast<UtauErrorCode>(error);
    
    // Set context for better error reporting
    auto& error_handler = UtauErrorHandler::instance();
    error_handler.set_context("component", "UTAU Argument Parser");
    if (!details.empty()) {
        error_handler.set_context("details", details);
    }
    
    // Report through the comprehensive error handling system
    error_handler.fatal_exit(utau_code, details.empty() ? get_error_description(error) : details);
}

std::string UtauArgumentParser::get_error_description(ResamplerError error) {
    // Use the new error handler for localized messages
    UtauErrorCode utau_code = static_cast<UtauErrorCode>(error);
    return UtauErrorHandler::instance().get_localized_message(utau_code);
}

// Private helper methods
bool UtauArgumentParser::is_shift_jis_encoded(const std::string& str) {
    // Simple heuristic: check for common Shift-JIS byte patterns
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char byte = static_cast<unsigned char>(str[i]);
        
        // Shift-JIS first byte ranges
        if ((byte >= 0x81 && byte <= 0x9F) || (byte >= 0xE0 && byte <= 0xFC)) {
            return true;
        }
    }
    
    return false;
}

std::string UtauArgumentParser::shift_jis_to_utf8(const std::string& shift_jis_str) {
#ifdef _WIN32
    // Convert Shift-JIS to UTF-16 first
    int wide_size = MultiByteToWideChar(932, 0, shift_jis_str.c_str(), -1, nullptr, 0);
    if (wide_size <= 0) return shift_jis_str;
    
    std::wstring wide_str(wide_size - 1, L'\0');
    MultiByteToWideChar(932, 0, shift_jis_str.c_str(), -1, &wide_str[0], wide_size);
    
    // Convert UTF-16 to UTF-8
    return convert_from_wide(wide_str);
#else
    // Use iconv on Unix systems
    iconv_t cd = iconv_open("UTF-8", "SHIFT-JIS");
    if (cd == (iconv_t)-1) {
        return shift_jis_str; // Fallback to original
    }
    
    const char* input = shift_jis_str.c_str();
    size_t input_len = shift_jis_str.length();
    size_t output_len = input_len * 3; // UTF-8 can be up to 3x longer
    
    std::string result(output_len, '\0');
    char* output = &result[0];
    size_t output_remaining = output_len;
    
    if (iconv(cd, const_cast<char**>(&input), &input_len, &output, &output_remaining) == (size_t)-1) {
        iconv_close(cd);
        return shift_jis_str; // Fallback to original
    }
    
    iconv_close(cd);
    result.resize(output_len - output_remaining);
    return result;
#endif
}

void UtauArgumentParser::log_debug(const std::string& message) {
    if (!debug_mode_) return;
    LOG_DEBUG(message);
}

void UtauArgumentParser::log_error(const std::string& message) {
    LOG_ERROR(message);
}

void UtauArgumentParser::set_log_file(const std::string& log_path) {
    log_file_path_ = log_path;
    
    // Configure the global UTAU logger to use this file
    if (!log_path.empty()) {
        UtauLogger::instance().set_log_file(log_path);
        UtauLogger::instance().set_output(LogOutput::BOTH); // Console + file
        LOG_INFO("Logging configured to file: " + log_path);
    } else {
        UtauLogger::instance().set_output(LogOutput::CONSOLE);
        LOG_INFO("Logging configured to console only");
    }
}

#ifdef _WIN32
std::string UtauArgumentParser::get_windows_error_string(DWORD error_code) {
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message_buffer, 0, nullptr);
    
    std::string message(message_buffer, size);
    LocalFree(message_buffer);
    
    return message;
}

bool UtauArgumentParser::set_console_utf8_mode() {
    // Set console to UTF-8 mode for proper character display
    return SetConsoleOutputCP(CP_UTF8) && SetConsoleCP(CP_UTF8);
}
#endif

} // namespace utau
} // namespace nexussynth
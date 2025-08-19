#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <exception>
#include <functional>
#include <chrono>
#include <memory>
#include "utau_logger.h"

namespace nexussynth {
namespace utau {

/**
 * @brief Extended UTAU resampler error codes with detailed categorization
 * 
 * Maintains compatibility with existing ResamplerError while providing
 * more granular error classification for better debugging and user feedback
 */
enum class UtauErrorCode {
    // Standard UTAU error codes (0-7) - maintain compatibility
    SUCCESS = 0,
    GENERAL_ERROR = 1,
    FILE_NOT_FOUND = 2,
    INVALID_WAV_FORMAT = 3,
    OUT_OF_MEMORY = 4,
    INVALID_PARAMETERS = 5,
    UNSUPPORTED_SAMPLE_RATE = 6,
    PROCESSING_ERROR = 7,
    
    // Extended error codes (8+) - NexusSynth specific
    PERMISSION_DENIED = 8,          // File access permissions
    DISK_FULL = 9,                  // Insufficient disk space
    CORRUPTED_INPUT = 10,           // Malformed input file
    INCOMPATIBLE_FORMAT = 11,       // Unsupported file format version
    SYNTHESIS_FAILURE = 12,         // Audio synthesis engine failure
    MODEL_LOAD_ERROR = 13,          // Voice model loading failure
    PARAMETER_OUT_OF_RANGE = 14,    // Parameter values outside valid range
    DEPENDENCY_ERROR = 15,          // Missing required libraries/files
    INITIALIZATION_ERROR = 16,      // Engine initialization failure
    TIMEOUT_ERROR = 17,             // Operation timeout
    THREAD_ERROR = 18,              // Threading/concurrency error
    CONFIGURATION_ERROR = 19,       // Invalid configuration
    NETWORK_ERROR = 20,             // Network-related error (if applicable)
    LICENSE_ERROR = 21,             // License validation failure
    VERSION_MISMATCH = 22,          // Incompatible version
    RESOURCE_EXHAUSTED = 23         // System resource exhaustion
};

/**
 * @brief Error severity levels for categorizing errors
 */
enum class ErrorSeverity {
    INFO,       // Informational - operation can continue
    WARNING,    // Warning - operation continues with degraded performance
    ERROR,      // Error - operation fails but recovery possible
    FATAL       // Fatal - immediate termination required
};

/**
 * @brief Error categories for grouping related errors
 */
enum class ErrorCategory {
    SYSTEM,         // System-level errors (file I/O, memory, permissions)
    AUDIO,          // Audio processing errors (format, synthesis, analysis)
    PARAMETER,      // Parameter validation and conversion errors
    MODEL,          // Voice model and training errors
    NETWORK,        // Network and communication errors
    LICENSE,        // Licensing and validation errors
    INTERNAL        // Internal engine errors
};

/**
 * @brief Comprehensive error information structure
 */
struct ErrorInfo {
    UtauErrorCode code;
    ErrorSeverity severity;
    ErrorCategory category;
    std::string message;
    std::string technical_details;
    std::string user_message;
    std::string suggested_action;
    std::chrono::system_clock::time_point timestamp;
    std::string source_file;
    int source_line;
    std::string function_name;
    
    // Context information
    std::unordered_map<std::string, std::string> context;
    
    ErrorInfo(UtauErrorCode err_code, const std::string& msg = "")
        : code(err_code), message(msg), timestamp(std::chrono::system_clock::now()) {
        classify_error();
    }
    
private:
    void classify_error();
};

/**
 * @brief Custom exception class for UTAU-specific errors
 */
class UtauException : public std::exception {
public:
    explicit UtauException(const ErrorInfo& info) : error_info_(info) {}
    explicit UtauException(UtauErrorCode code, const std::string& message = "")
        : error_info_(code, message) {}
    
    const char* what() const noexcept override {
        return error_info_.message.c_str();
    }
    
    const ErrorInfo& get_error_info() const { return error_info_; }
    UtauErrorCode get_error_code() const { return error_info_.code; }
    ErrorSeverity get_severity() const { return error_info_.severity; }
    
private:
    ErrorInfo error_info_;
};

/**
 * @brief Error recovery strategy function type
 */
using ErrorRecoveryStrategy = std::function<bool(const ErrorInfo&)>;

/**
 * @brief Comprehensive UTAU error handler with logging and recovery
 */
class UtauErrorHandler {
public:
    UtauErrorHandler();
    ~UtauErrorHandler();
    
    // Singleton access
    static UtauErrorHandler& instance();
    
    // Error reporting methods
    void report_error(UtauErrorCode code, const std::string& message = "");
    void report_error(const ErrorInfo& error_info);
    void report_exception(const std::exception& e, const std::string& context = "");
    
    // Error handling configuration
    void set_exit_on_fatal(bool exit_on_fatal) { exit_on_fatal_ = exit_on_fatal; }
    void set_log_all_errors(bool log_all) { log_all_errors_ = log_all; }
    void set_user_friendly_messages(bool friendly) { user_friendly_messages_ = friendly; }
    void set_debug_mode(bool debug) { debug_mode_ = debug; }
    
    // Recovery strategies
    void register_recovery_strategy(UtauErrorCode code, ErrorRecoveryStrategy strategy);
    void register_category_recovery(ErrorCategory category, ErrorRecoveryStrategy strategy);
    bool attempt_recovery(const ErrorInfo& error_info);
    
    // Error statistics and reporting
    size_t get_error_count(ErrorSeverity severity = ErrorSeverity::ERROR) const;
    size_t get_error_count(ErrorCategory category) const;
    std::vector<ErrorInfo> get_recent_errors(size_t max_count = 10) const;
    void clear_error_history();
    
    // Error message localization
    void set_language(const std::string& language_code);
    std::string get_localized_message(UtauErrorCode code) const;
    std::string get_localized_suggestion(UtauErrorCode code) const;
    
    // Context management
    void set_context(const std::string& key, const std::string& value);
    void clear_context();
    std::string get_context_string() const;
    
    // UTAU compatibility methods
    [[noreturn]] void fatal_exit(UtauErrorCode code, const std::string& message = "");
    int get_exit_code(UtauErrorCode code) const;
    bool is_recoverable_error(UtauErrorCode code) const;
    
    // Error validation and testing
    bool validate_error_system();
    void test_all_error_codes();
    
private:
    bool exit_on_fatal_;
    bool log_all_errors_;
    bool user_friendly_messages_;
    bool debug_mode_;
    std::string current_language_;
    
    // Error tracking
    std::vector<ErrorInfo> error_history_;
    size_t max_history_size_;
    mutable std::mutex error_mutex_;
    
    // Recovery strategies
    std::unordered_map<UtauErrorCode, ErrorRecoveryStrategy> code_recovery_strategies_;
    std::unordered_map<ErrorCategory, ErrorRecoveryStrategy> category_recovery_strategies_;
    
    // Context information
    std::unordered_map<std::string, std::string> current_context_;
    
    // Localization maps
    std::unordered_map<std::string, std::unordered_map<UtauErrorCode, std::string>> localized_messages_;
    std::unordered_map<std::string, std::unordered_map<UtauErrorCode, std::string>> localized_suggestions_;
    
    // Helper methods
    void log_error(const ErrorInfo& error_info);
    void handle_fatal_error(const ErrorInfo& error_info);
    std::string format_error_message(const ErrorInfo& error_info) const;
    std::string get_severity_string(ErrorSeverity severity) const;
    std::string get_category_string(ErrorCategory category) const;
    void initialize_default_messages();
    void initialize_recovery_strategies();
    ErrorInfo create_error_info(UtauErrorCode code, const std::string& message,
                               const std::string& source_file = "", int source_line = 0,
                               const std::string& function_name = "");
};

/**
 * @brief Utility functions for error handling
 */
namespace ErrorUtils {
    
    // Error code conversion utilities
    UtauErrorCode from_system_error(int system_errno);
    UtauErrorCode from_exception(const std::exception& e);
    std::string get_system_error_message(int error_code);
    
    // Error context helpers
    std::string get_current_stack_trace();
    std::string get_system_info();
    std::string get_memory_usage();
    
    // Validation utilities
    bool is_valid_error_code(int code);
    bool is_standard_utau_code(UtauErrorCode code);
    
    // UTAU compatibility testing
    struct UtauCompatibilityTest {
        UtauErrorCode code;
        std::string test_scenario;
        std::string expected_behavior;
        bool passed;
    };
    
    std::vector<UtauCompatibilityTest> run_compatibility_tests();
    bool verify_exit_code_compliance();
    
} // namespace ErrorUtils

/**
 * @brief Convenient macros for error reporting with file/line information
 */
#define UTAU_REPORT_ERROR(code, message) \
    nexussynth::utau::UtauErrorHandler::instance().report_error( \
        nexussynth::utau::UtauErrorHandler::instance().create_error_info( \
            code, message, __FILE__, __LINE__, __FUNCTION__))

#define UTAU_FATAL_ERROR(code, message) \
    nexussynth::utau::UtauErrorHandler::instance().fatal_exit(code, message)

#define UTAU_THROW_ERROR(code, message) \
    throw nexussynth::utau::UtauException(code, message)

#define UTAU_TRY_RECOVER(code, message) \
    do { \
        auto error_info = nexussynth::utau::UtauErrorHandler::instance().create_error_info( \
            code, message, __FILE__, __LINE__, __FUNCTION__); \
        if (!nexussynth::utau::UtauErrorHandler::instance().attempt_recovery(error_info)) { \
            nexussynth::utau::UtauErrorHandler::instance().report_error(error_info); \
        } \
    } while(0)

} // namespace utau
} // namespace nexussynth
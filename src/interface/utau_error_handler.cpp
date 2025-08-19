#include "nexussynth/utau_error_handler.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#else
#include <sys/utsname.h>
#include <sys/resource.h>
#include <execinfo.h>
#include <errno.h>
#include <string.h>
#endif

namespace nexussynth {
namespace utau {

// ErrorInfo implementation
void ErrorInfo::classify_error() {
    // Classify severity and category based on error code
    switch (code) {
        case UtauErrorCode::SUCCESS:
            severity = ErrorSeverity::INFO;
            category = ErrorCategory::SYSTEM;
            break;
            
        case UtauErrorCode::FILE_NOT_FOUND:
        case UtauErrorCode::PERMISSION_DENIED:
        case UtauErrorCode::DISK_FULL:
            severity = ErrorSeverity::ERROR;
            category = ErrorCategory::SYSTEM;
            break;
            
        case UtauErrorCode::INVALID_WAV_FORMAT:
        case UtauErrorCode::UNSUPPORTED_SAMPLE_RATE:
        case UtauErrorCode::CORRUPTED_INPUT:
        case UtauErrorCode::INCOMPATIBLE_FORMAT:
            severity = ErrorSeverity::ERROR;
            category = ErrorCategory::AUDIO;
            break;
            
        case UtauErrorCode::INVALID_PARAMETERS:
        case UtauErrorCode::PARAMETER_OUT_OF_RANGE:
            severity = ErrorSeverity::ERROR;
            category = ErrorCategory::PARAMETER;
            break;
            
        case UtauErrorCode::OUT_OF_MEMORY:
        case UtauErrorCode::RESOURCE_EXHAUSTED:
            severity = ErrorSeverity::FATAL;
            category = ErrorCategory::SYSTEM;
            break;
            
        case UtauErrorCode::SYNTHESIS_FAILURE:
        case UtauErrorCode::MODEL_LOAD_ERROR:
            severity = ErrorSeverity::ERROR;
            category = ErrorCategory::MODEL;
            break;
            
        case UtauErrorCode::INITIALIZATION_ERROR:
        case UtauErrorCode::DEPENDENCY_ERROR:
            severity = ErrorSeverity::FATAL;
            category = ErrorCategory::INTERNAL;
            break;
            
        case UtauErrorCode::LICENSE_ERROR:
            severity = ErrorSeverity::ERROR;
            category = ErrorCategory::LICENSE;
            break;
            
        default:
            severity = ErrorSeverity::ERROR;
            category = ErrorCategory::INTERNAL;
            break;
    }
}

// UtauErrorHandler implementation
UtauErrorHandler::UtauErrorHandler()
    : exit_on_fatal_(true)
    , log_all_errors_(true)
    , user_friendly_messages_(false)
    , debug_mode_(false)
    , current_language_("en")
    , max_history_size_(100) {
    
    initialize_default_messages();
    initialize_recovery_strategies();
}

UtauErrorHandler::~UtauErrorHandler() = default;

UtauErrorHandler& UtauErrorHandler::instance() {
    static UtauErrorHandler instance;
    return instance;
}

void UtauErrorHandler::report_error(UtauErrorCode code, const std::string& message) {
    ErrorInfo error_info(code, message);
    error_info.context = current_context_;
    report_error(error_info);
}

void UtauErrorHandler::report_error(const ErrorInfo& error_info) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    // Add to error history
    error_history_.push_back(error_info);
    if (error_history_.size() > max_history_size_) {
        error_history_.erase(error_history_.begin());
    }
    
    // Log the error if enabled
    if (log_all_errors_ || error_info.severity >= ErrorSeverity::ERROR) {
        log_error(error_info);
    }
    
    // Handle fatal errors
    if (error_info.severity == ErrorSeverity::FATAL) {
        handle_fatal_error(error_info);
    }
}

void UtauErrorHandler::report_exception(const std::exception& e, const std::string& context) {
    UtauErrorCode code = ErrorUtils::from_exception(e);
    std::string message = std::string(e.what());
    if (!context.empty()) {
        message += " (Context: " + context + ")";
    }
    
    report_error(code, message);
}

void UtauErrorHandler::register_recovery_strategy(UtauErrorCode code, ErrorRecoveryStrategy strategy) {
    code_recovery_strategies_[code] = strategy;
}

void UtauErrorHandler::register_category_recovery(ErrorCategory category, ErrorRecoveryStrategy strategy) {
    category_recovery_strategies_[category] = strategy;
}

bool UtauErrorHandler::attempt_recovery(const ErrorInfo& error_info) {
    // Try code-specific recovery first
    auto code_it = code_recovery_strategies_.find(error_info.code);
    if (code_it != code_recovery_strategies_.end()) {
        if (code_it->second(error_info)) {
            LOG_INFO("Error recovery successful for code " + std::to_string(static_cast<int>(error_info.code)));
            return true;
        }
    }
    
    // Try category-based recovery
    auto category_it = category_recovery_strategies_.find(error_info.category);
    if (category_it != category_recovery_strategies_.end()) {
        if (category_it->second(error_info)) {
            LOG_INFO("Error recovery successful for category " + get_category_string(error_info.category));
            return true;
        }
    }
    
    return false;
}

size_t UtauErrorHandler::get_error_count(ErrorSeverity severity) const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return std::count_if(error_history_.begin(), error_history_.end(),
        [severity](const ErrorInfo& info) { return info.severity == severity; });
}

size_t UtauErrorHandler::get_error_count(ErrorCategory category) const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return std::count_if(error_history_.begin(), error_history_.end(),
        [category](const ErrorInfo& info) { return info.category == category; });
}

std::vector<ErrorInfo> UtauErrorHandler::get_recent_errors(size_t max_count) const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    size_t start_idx = (error_history_.size() > max_count) ? 
        error_history_.size() - max_count : 0;
    return std::vector<ErrorInfo>(error_history_.begin() + start_idx, error_history_.end());
}

void UtauErrorHandler::clear_error_history() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    error_history_.clear();
}

void UtauErrorHandler::set_language(const std::string& language_code) {
    current_language_ = language_code;
}

std::string UtauErrorHandler::get_localized_message(UtauErrorCode code) const {
    auto lang_it = localized_messages_.find(current_language_);
    if (lang_it != localized_messages_.end()) {
        auto msg_it = lang_it->second.find(code);
        if (msg_it != lang_it->second.end()) {
            return msg_it->second;
        }
    }
    
    // Fallback to English
    auto en_it = localized_messages_.find("en");
    if (en_it != localized_messages_.end()) {
        auto msg_it = en_it->second.find(code);
        if (msg_it != en_it->second.end()) {
            return msg_it->second;
        }
    }
    
    return "Unknown error";
}

std::string UtauErrorHandler::get_localized_suggestion(UtauErrorCode code) const {
    auto lang_it = localized_suggestions_.find(current_language_);
    if (lang_it != localized_suggestions_.end()) {
        auto sug_it = lang_it->second.find(code);
        if (sug_it != lang_it->second.end()) {
            return sug_it->second;
        }
    }
    
    // Fallback to English
    auto en_it = localized_suggestions_.find("en");
    if (en_it != localized_suggestions_.end()) {
        auto sug_it = en_it->second.find(code);
        if (sug_it != en_it->second.end()) {
            return sug_it->second;
        }
    }
    
    return "Contact support for assistance";
}

void UtauErrorHandler::set_context(const std::string& key, const std::string& value) {
    current_context_[key] = value;
}

void UtauErrorHandler::clear_context() {
    current_context_.clear();
}

std::string UtauErrorHandler::get_context_string() const {
    std::ostringstream oss;
    for (const auto& [key, value] : current_context_) {
        if (oss.tellp() > 0) oss << ", ";
        oss << key << "=" << value;
    }
    return oss.str();
}

[[noreturn]] void UtauErrorHandler::fatal_exit(UtauErrorCode code, const std::string& message) {
    ErrorInfo error_info(code, message);
    error_info.severity = ErrorSeverity::FATAL;
    
    LOG_FATAL("Fatal error: " + message);
    UtauLogger::instance().flush();
    
    std::exit(get_exit_code(code));
}

int UtauErrorHandler::get_exit_code(UtauErrorCode code) const {
    // Map to standard UTAU exit codes (0-7) for compatibility
    int exit_code = static_cast<int>(code);
    if (exit_code > 7) {
        // Map extended codes to standard range
        switch (code) {
            case UtauErrorCode::PERMISSION_DENIED:
            case UtauErrorCode::DISK_FULL:
                return static_cast<int>(UtauErrorCode::FILE_NOT_FOUND);
            case UtauErrorCode::CORRUPTED_INPUT:
            case UtauErrorCode::INCOMPATIBLE_FORMAT:
                return static_cast<int>(UtauErrorCode::INVALID_WAV_FORMAT);
            case UtauErrorCode::PARAMETER_OUT_OF_RANGE:
                return static_cast<int>(UtauErrorCode::INVALID_PARAMETERS);
            case UtauErrorCode::RESOURCE_EXHAUSTED:
                return static_cast<int>(UtauErrorCode::OUT_OF_MEMORY);
            default:
                return static_cast<int>(UtauErrorCode::GENERAL_ERROR);
        }
    }
    return exit_code;
}

bool UtauErrorHandler::is_recoverable_error(UtauErrorCode code) const {
    switch (code) {
        case UtauErrorCode::SUCCESS:
            return true;
        case UtauErrorCode::OUT_OF_MEMORY:
        case UtauErrorCode::INITIALIZATION_ERROR:
        case UtauErrorCode::RESOURCE_EXHAUSTED:
            return false;  // Fatal errors - no recovery
        default:
            return true;   // Most errors are recoverable
    }
}

bool UtauErrorHandler::validate_error_system() {
    try {
        // Test basic error reporting
        report_error(UtauErrorCode::SUCCESS, "Test error");
        
        // Test recovery mechanism
        register_recovery_strategy(UtauErrorCode::GENERAL_ERROR, 
            [](const ErrorInfo&) { return true; });
        
        ErrorInfo test_error(UtauErrorCode::GENERAL_ERROR, "Test recovery");
        bool recovery_worked = attempt_recovery(test_error);
        
        return recovery_worked;
    } catch (...) {
        return false;
    }
}

void UtauErrorHandler::test_all_error_codes() {
    LOG_INFO("Testing all UTAU error codes...");
    
    std::vector<UtauErrorCode> test_codes = {
        UtauErrorCode::SUCCESS,
        UtauErrorCode::GENERAL_ERROR,
        UtauErrorCode::FILE_NOT_FOUND,
        UtauErrorCode::INVALID_WAV_FORMAT,
        UtauErrorCode::OUT_OF_MEMORY,
        UtauErrorCode::INVALID_PARAMETERS,
        UtauErrorCode::UNSUPPORTED_SAMPLE_RATE,
        UtauErrorCode::PROCESSING_ERROR
    };
    
    for (auto code : test_codes) {
        std::string msg = "Test message for code " + std::to_string(static_cast<int>(code));
        LOG_DEBUG("Testing error code: " + msg);
        
        // Don't actually report fatal errors during testing
        if (code != UtauErrorCode::OUT_OF_MEMORY) {
            ErrorInfo test_error(code, msg);
            if (test_error.severity != ErrorSeverity::FATAL) {
                report_error(test_error);
            }
        }
    }
    
    LOG_INFO("Error code testing completed");
}

// Private helper methods
void UtauErrorHandler::log_error(const ErrorInfo& error_info) {
    std::string formatted_msg = format_error_message(error_info);
    
    switch (error_info.severity) {
        case ErrorSeverity::INFO:
            LOG_INFO(formatted_msg);
            break;
        case ErrorSeverity::WARNING:
            LOG_WARN(formatted_msg);
            break;
        case ErrorSeverity::ERROR:
            LOG_ERROR(formatted_msg);
            break;
        case ErrorSeverity::FATAL:
            LOG_FATAL(formatted_msg);
            break;
    }
}

void UtauErrorHandler::handle_fatal_error(const ErrorInfo& error_info) {
    LOG_FATAL("Fatal error encountered - preparing for shutdown");
    
    if (debug_mode_) {
        LOG_DEBUG("Stack trace: " + ErrorUtils::get_current_stack_trace());
        LOG_DEBUG("System info: " + ErrorUtils::get_system_info());
        LOG_DEBUG("Memory usage: " + ErrorUtils::get_memory_usage());
    }
    
    UtauLogger::instance().flush();
    
    if (exit_on_fatal_) {
        std::exit(get_exit_code(error_info.code));
    }
}

std::string UtauErrorHandler::format_error_message(const ErrorInfo& error_info) const {
    std::ostringstream oss;
    
    if (user_friendly_messages_) {
        oss << get_localized_message(error_info.code);
        if (!error_info.user_message.empty()) {
            oss << " - " << error_info.user_message;
        }
    } else {
        oss << "[" << get_severity_string(error_info.severity) << "] ";
        oss << "[" << get_category_string(error_info.category) << "] ";
        oss << "Error " << static_cast<int>(error_info.code) << ": " << error_info.message;
        
        if (!error_info.technical_details.empty()) {
            oss << " (" << error_info.technical_details << ")";
        }
        
        if (!current_context_.empty()) {
            oss << " [Context: " << get_context_string() << "]";
        }
    }
    
    return oss.str();
}

std::string UtauErrorHandler::get_severity_string(ErrorSeverity severity) const {
    switch (severity) {
        case ErrorSeverity::INFO: return "INFO";
        case ErrorSeverity::WARNING: return "WARN";
        case ErrorSeverity::ERROR: return "ERROR";
        case ErrorSeverity::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string UtauErrorHandler::get_category_string(ErrorCategory category) const {
    switch (category) {
        case ErrorCategory::SYSTEM: return "SYSTEM";
        case ErrorCategory::AUDIO: return "AUDIO";
        case ErrorCategory::PARAMETER: return "PARAMETER";
        case ErrorCategory::MODEL: return "MODEL";
        case ErrorCategory::NETWORK: return "NETWORK";
        case ErrorCategory::LICENSE: return "LICENSE";
        case ErrorCategory::INTERNAL: return "INTERNAL";
        default: return "UNKNOWN";
    }
}

void UtauErrorHandler::initialize_default_messages() {
    // English messages
    auto& en_messages = localized_messages_["en"];
    en_messages[UtauErrorCode::SUCCESS] = "Operation completed successfully";
    en_messages[UtauErrorCode::GENERAL_ERROR] = "An error occurred during processing";
    en_messages[UtauErrorCode::FILE_NOT_FOUND] = "The specified file could not be found";
    en_messages[UtauErrorCode::INVALID_WAV_FORMAT] = "The audio file format is not supported";
    en_messages[UtauErrorCode::OUT_OF_MEMORY] = "Insufficient memory to complete the operation";
    en_messages[UtauErrorCode::INVALID_PARAMETERS] = "One or more parameters are invalid";
    en_messages[UtauErrorCode::UNSUPPORTED_SAMPLE_RATE] = "The audio sample rate is not supported";
    en_messages[UtauErrorCode::PROCESSING_ERROR] = "An error occurred during audio processing";
    en_messages[UtauErrorCode::PERMISSION_DENIED] = "Access to the file or directory is denied";
    en_messages[UtauErrorCode::DISK_FULL] = "Insufficient disk space to complete the operation";
    
    // English suggestions
    auto& en_suggestions = localized_suggestions_["en"];
    en_suggestions[UtauErrorCode::FILE_NOT_FOUND] = "Check the file path and ensure the file exists";
    en_suggestions[UtauErrorCode::INVALID_WAV_FORMAT] = "Use a supported audio format (WAV, 16-bit PCM recommended)";
    en_suggestions[UtauErrorCode::OUT_OF_MEMORY] = "Close other applications to free memory";
    en_suggestions[UtauErrorCode::PERMISSION_DENIED] = "Run as administrator or check file permissions";
    en_suggestions[UtauErrorCode::DISK_FULL] = "Free up disk space and try again";
    
    // Korean messages (한국어 메시지)
    auto& ko_messages = localized_messages_["ko"];
    ko_messages[UtauErrorCode::SUCCESS] = "작업이 성공적으로 완료되었습니다";
    ko_messages[UtauErrorCode::GENERAL_ERROR] = "처리 중 오류가 발생했습니다";
    ko_messages[UtauErrorCode::FILE_NOT_FOUND] = "지정된 파일을 찾을 수 없습니다";
    ko_messages[UtauErrorCode::INVALID_WAV_FORMAT] = "지원하지 않는 오디오 파일 형식입니다";
    ko_messages[UtauErrorCode::OUT_OF_MEMORY] = "작업을 완료하기에 메모리가 부족합니다";
    ko_messages[UtauErrorCode::INVALID_PARAMETERS] = "하나 이상의 매개변수가 유효하지 않습니다";
    ko_messages[UtauErrorCode::UNSUPPORTED_SAMPLE_RATE] = "지원하지 않는 오디오 샘플 레이트입니다";
    ko_messages[UtauErrorCode::PROCESSING_ERROR] = "오디오 처리 중 오류가 발생했습니다";
}

void UtauErrorHandler::initialize_recovery_strategies() {
    // File access recovery strategy
    register_recovery_strategy(UtauErrorCode::FILE_NOT_FOUND, [](const ErrorInfo& error) {
        LOG_INFO("Attempting file recovery for: " + error.message);
        // Could implement file search, temporary file creation, etc.
        return false; // For now, no automatic recovery
    });
    
    // Memory recovery strategy
    register_recovery_strategy(UtauErrorCode::OUT_OF_MEMORY, [](const ErrorInfo& /* error */) {
        LOG_WARN("Memory exhausted - attempting garbage collection");
        // Could trigger garbage collection, buffer cleanup, etc.
        return false; // Memory errors are typically fatal
    });
    
    // Parameter validation recovery
    register_category_recovery(ErrorCategory::PARAMETER, [](const ErrorInfo& error) {
        LOG_INFO("Attempting parameter correction for: " + error.message);
        // Could implement parameter sanitization, default value substitution
        return false; // Requires user input typically
    });
}

ErrorInfo UtauErrorHandler::create_error_info(UtauErrorCode code, const std::string& message,
                                            const std::string& source_file, int source_line,
                                            const std::string& function_name) {
    ErrorInfo info(code, message);
    info.source_file = source_file;
    info.source_line = source_line;
    info.function_name = function_name;
    info.context = current_context_;
    return info;
}

// ErrorUtils implementation
namespace ErrorUtils {

UtauErrorCode from_system_error(int system_errno) {
    switch (system_errno) {
        case ENOENT:
            return UtauErrorCode::FILE_NOT_FOUND;
        case EACCES:
            return UtauErrorCode::PERMISSION_DENIED;
        case ENOMEM:
            return UtauErrorCode::OUT_OF_MEMORY;
        case ENOSPC:
            return UtauErrorCode::DISK_FULL;
        case EINVAL:
            return UtauErrorCode::INVALID_PARAMETERS;
        default:
            return UtauErrorCode::GENERAL_ERROR;
    }
}

UtauErrorCode from_exception(const std::exception& e) {
    std::string what = e.what();
    std::transform(what.begin(), what.end(), what.begin(), ::tolower);
    
    if (what.find("memory") != std::string::npos || what.find("alloc") != std::string::npos) {
        return UtauErrorCode::OUT_OF_MEMORY;
    }
    if (what.find("file") != std::string::npos || what.find("path") != std::string::npos) {
        return UtauErrorCode::FILE_NOT_FOUND;
    }
    if (what.find("invalid") != std::string::npos || what.find("argument") != std::string::npos) {
        return UtauErrorCode::INVALID_PARAMETERS;
    }
    
    return UtauErrorCode::GENERAL_ERROR;
}

std::string get_system_error_message(int error_code) {
#ifdef _WIN32
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 (LPSTR)&message_buffer, 0, NULL);
    
    std::string message(message_buffer, size);
    LocalFree(message_buffer);
    return message;
#else
    return std::string(strerror(error_code));
#endif
}

std::string get_current_stack_trace() {
    std::string trace = "Stack trace not available";
    
#ifndef _WIN32
    void* array[10];
    size_t size = backtrace(array, 10);
    char** strings = backtrace_symbols(array, size);
    
    if (strings != nullptr) {
        std::ostringstream oss;
        for (size_t i = 0; i < size; i++) {
            oss << strings[i] << "\n";
        }
        trace = oss.str();
        free(strings);
    }
#endif
    
    return trace;
}

std::string get_system_info() {
    std::ostringstream oss;
    
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    oss << "Windows System - Processors: " << sys_info.dwNumberOfProcessors;
#else
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        oss << sys_info.sysname << " " << sys_info.release << " " << sys_info.machine;
    }
#endif
    
    return oss.str();
}

std::string get_memory_usage() {
    std::ostringstream oss;
    
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        oss << "Memory usage: " << (pmc.WorkingSetSize / 1024 / 1024) << " MB";
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        oss << "Memory usage: " << (usage.ru_maxrss / 1024) << " MB";
    }
#endif
    
    return oss.str();
}

bool is_valid_error_code(int code) {
    return code >= 0 && code <= static_cast<int>(UtauErrorCode::VERSION_MISMATCH);
}

bool is_standard_utau_code(UtauErrorCode code) {
    return static_cast<int>(code) <= 7;
}

std::vector<UtauCompatibilityTest> run_compatibility_tests() {
    std::vector<UtauCompatibilityTest> results;
    
    // Test standard error codes
    std::vector<UtauErrorCode> standard_codes = {
        UtauErrorCode::SUCCESS,
        UtauErrorCode::GENERAL_ERROR,
        UtauErrorCode::FILE_NOT_FOUND,
        UtauErrorCode::INVALID_WAV_FORMAT,
        UtauErrorCode::OUT_OF_MEMORY,
        UtauErrorCode::INVALID_PARAMETERS,
        UtauErrorCode::UNSUPPORTED_SAMPLE_RATE,
        UtauErrorCode::PROCESSING_ERROR
    };
    
    for (auto code : standard_codes) {
        UtauCompatibilityTest test;
        test.code = code;
        test.test_scenario = "Standard UTAU error code " + std::to_string(static_cast<int>(code));
        test.expected_behavior = "Should map to exit code " + std::to_string(static_cast<int>(code));
        test.passed = (UtauErrorHandler::instance().get_exit_code(code) == static_cast<int>(code));
        results.push_back(test);
    }
    
    return results;
}

bool verify_exit_code_compliance() {
    auto tests = run_compatibility_tests();
    return std::all_of(tests.begin(), tests.end(), 
        [](const UtauCompatibilityTest& test) { return test.passed; });
}

} // namespace ErrorUtils

} // namespace utau
} // namespace nexussynth
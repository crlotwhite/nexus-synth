#include "nexussynth/utau_logger.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

namespace nexussynth {
namespace utau {

namespace {
    // ANSI color codes for different log levels
    const std::map<LogLevel, std::string> LEVEL_COLORS = {
        {LogLevel::DEBUG, "\033[36m"},    // Cyan
        {LogLevel::INFO, "\033[32m"},     // Green  
        {LogLevel::WARN, "\033[33m"},     // Yellow
        {LogLevel::ERROR, "\033[31m"},    // Red
        {LogLevel::FATAL, "\033[35m"}     // Magenta
    };
    
    const std::string COLOR_RESET = "\033[0m";
    
    // Windows console color codes
    #ifdef _WIN32
    const std::map<LogLevel, WORD> WIN_LEVEL_COLORS = {
        {LogLevel::DEBUG, FOREGROUND_BLUE | FOREGROUND_GREEN},
        {LogLevel::INFO, FOREGROUND_GREEN},
        {LogLevel::WARN, FOREGROUND_RED | FOREGROUND_GREEN},
        {LogLevel::ERROR, FOREGROUND_RED},
        {LogLevel::FATAL, FOREGROUND_RED | FOREGROUND_BLUE}
    };
    #endif
}

// UtauLogger implementation
UtauLogger& UtauLogger::instance() {
    static UtauLogger global_instance("Global");
    return global_instance;
}

UtauLogger::UtauLogger(const std::string& name) 
    : logger_name_(name) {
    stats_.start_time = std::chrono::steady_clock::now();
    initialize_console_output();
}

UtauLogger::~UtauLogger() {
    close();
}

void UtauLogger::set_log_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (file_stream_) {
        file_stream_->close();
        file_stream_.reset();
    }
    
    if (!file_path.empty()) {
        log_file_path_ = file_path;
        
        // Create directory if it doesn't exist
        std::filesystem::path log_path(file_path);
        std::filesystem::create_directories(log_path.parent_path());
        
        file_stream_ = std::make_unique<std::ofstream>(file_path, std::ios::app);
        if (!file_stream_->is_open()) {
            std::cerr << "Warning: Failed to open log file: " << file_path << std::endl;
            file_stream_.reset();
        }
    }
}

void UtauLogger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void UtauLogger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void UtauLogger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void UtauLogger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void UtauLogger::fatal(const std::string& message) {
    log(LogLevel::FATAL, message);
}

void UtauLogger::log(LogLevel level, const std::string& message) {
    if (!should_log(level)) return;
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Update statistics
    switch (level) {
        case LogLevel::DEBUG: stats_.debug_count++; break;
        case LogLevel::INFO: stats_.info_count++; break;
        case LogLevel::WARN: stats_.warn_count++; break;
        case LogLevel::ERROR: stats_.error_count++; break;
        case LogLevel::FATAL: stats_.fatal_count++; break;
    }
    
    std::string formatted_message = format_message(level, message);
    stats_.total_bytes_written += formatted_message.size();
    
    // Write to appropriate outputs
    if (output_dest_ == LogOutput::CONSOLE || output_dest_ == LogOutput::BOTH) {
        write_to_console(formatted_message, level);
    }
    
    if ((output_dest_ == LogOutput::FILE || output_dest_ == LogOutput::BOTH) && file_stream_) {
        write_to_file(formatted_message);
        check_and_rotate_log();
    }
}

void UtauLogger::log_resampler_start(const std::string& input_file, const std::string& output_file) {
    info_f("Starting resampler: %s -> %s", input_file.c_str(), output_file.c_str());
}

void UtauLogger::log_resampler_end(bool success, double processing_time_ms) {
    if (success) {
        info_f("Resampling completed successfully in %.2fms", processing_time_ms);
    } else {
        error_f("Resampling failed after %.2fms", processing_time_ms);
    }
}

void UtauLogger::log_flag_conversion(const std::string& flags, const std::string& result) {
    debug_f("Flag conversion: '%s' -> %s", flags.c_str(), result.c_str());
}

void UtauLogger::log_file_operation(const std::string& operation, const std::string& file_path, bool success) {
    if (success) {
        debug_f("File %s successful: %s", operation.c_str(), file_path.c_str());
    } else {
        warn_f("File %s failed: %s", operation.c_str(), file_path.c_str());
    }
}

void UtauLogger::log_parameter_validation(const std::string& parameter, const std::string& value, bool valid) {
    if (valid) {
        debug_f("Parameter validation OK: %s = %s", parameter.c_str(), value.c_str());
    } else {
        warn_f("Parameter validation failed: %s = %s", parameter.c_str(), value.c_str());
    }
}

void UtauLogger::flush() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout.flush();
    std::cerr.flush();
    if (file_stream_) {
        file_stream_->flush();
    }
}

void UtauLogger::close() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (file_stream_) {
        file_stream_->close();
        file_stream_.reset();
    }
}

void UtauLogger::reset_stats() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    stats_ = LogStats{};
    stats_.start_time = std::chrono::steady_clock::now();
}

std::string UtauLogger::format_message(LogLevel level, const std::string& message) {
    std::ostringstream oss;
    
    if (format_.include_timestamp) {
        oss << "[" << get_timestamp() << "]";
    }
    
    if (format_.include_level) {
        oss << "[" << get_level_string(level) << "]";
    }
    
    if (format_.include_thread_id) {
        oss << "[" << std::this_thread::get_id() << "]";
    }
    
    if (!logger_name_.empty()) {
        oss << "[" << logger_name_ << "]";
    }
    
    oss << " " << message;
    
    return oss.str();
}

std::string UtauLogger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), format_.timestamp_format.c_str());
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string UtauLogger::get_level_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string UtauLogger::get_level_color(LogLevel level) {
    auto it = LEVEL_COLORS.find(level);
    return (it != LEVEL_COLORS.end()) ? it->second : "";
}

void UtauLogger::write_to_console(const std::string& message, LogLevel level) {
    std::ostream& stream = (level >= LogLevel::ERROR) ? std::cerr : std::cout;
    
    if (format_.use_colors) {
        #ifdef _WIN32
        if (format_.enable_windows_colors) {
            HANDLE console_handle = (level >= LogLevel::ERROR) ? 
                GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
            
            auto color_it = WIN_LEVEL_COLORS.find(level);
            if (color_it != WIN_LEVEL_COLORS.end()) {
                SetConsoleTextAttribute(console_handle, color_it->second);
            }
            
            stream << message << std::endl;
            
            // Reset to default color
            SetConsoleTextAttribute(console_handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        } else {
            stream << get_level_color(level) << message << COLOR_RESET << std::endl;
        }
        #else
        // Unix/Linux ANSI colors
        stream << get_level_color(level) << message << COLOR_RESET << std::endl;
        #endif
    } else {
        stream << message << std::endl;
    }
}

void UtauLogger::write_to_file(const std::string& message) {
    if (file_stream_ && file_stream_->is_open()) {
        *file_stream_ << message << std::endl;
    }
}

void UtauLogger::check_and_rotate_log() {
    if (!rotation_.enabled || !file_stream_ || log_file_path_.empty()) return;
    
    // Check file size
    auto current_pos = file_stream_->tellp();
    if (current_pos > 0 && static_cast<size_t>(current_pos) >= rotation_.max_file_size) {
        rotate_log_file();
    }
}

void UtauLogger::rotate_log_file() {
    if (!file_stream_ || log_file_path_.empty()) return;
    
    // Close current file
    file_stream_->close();
    
    // Get backup files and remove excess ones
    auto backup_files = get_backup_files();
    while (backup_files.size() >= rotation_.max_backup_files) {
        std::filesystem::remove(backup_files.back());
        backup_files.pop_back();
    }
    
    // Rename existing backups
    for (int i = static_cast<int>(backup_files.size()) - 1; i >= 0; --i) {
        std::string old_name = backup_files[i];
        std::string new_name = log_file_path_ + rotation_.backup_suffix + "." + std::to_string(i + 2);
        std::filesystem::rename(old_name, new_name);
    }
    
    // Move current log to backup.1
    std::string backup_name = log_file_path_ + rotation_.backup_suffix + ".1";
    std::filesystem::rename(log_file_path_, backup_name);
    
    // Create new log file
    file_stream_ = std::make_unique<std::ofstream>(log_file_path_, std::ios::app);
}

std::vector<std::string> UtauLogger::get_backup_files() {
    std::vector<std::string> backup_files;
    std::string backup_pattern = log_file_path_ + rotation_.backup_suffix;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(log_file_path_).parent_path())) {
            std::string filename = entry.path().filename().string();
            if (filename.find(std::filesystem::path(backup_pattern).filename().string()) == 0) {
                backup_files.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors during backup enumeration
    }
    
    // Sort by modification time (newest first)
    std::sort(backup_files.begin(), backup_files.end(), 
        [](const std::string& a, const std::string& b) {
            try {
                return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
            } catch (...) {
                return false;
            }
        });
    
    return backup_files;
}

void UtauLogger::initialize_console_output() {
    #ifdef _WIN32
    setup_windows_console();
    #endif
}

void UtauLogger::setup_windows_console() {
    #ifdef _WIN32
    if (format_.utf8_console) {
        // Set console to UTF-8 mode
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        
        // Enable virtual terminal processing for ANSI colors
        HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (console_handle != INVALID_HANDLE_VALUE) {
            DWORD console_mode = 0;
            GetConsoleMode(console_handle, &console_mode);
            console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(console_handle, console_mode);
        }
        
        console_handle = GetStdHandle(STD_ERROR_HANDLE);
        if (console_handle != INVALID_HANDLE_VALUE) {
            DWORD console_mode = 0;
            GetConsoleMode(console_handle, &console_mode);
            console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(console_handle, console_mode);
        }
    }
    #endif
}

// PerformanceTimer implementation
UtauLogger::PerformanceTimer::PerformanceTimer(UtauLogger& logger, LogLevel level, const std::string& operation)
    : logger_(logger), level_(level), operation_(operation),
      start_time_(std::chrono::high_resolution_clock::now()) {
    logger_.log(level_, operation_ + " started");
}

UtauLogger::PerformanceTimer::~PerformanceTimer() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
    double ms = duration.count() / 1000.0;
    
    logger_.log(level_, operation_ + " completed in " + std::to_string(ms) + "ms");
}

// ScopedLevel implementation
UtauLogger::ScopedLevel::ScopedLevel(UtauLogger& logger, LogLevel new_level) 
    : logger_(logger), original_level_(logger.min_level_) {
    logger_.set_level(new_level);
}

UtauLogger::ScopedLevel::~ScopedLevel() {
    logger_.set_level(original_level_);
}

// Utility functions
namespace LoggingUtils {

bool initialize_utau_logging(const std::string& log_file_path, bool debug_mode) {
    auto& logger = UtauLogger::instance();
    
    // Set appropriate log level
    logger.set_level(debug_mode ? LogLevel::DEBUG : LogLevel::INFO);
    
    // Configure output destination
    if (log_file_path.empty()) {
        logger.set_output(LogOutput::CONSOLE);
    } else {
        logger.set_log_file(log_file_path);
        logger.set_output(LogOutput::BOTH);
    }
    
    // Configure UTAU-appropriate formatting
    LogFormat format;
    format.include_timestamp = true;
    format.include_level = true;
    format.include_thread_id = false;
    format.use_colors = true;
    format.enable_windows_colors = true;
    format.utf8_console = true;
    logger.set_format(format);
    
    // Configure log rotation for file output
    if (!log_file_path.empty()) {
        LogRotation rotation;
        rotation.enabled = true;
        rotation.max_file_size = 10 * 1024 * 1024; // 10MB
        rotation.max_backup_files = 3;
        logger.set_rotation(rotation);
    }
    
    logger.info("UTAU logging system initialized");
    return true;
}

void configure_for_mode(const std::string& mode) {
    auto& logger = UtauLogger::instance();
    
    if (mode == "resampler") {
        // Standard resampler operation - minimal logging
        logger.set_level(LogLevel::WARN);
    } else if (mode == "converter") {
        // Voice bank conversion - more detailed logging
        logger.set_level(LogLevel::INFO);
    } else if (mode == "test") {
        // Testing mode - maximum logging
        logger.set_level(LogLevel::DEBUG);
    }
    
    logger.info("Logging configured for mode: " + mode);
}

std::string get_default_log_path(const std::string& base_name) {
    std::filesystem::path log_dir;
    
    #ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        log_dir = std::filesystem::path(appdata) / "NexusSynth" / "logs";
    } else {
        log_dir = std::filesystem::current_path() / "logs";
    }
    #else
    const char* home = std::getenv("HOME");
    if (home) {
        log_dir = std::filesystem::path(home) / ".local" / "share" / "NexusSynth" / "logs";
    } else {
        log_dir = std::filesystem::current_path() / "logs";
    }
    #endif
    
    // Create directory if it doesn't exist
    std::filesystem::create_directories(log_dir);
    
    // Generate timestamped filename
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << base_name << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".log";
    
    return (log_dir / oss.str()).string();
}

std::unique_ptr<UtauLogger> create_scoped_logger(const std::string& operation_name, LogLevel level) {
    auto logger = std::make_unique<UtauLogger>(operation_name);
    logger->set_level(level);
    logger->set_output(LogOutput::CONSOLE);
    
    LogFormat format;
    format.include_timestamp = true;
    format.include_level = true;
    format.use_colors = true;
    logger->set_format(format);
    
    return logger;
}

bool validate_log_config(const LogFormat& format, const LogRotation& rotation) {
    // Validate timestamp format
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream test_stream;
        test_stream << std::put_time(std::localtime(&time_t), format.timestamp_format.c_str());
        if (test_stream.str().empty()) {
            return false;
        }
    } catch (...) {
        return false;
    }
    
    // Validate rotation settings
    if (rotation.enabled) {
        if (rotation.max_file_size == 0 || rotation.max_backup_files == 0) {
            return false;
        }
    }
    
    return true;
}

} // namespace LoggingUtils

} // namespace utau
} // namespace nexussynth
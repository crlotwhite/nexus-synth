#pragma once

#include <memory>
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <vector>
#include <map>

namespace nexussynth {
namespace utau {

/**
 * @brief Log levels following standard conventions
 * 
 * Ordered by severity from lowest (DEBUG) to highest (FATAL)
 */
enum class LogLevel {
    DEBUG = 0,    ///< Detailed debug information
    INFO = 1,     ///< General information messages  
    WARN = 2,     ///< Warning messages
    ERROR = 3,    ///< Error messages
    FATAL = 4     ///< Critical errors that cause termination
};

/**
 * @brief Log output destinations
 */
enum class LogOutput {
    NONE = 0,        ///< No output
    CONSOLE = 1,     ///< Console/stdout output
    FILE = 2,        ///< File output
    BOTH = 3         ///< Both console and file output
};

/**
 * @brief Log format configuration
 */
struct LogFormat {
    bool include_timestamp = true;     ///< Include timestamp in log messages
    bool include_level = true;         ///< Include log level in messages
    bool include_thread_id = false;    ///< Include thread ID in messages
    bool use_colors = true;           ///< Use ANSI color codes for console output
    std::string timestamp_format = "%Y-%m-%d %H:%M:%S"; ///< Timestamp format string
    
    // Windows-specific settings
    bool enable_windows_colors = true; ///< Enable Windows console colors
    bool utf8_console = true;         ///< Use UTF-8 console output on Windows
};

/**
 * @brief Log rotation configuration
 */
struct LogRotation {
    bool enabled = false;             ///< Enable log rotation
    size_t max_file_size = 10 * 1024 * 1024; ///< 10MB default max file size
    size_t max_backup_files = 5;      ///< Maximum number of backup files
    std::string backup_suffix = ".backup"; ///< Suffix for backup files
};

/**
 * @brief UTAU-compatible logger with Windows console support
 * 
 * Provides structured logging with multiple output destinations,
 * log levels, file rotation, and Windows console color support.
 * Designed to integrate seamlessly with UTAU resampler interface requirements.
 */
class UtauLogger {
public:
    // Singleton access for global logging
    static UtauLogger& instance();
    
    // Constructor for custom instances
    explicit UtauLogger(const std::string& name = "NexusSynth");
    ~UtauLogger();
    
    // Configuration
    void set_level(LogLevel level) { min_level_ = level; }
    void set_output(LogOutput output) { output_dest_ = output; }
    void set_log_file(const std::string& file_path);
    void set_format(const LogFormat& format) { format_ = format; }
    void set_rotation(const LogRotation& rotation) { rotation_ = rotation; }
    
    // Main logging interface
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    // Formatted logging with printf-style arguments
    template<typename... Args>
    void debug_f(const std::string& format, Args... args) {
        if (should_log(LogLevel::DEBUG)) {
            log(LogLevel::DEBUG, format_string(format, args...));
        }
    }
    
    template<typename... Args>
    void info_f(const std::string& format, Args... args) {
        if (should_log(LogLevel::INFO)) {
            log(LogLevel::INFO, format_string(format, args...));
        }
    }
    
    template<typename... Args>
    void warn_f(const std::string& format, Args... args) {
        if (should_log(LogLevel::WARN)) {
            log(LogLevel::WARN, format_string(format, args...));
        }
    }
    
    template<typename... Args>
    void error_f(const std::string& format, Args... args) {
        if (should_log(LogLevel::ERROR)) {
            log(LogLevel::ERROR, format_string(format, args...));
        }
    }
    
    template<typename... Args>
    void fatal_f(const std::string& format, Args... args) {
        if (should_log(LogLevel::FATAL)) {
            log(LogLevel::FATAL, format_string(format, args...));
        }
    }
    
    // Generic log method
    void log(LogLevel level, const std::string& message);
    
    // Conditional logging
    void log_if(bool condition, LogLevel level, const std::string& message) {
        if (condition) log(level, message);
    }
    
    // Context-aware logging for UTAU operations
    void log_resampler_start(const std::string& input_file, const std::string& output_file);
    void log_resampler_end(bool success, double processing_time_ms);
    void log_flag_conversion(const std::string& flags, const std::string& result);
    void log_file_operation(const std::string& operation, const std::string& file_path, bool success);
    void log_parameter_validation(const std::string& parameter, const std::string& value, bool valid);
    
    // Performance tracking
    struct PerformanceTimer {
        explicit PerformanceTimer(UtauLogger& logger, LogLevel level, const std::string& operation);
        ~PerformanceTimer();
        
    private:
        UtauLogger& logger_;
        LogLevel level_;
        std::string operation_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };
    
    PerformanceTimer create_timer(LogLevel level, const std::string& operation) {
        return PerformanceTimer(*this, level, operation);
    }
    
    // Utility methods
    void flush();
    void close();
    bool is_enabled(LogLevel level) const { return level >= min_level_; }
    
    // Statistics and diagnostics
    struct LogStats {
        size_t debug_count = 0;
        size_t info_count = 0;
        size_t warn_count = 0;
        size_t error_count = 0;
        size_t fatal_count = 0;
        size_t total_bytes_written = 0;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
    };
    
    LogStats get_stats() const { return stats_; }
    void reset_stats();
    
    // Scoped log level changes
    class ScopedLevel {
    public:
        ScopedLevel(UtauLogger& logger, LogLevel new_level);
        ~ScopedLevel();
    private:
        UtauLogger& logger_;
        LogLevel original_level_;
    };
    
    ScopedLevel scoped_level(LogLevel level) {
        return ScopedLevel(*this, level);
    }

private:
    std::string logger_name_;
    LogLevel min_level_ = LogLevel::INFO;
    LogOutput output_dest_ = LogOutput::BOTH;
    LogFormat format_;
    LogRotation rotation_;
    
    std::string log_file_path_;
    std::unique_ptr<std::ofstream> file_stream_;
    mutable std::mutex log_mutex_;
    
    LogStats stats_;
    
    // Internal helpers
    bool should_log(LogLevel level) const { return level >= min_level_; }
    std::string format_message(LogLevel level, const std::string& message);
    std::string get_timestamp();
    std::string get_level_string(LogLevel level);
    std::string get_level_color(LogLevel level);
    
    void write_to_console(const std::string& message, LogLevel level);
    void write_to_file(const std::string& message);
    
    // File rotation
    void check_and_rotate_log();
    void rotate_log_file();
    std::vector<std::string> get_backup_files();
    
    // Platform-specific initialization
    void initialize_console_output();
    void setup_windows_console();
    
    // Template helper for formatted strings
    template<typename... Args>
    std::string format_string(const std::string& format, Args... args) {
        // Simple sprintf-style formatting - could be enhanced with fmt library
        size_t size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1);
    }
    
    // Prevent copying for singleton
    UtauLogger(const UtauLogger&) = delete;
    UtauLogger& operator=(const UtauLogger&) = delete;
};

/**
 * @brief Global logging macros for convenience
 */
#define LOG_DEBUG(msg) nexussynth::utau::UtauLogger::instance().debug(msg)
#define LOG_INFO(msg) nexussynth::utau::UtauLogger::instance().info(msg)
#define LOG_WARN(msg) nexussynth::utau::UtauLogger::instance().warn(msg)
#define LOG_ERROR(msg) nexussynth::utau::UtauLogger::instance().error(msg)
#define LOG_FATAL(msg) nexussynth::utau::UtauLogger::instance().fatal(msg)

#define LOG_DEBUG_F(fmt, ...) nexussynth::utau::UtauLogger::instance().debug_f(fmt, __VA_ARGS__)
#define LOG_INFO_F(fmt, ...) nexussynth::utau::UtauLogger::instance().info_f(fmt, __VA_ARGS__)
#define LOG_WARN_F(fmt, ...) nexussynth::utau::UtauLogger::instance().warn_f(fmt, __VA_ARGS__)
#define LOG_ERROR_F(fmt, ...) nexussynth::utau::UtauLogger::instance().error_f(fmt, __VA_ARGS__)
#define LOG_FATAL_F(fmt, ...) nexussynth::utau::UtauLogger::instance().fatal_f(fmt, __VA_ARGS__)

// Conditional logging macros
#define LOG_IF(condition, level, msg) nexussynth::utau::UtauLogger::instance().log_if(condition, level, msg)

// Performance timing macro
#define LOG_TIMER(level, operation) auto timer = nexussynth::utau::UtauLogger::instance().create_timer(level, operation)

/**
 * @brief Utility functions for UTAU logging
 */
namespace LoggingUtils {
    /**
     * @brief Initialize logging system with UTAU-specific defaults
     * @param log_file_path Path to log file (empty for console-only logging)
     * @param debug_mode Enable debug-level logging
     * @return true if initialization succeeded
     */
    bool initialize_utau_logging(const std::string& log_file_path = "", bool debug_mode = false);
    
    /**
     * @brief Configure logging for different UTAU operation modes
     * @param mode Operation mode (e.g., "resampler", "converter", "test")
     */
    void configure_for_mode(const std::string& mode);
    
    /**
     * @brief Get suggested log file path for UTAU operations
     * @param base_name Base name for log file
     * @return Suggested full path for log file
     */
    std::string get_default_log_path(const std::string& base_name = "nexussynth");
    
    /**
     * @brief Create a scoped logger for specific operations
     * @param operation_name Name of the operation
     * @param level Minimum log level for this operation
     * @return Scoped logger instance
     */
    std::unique_ptr<UtauLogger> create_scoped_logger(const std::string& operation_name, LogLevel level = LogLevel::INFO);
    
    /**
     * @brief Validate log configuration
     * @param config Log configuration to validate
     * @return true if configuration is valid
     */
    bool validate_log_config(const LogFormat& format, const LogRotation& rotation);
}

} // namespace utau
} // namespace nexussynth
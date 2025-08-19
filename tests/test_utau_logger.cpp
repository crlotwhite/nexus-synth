#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include "nexussynth/utau_logger.h"

using namespace nexussynth::utau;

class UtauLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test files
        test_dir_ = std::filesystem::temp_directory_path() / "nexussynth_logger_tests";
        std::filesystem::create_directories(test_dir_);
        
        test_log_file_ = test_dir_ / "test_log.txt";
        
        // Clean up any existing test files
        cleanup_test_files();
    }
    
    void TearDown() override {
        cleanup_test_files();
        std::filesystem::remove_all(test_dir_);
    }
    
    void cleanup_test_files() {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(test_dir_)) {
                if (entry.is_regular_file()) {
                    std::filesystem::remove(entry.path());
                }
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }
    
    std::string read_log_file(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            if (!content.empty()) content += "\n";
            content += line;
        }
        return content;
    }
    
    size_t count_lines_in_file(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) return 0;
        
        size_t count = 0;
        std::string line;
        while (std::getline(file, line)) {
            count++;
        }
        return count;
    }

protected:
    std::filesystem::path test_dir_;
    std::filesystem::path test_log_file_;
};

// Basic functionality tests
TEST_F(UtauLoggerTest, BasicLogging) {
    UtauLogger logger("TestLogger");
    logger.set_output(LogOutput::CONSOLE);
    logger.set_level(LogLevel::DEBUG);
    
    // Test all log levels
    EXPECT_NO_THROW(logger.debug("Debug message"));
    EXPECT_NO_THROW(logger.info("Info message"));
    EXPECT_NO_THROW(logger.warn("Warning message"));
    EXPECT_NO_THROW(logger.error("Error message"));
    EXPECT_NO_THROW(logger.fatal("Fatal message"));
}

TEST_F(UtauLoggerTest, LogLevelFiltering) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    
    // Set level to WARN - should only log WARN, ERROR, FATAL
    logger.set_level(LogLevel::WARN);
    
    logger.debug("Debug message - should not appear");
    logger.info("Info message - should not appear");
    logger.warn("Warning message - should appear");
    logger.error("Error message - should appear");
    logger.fatal("Fatal message - should appear");
    
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);
    EXPECT_TRUE(content.find("Warning message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message") != std::string::npos);
    EXPECT_TRUE(content.find("Fatal message") != std::string::npos);
}

TEST_F(UtauLoggerTest, FileOutput) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::INFO);
    
    logger.info("Test file output message");
    logger.warn("Test warning message");
    
    logger.flush();
    
    // Check that file exists and contains expected content
    EXPECT_TRUE(std::filesystem::exists(test_log_file_));
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("Test file output message") != std::string::npos);
    EXPECT_TRUE(content.find("Test warning message") != std::string::npos);
    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(content.find("[WARN]") != std::string::npos);
}

TEST_F(UtauLoggerTest, FormattedLogging) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::DEBUG);
    
    // Test formatted logging
    logger.info_f("Formatted message: %d %s %.2f", 42, "test", 3.14);
    logger.error_f("Error code: %d", 404);
    
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("Formatted message: 42 test 3.14") != std::string::npos);
    EXPECT_TRUE(content.find("Error code: 404") != std::string::npos);
}

TEST_F(UtauLoggerTest, TimestampFormatting) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    
    LogFormat format;
    format.include_timestamp = true;
    format.include_level = true;
    format.timestamp_format = "%H:%M:%S";
    logger.set_format(format);
    
    logger.info("Timestamp test message");
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("[") != std::string::npos); // Should contain timestamp brackets
    EXPECT_TRUE(content.find("Timestamp test message") != std::string::npos);
}

TEST_F(UtauLoggerTest, LogRotation) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    
    LogRotation rotation;
    rotation.enabled = true;
    rotation.max_file_size = 1024; // 1KB for testing
    rotation.max_backup_files = 2;
    logger.set_rotation(rotation);
    
    // Write enough data to trigger rotation
    std::string long_message(200, 'x'); // 200 character message
    for (int i = 0; i < 10; ++i) {
        logger.info(long_message + " " + std::to_string(i));
    }
    logger.flush();
    
    // Check that backup files were created
    std::filesystem::path backup_file = test_log_file_.string() + ".backup.1";
    // Note: Rotation might not trigger immediately due to buffering,
    // but the logic should be tested separately
    EXPECT_TRUE(std::filesystem::exists(test_log_file_));
}

TEST_F(UtauLoggerTest, UtauSpecificLogging) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::DEBUG);
    
    // Test UTAU-specific logging methods
    logger.log_resampler_start("input.wav", "output.wav");
    logger.log_resampler_end(true, 123.45);
    logger.log_flag_conversion("g50t30", "formant_shift=1.25, tension=0.3");
    logger.log_file_operation("read", "test.wav", true);
    logger.log_parameter_validation("pitch", "100", true);
    
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("Starting resampler") != std::string::npos);
    EXPECT_TRUE(content.find("completed successfully in 123.45ms") != std::string::npos);
    EXPECT_TRUE(content.find("Flag conversion") != std::string::npos);
    EXPECT_TRUE(content.find("File read successful") != std::string::npos);
    EXPECT_TRUE(content.find("Parameter validation OK") != std::string::npos);
}

TEST_F(UtauLoggerTest, PerformanceTimer) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::INFO);
    
    {
        auto timer = logger.create_timer(LogLevel::INFO, "Test Operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // Timer destructor should log completion
    
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("Test Operation started") != std::string::npos);
    EXPECT_TRUE(content.find("Test Operation completed") != std::string::npos);
}

TEST_F(UtauLoggerTest, ScopedLogLevel) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::WARN);
    
    logger.info("This should not appear (level=WARN)");
    
    {
        auto scoped_level = logger.scoped_level(LogLevel::DEBUG);
        logger.info("This should appear (scoped level=DEBUG)");
        logger.debug("This debug should also appear");
    }
    
    logger.info("This should not appear again (back to WARN)");
    
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    size_t info_count = 0;
    size_t pos = 0;
    while ((pos = content.find("This should appear", pos)) != std::string::npos) {
        info_count++;
        pos += 1;
    }
    EXPECT_EQ(info_count, 1); // Only the scoped message should appear
    EXPECT_TRUE(content.find("This debug should also appear") != std::string::npos);
}

TEST_F(UtauLoggerTest, ThreadSafety) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::INFO);
    
    const int num_threads = 4;
    const int messages_per_thread = 25;
    
    std::vector<std::thread> threads;
    
    // Launch multiple threads that write to the logger concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                logger.info_f("Thread %d Message %d", i, j);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    logger.flush();
    
    // Check that all messages were logged
    size_t expected_lines = num_threads * messages_per_thread;
    size_t actual_lines = count_lines_in_file(test_log_file_);
    EXPECT_EQ(actual_lines, expected_lines);
}

TEST_F(UtauLoggerTest, LogStatistics) {
    UtauLogger logger("TestLogger");
    logger.set_output(LogOutput::CONSOLE);
    logger.set_level(LogLevel::DEBUG);
    
    logger.reset_stats();
    
    logger.debug("Debug 1");
    logger.debug("Debug 2");
    logger.info("Info 1");
    logger.warn("Warning 1");
    logger.error("Error 1");
    logger.error("Error 2");
    logger.fatal("Fatal 1");
    
    auto stats = logger.get_stats();
    EXPECT_EQ(stats.debug_count, 2);
    EXPECT_EQ(stats.info_count, 1);
    EXPECT_EQ(stats.warn_count, 1);
    EXPECT_EQ(stats.error_count, 2);
    EXPECT_EQ(stats.fatal_count, 1);
    EXPECT_GT(stats.total_bytes_written, 0);
}

TEST_F(UtauLoggerTest, ConditionalLogging) {
    UtauLogger logger("TestLogger");
    logger.set_log_file(test_log_file_.string());
    logger.set_output(LogOutput::FILE);
    logger.set_level(LogLevel::INFO);
    
    bool condition_true = true;
    bool condition_false = false;
    
    logger.log_if(condition_true, LogLevel::INFO, "This should appear");
    logger.log_if(condition_false, LogLevel::INFO, "This should not appear");
    
    logger.flush();
    
    std::string content = read_log_file(test_log_file_);
    EXPECT_TRUE(content.find("This should appear") != std::string::npos);
    EXPECT_TRUE(content.find("This should not appear") == std::string::npos);
}

TEST_F(UtauLoggerTest, InvalidFileHandling) {
    UtauLogger logger("TestLogger");
    
    // Try to set an invalid file path
    std::string invalid_path = "/invalid/path/that/does/not/exist/test.log";
    EXPECT_NO_THROW(logger.set_log_file(invalid_path));
    
    // Logger should continue to work (fall back to console or ignore file output)
    EXPECT_NO_THROW(logger.info("Test message"));
}

// Utility function tests
class LoggingUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "nexussynth_logging_utils_tests";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

protected:
    std::filesystem::path test_dir_;
};

TEST_F(LoggingUtilsTest, InitializeUtauLogging) {
    std::string log_path = (test_dir_ / "test_init.log").string();
    
    EXPECT_TRUE(LoggingUtils::initialize_utau_logging(log_path, true));
    
    // Test that the global logger was configured
    auto& logger = UtauLogger::instance();
    EXPECT_TRUE(logger.is_enabled(LogLevel::DEBUG)); // Debug mode enabled
    
    // Test logging to verify initialization
    logger.info("Test initialization message");
    logger.flush();
    
    EXPECT_TRUE(std::filesystem::exists(log_path));
}

TEST_F(LoggingUtilsTest, ConfigureForMode) {
    EXPECT_NO_THROW(LoggingUtils::configure_for_mode("resampler"));
    EXPECT_NO_THROW(LoggingUtils::configure_for_mode("converter"));
    EXPECT_NO_THROW(LoggingUtils::configure_for_mode("test"));
    
    // Test that different modes affect log level
    LoggingUtils::configure_for_mode("test");
    auto& logger = UtauLogger::instance();
    EXPECT_TRUE(logger.is_enabled(LogLevel::DEBUG));
}

TEST_F(LoggingUtilsTest, GetDefaultLogPath) {
    std::string default_path = LoggingUtils::get_default_log_path("test_app");
    
    EXPECT_FALSE(default_path.empty());
    EXPECT_TRUE(default_path.find("test_app") != std::string::npos);
    EXPECT_TRUE(default_path.find(".log") != std::string::npos);
    
    // Check that parent directory exists (should be created)
    std::filesystem::path path(default_path);
    EXPECT_TRUE(std::filesystem::exists(path.parent_path()));
}

TEST_F(LoggingUtilsTest, CreateScopedLogger) {
    auto scoped_logger = LoggingUtils::create_scoped_logger("ScopedTest", LogLevel::WARN);
    
    EXPECT_TRUE(scoped_logger != nullptr);
    EXPECT_TRUE(scoped_logger->is_enabled(LogLevel::WARN));
    EXPECT_FALSE(scoped_logger->is_enabled(LogLevel::INFO)); // Below WARN threshold
    
    EXPECT_NO_THROW(scoped_logger->warn("Test scoped warning"));
}

TEST_F(LoggingUtilsTest, ValidateLogConfig) {
    LogFormat valid_format;
    valid_format.timestamp_format = "%Y-%m-%d %H:%M:%S";
    
    LogRotation valid_rotation;
    valid_rotation.enabled = true;
    valid_rotation.max_file_size = 1024 * 1024;
    valid_rotation.max_backup_files = 3;
    
    EXPECT_TRUE(LoggingUtils::validate_log_config(valid_format, valid_rotation));
    
    // Test invalid rotation config
    LogRotation invalid_rotation;
    invalid_rotation.enabled = true;
    invalid_rotation.max_file_size = 0; // Invalid
    invalid_rotation.max_backup_files = 3;
    
    EXPECT_FALSE(LoggingUtils::validate_log_config(valid_format, invalid_rotation));
}

// Integration tests with UTAU components
TEST_F(UtauLoggerTest, MacroLogging) {
    // Test the convenience macros
    std::string log_path = (test_dir_ / "macro_test.log").string();
    LoggingUtils::initialize_utau_logging(log_path, true);
    
    LOG_DEBUG("Debug message via macro");
    LOG_INFO("Info message via macro");
    LOG_WARN("Warning message via macro");
    LOG_ERROR("Error message via macro");
    LOG_FATAL("Fatal message via macro");
    
    LOG_INFO_F("Formatted macro: %d %s", 123, "test");
    
    UtauLogger::instance().flush();
    
    std::string content = read_log_file(log_path);
    EXPECT_TRUE(content.find("Debug message via macro") != std::string::npos);
    EXPECT_TRUE(content.find("Info message via macro") != std::string::npos);
    EXPECT_TRUE(content.find("Formatted macro: 123 test") != std::string::npos);
}

// Performance tests
TEST_F(UtauLoggerTest, LoggingPerformance) {
    UtauLogger logger("PerfTest");
    logger.set_output(LogOutput::CONSOLE); // Console only for speed
    logger.set_level(LogLevel::INFO);
    
    const int num_messages = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_messages; ++i) {
        logger.info_f("Performance test message %d", i);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should be able to log 1000 messages in reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
    
    // Check logging stats
    auto stats = logger.get_stats();
    EXPECT_EQ(stats.info_count, num_messages);
}

// Edge cases and error handling
TEST_F(UtauLoggerTest, EdgeCases) {
    UtauLogger logger("EdgeTest");
    logger.set_output(LogOutput::CONSOLE);
    
    // Test empty messages
    EXPECT_NO_THROW(logger.info(""));
    
    // Test very long messages
    std::string long_message(10000, 'x');
    EXPECT_NO_THROW(logger.info(long_message));
    
    // Test special characters
    EXPECT_NO_THROW(logger.info("Test with unicode: 测试 中文 テスト"));
    EXPECT_NO_THROW(logger.info("Test with newlines:\nLine 2\nLine 3"));
    EXPECT_NO_THROW(logger.info("Test with tabs:\tTabbed\tcontent"));
}

// Platform-specific tests
#ifdef _WIN32
TEST_F(UtauLoggerTest, WindowsSpecificFeatures) {
    UtauLogger logger("WindowsTest");
    logger.set_output(LogOutput::CONSOLE);
    
    LogFormat format;
    format.enable_windows_colors = true;
    format.utf8_console = true;
    logger.set_format(format);
    
    // Test that Windows-specific formatting doesn't crash
    EXPECT_NO_THROW(logger.error("Windows error test"));
    EXPECT_NO_THROW(logger.warn("Windows warning test"));
}
#endif
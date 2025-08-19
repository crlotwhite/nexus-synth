#include <gtest/gtest.h>
#include "nexussynth/utau_error_handler.h"
#include <thread>
#include <chrono>
#include <sstream>

using namespace nexussynth::utau;

class UtauErrorHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset error handler state
        auto& handler = UtauErrorHandler::instance();
        handler.clear_error_history();
        handler.clear_context();
        handler.set_exit_on_fatal(false); // Prevent actual exit during tests
        handler.set_debug_mode(true);
    }
    
    void TearDown() override {
        auto& handler = UtauErrorHandler::instance();
        handler.clear_error_history();
        handler.clear_context();
        handler.set_exit_on_fatal(true); // Restore default behavior
    }
};

// Test basic error code classification
TEST_F(UtauErrorHandlerTest, ErrorCodeClassification) {
    ErrorInfo success_info(UtauErrorCode::SUCCESS);
    EXPECT_EQ(success_info.severity, ErrorSeverity::INFO);
    EXPECT_EQ(success_info.category, ErrorCategory::SYSTEM);
    
    ErrorInfo file_error(UtauErrorCode::FILE_NOT_FOUND);
    EXPECT_EQ(file_error.severity, ErrorSeverity::ERROR);
    EXPECT_EQ(file_error.category, ErrorCategory::SYSTEM);
    
    ErrorInfo audio_error(UtauErrorCode::INVALID_WAV_FORMAT);
    EXPECT_EQ(audio_error.severity, ErrorSeverity::ERROR);
    EXPECT_EQ(audio_error.category, ErrorCategory::AUDIO);
    
    ErrorInfo param_error(UtauErrorCode::INVALID_PARAMETERS);
    EXPECT_EQ(param_error.severity, ErrorSeverity::ERROR);
    EXPECT_EQ(param_error.category, ErrorCategory::PARAMETER);
    
    ErrorInfo fatal_error(UtauErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(fatal_error.severity, ErrorSeverity::FATAL);
    EXPECT_EQ(fatal_error.category, ErrorCategory::SYSTEM);
}

// Test error reporting and history tracking
TEST_F(UtauErrorHandlerTest, ErrorReportingAndHistory) {
    auto& handler = UtauErrorHandler::instance();
    
    // Initially no errors
    EXPECT_EQ(handler.get_error_count(), 0);
    
    // Report some errors
    handler.report_error(UtauErrorCode::FILE_NOT_FOUND, "Test file error");
    handler.report_error(UtauErrorCode::INVALID_PARAMETERS, "Test param error");
    
    // Check error counts
    EXPECT_EQ(handler.get_error_count(), 2);
    EXPECT_EQ(handler.get_error_count(ErrorSeverity::ERROR), 2);
    EXPECT_EQ(handler.get_error_count(ErrorCategory::SYSTEM), 1);
    EXPECT_EQ(handler.get_error_count(ErrorCategory::PARAMETER), 1);
    
    // Check recent errors
    auto recent_errors = handler.get_recent_errors(5);
    EXPECT_EQ(recent_errors.size(), 2);
    EXPECT_EQ(recent_errors[0].code, UtauErrorCode::FILE_NOT_FOUND);
    EXPECT_EQ(recent_errors[1].code, UtauErrorCode::INVALID_PARAMETERS);
}

// Test exit code mapping
TEST_F(UtauErrorHandlerTest, ExitCodeMapping) {
    auto& handler = UtauErrorHandler::instance();
    
    // Test standard UTAU codes (0-7)
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::SUCCESS), 0);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::GENERAL_ERROR), 1);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::FILE_NOT_FOUND), 2);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::INVALID_WAV_FORMAT), 3);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::OUT_OF_MEMORY), 4);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::INVALID_PARAMETERS), 5);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::UNSUPPORTED_SAMPLE_RATE), 6);
    EXPECT_EQ(handler.get_exit_code(UtauErrorCode::PROCESSING_ERROR), 7);
    
    // Test extended codes map to standard range
    EXPECT_LE(handler.get_exit_code(UtauErrorCode::PERMISSION_DENIED), 7);
    EXPECT_LE(handler.get_exit_code(UtauErrorCode::CORRUPTED_INPUT), 7);
    EXPECT_LE(handler.get_exit_code(UtauErrorCode::PARAMETER_OUT_OF_RANGE), 7);
}

// Test recovery mechanism
TEST_F(UtauErrorHandlerTest, RecoveryMechanism) {
    auto& handler = UtauErrorHandler::instance();
    
    // Register a recovery strategy that always succeeds
    bool recovery_called = false;
    handler.register_recovery_strategy(UtauErrorCode::GENERAL_ERROR, 
        [&recovery_called](const ErrorInfo&) { 
            recovery_called = true;
            return true; 
        });
    
    // Test successful recovery
    ErrorInfo error_info(UtauErrorCode::GENERAL_ERROR, "Test recovery");
    bool recovered = handler.attempt_recovery(error_info);
    
    EXPECT_TRUE(recovered);
    EXPECT_TRUE(recovery_called);
    
    // Test failed recovery
    recovery_called = false;
    handler.register_recovery_strategy(UtauErrorCode::FILE_NOT_FOUND, 
        [&recovery_called](const ErrorInfo&) { 
            recovery_called = true;
            return false; 
        });
    
    ErrorInfo file_error(UtauErrorCode::FILE_NOT_FOUND, "Test failed recovery");
    bool file_recovered = handler.attempt_recovery(file_error);
    
    EXPECT_FALSE(file_recovered);
    EXPECT_TRUE(recovery_called);
}

// Test category-based recovery
TEST_F(UtauErrorHandlerTest, CategoryRecovery) {
    auto& handler = UtauErrorHandler::instance();
    
    bool category_recovery_called = false;
    handler.register_category_recovery(ErrorCategory::PARAMETER, 
        [&category_recovery_called](const ErrorInfo&) { 
            category_recovery_called = true;
            return true; 
        });
    
    ErrorInfo param_error(UtauErrorCode::INVALID_PARAMETERS, "Test category recovery");
    bool recovered = handler.attempt_recovery(param_error);
    
    EXPECT_TRUE(recovered);
    EXPECT_TRUE(category_recovery_called);
}

// Test localization
TEST_F(UtauErrorHandlerTest, Localization) {
    auto& handler = UtauErrorHandler::instance();
    
    // Test English messages (default)
    handler.set_language("en");
    std::string en_message = handler.get_localized_message(UtauErrorCode::FILE_NOT_FOUND);
    EXPECT_FALSE(en_message.empty());
    EXPECT_NE(en_message, "Unknown error");
    
    // Test Korean messages
    handler.set_language("ko");
    std::string ko_message = handler.get_localized_message(UtauErrorCode::FILE_NOT_FOUND);
    EXPECT_FALSE(ko_message.empty());
    EXPECT_NE(ko_message, en_message); // Should be different from English
    
    // Test fallback to English for unsupported language
    handler.set_language("unsupported");
    std::string fallback_message = handler.get_localized_message(UtauErrorCode::FILE_NOT_FOUND);
    EXPECT_EQ(fallback_message, en_message); // Should fall back to English
}

// Test context management
TEST_F(UtauErrorHandlerTest, ContextManagement) {
    auto& handler = UtauErrorHandler::instance();
    
    // Set some context
    handler.set_context("input_file", "test.wav");
    handler.set_context("operation", "synthesis");
    
    std::string context_str = handler.get_context_string();
    EXPECT_FALSE(context_str.empty());
    EXPECT_NE(context_str.find("input_file=test.wav"), std::string::npos);
    EXPECT_NE(context_str.find("operation=synthesis"), std::string::npos);
    
    // Clear context
    handler.clear_context();
    context_str = handler.get_context_string();
    EXPECT_TRUE(context_str.empty());
}

// Test error validation
TEST_F(UtauErrorHandlerTest, ErrorValidation) {
    auto& handler = UtauErrorHandler::instance();
    
    // Test error system validation
    bool system_valid = handler.validate_error_system();
    EXPECT_TRUE(system_valid);
    
    // Test recoverable error detection
    EXPECT_TRUE(handler.is_recoverable_error(UtauErrorCode::SUCCESS));
    EXPECT_TRUE(handler.is_recoverable_error(UtauErrorCode::FILE_NOT_FOUND));
    EXPECT_TRUE(handler.is_recoverable_error(UtauErrorCode::INVALID_PARAMETERS));
    EXPECT_FALSE(handler.is_recoverable_error(UtauErrorCode::OUT_OF_MEMORY));
    EXPECT_FALSE(handler.is_recoverable_error(UtauErrorCode::INITIALIZATION_ERROR));
}

// Test thread safety
TEST_F(UtauErrorHandlerTest, ThreadSafety) {
    auto& handler = UtauErrorHandler::instance();
    
    const int num_threads = 4;
    const int errors_per_thread = 25;
    
    std::vector<std::thread> threads;
    
    // Launch multiple threads that report errors concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&handler, i, errors_per_thread]() {
            for (int j = 0; j < errors_per_thread; ++j) {
                handler.report_error(UtauErrorCode::GENERAL_ERROR, 
                    "Thread " + std::to_string(i) + " Error " + std::to_string(j));
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all errors were recorded
    EXPECT_EQ(handler.get_error_count(), num_threads * errors_per_thread);
    
    auto recent_errors = handler.get_recent_errors(num_threads * errors_per_thread);
    EXPECT_EQ(recent_errors.size(), num_threads * errors_per_thread);
}

// Test UtauException class
TEST_F(UtauErrorHandlerTest, UtauException) {
    ErrorInfo error_info(UtauErrorCode::INVALID_PARAMETERS, "Test exception message");
    UtauException exception(error_info);
    
    EXPECT_EQ(exception.get_error_code(), UtauErrorCode::INVALID_PARAMETERS);
    EXPECT_EQ(exception.get_severity(), ErrorSeverity::ERROR);
    EXPECT_STREQ(exception.what(), "Test exception message");
    
    // Test constructor with just code and message
    UtauException simple_exception(UtauErrorCode::FILE_NOT_FOUND, "Simple exception");
    EXPECT_EQ(simple_exception.get_error_code(), UtauErrorCode::FILE_NOT_FOUND);
    EXPECT_STREQ(simple_exception.what(), "Simple exception");
}

// Test exception reporting
TEST_F(UtauErrorHandlerTest, ExceptionReporting) {
    auto& handler = UtauErrorHandler::instance();
    
    try {
        throw std::runtime_error("Test runtime error");
    } catch (const std::exception& e) {
        handler.report_exception(e, "Test context");
    }
    
    EXPECT_EQ(handler.get_error_count(), 1);
    
    auto recent_errors = handler.get_recent_errors(1);
    EXPECT_FALSE(recent_errors.empty());
    EXPECT_NE(recent_errors[0].message.find("Test runtime error"), std::string::npos);
    EXPECT_NE(recent_errors[0].message.find("Test context"), std::string::npos);
}

// Test ErrorUtils functions
TEST_F(UtauErrorHandlerTest, ErrorUtilsFunctions) {
    // Test system error conversion
    UtauErrorCode code = ErrorUtils::from_system_error(ENOENT);
    EXPECT_EQ(code, UtauErrorCode::FILE_NOT_FOUND);
    
    code = ErrorUtils::from_system_error(EACCES);
    EXPECT_EQ(code, UtauErrorCode::PERMISSION_DENIED);
    
    code = ErrorUtils::from_system_error(ENOMEM);
    EXPECT_EQ(code, UtauErrorCode::OUT_OF_MEMORY);
    
    // Test exception conversion
    std::runtime_error memory_error("memory allocation failed");
    code = ErrorUtils::from_exception(memory_error);
    EXPECT_EQ(code, UtauErrorCode::OUT_OF_MEMORY);
    
    std::invalid_argument param_error("invalid argument provided");
    code = ErrorUtils::from_exception(param_error);
    EXPECT_EQ(code, UtauErrorCode::INVALID_PARAMETERS);
    
    // Test error code validation
    EXPECT_TRUE(ErrorUtils::is_valid_error_code(0));
    EXPECT_TRUE(ErrorUtils::is_valid_error_code(7));
    EXPECT_TRUE(ErrorUtils::is_valid_error_code(15));
    EXPECT_FALSE(ErrorUtils::is_valid_error_code(-1));
    EXPECT_FALSE(ErrorUtils::is_valid_error_code(1000));
    
    // Test standard UTAU code detection
    EXPECT_TRUE(ErrorUtils::is_standard_utau_code(UtauErrorCode::SUCCESS));
    EXPECT_TRUE(ErrorUtils::is_standard_utau_code(UtauErrorCode::PROCESSING_ERROR));
    EXPECT_FALSE(ErrorUtils::is_standard_utau_code(UtauErrorCode::PERMISSION_DENIED));
    EXPECT_FALSE(ErrorUtils::is_standard_utau_code(UtauErrorCode::TIMEOUT_ERROR));
}

// Test system information utilities
TEST_F(UtauErrorHandlerTest, SystemInfoUtilities) {
    std::string stack_trace = ErrorUtils::get_current_stack_trace();
    EXPECT_FALSE(stack_trace.empty());
    
    std::string system_info = ErrorUtils::get_system_info();
    EXPECT_FALSE(system_info.empty());
    
    std::string memory_usage = ErrorUtils::get_memory_usage();
    EXPECT_FALSE(memory_usage.empty());
    
    std::string error_message = ErrorUtils::get_system_error_message(ENOENT);
    EXPECT_FALSE(error_message.empty());
}

// Test UTAU compatibility
TEST_F(UtauErrorHandlerTest, UtauCompatibility) {
    auto compatibility_tests = ErrorUtils::run_compatibility_tests();
    EXPECT_FALSE(compatibility_tests.empty());
    
    // All standard error codes should pass compatibility tests
    bool all_passed = true;
    for (const auto& test : compatibility_tests) {
        if (!test.passed) {
            all_passed = false;
            std::cout << "Compatibility test failed: " << test.test_scenario << std::endl;
        }
    }
    EXPECT_TRUE(all_passed);
    
    // Verify exit code compliance
    bool compliance = ErrorUtils::verify_exit_code_compliance();
    EXPECT_TRUE(compliance);
}

// Test error message formatting
TEST_F(UtauErrorHandlerTest, ErrorMessageFormatting) {
    auto& handler = UtauErrorHandler::instance();
    
    // Test technical message format (default)
    handler.set_user_friendly_messages(false);
    handler.set_context("test_key", "test_value");
    handler.report_error(UtauErrorCode::FILE_NOT_FOUND, "Test file missing");
    
    auto errors = handler.get_recent_errors(1);
    EXPECT_FALSE(errors.empty());
    
    // Test user-friendly message format
    handler.set_user_friendly_messages(true);
    handler.clear_error_history();
    handler.report_error(UtauErrorCode::INVALID_PARAMETERS, "User-friendly test");
    
    errors = handler.get_recent_errors(1);
    EXPECT_FALSE(errors.empty());
}

// Test error history management
TEST_F(UtauErrorHandlerTest, ErrorHistoryManagement) {
    auto& handler = UtauErrorHandler::instance();
    
    // Report many errors to test history limits
    for (int i = 0; i < 150; ++i) {
        handler.report_error(UtauErrorCode::GENERAL_ERROR, "Error " + std::to_string(i));
    }
    
    // Should be limited to max history size (100 by default)
    EXPECT_LE(handler.get_error_count(), 100);
    
    // Test selective error retrieval
    auto recent_10 = handler.get_recent_errors(10);
    EXPECT_EQ(recent_10.size(), 10);
    
    // Clear history
    handler.clear_error_history();
    EXPECT_EQ(handler.get_error_count(), 0);
}

// Test configuration options
TEST_F(UtauErrorHandlerTest, ConfigurationOptions) {
    auto& handler = UtauErrorHandler::instance();
    
    // Test debug mode
    handler.set_debug_mode(true);
    handler.set_debug_mode(false);
    
    // Test log all errors
    handler.set_log_all_errors(true);
    handler.set_log_all_errors(false);
    
    // Test user-friendly messages
    handler.set_user_friendly_messages(true);
    handler.set_user_friendly_messages(false);
    
    // Test exit on fatal (already tested in SetUp/TearDown)
    handler.set_exit_on_fatal(false);
    handler.set_exit_on_fatal(true);
    
    // Configuration changes should not cause crashes
    handler.report_error(UtauErrorCode::SUCCESS, "Configuration test");
    EXPECT_EQ(handler.get_error_count(), 1);
}

// Test macro functionality
TEST_F(UtauErrorHandlerTest, ErrorMacros) {
    auto& handler = UtauErrorHandler::instance();
    
    // Test UTAU_TRY_RECOVER macro (should compile and run without crashing)
    UTAU_TRY_RECOVER(UtauErrorCode::GENERAL_ERROR, "Macro test");
    
    // Should have recorded the error
    EXPECT_GT(handler.get_error_count(), 0);
    
    // Test exception throwing macro
    bool exception_caught = false;
    try {
        UTAU_THROW_ERROR(UtauErrorCode::INVALID_PARAMETERS, "Macro exception test");
    } catch (const UtauException& e) {
        exception_caught = true;
        EXPECT_EQ(e.get_error_code(), UtauErrorCode::INVALID_PARAMETERS);
    }
    EXPECT_TRUE(exception_caught);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
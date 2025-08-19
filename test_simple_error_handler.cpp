#include "src/interface/utau_error_handler.cpp"
#include <iostream>

int main() {
    using namespace nexussynth::utau;
    
    std::cout << "Testing UTAU Error Handler..." << std::endl;
    
    // Test basic error handler functionality
    auto& handler = UtauErrorHandler::instance();
    handler.set_exit_on_fatal(false); // Prevent actual exit
    
    // Test error reporting
    std::cout << "1. Testing error reporting..." << std::endl;
    handler.report_error(UtauErrorCode::SUCCESS, "Test success message");
    
    // Test error counts
    std::cout << "2. Testing error tracking..." << std::endl;
    handler.report_error(UtauErrorCode::INVALID_PARAMETERS, "Test parameter error");
    handler.report_error(UtauErrorCode::FILE_NOT_FOUND, "Test file error");
    
    size_t error_count = handler.get_error_count();
    std::cout << "   Error count: " << error_count << std::endl;
    
    // Test exit code mapping
    std::cout << "3. Testing exit code mapping..." << std::endl;
    int exit_code = handler.get_exit_code(UtauErrorCode::INVALID_PARAMETERS);
    std::cout << "   INVALID_PARAMETERS exit code: " << exit_code << std::endl;
    
    // Test localized messages
    std::cout << "4. Testing localized messages..." << std::endl;
    handler.set_language("en");
    std::string en_msg = handler.get_localized_message(UtauErrorCode::FILE_NOT_FOUND);
    std::cout << "   English message: " << en_msg << std::endl;
    
    handler.set_language("ko");
    std::string ko_msg = handler.get_localized_message(UtauErrorCode::FILE_NOT_FOUND);
    std::cout << "   Korean message: " << ko_msg << std::endl;
    
    // Test context
    std::cout << "5. Testing context management..." << std::endl;
    handler.set_context("test_key", "test_value");
    std::string context = handler.get_context_string();
    std::cout << "   Context: " << context << std::endl;
    
    // Test error validation
    std::cout << "6. Testing error system validation..." << std::endl;
    bool valid = handler.validate_error_system();
    std::cout << "   System valid: " << (valid ? "YES" : "NO") << std::endl;
    
    // Test UTAU compatibility
    std::cout << "7. Testing UTAU compatibility..." << std::endl;
    auto compatibility_tests = ErrorUtils::run_compatibility_tests();
    bool all_passed = true;
    for (const auto& test : compatibility_tests) {
        if (!test.passed) {
            all_passed = false;
            std::cout << "   FAILED: " << test.test_scenario << std::endl;
        }
    }
    std::cout << "   Compatibility: " << (all_passed ? "PASS" : "FAIL") << std::endl;
    
    std::cout << "All tests completed!" << std::endl;
    return 0;
}
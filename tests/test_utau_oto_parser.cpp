#include "nexussynth/utau_oto_parser.h"
#include <iostream>
#include <vector>
#include <string>
#include <limits>

using namespace nexussynth::utau;

int main() {
    std::cout << "Testing UTAU oto.ini parser functionality...\n";
    
    // Test encoding detection
    std::cout << "\n=== Testing Encoding Detection ===\n";
    try {
        // Test ASCII detection
        std::vector<uint8_t> ascii_data = {0x68, 0x65, 0x6C, 0x6C, 0x6F}; // "hello"
        auto encoding = EncodingDetector::detect_encoding(ascii_data);
        std::cout << "ASCII test: " << EncodingDetector::encoding_to_string(encoding) << "\n";
        
        // Test UTF-8 BOM detection
        std::vector<uint8_t> utf8_bom_data = {0xEF, 0xBB, 0xBF, 0x68, 0x65, 0x6C, 0x6C, 0x6F};
        encoding = EncodingDetector::detect_encoding(utf8_bom_data);
        std::cout << "UTF-8 BOM test: " << EncodingDetector::encoding_to_string(encoding) << "\n";
        
        // Test Shift-JIS detection (simulated)
        std::vector<uint8_t> sjis_data = {0x82, 0xA0, 0x82, 0xA2}; // Common Shift-JIS pattern
        encoding = EncodingDetector::detect_encoding(sjis_data);
        std::cout << "Shift-JIS test: " << EncodingDetector::encoding_to_string(encoding) << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "Encoding detection test failed: " << e.what() << "\n";
    }
    
    // Test OtoEntry validation
    std::cout << "\n=== Testing OtoEntry Validation ===\n";
    try {
        OtoEntry valid_entry("test.wav", "a", 100.0, 50.0, 20.0, 80.0, 10.0);
        std::cout << "Valid entry test: " << (valid_entry.is_valid() ? "PASSED" : "FAILED") << "\n";
        std::cout << "Entry string: " << valid_entry.to_string() << "\n";
        
        OtoEntry invalid_entry("", "", std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0, 0.0);
        std::cout << "Invalid entry test: " << (invalid_entry.is_valid() ? "FAILED" : "PASSED") << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "OtoEntry validation test failed: " << e.what() << "\n";
    }
    
    // Test oto.ini parsing with sample data
    std::cout << "\n=== Testing Oto.ini Parsing ===\n";
    try {
        std::string sample_oto = 
            "# Sample oto.ini file\n"
            "a.wav=a,100.0,50.0,20.0,80.0,10.0\n"
            "ka.wav=ka,150.0,60.0,25.0,90.0,15.0\n"
            "sa.wav=sa,200.0,70.0,30.0,100.0,20.0\n"
            "ta.wav=ta,250.0,80.0,35.0,110.0,25.0\n"
            "# VCV entries\n"
            "aka.wav=a ka,300.0,40.0,40.0,120.0,30.0\n"
            "invalid_line_missing_equals\n"
            "empty.wav=,0,0,0,0,0\n";
        
        OtoIniParser parser;
        auto result = parser.parse_string(sample_oto, "test_oto.ini");
        
        std::cout << "Parse success: " << (result.success ? "YES" : "NO") << "\n";
        std::cout << "Entries parsed: " << result.entries.size() << "\n";
        std::cout << "Errors: " << result.errors.size() << "\n";
        std::cout << "Warnings: " << result.warnings.size() << "\n";
        
        // Print errors if any
        for (const auto& error : result.errors) {
            std::cout << "  Error: " << error << "\n";
        }
        
        // Print parsed entries
        std::cout << "\nParsed entries:\n";
        for (const auto& entry : result.entries) {
            std::cout << "  " << entry.filename << " -> " << entry.alias 
                     << " [offset=" << entry.offset 
                     << ", consonant=" << entry.consonant 
                     << ", preutterance=" << entry.preutterance 
                     << ", overlap=" << entry.overlap << "]\n";
        }
        
        // Test voicebank analysis
        std::cout << "\nVoicebank analysis:\n";
        const auto& info = result.voicebank_info;
        std::cout << "  Total entries: " << info.total_entries << "\n";
        std::cout << "  Unique phonemes: " << info.phonemes.size() << "\n";
        std::cout << "  Audio files: " << info.filenames.size() << "\n";
        std::cout << "  Entries with timing: " << info.entries_with_timing << "\n";
        std::cout << "  Duplicate aliases: " << info.duplicate_aliases << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "Oto.ini parsing test failed: " << e.what() << "\n";
    }
    
    // Test parser options
    std::cout << "\n=== Testing Parser Options ===\n";
    try {
        OtoIniParser::ParseOptions strict_options;
        strict_options.strict_validation = true;
        strict_options.skip_invalid_entries = false;
        strict_options.default_preutterance = 100.0;
        strict_options.default_overlap = 20.0;
        
        OtoIniParser strict_parser(strict_options);
        
        std::string test_data = 
            "test.wav=alias,,,,\n"  // Missing values should use defaults
            "invalid.wav=bad_alias,not_a_number,0,0,0,0\n";  // Should fail strict validation
        
        auto result = strict_parser.parse_string(test_data);
        std::cout << "Strict parser success: " << (result.success ? "YES" : "NO") << "\n";
        std::cout << "Entries with defaults: " << result.entries.size() << "\n";
        
        if (!result.entries.empty()) {
            const auto& entry = result.entries[0];
            std::cout << "  Default preutterance applied: " << entry.preutterance << "\n";
            std::cout << "  Default overlap applied: " << entry.overlap << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cout << "Parser options test failed: " << e.what() << "\n";
    }
    
    // Test utility functions
    std::cout << "\n=== Testing Utility Functions ===\n";
    try {
        std::vector<OtoEntry> test_entries = {
            {"a.wav", "a", 100.0, 50.0, 20.0, 80.0, 10.0},
            {"ka.wav", "ka", 150.0, 60.0, 25.0, 90.0, 15.0},
            {"a.wav", "a", 200.0, 40.0, 15.0, 70.0, 5.0},  // Duplicate alias
        };
        
        auto unique_phonemes = utils::extract_unique_phonemes(test_entries);
        std::cout << "Unique phonemes: ";
        for (const auto& phoneme : unique_phonemes) {
            std::cout << phoneme << " ";
        }
        std::cout << "\n";
        
        auto duplicates = utils::find_duplicate_aliases(test_entries);
        std::cout << "Duplicate aliases found: " << duplicates.size() << "\n";
        for (const auto& dup : duplicates) {
            std::cout << "  " << dup << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cout << "Utility functions test failed: " << e.what() << "\n";
    }
    
    // Test edge cases
    std::cout << "\n=== Testing Edge Cases ===\n";
    try {
        // Test empty input
        OtoIniParser parser;
        auto empty_result = parser.parse_string("");
        std::cout << "Empty input test: " << (empty_result.success ? "PASSED" : "FAILED") << "\n";
        
        // Test whitespace handling
        std::string whitespace_data = 
            "  file.wav  =  alias  ,  100  ,  50  ,  20  ,  80  ,  10  \n"
            "\t\tfile2.wav\t=\talias2\t,\t200\t,\t60\t,\t30\t,\t90\t,\t20\t\n";
        
        auto ws_result = parser.parse_string(whitespace_data);
        std::cout << "Whitespace handling test: " << (ws_result.success ? "PASSED" : "FAILED") << "\n";
        std::cout << "Entries parsed from whitespace data: " << ws_result.entries.size() << "\n";
        
        // Test very large values
        std::string large_values_data = "huge.wav=big,999999.999,999999.999,999999.999,999999.999,999999.999\n";
        auto large_result = parser.parse_string(large_values_data);
        std::cout << "Large values test: " << (large_result.success ? "PASSED" : "FAILED") << "\n";
        
    } catch (const std::exception& e) {
        std::cout << "Edge cases test failed: " << e.what() << "\n";
    }
    
    std::cout << "\nAll UTAU oto.ini parser tests completed!\n";
    return 0;
}
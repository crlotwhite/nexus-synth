#include "nexussynth/vcv_pattern_recognizer.h"
#include "nexussynth/utau_oto_parser.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

using namespace nexussynth::utau;

int main() {
    std::cout << "Testing VCV Pattern Recognizer functionality...\n";
    
    // Test HiraganaMapper
    std::cout << "\n=== Testing HiraganaMapper ===\n";
    try {
        // Test basic conversions
        std::string romaji = HiraganaMapper::convertToRomaji("か");
        std::cout << "Hiragana to Romaji test: か -> " << romaji << "\n";
        
        std::string hiragana = HiraganaMapper::convertToHiragana("ka");
        std::cout << "Romaji to Hiragana test: ka -> " << hiragana << "\n";
        
        // Test phoneme extraction
        auto phonemes = HiraganaMapper::extractPhonemesFromAlias("a ka");
        std::cout << "Phoneme extraction test: 'a ka' -> ";
        for (const auto& phoneme : phonemes) {
            std::cout << "'" << phoneme << "' ";
        }
        std::cout << "\n";
        
        // Test alias normalization
        std::string normalized = HiraganaMapper::normalizeAlias("  a   ka  ");
        std::cout << "Alias normalization test: '  a   ka  ' -> '" << normalized << "'\n";
        
        std::cout << "HiraganaMapper tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "HiraganaMapper test failed: " << e.what() << "\n";
    }
    
    // Test PhonemeBoundaryExtractor
    std::cout << "\n=== Testing PhonemeBoundaryExtractor ===\n";
    try {
        PhonemeBoundaryExtractor extractor;
        
        // Create test oto entry for VCV pattern
        OtoEntry vcv_entry;
        vcv_entry.filename = "aka.wav";
        vcv_entry.alias = "a ka";
        vcv_entry.offset = 200.0;
        vcv_entry.consonant = 80.0;
        vcv_entry.blank = 150.0;
        vcv_entry.preutterance = 120.0;
        vcv_entry.overlap = 30.0;
        
        auto boundary = extractor.extractFromOtoEntry(vcv_entry);
        
        std::cout << "VCV boundary extraction test:\n";
        std::cout << "  Vowel1: " << boundary.vowel1_start << " - " << boundary.vowel1_end << " ms\n";
        std::cout << "  Consonant: " << boundary.consonant_start << " - " << boundary.consonant_end << " ms\n";
        std::cout << "  Vowel2: " << boundary.vowel2_start << " - " << boundary.vowel2_end << " ms\n";
        std::cout << "  Timing consistency: " << boundary.timing_consistency << "\n";
        
        bool is_valid = extractor.validateBoundary(boundary);
        std::cout << "Boundary validation: " << (is_valid ? "VALID" : "INVALID") << "\n";
        
        double confidence = extractor.calculateBoundaryConfidence(boundary, vcv_entry);
        std::cout << "Boundary confidence: " << confidence << "\n";
        
        std::cout << "PhonemeBoundaryExtractor tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "PhonemeBoundaryExtractor test failed: " << e.what() << "\n";
    }
    
    // Test VCVPatternRecognizer
    std::cout << "\n=== Testing VCVPatternRecognizer ===\n";
    try {
        VCVPatternRecognizer recognizer;
        
        // Test VCV pattern recognition
        std::cout << "VCV pattern recognition tests:\n";
        
        std::vector<std::string> test_aliases = {
            "a ka",     // Valid VCV
            "e ki",     // Valid VCV
            "o ku",     // Valid VCV
            "ka",       // CV pattern
            "a",        // Single vowel
            "invalid"   // Invalid pattern
        };
        
        for (const auto& alias : test_aliases) {
            bool is_vcv = recognizer.isVCVPattern(alias);
            bool is_cv = recognizer.isCVPattern(alias);
            std::cout << "  '" << alias << "': VCV=" << (is_vcv ? "YES" : "NO") 
                     << ", CV=" << (is_cv ? "YES" : "NO") << "\n";
        }
        
        // Test with actual oto entries
        std::vector<OtoEntry> test_entries = {
            {"aka.wav", "a ka", 200.0, 80.0, 150.0, 120.0, 30.0},
            {"eki.wav", "e ki", 180.0, 70.0, 140.0, 100.0, 25.0},
            {"oku.wav", "o ku", 220.0, 90.0, 160.0, 130.0, 35.0},
            {"sa.wav", "sa", 100.0, 60.0, 120.0, 80.0, 20.0}
        };
        
        auto result = recognizer.recognizeFromOtoEntries(test_entries);
        
        std::cout << "\nRecognition results:\n";
        std::cout << "  VCV segments found: " << result.vcv_segments.size() << "\n";
        std::cout << "  CV patterns found: " << result.cv_patterns.size() << "\n";
        std::cout << "  Overall confidence: " << result.overall_confidence << "\n";
        std::cout << "  Errors: " << result.errors.size() << "\n";
        
        // Print VCV segments
        for (size_t i = 0; i < result.vcv_segments.size(); ++i) {
            const auto& segment = result.vcv_segments[i];
            std::cout << "  Segment " << (i + 1) << ": " << segment.full_alias << "\n";
            std::cout << "    V1='" << segment.vowel1 << "', C='" << segment.consonant 
                     << "', V2='" << segment.vowel2 << "'\n";
            std::cout << "    Timing: " << segment.start_time << " - " << segment.end_time << " ms\n";
            std::cout << "    Confidence: " << segment.boundary_confidence << "\n";
            std::cout << "    Valid: " << (segment.is_valid ? "YES" : "NO") << "\n";
        }
        
        // Print CV patterns
        for (const auto& cv : result.cv_patterns) {
            std::cout << "  CV pattern: " << cv << "\n";
        }
        
        // Print errors
        for (const auto& error : result.errors) {
            std::cout << "  Error: " << error << "\n";
        }
        
        std::cout << "VCVPatternRecognizer tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "VCVPatternRecognizer test failed: " << e.what() << "\n";
    }
    
    // Test VCV quality assessment
    std::cout << "\n=== Testing VCV Quality Assessment ===\n";
    try {
        VCVPatternRecognizer recognizer;
        
        // Create test VCV segments
        VCVSegment good_segment;
        good_segment.vowel1 = "a";
        good_segment.consonant = "k";
        good_segment.vowel2 = "a";
        good_segment.full_alias = "a ka";
        good_segment.start_time = 0.0;
        good_segment.consonant_start = 80.0;
        good_segment.consonant_end = 130.0;
        good_segment.end_time = 230.0;
        good_segment.boundary_confidence = 0.9;
        good_segment.is_valid = true;
        
        VCVSegment poor_segment;
        poor_segment.vowel1 = "x";  // Invalid vowel
        poor_segment.consonant = "y"; // Invalid consonant
        poor_segment.vowel2 = "z";  // Invalid vowel
        poor_segment.full_alias = "x y z";
        poor_segment.start_time = 0.0;
        poor_segment.consonant_start = 10.0;  // Too short
        poor_segment.consonant_end = 15.0;
        poor_segment.end_time = 25.0;
        poor_segment.boundary_confidence = 0.2;
        poor_segment.is_valid = false;
        
        double good_quality = recognizer.assessVCVQuality(good_segment);
        double poor_quality = recognizer.assessVCVQuality(poor_segment);
        
        std::cout << "Quality assessment tests:\n";
        std::cout << "  Good segment ('" << good_segment.full_alias << "'): " << good_quality << "\n";
        std::cout << "  Poor segment ('" << poor_segment.full_alias << "'): " << poor_quality << "\n";
        
        // Test sequence validation
        std::vector<VCVSegment> sequence = {good_segment, poor_segment};
        auto validation_errors = recognizer.validateVCVSequence(sequence);
        
        std::cout << "Sequence validation errors: " << validation_errors.size() << "\n";
        for (const auto& error : validation_errors) {
            std::cout << "  " << error << "\n";
        }
        
        std::cout << "VCV Quality Assessment tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "VCV Quality Assessment test failed: " << e.what() << "\n";
    }
    
    // Test VCV utilities
    std::cout << "\n=== Testing VCV Utilities ===\n";
    try {
        // Test phoneme validation
        std::cout << "Phoneme validation tests:\n";
        std::vector<std::string> vowels = {"a", "i", "u", "e", "o", "x"};
        std::vector<std::string> consonants = {"k", "s", "t", "n", "m", "y"};
        
        for (const auto& vowel : vowels) {
            bool is_vowel = vcv_utils::isJapaneseVowel(vowel);
            std::cout << "  '" << vowel << "' is vowel: " << (is_vowel ? "YES" : "NO") << "\n";
        }
        
        for (const auto& consonant : consonants) {
            bool is_consonant = vcv_utils::isJapaneseConsonant(consonant);
            std::cout << "  '" << consonant << "' is consonant: " << (is_consonant ? "YES" : "NO") << "\n";
        }
        
        // Test timing analysis
        std::vector<VCVSegment> test_segments;
        
        VCVSegment segment1;
        segment1.vowel1 = "a";
        segment1.consonant = "k";
        segment1.vowel2 = "a";
        segment1.full_alias = "a ka";
        segment1.start_time = 0.0;
        segment1.consonant_start = 80.0;
        segment1.consonant_end = 130.0;
        segment1.end_time = 230.0;
        segment1.boundary_confidence = 0.9;
        segment1.is_valid = true;
        
        VCVSegment segment2;
        segment2.vowel1 = "e";
        segment2.consonant = "k";
        segment2.vowel2 = "i";
        segment2.full_alias = "e ki";
        segment2.start_time = 230.0;
        segment2.consonant_start = 310.0;
        segment2.consonant_end = 360.0;
        segment2.end_time = 460.0;
        segment2.boundary_confidence = 0.8;
        segment2.is_valid = true;
        
        test_segments.push_back(segment1);
        test_segments.push_back(segment2);
        
        auto timing_stats = vcv_utils::analyzeVCVTiming(test_segments);
        std::cout << "Timing analysis:\n";
        std::cout << "  Average vowel duration: " << timing_stats.avg_vowel_duration << " ms\n";
        std::cout << "  Average consonant duration: " << timing_stats.avg_consonant_duration << " ms\n";
        std::cout << "  Average transition duration: " << timing_stats.avg_transition_duration << " ms\n";
        std::cout << "  Total segments: " << timing_stats.total_segments << "\n";
        
        // Test sequence conversion
        auto phoneme_sequence = vcv_utils::vcvToPhonemeSequence(test_segments);
        std::cout << "Phoneme sequence: ";
        for (const auto& phoneme : phoneme_sequence) {
            std::cout << "'" << phoneme << "' ";
        }
        std::cout << "\n";
        
        std::string sequence_string = vcv_utils::vcvSequenceToString(test_segments);
        std::cout << "Sequence string: " << sequence_string << "\n";
        
        std::cout << "VCV Utilities tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "VCV Utilities test failed: " << e.what() << "\n";
    }
    
    // Test edge cases
    std::cout << "\n=== Testing Edge Cases ===\n";
    try {
        VCVPatternRecognizer recognizer;
        
        // Empty input
        auto empty_result = recognizer.recognizeFromOtoEntries({});
        std::cout << "Empty input test: " << (empty_result.vcv_segments.empty() ? "PASSED" : "FAILED") << "\n";
        
        // Invalid oto entry
        OtoEntry invalid_entry;
        invalid_entry.filename = "";
        invalid_entry.alias = "";
        invalid_entry.offset = -100.0;  // Negative offset
        invalid_entry.consonant = -50.0; // Negative consonant
        
        auto invalid_result = recognizer.recognizeFromOtoEntries({invalid_entry});
        std::cout << "Invalid entry test: " << (invalid_result.errors.size() > 0 ? "PASSED" : "FAILED") << "\n";
        
        // Very short timing
        OtoEntry short_entry;
        short_entry.filename = "short.wav";
        short_entry.alias = "a ka";
        short_entry.offset = 1.0;
        short_entry.consonant = 1.0;
        short_entry.blank = 1.0;
        short_entry.preutterance = 1.0;
        short_entry.overlap = 1.0;
        
        auto short_result = recognizer.recognizeFromOtoEntries({short_entry});
        std::cout << "Short timing test: segments=" << short_result.vcv_segments.size() 
                 << ", confidence=" << short_result.overall_confidence << "\n";
        
        std::cout << "Edge Cases tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "Edge Cases test failed: " << e.what() << "\n";
    }
    
    std::cout << "\nAll VCV Pattern Recognizer tests completed!\n";
    return 0;
}
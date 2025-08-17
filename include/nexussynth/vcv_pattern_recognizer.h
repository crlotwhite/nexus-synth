#pragma once

#include "utau_oto_parser.h"
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <optional>

namespace nexussynth {
namespace utau {

    /**
     * @brief VCV (Vowel-Consonant-Vowel) segment structure
     * 
     * Represents a Japanese VCV phoneme pattern with timing and boundary information
     */
    struct VCVSegment {
        std::string vowel1;          // First vowel (e.g., "a" in "aka")
        std::string consonant;       // Consonant (e.g., "k" in "aka")
        std::string vowel2;          // Second vowel (e.g., "a" in "aka")
        std::string full_alias;      // Complete alias (e.g., "a ka")
        
        // Timing information from oto.ini
        double start_time;           // VCV segment start (ms)
        double consonant_start;      // Consonant boundary start (ms)
        double consonant_end;        // Consonant boundary end (ms)
        double end_time;             // VCV segment end (ms)
        
        // Quality metrics
        double boundary_confidence;  // Boundary detection confidence [0.0-1.0]
        bool is_valid;              // Whether segment has valid boundaries
        
        VCVSegment() 
            : start_time(0.0), consonant_start(0.0), consonant_end(0.0), end_time(0.0)
            , boundary_confidence(0.0), is_valid(false) {}
    };

    /**
     * @brief Phoneme boundary information extracted from timing parameters
     */
    struct PhonemeBoundary {
        double vowel1_start;         // First vowel start time (ms)
        double vowel1_end;           // First vowel end time (ms)
        double consonant_start;      // Consonant start time (ms)
        double consonant_end;        // Consonant end time (ms)
        double vowel2_start;         // Second vowel start time (ms)
        double vowel2_end;           // Second vowel end time (ms)
        
        // Confidence metrics
        double spectral_clarity;     // Spectral boundary clarity
        double timing_consistency;   // Timing parameter consistency
        
        PhonemeBoundary()
            : vowel1_start(0.0), vowel1_end(0.0), consonant_start(0.0)
            , consonant_end(0.0), vowel2_start(0.0), vowel2_end(0.0)
            , spectral_clarity(0.0), timing_consistency(0.0) {}
    };

    /**
     * @brief Japanese hiragana to romaji mapping utility
     */
    class HiraganaMapper {
    public:
        static std::string convertToRomaji(const std::string& hiragana);
        static std::string convertToHiragana(const std::string& romaji);
        static bool isValidHiragana(const std::string& str);
        static bool isValidRomaji(const std::string& str);
        
        // VCV-specific conversions
        static std::vector<std::string> extractPhonemesFromAlias(const std::string& alias);
        static std::string normalizeAlias(const std::string& alias);
        
    private:
        static const std::unordered_map<std::string, std::string> hiragana_to_romaji_map;
        static const std::unordered_map<std::string, std::string> romaji_to_hiragana_map;
        static const std::regex hiragana_pattern;
        static const std::regex romaji_pattern;
    };

    /**
     * @brief Phoneme boundary extractor from oto.ini timing parameters
     */
    class PhonemeBoundaryExtractor {
    public:
        struct ExtractionOptions {
            bool auto_detect_boundaries;    // Use spectral analysis for boundary detection
            bool validate_timing;           // Validate timing parameter consistency
            double minimum_consonant_length; // Minimum consonant duration (ms)
            double minimum_vowel_length;    // Minimum vowel duration (ms)
            
            ExtractionOptions()
                : auto_detect_boundaries(true), validate_timing(true)
                , minimum_consonant_length(20.0), minimum_vowel_length(30.0) {}
        };
        
        PhonemeBoundaryExtractor();
        explicit PhonemeBoundaryExtractor(const ExtractionOptions& options);
        
        // Extract boundaries from oto.ini timing parameters
        PhonemeBoundary extractFromOtoEntry(const OtoEntry& entry);
        std::vector<PhonemeBoundary> extractFromOtoEntries(const std::vector<OtoEntry>& entries);
        
        // Validate and correct boundary timing
        bool validateBoundary(const PhonemeBoundary& boundary);
        PhonemeBoundary correctBoundary(const PhonemeBoundary& boundary);
        
        // Confidence scoring
        double calculateBoundaryConfidence(const PhonemeBoundary& boundary, const OtoEntry& entry);
        
        // Configuration
        void setOptions(const ExtractionOptions& options) { options_ = options; }
        const ExtractionOptions& getOptions() const { return options_; }
        
    private:
        ExtractionOptions options_;
        
        // Internal boundary calculation methods
        PhonemeBoundary calculateFromTiming(const OtoEntry& entry);
        bool isTimingConsistent(const OtoEntry& entry);
        double estimateConsonantDuration(const std::string& consonant);
    };

    /**
     * @brief VCV pattern recognizer for Japanese phoneme sequences
     */
    class VCVPatternRecognizer {
    public:
        struct RecognitionOptions {
            bool strict_vcv_matching;        // Require strict VCV pattern (V-C-V)
            bool allow_cv_patterns;          // Allow CV patterns as degenerate VCV
            bool normalize_aliases;          // Normalize alias strings before matching
            double confidence_threshold;     // Minimum confidence for valid pattern
            
            RecognitionOptions()
                : strict_vcv_matching(false), allow_cv_patterns(true)
                , normalize_aliases(true), confidence_threshold(0.5) {}
        };
        
        struct RecognitionResult {
            std::vector<VCVSegment> vcv_segments;
            std::vector<std::string> cv_patterns;      // Non-VCV patterns found
            std::vector<std::string> errors;           // Recognition errors
            double overall_confidence;                 // Overall recognition confidence
            
            RecognitionResult() : overall_confidence(0.0) {}
        };
        
        VCVPatternRecognizer();
        explicit VCVPatternRecognizer(const RecognitionOptions& options);
        
        // Main recognition methods
        RecognitionResult recognizeFromOtoEntries(const std::vector<OtoEntry>& entries);
        RecognitionResult recognizeFromAlias(const std::string& alias);
        std::vector<VCVSegment> extractVCVSequence(const std::vector<OtoEntry>& entries);
        
        // Pattern analysis
        bool isVCVPattern(const std::string& alias);
        bool isCVPattern(const std::string& alias);
        std::vector<std::string> segmentAlias(const std::string& alias);
        
        // Quality assessment
        double assessVCVQuality(const VCVSegment& segment);
        std::vector<std::string> validateVCVSequence(const std::vector<VCVSegment>& sequence);
        
        // Configuration
        void setOptions(const RecognitionOptions& options) { options_ = options; }
        const RecognitionOptions& getOptions() const { return options_; }
        
        // Integration with boundary extractor
        void setBoundaryExtractor(std::shared_ptr<PhonemeBoundaryExtractor> extractor) {
            boundary_extractor_ = extractor;
        }
        
    private:
        RecognitionOptions options_;
        std::shared_ptr<PhonemeBoundaryExtractor> boundary_extractor_;
        HiraganaMapper hiragana_mapper_;
        
        // Internal recognition methods
        VCVSegment createVCVSegment(const OtoEntry& entry);
        bool matchesVCVPattern(const std::string& alias);
        std::vector<std::string> tokenizeAlias(const std::string& alias);
        double calculatePatternConfidence(const std::string& alias, const VCVSegment& segment);
        
        // Validation helpers
        bool isValidVowel(const std::string& phoneme);
        bool isValidConsonant(const std::string& phoneme);
        bool hasValidTransition(const std::string& v1, const std::string& c, const std::string& v2);
        
        // Utility methods
        std::string extractVowelFromPhoneme(const std::string& phoneme);
        std::string extractConsonantFromPhoneme(const std::string& phoneme);
        std::string normalizeAliasString(const std::string& alias);
    };

    /**
     * @brief Utility functions for VCV pattern processing
     */
    namespace vcv_utils {
        
        // Pattern matching utilities
        bool isJapaneseVowel(const std::string& phoneme);
        bool isJapaneseConsonant(const std::string& phoneme);
        bool isValidVCVTransition(const std::string& from, const std::string& to);
        
        // Timing analysis
        struct TimingStats {
            double avg_vowel_duration;
            double avg_consonant_duration;
            double avg_transition_duration;
            size_t total_segments;
        };
        
        TimingStats analyzeVCVTiming(const std::vector<VCVSegment>& segments);
        
        // Quality metrics
        double calculateCoarticulationScore(const VCVSegment& segment);
        double calculateNaturalnessScore(const std::vector<VCVSegment>& sequence);
        
        // Conversion utilities
        std::vector<std::string> vcvToPhonemeSequence(const std::vector<VCVSegment>& segments);
        std::string vcvSequenceToString(const std::vector<VCVSegment>& segments);
        
    } // namespace vcv_utils

} // namespace utau
} // namespace nexussynth
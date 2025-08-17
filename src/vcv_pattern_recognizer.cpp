#include "nexussynth/vcv_pattern_recognizer.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cctype>

namespace nexussynth {
namespace utau {

    // HiraganaMapper static member definitions
    const std::unordered_map<std::string, std::string> HiraganaMapper::hiragana_to_romaji_map = {
        // Basic vowels
        {"あ", "a"}, {"い", "i"}, {"う", "u"}, {"え", "e"}, {"お", "o"},
        
        // K-row
        {"か", "ka"}, {"き", "ki"}, {"く", "ku"}, {"け", "ke"}, {"こ", "ko"},
        {"が", "ga"}, {"ぎ", "gi"}, {"ぐ", "gu"}, {"げ", "ge"}, {"ご", "go"},
        
        // S-row
        {"さ", "sa"}, {"し", "shi"}, {"す", "su"}, {"せ", "se"}, {"そ", "so"},
        {"ざ", "za"}, {"じ", "ji"}, {"ず", "zu"}, {"ぜ", "ze"}, {"ぞ", "zo"},
        
        // T-row
        {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
        {"だ", "da"}, {"ぢ", "ji"}, {"づ", "zu"}, {"で", "de"}, {"ど", "do"},
        
        // N-row
        {"な", "na"}, {"に", "ni"}, {"ぬ", "nu"}, {"ね", "ne"}, {"の", "no"},
        
        // H-row
        {"は", "ha"}, {"ひ", "hi"}, {"ふ", "fu"}, {"へ", "he"}, {"ほ", "ho"},
        {"ば", "ba"}, {"び", "bi"}, {"ぶ", "bu"}, {"べ", "be"}, {"ぼ", "bo"},
        {"ぱ", "pa"}, {"ぴ", "pi"}, {"ぷ", "pu"}, {"ぺ", "pe"}, {"ぽ", "po"},
        
        // M-row
        {"ま", "ma"}, {"み", "mi"}, {"む", "mu"}, {"め", "me"}, {"も", "mo"},
        
        // Y-row
        {"や", "ya"}, {"ゆ", "yu"}, {"よ", "yo"},
        
        // R-row
        {"ら", "ra"}, {"り", "ri"}, {"る", "ru"}, {"れ", "re"}, {"ろ", "ro"},
        
        // W-row and N
        {"わ", "wa"}, {"を", "wo"}, {"ん", "n"},
        
        // Small characters
        {"ゃ", "ya"}, {"ゅ", "yu"}, {"ょ", "yo"},
        {"っ", "xtu"}, {"ぁ", "xa"}, {"ぃ", "xi"}, {"ぅ", "xu"}, {"ぇ", "xe"}, {"ぉ", "xo"}
    };
    
    const std::unordered_map<std::string, std::string> HiraganaMapper::romaji_to_hiragana_map = {
        // Reverse mapping (simplified for basic phonemes)
        {"a", "あ"}, {"i", "い"}, {"u", "う"}, {"e", "え"}, {"o", "お"},
        {"ka", "か"}, {"ki", "き"}, {"ku", "く"}, {"ke", "け"}, {"ko", "こ"},
        {"ga", "が"}, {"gi", "ぎ"}, {"gu", "ぐ"}, {"ge", "げ"}, {"go", "ご"},
        {"sa", "さ"}, {"shi", "し"}, {"su", "す"}, {"se", "せ"}, {"so", "そ"},
        {"za", "ざ"}, {"ji", "じ"}, {"zu", "ず"}, {"ze", "ぜ"}, {"zo", "ぞ"},
        {"ta", "た"}, {"chi", "ち"}, {"tsu", "つ"}, {"te", "て"}, {"to", "と"},
        {"da", "だ"}, {"de", "で"}, {"do", "ど"},
        {"na", "な"}, {"ni", "に"}, {"nu", "ぬ"}, {"ne", "ね"}, {"no", "の"},
        {"ha", "は"}, {"hi", "ひ"}, {"fu", "ふ"}, {"he", "へ"}, {"ho", "ほ"},
        {"ba", "ば"}, {"bi", "び"}, {"bu", "ぶ"}, {"be", "べ"}, {"bo", "ぼ"},
        {"pa", "ぱ"}, {"pi", "ぴ"}, {"pu", "ぷ"}, {"pe", "ぺ"}, {"po", "ぽ"},
        {"ma", "ま"}, {"mi", "み"}, {"mu", "む"}, {"me", "め"}, {"mo", "も"},
        {"ya", "や"}, {"yu", "ゆ"}, {"yo", "よ"},
        {"ra", "ら"}, {"ri", "り"}, {"ru", "る"}, {"re", "れ"}, {"ro", "ろ"},
        {"wa", "わ"}, {"wo", "を"}, {"n", "ん"}
    };
    
    const std::regex HiraganaMapper::hiragana_pattern(u8"[あ-ゖ]+");
    const std::regex HiraganaMapper::romaji_pattern("[a-z]+");

    // HiraganaMapper implementation
    std::string HiraganaMapper::convertToRomaji(const std::string& hiragana) {
        std::string result;
        
        // Simple character-by-character conversion
        // In a real implementation, would need proper UTF-8 handling
        for (const auto& pair : hiragana_to_romaji_map) {
            size_t pos = hiragana.find(pair.first);
            if (pos != std::string::npos) {
                result = pair.second;
                break;
            }
        }
        
        if (result.empty()) {
            result = hiragana; // Fallback to original
        }
        
        return result;
    }
    
    std::string HiraganaMapper::convertToHiragana(const std::string& romaji) {
        auto it = romaji_to_hiragana_map.find(romaji);
        if (it != romaji_to_hiragana_map.end()) {
            return it->second;
        }
        return romaji; // Fallback to original
    }
    
    bool HiraganaMapper::isValidHiragana(const std::string& str) {
        return std::regex_match(str, hiragana_pattern);
    }
    
    bool HiraganaMapper::isValidRomaji(const std::string& str) {
        return std::regex_match(str, romaji_pattern);
    }
    
    std::vector<std::string> HiraganaMapper::extractPhonemesFromAlias(const std::string& alias) {
        std::vector<std::string> phonemes;
        std::istringstream iss(alias);
        std::string phoneme;
        
        while (iss >> phoneme) {
            phonemes.push_back(phoneme);
        }
        
        return phonemes;
    }
    
    std::string HiraganaMapper::normalizeAlias(const std::string& alias) {
        std::string normalized = alias;
        
        // Remove extra whitespace
        std::replace(normalized.begin(), normalized.end(), '\t', ' ');
        
        // Normalize multiple spaces to single space
        std::regex multiple_spaces("\\s+");
        normalized = std::regex_replace(normalized, multiple_spaces, " ");
        
        // Trim leading/trailing whitespace
        size_t start = normalized.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        
        size_t end = normalized.find_last_not_of(" \t\r\n");
        return normalized.substr(start, end - start + 1);
    }

    // PhonemeBoundaryExtractor implementation
    PhonemeBoundaryExtractor::PhonemeBoundaryExtractor() = default;
    
    PhonemeBoundaryExtractor::PhonemeBoundaryExtractor(const ExtractionOptions& options) 
        : options_(options) {}
    
    PhonemeBoundary PhonemeBoundaryExtractor::extractFromOtoEntry(const OtoEntry& entry) {
        return calculateFromTiming(entry);
    }
    
    std::vector<PhonemeBoundary> PhonemeBoundaryExtractor::extractFromOtoEntries(
        const std::vector<OtoEntry>& entries) {
        
        std::vector<PhonemeBoundary> boundaries;
        boundaries.reserve(entries.size());
        
        for (const auto& entry : entries) {
            boundaries.push_back(extractFromOtoEntry(entry));
        }
        
        return boundaries;
    }
    
    bool PhonemeBoundaryExtractor::validateBoundary(const PhonemeBoundary& boundary) {
        // Check timing consistency
        if (boundary.vowel1_start >= boundary.vowel1_end ||
            boundary.consonant_start >= boundary.consonant_end ||
            boundary.vowel2_start >= boundary.vowel2_end) {
            return false;
        }
        
        // Check minimum durations
        double vowel1_duration = boundary.vowel1_end - boundary.vowel1_start;
        double consonant_duration = boundary.consonant_end - boundary.consonant_start;
        double vowel2_duration = boundary.vowel2_end - boundary.vowel2_start;
        
        if (vowel1_duration < options_.minimum_vowel_length ||
            consonant_duration < options_.minimum_consonant_length ||
            vowel2_duration < options_.minimum_vowel_length) {
            return false;
        }
        
        return true;
    }
    
    PhonemeBoundary PhonemeBoundaryExtractor::correctBoundary(const PhonemeBoundary& boundary) {
        PhonemeBoundary corrected = boundary;
        
        // Apply minimum duration constraints
        if (corrected.vowel1_end - corrected.vowel1_start < options_.minimum_vowel_length) {
            corrected.vowel1_end = corrected.vowel1_start + options_.minimum_vowel_length;
        }
        
        if (corrected.consonant_end - corrected.consonant_start < options_.minimum_consonant_length) {
            corrected.consonant_end = corrected.consonant_start + options_.minimum_consonant_length;
        }
        
        if (corrected.vowel2_end - corrected.vowel2_start < options_.minimum_vowel_length) {
            corrected.vowel2_end = corrected.vowel2_start + options_.minimum_vowel_length;
        }
        
        return corrected;
    }
    
    double PhonemeBoundaryExtractor::calculateBoundaryConfidence(
        const PhonemeBoundary& boundary, const OtoEntry& entry) {
        
        double confidence = 1.0;
        
        // Reduce confidence for timing inconsistencies
        if (!isTimingConsistent(entry)) {
            confidence *= 0.7;
        }
        
        // Reduce confidence for very short segments
        double total_duration = boundary.vowel2_end - boundary.vowel1_start;
        if (total_duration < 100.0) { // Less than 100ms
            confidence *= 0.5;
        }
        
        // Check for reasonable proportions
        double consonant_ratio = (boundary.consonant_end - boundary.consonant_start) / total_duration;
        if (consonant_ratio < 0.1 || consonant_ratio > 0.8) {
            confidence *= 0.8;
        }
        
        return std::max(0.0, std::min(1.0, confidence));
    }
    
    PhonemeBoundary PhonemeBoundaryExtractor::calculateFromTiming(const OtoEntry& entry) {
        PhonemeBoundary boundary;
        
        // Calculate boundaries based on oto.ini timing parameters
        // Format: filename=alias,offset,consonant,blank,preutterance,overlap
        
        double offset = entry.offset;
        double consonant_length = entry.consonant;
        double blank = entry.blank;
        double preutterance = entry.preutterance;
        // double overlap = entry.overlap; // Currently unused
        
        // Calculate segment boundaries
        boundary.vowel1_start = offset - preutterance;
        boundary.vowel1_end = offset;
        boundary.consonant_start = offset;
        boundary.consonant_end = offset + consonant_length;
        boundary.vowel2_start = offset + consonant_length;
        boundary.vowel2_end = offset + consonant_length + blank;
        
        // Calculate quality metrics
        boundary.timing_consistency = isTimingConsistent(entry) ? 1.0 : 0.5;
        boundary.spectral_clarity = 0.8; // Default value, would need spectral analysis
        
        return boundary;
    }
    
    bool PhonemeBoundaryExtractor::isTimingConsistent(const OtoEntry& entry) {
        // Check for negative or invalid values
        if (entry.offset < 0 || entry.consonant < 0 || entry.blank < 0) {
            return false;
        }
        
        // Check for reasonable proportions
        double total_length = entry.consonant + entry.blank;
        if (total_length <= 0) {
            return false;
        }
        
        // Preutterance should not be too large compared to total length
        if (entry.preutterance > total_length * 2) {
            return false;
        }
        
        return true;
    }
    
    double PhonemeBoundaryExtractor::estimateConsonantDuration(const std::string& consonant) {
        // Simple duration estimates for different consonant types
        if (consonant == "k" || consonant == "g" || consonant == "t" || consonant == "d") {
            return 50.0; // Plosives: ~50ms
        } else if (consonant == "s" || consonant == "sh" || consonant == "z") {
            return 80.0; // Fricatives: ~80ms
        } else if (consonant == "m" || consonant == "n") {
            return 60.0; // Nasals: ~60ms
        } else if (consonant == "r") {
            return 40.0; // Taps: ~40ms
        }
        
        return 60.0; // Default duration
    }

    // VCVPatternRecognizer implementation
    VCVPatternRecognizer::VCVPatternRecognizer() 
        : boundary_extractor_(std::make_shared<PhonemeBoundaryExtractor>()) {}
    
    VCVPatternRecognizer::VCVPatternRecognizer(const RecognitionOptions& options)
        : options_(options), boundary_extractor_(std::make_shared<PhonemeBoundaryExtractor>()) {}
    
    VCVPatternRecognizer::RecognitionResult VCVPatternRecognizer::recognizeFromOtoEntries(
        const std::vector<OtoEntry>& entries) {
        
        RecognitionResult result;
        
        for (const auto& entry : entries) {
            if (isVCVPattern(entry.alias)) {
                VCVSegment segment = createVCVSegment(entry);
                if (segment.is_valid && segment.boundary_confidence >= options_.confidence_threshold) {
                    result.vcv_segments.push_back(segment);
                }
            } else if (options_.allow_cv_patterns && isCVPattern(entry.alias)) {
                result.cv_patterns.push_back(entry.alias);
            } else {
                result.errors.push_back("Unrecognized pattern: " + entry.alias);
            }
        }
        
        // Calculate overall confidence
        double total_confidence = 0.0;
        for (const auto& segment : result.vcv_segments) {
            total_confidence += segment.boundary_confidence;
        }
        
        if (!result.vcv_segments.empty()) {
            result.overall_confidence = total_confidence / result.vcv_segments.size();
        }
        
        return result;
    }
    
    VCVPatternRecognizer::RecognitionResult VCVPatternRecognizer::recognizeFromAlias(
        const std::string& alias) {
        
        RecognitionResult result;
        
        // Create dummy OtoEntry for single alias recognition
        OtoEntry dummy_entry;
        dummy_entry.alias = alias;
        dummy_entry.filename = "dummy.wav";
        dummy_entry.offset = 100.0;
        dummy_entry.consonant = 50.0;
        dummy_entry.blank = 100.0;
        dummy_entry.preutterance = 80.0;
        dummy_entry.overlap = 20.0;
        
        return recognizeFromOtoEntries({dummy_entry});
    }
    
    std::vector<VCVSegment> VCVPatternRecognizer::extractVCVSequence(
        const std::vector<OtoEntry>& entries) {
        
        auto result = recognizeFromOtoEntries(entries);
        return result.vcv_segments;
    }
    
    bool VCVPatternRecognizer::isVCVPattern(const std::string& alias) {
        return matchesVCVPattern(alias);
    }
    
    bool VCVPatternRecognizer::isCVPattern(const std::string& alias) {
        auto phonemes = hiragana_mapper_.extractPhonemesFromAlias(alias);
        
        if (phonemes.size() == 1) {
            // Single phoneme - check if it's CV
            std::string phoneme = phonemes[0];
            return phoneme.length() >= 2 && isValidConsonant(phoneme.substr(0, 1)) && 
                   isValidVowel(phoneme.substr(1, 1));
        }
        
        return false;
    }
    
    std::vector<std::string> VCVPatternRecognizer::segmentAlias(const std::string& alias) {
        return hiragana_mapper_.extractPhonemesFromAlias(alias);
    }
    
    double VCVPatternRecognizer::assessVCVQuality(const VCVSegment& segment) {
        double quality = 1.0;
        
        // Check boundary confidence
        quality *= segment.boundary_confidence;
        
        // Check vowel-consonant-vowel validity
        if (!isValidVowel(segment.vowel1) || !isValidVowel(segment.vowel2) || 
            !isValidConsonant(segment.consonant)) {
            quality *= 0.5;
        }
        
        // Check transition validity
        if (!hasValidTransition(segment.vowel1, segment.consonant, segment.vowel2)) {
            quality *= 0.7;
        }
        
        // Check timing consistency
        double total_duration = segment.end_time - segment.start_time;
        double consonant_duration = segment.consonant_end - segment.consonant_start;
        
        if (total_duration <= 0 || consonant_duration <= 0) {
            quality = 0.0;
        } else {
            double consonant_ratio = consonant_duration / total_duration;
            if (consonant_ratio < 0.1 || consonant_ratio > 0.8) {
                quality *= 0.8;
            }
        }
        
        return std::max(0.0, std::min(1.0, quality));
    }
    
    std::vector<std::string> VCVPatternRecognizer::validateVCVSequence(
        const std::vector<VCVSegment>& sequence) {
        
        std::vector<std::string> errors;
        
        for (size_t i = 0; i < sequence.size(); ++i) {
            const auto& segment = sequence[i];
            
            // Validate individual segment
            if (!segment.is_valid) {
                errors.push_back("Invalid segment at index " + std::to_string(i) + ": " + segment.full_alias);
            }
            
            // Check timing continuity between segments
            if (i > 0) {
                const auto& prev_segment = sequence[i - 1];
                if (segment.start_time < prev_segment.end_time) {
                    errors.push_back("Timing overlap between segments " + std::to_string(i - 1) + 
                                   " and " + std::to_string(i));
                }
            }
            
            // Check quality threshold
            double quality = assessVCVQuality(segment);
            if (quality < options_.confidence_threshold) {
                errors.push_back("Low quality segment at index " + std::to_string(i) + 
                               ": " + segment.full_alias + " (quality: " + std::to_string(quality) + ")");
            }
        }
        
        return errors;
    }
    
    VCVSegment VCVPatternRecognizer::createVCVSegment(const OtoEntry& entry) {
        VCVSegment segment;
        
        // Extract phonemes from alias
        auto phonemes = tokenizeAlias(entry.alias);
        
        if (phonemes.size() >= 2) {
            // Parse VCV pattern
            if (phonemes.size() == 2) {
                // Format: "a ka" -> vowel1="a", consonant="k", vowel2="a"
                segment.vowel1 = phonemes[0];
                std::string cv = phonemes[1];
                if (cv.length() >= 2) {
                    segment.consonant = cv.substr(0, cv.length() - 1);
                    segment.vowel2 = cv.substr(cv.length() - 1);
                }
            } else if (phonemes.size() == 3) {
                // Format: "a k a" -> vowel1="a", consonant="k", vowel2="a"
                segment.vowel1 = phonemes[0];
                segment.consonant = phonemes[1];
                segment.vowel2 = phonemes[2];
            }
        }
        
        segment.full_alias = entry.alias;
        
        // Extract timing from oto entry
        if (boundary_extractor_) {
            auto boundary = boundary_extractor_->extractFromOtoEntry(entry);
            segment.start_time = boundary.vowel1_start;
            segment.consonant_start = boundary.consonant_start;
            segment.consonant_end = boundary.consonant_end;
            segment.end_time = boundary.vowel2_end;
            segment.boundary_confidence = boundary_extractor_->calculateBoundaryConfidence(boundary, entry);
        }
        
        // Validate segment
        segment.is_valid = !segment.vowel1.empty() && !segment.consonant.empty() && 
                          !segment.vowel2.empty() && segment.boundary_confidence > 0.0;
        
        return segment;
    }
    
    bool VCVPatternRecognizer::matchesVCVPattern(const std::string& alias) {
        auto phonemes = tokenizeAlias(alias);
        
        if (phonemes.size() == 2) {
            // Check "V CV" pattern
            return isValidVowel(phonemes[0]) && isCVPattern(phonemes[1]);
        } else if (phonemes.size() == 3) {
            // Check "V C V" pattern
            return isValidVowel(phonemes[0]) && isValidConsonant(phonemes[1]) && isValidVowel(phonemes[2]);
        }
        
        return false;
    }
    
    std::vector<std::string> VCVPatternRecognizer::tokenizeAlias(const std::string& alias) {
        std::string normalized = normalizeAliasString(alias);
        return hiragana_mapper_.extractPhonemesFromAlias(normalized);
    }
    
    double VCVPatternRecognizer::calculatePatternConfidence(const std::string& /* alias */, 
                                                           const VCVSegment& segment) {
        double confidence = 1.0;
        
        // Check if phonemes are valid Japanese phonemes
        if (!vcv_utils::isJapaneseVowel(segment.vowel1) || 
            !vcv_utils::isJapaneseVowel(segment.vowel2) ||
            !vcv_utils::isJapaneseConsonant(segment.consonant)) {
            confidence *= 0.5;
        }
        
        // Check transition validity
        if (!vcv_utils::isValidVCVTransition(segment.vowel1 + segment.consonant, 
                                            segment.consonant + segment.vowel2)) {
            confidence *= 0.8;
        }
        
        return confidence;
    }
    
    bool VCVPatternRecognizer::isValidVowel(const std::string& phoneme) {
        return vcv_utils::isJapaneseVowel(phoneme);
    }
    
    bool VCVPatternRecognizer::isValidConsonant(const std::string& phoneme) {
        return vcv_utils::isJapaneseConsonant(phoneme);
    }
    
    bool VCVPatternRecognizer::hasValidTransition(const std::string& v1, const std::string& c, 
                                                 const std::string& v2) {
        return vcv_utils::isValidVCVTransition(v1 + c, c + v2);
    }
    
    std::string VCVPatternRecognizer::extractVowelFromPhoneme(const std::string& phoneme) {
        if (phoneme.empty()) return "";
        
        // For CV phonemes, vowel is typically the last character
        return phoneme.substr(phoneme.length() - 1);
    }
    
    std::string VCVPatternRecognizer::extractConsonantFromPhoneme(const std::string& phoneme) {
        if (phoneme.length() <= 1) return "";
        
        // For CV phonemes, consonant is everything except the last character
        return phoneme.substr(0, phoneme.length() - 1);
    }
    
    std::string VCVPatternRecognizer::normalizeAliasString(const std::string& alias) {
        if (options_.normalize_aliases) {
            return hiragana_mapper_.normalizeAlias(alias);
        }
        return alias;
    }

    // VCV utilities implementation
    namespace vcv_utils {
        
        bool isJapaneseVowel(const std::string& phoneme) {
            return phoneme == "a" || phoneme == "i" || phoneme == "u" || 
                   phoneme == "e" || phoneme == "o";
        }
        
        bool isJapaneseConsonant(const std::string& phoneme) {
            static const std::vector<std::string> consonants = {
                "k", "g", "s", "sh", "z", "j", "t", "ch", "d", "n", 
                "h", "b", "p", "m", "y", "r", "w"
            };
            
            return std::find(consonants.begin(), consonants.end(), phoneme) != consonants.end();
        }
        
        bool isValidVCVTransition(const std::string& from, const std::string& to) {
            // Simple validation - more sophisticated phonological rules could be added
            if (from.empty() || to.empty()) return false;
            
            // Check for impossible transitions (simplified)
            if (from == "tsu" && to == "n") return false; // Example impossible transition
            
            return true;
        }
        
        TimingStats analyzeVCVTiming(const std::vector<VCVSegment>& segments) {
            TimingStats stats;
            stats.total_segments = segments.size();
            
            if (segments.empty()) {
                return stats;
            }
            
            double total_vowel_duration = 0.0;
            double total_consonant_duration = 0.0;
            double total_transition_duration = 0.0;
            
            for (const auto& segment : segments) {
                double vowel_duration = (segment.consonant_start - segment.start_time) + 
                                       (segment.end_time - segment.consonant_end);
                double consonant_duration = segment.consonant_end - segment.consonant_start;
                double transition_duration = segment.end_time - segment.start_time;
                
                total_vowel_duration += vowel_duration;
                total_consonant_duration += consonant_duration;
                total_transition_duration += transition_duration;
            }
            
            stats.avg_vowel_duration = total_vowel_duration / segments.size();
            stats.avg_consonant_duration = total_consonant_duration / segments.size();
            stats.avg_transition_duration = total_transition_duration / segments.size();
            
            return stats;
        }
        
        double calculateCoarticulationScore(const VCVSegment& segment) {
            // Simple coarticulation scoring based on phoneme compatibility
            double score = 1.0;
            
            // Penalize difficult transitions
            if ((segment.vowel1 == "i" && segment.consonant == "u") ||
                (segment.vowel2 == "u" && segment.consonant == "i")) {
                score *= 0.8;
            }
            
            // Bonus for natural transitions
            if ((segment.vowel1 == "a" && segment.vowel2 == "a") ||
                (segment.consonant == "k" || segment.consonant == "s")) {
                score *= 1.1;
            }
            
            return std::min(1.0, score);
        }
        
        double calculateNaturalnessScore(const std::vector<VCVSegment>& sequence) {
            if (sequence.empty()) return 0.0;
            
            double total_score = 0.0;
            
            for (const auto& segment : sequence) {
                total_score += calculateCoarticulationScore(segment);
            }
            
            return total_score / sequence.size();
        }
        
        std::vector<std::string> vcvToPhonemeSequence(const std::vector<VCVSegment>& segments) {
            std::vector<std::string> phonemes;
            
            for (size_t i = 0; i < segments.size(); ++i) {
                const auto& segment = segments[i];
                
                if (i == 0) {
                    phonemes.push_back(segment.vowel1);
                }
                phonemes.push_back(segment.consonant + segment.vowel2);
            }
            
            return phonemes;
        }
        
        std::string vcvSequenceToString(const std::vector<VCVSegment>& segments) {
            std::ostringstream oss;
            
            for (size_t i = 0; i < segments.size(); ++i) {
                if (i > 0) oss << " -> ";
                oss << segments[i].full_alias;
            }
            
            return oss.str();
        }
        
    } // namespace vcv_utils

} // namespace utau
} // namespace nexussynth
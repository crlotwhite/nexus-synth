#pragma once

#include "utau_oto_parser.h"
#include "vcv_pattern_recognizer.h"
#include "midi_phoneme_integrator.h"
#include <Eigen/Dense>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <optional>

namespace nexussynth {
namespace context {

    /**
     * @brief Binary phoneme features for Japanese phonemes
     */
    struct PhonemeFeatures {
        // Phoneme type classification (8 bits)
        bool is_vowel;           // Vowel
        bool is_consonant;       // Consonant
        bool is_silence;         // Silence/pause
        bool is_long_vowel;      // Long vowel
        bool is_nasal;           // Nasal consonant
        bool is_fricative;       // Fricative consonant
        bool is_plosive;         // Plosive/stop consonant
        bool is_semivowel;       // Semivowel/glide
        
        // Place of articulation (6 bits)
        bool place_bilabial;     // Bilabial
        bool place_alveolar;     // Alveolar
        bool place_palatal;      // Palatal
        bool place_velar;        // Velar
        bool place_glottal;      // Glottal
        bool place_dental;       // Dental
        
        // Manner of articulation (8 bits)
        bool manner_stop;        // Stop/plosive
        bool manner_fricative;   // Fricative
        bool manner_nasal;       // Nasal
        bool manner_liquid;      // Liquid/approximant
        bool manner_glide;       // Glide
        bool voiced;             // Voiced
        bool aspirated;          // Aspirated
        bool palatalized;        // Palatalized
        
        // Vowel characteristics (10 bits)
        bool vowel_front;        // Front vowel
        bool vowel_central;      // Central vowel
        bool vowel_back;         // Back vowel
        bool vowel_high;         // High vowel
        bool vowel_mid;          // Mid vowel
        bool vowel_low;          // Low vowel
        bool vowel_rounded;      // Rounded vowel
        bool vowel_unrounded;    // Unrounded vowel
        bool vowel_long;         // Long vowel
        bool vowel_nasalized;    // Nasalized vowel
        
        // Convert to vector for processing
        std::vector<float> toBinaryVector() const;
        static constexpr size_t FEATURE_SIZE = 32;
        
        PhonemeFeatures();
    };

    /**
     * @brief Syllable and position encoding information
     */
    struct PositionEncoding {
        // Position within linguistic units (0.0 ~ 1.0)
        float position_in_syllable;      // Position within syllable
        float position_in_mora;          // Position within mora
        float position_in_word;          // Position within word
        float position_in_phrase;        // Position within phrase
        float position_in_utterance;     // Position within entire utterance
        
        // Boundary information
        bool is_syllable_initial;        // First phoneme in syllable
        bool is_syllable_final;          // Last phoneme in syllable
        bool is_word_initial;            // First syllable in word
        bool is_word_final;              // Last syllable in word
        bool is_phrase_initial;          // First word in phrase
        bool is_phrase_final;            // Last word in phrase
        
        // Accent and prosodic information
        float accent_strength;           // Accent strength (0.0 ~ 1.0)
        bool has_accent;                 // Has accent on this mora
        int accent_position;             // Accent position in word (mora units)
        bool is_major_phrase_boundary;   // Major phrase boundary
        bool is_minor_phrase_boundary;   // Minor phrase boundary
        
        // Convert to vector for processing
        std::vector<float> toVector() const;
        static constexpr size_t ENCODING_SIZE = 16;
        
        PositionEncoding();
    };

    /**
     * @brief Phoneme timing and duration information
     */
    struct PhonemeTimingInfo {
        std::string phoneme;             // Phoneme string (e.g., "a", "ka")
        double start_time_ms;            // Start time in milliseconds
        double duration_ms;              // Phoneme duration
        double end_time_ms;              // End time in milliseconds
        
        // VCV specific timing
        double consonant_start_ms;       // Consonant start (for VCV)
        double consonant_end_ms;         // Consonant end (for VCV)
        double transition_duration_ms;   // Transition duration
        
        // Quality metrics
        double timing_confidence;        // Timing accuracy confidence [0.0-1.0]
        bool is_valid;                  // Whether timing is valid
        
        PhonemeTimingInfo();
    };

    /**
     * @brief Context features for a single frame/phoneme
     */
    struct ContextFeatures {
        // Multi-scale context windows
        std::vector<PhonemeFeatures> phoneme_context;    // ±3 phoneme window (7 total)
        std::vector<PositionEncoding> position_context;  // Position encodings for context
        
        // Current frame information
        PhonemeTimingInfo current_timing;                // Current phoneme timing
        midi::MidiNote current_midi;                     // Current MIDI note (if available)
        utau::VCVSegment current_vcv;                    // Current VCV segment (if applicable)
        
        // Frame-level features
        double frame_time_ms;                            // Frame timestamp
        size_t frame_index;                              // Frame index in utterance
        
        // Convert to single feature vector
        Eigen::VectorXd toFeatureVector() const;
        static constexpr size_t CONTEXT_WINDOW_SIZE = 7; // ±3 phonemes
        static size_t getTotalDimension();
        
        ContextFeatures();
    };

    /**
     * @brief Japanese phoneme classifier and feature extractor
     */
    class JapanesePhonemeClassifier {
    public:
        JapanesePhonemeClassifier();
        
        // Phoneme classification
        PhonemeFeatures classifyPhoneme(const std::string& phoneme) const;
        bool isJapaneseVowel(const std::string& phoneme) const;
        bool isJapaneseConsonant(const std::string& phoneme) const;
        bool isValidJapanesePhoneme(const std::string& phoneme) const;
        
        // Phoneme analysis
        std::string getPhonemeCategory(const std::string& phoneme) const;
        std::vector<std::string> getPhonemeFeatureLabels(const std::string& phoneme) const;
        
        // Similarity and distance
        double calculatePhonemeDistance(
            const std::string& phoneme1, 
            const std::string& phoneme2) const;
        std::vector<std::string> findSimilarPhonemes(
            const std::string& phoneme, 
            double threshold = 0.8) const;
        
    private:
        // Phoneme feature lookup tables
        std::unordered_map<std::string, PhonemeFeatures> phoneme_features_;
        
        void initializePhonemeFeatures();
        void initializeVowelFeatures();
        void initializeConsonantFeatures();
        void initializeSpecialPhonemes();
    };

    /**
     * @brief Context window extractor for multi-scale temporal features
     */
    class ContextWindowExtractor {
    public:
        struct WindowConfig {
            size_t phoneme_window;       // ±N phonemes (default: 3)
            size_t syllable_window;      // ±N syllables (default: 2)
            size_t mora_window;          // ±N mora (default: 2)
            bool enable_padding;         // Enable padding for boundaries
            std::string padding_symbol;  // Symbol for padding (default: "<SIL>")
            
            WindowConfig() 
                : phoneme_window(3), syllable_window(2), mora_window(2)
                , enable_padding(true), padding_symbol("<SIL>") {}
        };
        
        ContextWindowExtractor();
        explicit ContextWindowExtractor(const WindowConfig& config);
        
        // Extract context windows
        std::vector<PhonemeFeatures> extractPhonemeContext(
            const std::vector<PhonemeTimingInfo>& phonemes,
            size_t current_index) const;
        
        std::vector<PositionEncoding> extractPositionContext(
            const std::vector<PhonemeTimingInfo>& phonemes,
            size_t current_index) const;
        
        // Configuration
        void setConfig(const WindowConfig& config) { config_ = config; }
        const WindowConfig& getConfig() const { return config_; }
        
    private:
        WindowConfig config_;
        JapanesePhonemeClassifier classifier_;
        
        // Helper methods
        std::vector<size_t> getContextIndices(
            size_t current_index, 
            size_t sequence_length, 
            size_t window_size) const;
        PhonemeFeatures getPaddingFeatures() const;
        PositionEncoding getPaddingPosition() const;
    };

    /**
     * @brief Position and prosodic information encoder
     */
    class PositionEncoder {
    public:
        struct AccentInfo {
            int accent_position;         // Mora-based accent position (-1 = no accent)
            float accent_strength;       // Accent strength [0.0-1.0]
            bool is_heiban;             // Flat accent type
            bool is_kifuku;             // Falling accent type
            
            AccentInfo() : accent_position(-1), accent_strength(0.0)
                        , is_heiban(false), is_kifuku(false) {}
        };
        
        PositionEncoder();
        
        // Position encoding
        PositionEncoding encodePosition(
            const std::vector<PhonemeTimingInfo>& phonemes,
            size_t phoneme_index,
            const AccentInfo& accent_info = AccentInfo()) const;
        
        // Syllable and mora analysis
        std::vector<std::vector<size_t>> extractSyllables(
            const std::vector<PhonemeTimingInfo>& phonemes) const;
        std::vector<std::vector<size_t>> extractMora(
            const std::vector<PhonemeTimingInfo>& phonemes) const;
        
        // Accent pattern detection
        AccentInfo detectAccentPattern(
            const std::vector<PhonemeTimingInfo>& phonemes,
            const midi::MidiParser::ParseResult& midi_data) const;
        
        // Boundary detection
        std::vector<bool> detectPhraseBoundaries(
            const std::vector<PhonemeTimingInfo>& phonemes) const;
        std::vector<bool> detectWordBoundaries(
            const std::vector<PhonemeTimingInfo>& phonemes) const;
        
    private:
        // Helper methods
        float calculateRelativePosition(size_t index, size_t start, size_t end) const;
        bool isSyllableBoundary(const PhonemeTimingInfo& prev, const PhonemeTimingInfo& curr) const;
        bool isMoraBoundary(const PhonemeTimingInfo& prev, const PhonemeTimingInfo& curr) const;
        float calculateAccentStrength(const AccentInfo& info, size_t mora_index) const;
    };

    /**
     * @brief Feature vector normalizer with multiple normalization strategies
     */
    class FeatureNormalizer {
    public:
        enum class NormalizationType {
            NONE,               // No normalization
            Z_SCORE,            // Mean 0, std 1
            MIN_MAX,            // [0, 1] range
            ROBUST_SCALING,     // Median-based scaling
            QUANTILE_UNIFORM,   // Quantile transformation
            LOG_SCALING         // Log transformation
        };
        
        struct NormalizationParams {
            Eigen::VectorXd mean;
            Eigen::VectorXd std;
            Eigen::VectorXd min;
            Eigen::VectorXd max;
            Eigen::VectorXd median;
            Eigen::VectorXd q25;    // 25th percentile
            Eigen::VectorXd q75;    // 75th percentile
            
            bool is_fitted = false;
        };
        
        FeatureNormalizer();
        explicit FeatureNormalizer(NormalizationType type);
        
        // Training and fitting
        void fit(const std::vector<Eigen::VectorXd>& training_data);
        void fitIncremental(const Eigen::VectorXd& sample);
        
        // Normalization
        Eigen::VectorXd normalize(const Eigen::VectorXd& features) const;
        Eigen::VectorXd denormalize(const Eigen::VectorXd& normalized_features) const;
        
        // Batch processing
        std::vector<Eigen::VectorXd> normalizeBatch(
            const std::vector<Eigen::VectorXd>& features) const;
        
        // Configuration
        void setNormalizationType(NormalizationType type) { type_ = type; }
        NormalizationType getNormalizationType() const { return type_; }
        
        // Serialization
        void saveParams(const std::string& filename) const;
        void loadParams(const std::string& filename);
        
        // Statistics
        const NormalizationParams& getParams() const { return params_; }
        bool isFitted() const { return params_.is_fitted; }
        
    private:
        NormalizationType type_;
        NormalizationParams params_;
        
        // Incremental statistics tracking
        size_t sample_count_;
        Eigen::VectorXd running_mean_;
        Eigen::VectorXd running_m2_;  // For Welford's algorithm
        
        // Normalization implementations
        Eigen::VectorXd zScoreNormalize(const Eigen::VectorXd& features) const;
        Eigen::VectorXd minMaxNormalize(const Eigen::VectorXd& features) const;
        Eigen::VectorXd robustScaleNormalize(const Eigen::VectorXd& features) const;
        Eigen::VectorXd quantileNormalize(const Eigen::VectorXd& features) const;
        Eigen::VectorXd logScaleNormalize(const Eigen::VectorXd& features) const;
        
        void calculateStatistics(const std::vector<Eigen::VectorXd>& data);
        void updateIncrementalStats(const Eigen::VectorXd& sample);
    };

    /**
     * @brief Main context feature extractor integrating all components
     */
    class ContextFeatureExtractor {
    public:
        struct ExtractionConfig {
            ContextWindowExtractor::WindowConfig window_config;
            FeatureNormalizer::NormalizationType normalization_type;
            bool include_midi_features;      // Include MIDI note features
            bool include_vcv_features;       // Include VCV pattern features
            bool include_timing_features;    // Include detailed timing features
            bool enable_caching;             // Enable feature caching
            
            ExtractionConfig()
                : normalization_type(FeatureNormalizer::NormalizationType::Z_SCORE)
                , include_midi_features(true), include_vcv_features(true)
                , include_timing_features(true), enable_caching(true) {}
        };
        
        ContextFeatureExtractor();
        explicit ContextFeatureExtractor(const ExtractionConfig& config);
        
        // Main extraction methods
        ContextFeatures extractFeatures(
            const std::vector<midi::MusicalPhoneme>& musical_phonemes,
            size_t current_index) const;
        
        std::vector<ContextFeatures> extractBatch(
            const std::vector<midi::MusicalPhoneme>& musical_phonemes) const;
        
        // Integration with existing systems
        ContextFeatures extractFromOtoEntries(
            const std::vector<utau::OtoEntry>& oto_entries,
            size_t current_index,
            const midi::MidiParser::ParseResult& midi_data = {}) const;
        
        ContextFeatures extractFromVCVSegments(
            const std::vector<utau::VCVSegment>& vcv_segments,
            size_t current_index) const;
        
        // Training and normalization
        void trainNormalizer(const std::vector<ContextFeatures>& training_data);
        void enableNormalization(bool enable = true) { use_normalization_ = enable; }
        
        // Configuration
        void setConfig(const ExtractionConfig& config);
        const ExtractionConfig& getConfig() const { return config_; }
        
        // Caching
        void clearCache();
        size_t getCacheSize() const;
        
        // Utilities
        static std::vector<PhonemeTimingInfo> convertFromMusicalPhonemes(
            const std::vector<midi::MusicalPhoneme>& musical_phonemes);
        static std::vector<PhonemeTimingInfo> convertFromOtoEntries(
            const std::vector<utau::OtoEntry>& oto_entries);
        
    private:
        ExtractionConfig config_;
        
        // Component instances
        JapanesePhonemeClassifier classifier_;
        ContextWindowExtractor window_extractor_;
        PositionEncoder position_encoder_;
        FeatureNormalizer normalizer_;
        
        // State
        bool use_normalization_;
        mutable std::unordered_map<std::string, ContextFeatures> feature_cache_;
        
        // Helper methods
        std::string generateCacheKey(
            const std::vector<PhonemeTimingInfo>& phonemes,
            size_t index) const;
        PositionEncoder::AccentInfo extractAccentInfo(
            const midi::MidiParser::ParseResult& midi_data,
            const std::vector<PhonemeTimingInfo>& phonemes) const;
        void addMidiFeatures(
            ContextFeatures& features,
            const midi::MusicalPhoneme& musical_phoneme) const;
        void addVCVFeatures(
            ContextFeatures& features,
            const utau::VCVSegment& vcv_segment) const;
    };

    /**
     * @brief Utility functions for context feature processing
     */
    namespace context_utils {
        
        // Conversion utilities
        std::vector<float> concatenateFeatures(const ContextFeatures& features);
        ContextFeatures splitFeatureVector(const std::vector<float>& feature_vector);
        
        // Validation utilities
        bool validateContextFeatures(const ContextFeatures& features);
        std::vector<std::string> validateFeatureBatch(
            const std::vector<ContextFeatures>& features);
        
        // Analysis utilities
        struct FeatureStatistics {
            size_t total_features;
            size_t phoneme_features;
            size_t position_features;
            size_t timing_features;
            double mean_dimension;
            double std_dimension;
            std::vector<std::string> unique_phonemes;
        };
        
        FeatureStatistics analyzeFeatures(const std::vector<ContextFeatures>& features);
        
        // Export utilities
        void exportFeaturesCSV(
            const std::vector<ContextFeatures>& features,
            const std::string& filename);
        void exportFeaturesJSON(
            const std::vector<ContextFeatures>& features,
            const std::string& filename);
        
        // Quality assessment
        double assessFeatureQuality(const std::vector<ContextFeatures>& features);
        std::vector<std::string> identifyFeatureAnomalies(
            const std::vector<ContextFeatures>& features);
        
    } // namespace context_utils

} // namespace context
} // namespace nexussynth
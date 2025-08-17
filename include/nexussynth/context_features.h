#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <Eigen/Core>

namespace nexussynth {
namespace hmm {

    /**
     * @brief Phoneme encoding schemes for context-dependent modeling
     * 
     * Supports both categorical (index-based) and one-hot encoding
     * for efficient neural network and HMM processing
     */
    enum class PhonemeEncoding {
        CATEGORICAL,    // Integer index encoding
        ONE_HOT,       // Binary vector encoding
        HYBRID         // Mixed categorical + continuous features
    };

    /**
     * @brief Phoneme inventory manager for context-dependent modeling
     * 
     * Manages the complete phoneme set and provides encoding/decoding
     * capabilities for different representation schemes
     */
    class PhonemeInventory {
    public:
        PhonemeInventory();
        
        // Phoneme registration and lookup
        int register_phoneme(const std::string& phoneme);
        int get_phoneme_id(const std::string& phoneme) const;
        std::string get_phoneme_name(int id) const;
        
        // Encoding operations
        Eigen::VectorXd encode_one_hot(const std::string& phoneme) const;
        Eigen::VectorXd encode_categorical(const std::string& phoneme) const;
        
        // Inventory properties
        size_t size() const { return phoneme_to_id_.size(); }
        std::vector<std::string> get_all_phonemes() const;
        
        // Default Japanese phoneme set initialization
        void initialize_japanese_phonemes();
        
    private:
        std::unordered_map<std::string, int> phoneme_to_id_;
        std::vector<std::string> id_to_phoneme_;
        int next_id_;
        
        // Special phoneme constants
        static constexpr int SILENCE_ID = 0;
        static constexpr int UNKNOWN_ID = 1;
    };

    /**
     * @brief Musical and prosodic feature normalizer
     * 
     * Handles normalization of continuous features like pitch, duration,
     * and tempo for stable HMM training and synthesis
     */
    class FeatureNormalizer {
    public:
        struct Stats {
            double mean = 0.0;
            double std_dev = 1.0;
            double min_val = 0.0;
            double max_val = 1.0;
        };
        
        FeatureNormalizer();
        
        // Statistical computation
        void compute_stats(const std::vector<double>& values);
        void set_stats(double mean, double std_dev, double min_val, double max_val);
        
        // Normalization operations
        double normalize_z_score(double value) const;
        double normalize_min_max(double value) const;
        double denormalize_z_score(double normalized_value) const;
        double denormalize_min_max(double normalized_value) const;
        
        // Pitch-specific normalization (cents)
        double normalize_pitch_cents(double cents) const;
        double denormalize_pitch_cents(double normalized_cents) const;
        
        const Stats& get_stats() const { return stats_; }
        
    private:
        Stats stats_;
        
        // Default ranges for musical features
        static constexpr double DEFAULT_PITCH_RANGE_CENTS = 4800.0;  // 4 octaves
        static constexpr double DEFAULT_DURATION_MAX_MS = 5000.0;    // 5 seconds
        static constexpr double DEFAULT_TEMPO_MAX_BPM = 300.0;       // Fast tempo
    };

    /**
     * @brief Enhanced context feature vector for HTS-style modeling
     * 
     * Comprehensive linguistic and musical context representation
     * supporting both categorical and continuous feature encoding
     */
    struct ContextFeatureVector {
        // Core phonetic context (quinphone model)
        std::string left_left_phoneme;      // LL context
        std::string left_phoneme;           // L context  
        std::string current_phoneme;        // C context (center)
        std::string right_phoneme;          // R context
        std::string right_right_phoneme;    // RR context
        
        // Syllable-level context
        int position_in_syllable;           // 1-based position
        int syllable_length;                // Total phones in syllable
        int syllables_from_phrase_start;    // Distance from phrase beginning
        int syllables_to_phrase_end;        // Distance to phrase end
        
        // Word-level context
        int position_in_word;               // 1-based position
        int word_length;                    // Total syllables in word
        int words_from_phrase_start;        // Distance from phrase beginning
        int words_to_phrase_end;            // Distance to phrase end
        
        // Phrase-level context
        int phrase_length_syllables;        // Total syllables in phrase
        int phrase_length_words;            // Total words in phrase
        
        // Musical context (UTAU-specific)
        double pitch_cents;                 // Pitch deviation in cents
        double note_duration_ms;            // Note duration in milliseconds
        double tempo_bpm;                   // Tempo in beats per minute
        std::string lyric;                  // Lyrics text
        
        // Timing and rhythm context
        int beat_position;                  // Position within musical beat
        double time_from_phrase_start_ms;   // Timing from phrase start
        double time_to_phrase_end_ms;       // Timing to phrase end
        
        // Prosodic stress and accent
        bool is_stressed;                   // Syllable stress
        bool is_accented;                   // Word accent
        int stress_level;                   // Stress intensity (0-3)
        
        ContextFeatureVector();
        
        // Feature vector generation
        Eigen::VectorXd to_feature_vector(const PhonemeInventory& inventory,
                                         const FeatureNormalizer& normalizer,
                                         PhonemeEncoding encoding = PhonemeEncoding::HYBRID) const;
        
        // Context validation
        bool is_valid() const;
        
        // Human-readable representation
        std::string to_string() const;
        
        // HTS-style label generation
        std::string to_hts_label() const;
        
    private:
        static constexpr int DEFAULT_POSITION = 1;
        static constexpr int DEFAULT_LENGTH = 1;
        static constexpr double DEFAULT_PITCH_CENTS = 0.0;
        static constexpr double DEFAULT_DURATION_MS = 500.0;
        static constexpr double DEFAULT_TEMPO_BPM = 120.0;
    };

    /**
     * @brief Question file format for decision tree clustering
     * 
     * Defines the question set used in HTS-style context clustering
     * for building decision trees over phonetic and prosodic contexts
     */
    class QuestionSet {
    public:
        struct Question {
            std::string name;           // Question identifier
            std::string pattern;        // Pattern to match in context
            std::string description;    // Human-readable description
            
            Question(const std::string& n, const std::string& p, const std::string& d = "")
                : name(n), pattern(p), description(d) {}
        };
        
        QuestionSet();
        
        // Question management
        void add_question(const std::string& name, const std::string& pattern, 
                         const std::string& description = "");
        
        // Standard question sets
        void initialize_phoneme_questions(const PhonemeInventory& inventory);
        void initialize_prosodic_questions();
        void initialize_musical_questions();
        
        // Question evaluation
        bool evaluate_question(const std::string& question_name, 
                              const ContextFeatureVector& context) const;
        
        // Question file I/O (HTS format)
        void save_to_file(const std::string& filename) const;
        void load_from_file(const std::string& filename);
        
        const std::vector<Question>& get_questions() const { return questions_; }
        
    private:
        std::vector<Question> questions_;
        std::unordered_map<std::string, size_t> question_index_;
        
        // Pattern matching helper
        bool matches_pattern(const std::string& pattern, const std::string& context_label) const;
    };

    /**
     * @brief Context feature extraction interface
     * 
     * Main interface for extracting context-dependent features from
     * linguistic and musical input for HMM-based synthesis
     */
    class ContextExtractor {
    public:
        ContextExtractor();
        
        // Component access
        PhonemeInventory& get_phoneme_inventory() { return phoneme_inventory_; }
        FeatureNormalizer& get_feature_normalizer() { return feature_normalizer_; }
        QuestionSet& get_question_set() { return question_set_; }
        
        // Feature extraction
        ContextFeatureVector extract_context(
            const std::vector<std::string>& phoneme_sequence,
            size_t target_index,
            const std::vector<double>& pitch_sequence = {},
            const std::vector<double>& duration_sequence = {},
            const std::vector<std::string>& lyric_sequence = {}
        ) const;
        
        // Batch processing
        std::vector<ContextFeatureVector> extract_sequence_contexts(
            const std::vector<std::string>& phoneme_sequence,
            const std::vector<double>& pitch_sequence = {},
            const std::vector<double>& duration_sequence = {},
            const std::vector<std::string>& lyric_sequence = {}
        ) const;
        
        // Feature vector conversion
        Eigen::MatrixXd contexts_to_matrix(
            const std::vector<ContextFeatureVector>& contexts,
            PhonemeEncoding encoding = PhonemeEncoding::HYBRID
        ) const;
        
        // Initialization
        void initialize_default();
        
    private:
        PhonemeInventory phoneme_inventory_;
        FeatureNormalizer feature_normalizer_;
        QuestionSet question_set_;
        
        // Helper methods
        void extract_syllable_context(ContextFeatureVector& context,
                                     const std::vector<std::string>& phonemes,
                                     size_t target_index) const;
        
        void extract_prosodic_context(ContextFeatureVector& context,
                                     const std::vector<double>& pitch_sequence,
                                     const std::vector<double>& duration_sequence,
                                     size_t target_index) const;
    };

} // namespace hmm
} // namespace nexussynth
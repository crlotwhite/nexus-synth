#include "nexussynth/context_features.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <fstream>
#include <regex>

namespace nexussynth {
namespace hmm {

// PhonemeInventory Implementation
PhonemeInventory::PhonemeInventory() : next_id_(0) {
    // Register special phonemes first
    register_phoneme("sil");     // Silence (ID 0)
    register_phoneme("unk");     // Unknown (ID 1)
}

int PhonemeInventory::register_phoneme(const std::string& phoneme) {
    auto it = phoneme_to_id_.find(phoneme);
    if (it != phoneme_to_id_.end()) {
        return it->second;
    }
    
    int id = next_id_++;
    phoneme_to_id_[phoneme] = id;
    
    if (id >= static_cast<int>(id_to_phoneme_.size())) {
        id_to_phoneme_.resize(id + 1);
    }
    id_to_phoneme_[id] = phoneme;
    
    return id;
}

int PhonemeInventory::get_phoneme_id(const std::string& phoneme) const {
    auto it = phoneme_to_id_.find(phoneme);
    return (it != phoneme_to_id_.end()) ? it->second : UNKNOWN_ID;
}

std::string PhonemeInventory::get_phoneme_name(int id) const {
    if (id >= 0 && id < static_cast<int>(id_to_phoneme_.size())) {
        return id_to_phoneme_[id];
    }
    return "unk";
}

Eigen::VectorXd PhonemeInventory::encode_one_hot(const std::string& phoneme) const {
    int id = get_phoneme_id(phoneme);
    Eigen::VectorXd encoding = Eigen::VectorXd::Zero(size());
    if (id >= 0 && id < static_cast<int>(size())) {
        encoding[id] = 1.0;
    }
    return encoding;
}

Eigen::VectorXd PhonemeInventory::encode_categorical(const std::string& phoneme) const {
    int id = get_phoneme_id(phoneme);
    Eigen::VectorXd encoding(1);
    encoding[0] = static_cast<double>(id);
    return encoding;
}

std::vector<std::string> PhonemeInventory::get_all_phonemes() const {
    return id_to_phoneme_;
}

void PhonemeInventory::initialize_japanese_phonemes() {
    // Japanese vowels
    std::vector<std::string> vowels = {"a", "i", "u", "e", "o"};
    
    // Japanese consonants and phonemes
    std::vector<std::string> consonants = {
        "k", "g", "s", "z", "t", "d", "n", "h", "b", "p", "m", "y", "r", "w"
    };
    
    // Common Japanese syllables
    std::vector<std::string> syllables = {
        "ka", "ki", "ku", "ke", "ko", "ga", "gi", "gu", "ge", "go",
        "sa", "shi", "su", "se", "so", "za", "ji", "zu", "ze", "zo",
        "ta", "chi", "tsu", "te", "to", "da", "di", "du", "de", "do",
        "na", "ni", "nu", "ne", "no", "ha", "hi", "fu", "he", "ho",
        "ba", "bi", "bu", "be", "bo", "pa", "pi", "pu", "pe", "po",
        "ma", "mi", "mu", "me", "mo", "ya", "yu", "yo",
        "ra", "ri", "ru", "re", "ro", "wa", "wi", "we", "wo", "n"
    };
    
    // Register all phonemes
    for (const auto& vowel : vowels) {
        register_phoneme(vowel);
    }
    for (const auto& consonant : consonants) {
        register_phoneme(consonant);
    }
    for (const auto& syllable : syllables) {
        register_phoneme(syllable);
    }
    
    // Special phonemes for UTAU
    register_phoneme("br");     // Breath
    register_phoneme("cl");     // Closure
}

// FeatureNormalizer Implementation
FeatureNormalizer::FeatureNormalizer() {
    // Initialize with default stats
    stats_.mean = 0.0;
    stats_.std_dev = 1.0;
    stats_.min_val = 0.0;
    stats_.max_val = 1.0;
}

void FeatureNormalizer::compute_stats(const std::vector<double>& values) {
    if (values.empty()) return;
    
    // Compute mean
    stats_.mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    
    // Compute standard deviation
    double variance = 0.0;
    for (double value : values) {
        variance += (value - stats_.mean) * (value - stats_.mean);
    }
    stats_.std_dev = std::sqrt(variance / values.size());
    
    // Compute min/max
    auto minmax = std::minmax_element(values.begin(), values.end());
    stats_.min_val = *minmax.first;
    stats_.max_val = *minmax.second;
}

void FeatureNormalizer::set_stats(double mean, double std_dev, double min_val, double max_val) {
    stats_.mean = mean;
    stats_.std_dev = std_dev;
    stats_.min_val = min_val;
    stats_.max_val = max_val;
}

double FeatureNormalizer::normalize_z_score(double value) const {
    if (stats_.std_dev == 0.0) return 0.0;
    return (value - stats_.mean) / stats_.std_dev;
}

double FeatureNormalizer::normalize_min_max(double value) const {
    if (stats_.max_val == stats_.min_val) return 0.0;
    return (value - stats_.min_val) / (stats_.max_val - stats_.min_val);
}

double FeatureNormalizer::denormalize_z_score(double normalized_value) const {
    return normalized_value * stats_.std_dev + stats_.mean;
}

double FeatureNormalizer::denormalize_min_max(double normalized_value) const {
    return normalized_value * (stats_.max_val - stats_.min_val) + stats_.min_val;
}

double FeatureNormalizer::normalize_pitch_cents(double cents) const {
    return cents / DEFAULT_PITCH_RANGE_CENTS;
}

double FeatureNormalizer::denormalize_pitch_cents(double normalized_cents) const {
    return normalized_cents * DEFAULT_PITCH_RANGE_CENTS;
}

// ContextFeatureVector Implementation
ContextFeatureVector::ContextFeatureVector() 
    : position_in_syllable(DEFAULT_POSITION)
    , syllable_length(DEFAULT_LENGTH)
    , syllables_from_phrase_start(DEFAULT_POSITION)
    , syllables_to_phrase_end(DEFAULT_LENGTH)
    , position_in_word(DEFAULT_POSITION)
    , word_length(DEFAULT_LENGTH)
    , words_from_phrase_start(DEFAULT_POSITION)
    , words_to_phrase_end(DEFAULT_LENGTH)
    , phrase_length_syllables(DEFAULT_LENGTH)
    , phrase_length_words(DEFAULT_LENGTH)
    , pitch_cents(DEFAULT_PITCH_CENTS)
    , note_duration_ms(DEFAULT_DURATION_MS)
    , tempo_bpm(DEFAULT_TEMPO_BPM)
    , beat_position(DEFAULT_POSITION)
    , time_from_phrase_start_ms(0.0)
    , time_to_phrase_end_ms(DEFAULT_DURATION_MS)
    , is_stressed(false)
    , is_accented(false)
    , stress_level(0) {
}

Eigen::VectorXd ContextFeatureVector::to_feature_vector(
    const PhonemeInventory& inventory,
    const FeatureNormalizer& normalizer,
    PhonemeEncoding encoding) const {
    
    std::vector<double> features;
    
    if (encoding == PhonemeEncoding::ONE_HOT) {
        // One-hot encoding for all phonemes
        auto ll_enc = inventory.encode_one_hot(left_left_phoneme);
        auto l_enc = inventory.encode_one_hot(left_phoneme);
        auto c_enc = inventory.encode_one_hot(current_phoneme);
        auto r_enc = inventory.encode_one_hot(right_phoneme);
        auto rr_enc = inventory.encode_one_hot(right_right_phoneme);
        
        // Append all one-hot vectors
        for (int i = 0; i < ll_enc.size(); ++i) features.push_back(ll_enc[i]);
        for (int i = 0; i < l_enc.size(); ++i) features.push_back(l_enc[i]);
        for (int i = 0; i < c_enc.size(); ++i) features.push_back(c_enc[i]);
        for (int i = 0; i < r_enc.size(); ++i) features.push_back(r_enc[i]);
        for (int i = 0; i < rr_enc.size(); ++i) features.push_back(rr_enc[i]);
        
    } else if (encoding == PhonemeEncoding::CATEGORICAL) {
        // Categorical encoding (integer IDs)
        features.push_back(static_cast<double>(inventory.get_phoneme_id(left_left_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(left_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(current_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(right_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(right_right_phoneme)));
        
    } else { // HYBRID
        // Categorical phoneme IDs + continuous features
        features.push_back(static_cast<double>(inventory.get_phoneme_id(left_left_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(left_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(current_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(right_phoneme)));
        features.push_back(static_cast<double>(inventory.get_phoneme_id(right_right_phoneme)));
    }
    
    // Add continuous features (normalized)
    features.push_back(static_cast<double>(position_in_syllable));
    features.push_back(static_cast<double>(syllable_length));
    features.push_back(static_cast<double>(position_in_word));
    features.push_back(static_cast<double>(word_length));
    features.push_back(normalizer.normalize_pitch_cents(pitch_cents));
    features.push_back(normalizer.normalize_min_max(note_duration_ms));
    features.push_back(normalizer.normalize_min_max(tempo_bpm));
    features.push_back(static_cast<double>(beat_position));
    features.push_back(is_stressed ? 1.0 : 0.0);
    features.push_back(is_accented ? 1.0 : 0.0);
    features.push_back(static_cast<double>(stress_level) / 3.0);  // Normalize 0-3 to 0-1
    
    // Convert to Eigen vector
    Eigen::VectorXd result(features.size());
    for (size_t i = 0; i < features.size(); ++i) {
        result[i] = features[i];
    }
    
    return result;
}

bool ContextFeatureVector::is_valid() const {
    return !current_phoneme.empty() && 
           position_in_syllable > 0 && 
           syllable_length > 0 &&
           position_in_word > 0 && 
           word_length > 0;
}

std::string ContextFeatureVector::to_string() const {
    std::ostringstream oss;
    oss << "Context[" << left_left_phoneme << "-" << left_phoneme << "-" 
        << current_phoneme << "+" << right_phoneme << "+" << right_right_phoneme 
        << " | syl:" << position_in_syllable << "/" << syllable_length
        << " | word:" << position_in_word << "/" << word_length 
        << " | pitch:" << pitch_cents << "c"
        << " | dur:" << note_duration_ms << "ms]";
    return oss.str();
}

std::string ContextFeatureVector::to_hts_label() const {
    std::ostringstream oss;
    oss << "/A:" << syllables_from_phrase_start << "_" << syllables_to_phrase_end
        << "/B:" << position_in_syllable << "_" << syllable_length
        << "/C:" << left_left_phoneme << "-" << left_phoneme << "+" << current_phoneme 
        << "++" << right_phoneme << "+" << right_right_phoneme
        << "/D:" << position_in_word << "_" << word_length
        << "/E:" << words_from_phrase_start << "_" << words_to_phrase_end
        << "/F:" << phrase_length_syllables << "_" << phrase_length_words
        << "/G:" << static_cast<int>(pitch_cents) << "_" << static_cast<int>(note_duration_ms)
        << "/H:" << static_cast<int>(tempo_bpm) << "_" << beat_position
        << "/I:" << (is_stressed ? 1 : 0) << "_" << (is_accented ? 1 : 0) << "_" << stress_level;
    return oss.str();
}

// QuestionSet Implementation
QuestionSet::QuestionSet() {
}

void QuestionSet::add_question(const std::string& name, const std::string& pattern, 
                              const std::string& description) {
    questions_.emplace_back(name, pattern, description);
    question_index_[name] = questions_.size() - 1;
}

void QuestionSet::initialize_phoneme_questions(const PhonemeInventory& inventory) {
    auto phonemes = inventory.get_all_phonemes();
    
    // Generate quinphone questions for each phoneme
    for (const auto& phoneme : phonemes) {
        if (phoneme.empty()) continue;
        
        // Left-left context questions
        add_question("LL-" + phoneme, "*-" + phoneme + "-*", 
                    "Left-left phoneme is " + phoneme);
        
        // Left context questions  
        add_question("L-" + phoneme, "*+" + phoneme + "+*",
                    "Left phoneme is " + phoneme);
        
        // Center phoneme questions
        add_question("C-" + phoneme, "*+" + phoneme + "+*",
                    "Center phoneme is " + phoneme);
        
        // Right context questions
        add_question("R-" + phoneme, "*+" + phoneme + "+*",
                    "Right phoneme is " + phoneme);
        
        // Right-right context questions
        add_question("RR-" + phoneme, "*+" + phoneme + "-*",
                    "Right-right phoneme is " + phoneme);
    }
}

void QuestionSet::initialize_prosodic_questions() {
    // Position in syllable questions
    for (int pos = 1; pos <= 5; ++pos) {
        add_question("Syl_Pos_" + std::to_string(pos), "/B:" + std::to_string(pos) + "_*",
                    "Position in syllable is " + std::to_string(pos));
    }
    
    // Syllable length questions
    for (int len = 1; len <= 8; ++len) {
        add_question("Syl_Len_" + std::to_string(len), "/B:*_" + std::to_string(len),
                    "Syllable length is " + std::to_string(len));
    }
    
    // Stress and accent questions
    add_question("Stressed", "/I:1_*", "Syllable is stressed");
    add_question("Accented", "/I:*_1_*", "Word is accented");
}

void QuestionSet::initialize_musical_questions() {
    // Pitch range questions (in cents)
    std::vector<std::pair<int, int>> pitch_ranges = {
        {-1200, -600}, {-600, -300}, {-300, -100}, {-100, 100},
        {100, 300}, {300, 600}, {600, 1200}, {1200, 2400}
    };
    
    for (const auto& range : pitch_ranges) {
        std::string name = "Pitch_" + std::to_string(range.first) + "_" + std::to_string(range.second);
        add_question(name, "/G:*_*", "Pitch in range " + std::to_string(range.first) + 
                    " to " + std::to_string(range.second) + " cents");
    }
    
    // Duration questions (in ms)
    std::vector<std::pair<int, int>> duration_ranges = {
        {0, 200}, {200, 400}, {400, 600}, {600, 1000}, {1000, 2000}, {2000, 5000}
    };
    
    for (const auto& range : duration_ranges) {
        std::string name = "Dur_" + std::to_string(range.first) + "_" + std::to_string(range.second);
        add_question(name, "/G:*_*", "Duration in range " + std::to_string(range.first) + 
                    " to " + std::to_string(range.second) + " ms");
    }
}

bool QuestionSet::evaluate_question(const std::string& question_name, 
                                   const ContextFeatureVector& context) const {
    auto it = question_index_.find(question_name);
    if (it == question_index_.end()) return false;
    
    const auto& question = questions_[it->second];
    std::string hts_label = context.to_hts_label();
    
    return matches_pattern(question.pattern, hts_label);
}

bool QuestionSet::matches_pattern(const std::string& pattern, const std::string& context_label) const {
    // Simple pattern matching - could be enhanced with regex
    return context_label.find(pattern) != std::string::npos;
}

void QuestionSet::save_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    for (const auto& question : questions_) {
        file << "QS \"" << question.name << "\" { " << question.pattern << " }\n";
    }
    
    file.close();
}

// ContextExtractor Implementation
ContextExtractor::ContextExtractor() {
    initialize_default();
}

void ContextExtractor::initialize_default() {
    phoneme_inventory_.initialize_japanese_phonemes();
    question_set_.initialize_phoneme_questions(phoneme_inventory_);
    question_set_.initialize_prosodic_questions();
    question_set_.initialize_musical_questions();
}

ContextFeatureVector ContextExtractor::extract_context(
    const std::vector<std::string>& phoneme_sequence,
    size_t target_index,
    const std::vector<double>& pitch_sequence,
    const std::vector<double>& duration_sequence,
    const std::vector<std::string>& lyric_sequence) const {
    
    ContextFeatureVector context;
    
    if (target_index >= phoneme_sequence.size()) {
        return context;
    }
    
    // Extract quinphone context
    context.current_phoneme = phoneme_sequence[target_index];
    context.left_phoneme = (target_index > 0) ? phoneme_sequence[target_index - 1] : "sil";
    context.right_phoneme = (target_index + 1 < phoneme_sequence.size()) ? 
                           phoneme_sequence[target_index + 1] : "sil";
    context.left_left_phoneme = (target_index > 1) ? phoneme_sequence[target_index - 2] : "sil";
    context.right_right_phoneme = (target_index + 2 < phoneme_sequence.size()) ? 
                                 phoneme_sequence[target_index + 2] : "sil";
    
    // Extract syllable and word context
    extract_syllable_context(context, phoneme_sequence, target_index);
    
    // Extract prosodic context
    extract_prosodic_context(context, pitch_sequence, duration_sequence, target_index);
    
    // Extract lyric if available
    if (!lyric_sequence.empty() && target_index < lyric_sequence.size()) {
        context.lyric = lyric_sequence[target_index];
    }
    
    return context;
}

void ContextExtractor::extract_syllable_context(ContextFeatureVector& context,
                                               const std::vector<std::string>& phonemes,
                                               size_t target_index) const {
    // Simplified syllable boundary detection
    // In a real implementation, this would use linguistic rules
    context.position_in_syllable = 1;
    context.syllable_length = 1;
    context.position_in_word = 1;
    context.word_length = 1;
    context.syllables_from_phrase_start = static_cast<int>(target_index + 1);
    context.syllables_to_phrase_end = static_cast<int>(phonemes.size() - target_index);
    context.words_from_phrase_start = 1;
    context.words_to_phrase_end = 1;
    context.phrase_length_syllables = static_cast<int>(phonemes.size());
    context.phrase_length_words = 1;
}

void ContextExtractor::extract_prosodic_context(ContextFeatureVector& context,
                                               const std::vector<double>& pitch_sequence,
                                               const std::vector<double>& duration_sequence,
                                               size_t target_index) const {
    if (!pitch_sequence.empty() && target_index < pitch_sequence.size()) {
        context.pitch_cents = pitch_sequence[target_index];
    }
    
    if (!duration_sequence.empty() && target_index < duration_sequence.size()) {
        context.note_duration_ms = duration_sequence[target_index];
    }
}

} // namespace hmm
} // namespace nexussynth
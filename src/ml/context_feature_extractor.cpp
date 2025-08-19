#include "nexussynth/context_feature_extractor.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <numeric>

namespace nexussynth {
namespace context {

    // PhonemeFeatures Implementation
    PhonemeFeatures::PhonemeFeatures() {
        // Initialize all features to false
        std::memset(this, 0, sizeof(PhonemeFeatures));
    }

    std::vector<float> PhonemeFeatures::toBinaryVector() const {
        std::vector<float> features;
        features.reserve(FEATURE_SIZE);
        
        // Phoneme type (8 bits)
        features.push_back(is_vowel ? 1.0f : 0.0f);
        features.push_back(is_consonant ? 1.0f : 0.0f);
        features.push_back(is_silence ? 1.0f : 0.0f);
        features.push_back(is_long_vowel ? 1.0f : 0.0f);
        features.push_back(is_nasal ? 1.0f : 0.0f);
        features.push_back(is_fricative ? 1.0f : 0.0f);
        features.push_back(is_plosive ? 1.0f : 0.0f);
        features.push_back(is_semivowel ? 1.0f : 0.0f);
        
        // Place of articulation (6 bits)
        features.push_back(place_bilabial ? 1.0f : 0.0f);
        features.push_back(place_alveolar ? 1.0f : 0.0f);
        features.push_back(place_palatal ? 1.0f : 0.0f);
        features.push_back(place_velar ? 1.0f : 0.0f);
        features.push_back(place_glottal ? 1.0f : 0.0f);
        features.push_back(place_dental ? 1.0f : 0.0f);
        
        // Manner of articulation (8 bits)
        features.push_back(manner_stop ? 1.0f : 0.0f);
        features.push_back(manner_fricative ? 1.0f : 0.0f);
        features.push_back(manner_nasal ? 1.0f : 0.0f);
        features.push_back(manner_liquid ? 1.0f : 0.0f);
        features.push_back(manner_glide ? 1.0f : 0.0f);
        features.push_back(voiced ? 1.0f : 0.0f);
        features.push_back(aspirated ? 1.0f : 0.0f);
        features.push_back(palatalized ? 1.0f : 0.0f);
        
        // Vowel characteristics (10 bits)
        features.push_back(vowel_front ? 1.0f : 0.0f);
        features.push_back(vowel_central ? 1.0f : 0.0f);
        features.push_back(vowel_back ? 1.0f : 0.0f);
        features.push_back(vowel_high ? 1.0f : 0.0f);
        features.push_back(vowel_mid ? 1.0f : 0.0f);
        features.push_back(vowel_low ? 1.0f : 0.0f);
        features.push_back(vowel_rounded ? 1.0f : 0.0f);
        features.push_back(vowel_unrounded ? 1.0f : 0.0f);
        features.push_back(vowel_long ? 1.0f : 0.0f);
        features.push_back(vowel_nasalized ? 1.0f : 0.0f);
        
        return features;
    }

    // PositionEncoding Implementation
    PositionEncoding::PositionEncoding() 
        : position_in_syllable(0.0f), position_in_mora(0.0f), position_in_word(0.0f)
        , position_in_phrase(0.0f), position_in_utterance(0.0f)
        , is_syllable_initial(false), is_syllable_final(false)
        , is_word_initial(false), is_word_final(false)
        , is_phrase_initial(false), is_phrase_final(false)
        , accent_strength(0.0f), has_accent(false), accent_position(-1)
        , is_major_phrase_boundary(false), is_minor_phrase_boundary(false) {
    }

    std::vector<float> PositionEncoding::toVector() const {
        std::vector<float> encoding;
        encoding.reserve(ENCODING_SIZE);
        
        // Position values (5 floats)
        encoding.push_back(position_in_syllable);
        encoding.push_back(position_in_mora);
        encoding.push_back(position_in_word);
        encoding.push_back(position_in_phrase);
        encoding.push_back(position_in_utterance);
        
        // Boundary flags (6 bools)
        encoding.push_back(is_syllable_initial ? 1.0f : 0.0f);
        encoding.push_back(is_syllable_final ? 1.0f : 0.0f);
        encoding.push_back(is_word_initial ? 1.0f : 0.0f);
        encoding.push_back(is_word_final ? 1.0f : 0.0f);
        encoding.push_back(is_phrase_initial ? 1.0f : 0.0f);
        encoding.push_back(is_phrase_final ? 1.0f : 0.0f);
        
        // Accent and prosodic features (5 values)
        encoding.push_back(accent_strength);
        encoding.push_back(has_accent ? 1.0f : 0.0f);
        encoding.push_back(static_cast<float>(accent_position) / 10.0f); // Normalized
        encoding.push_back(is_major_phrase_boundary ? 1.0f : 0.0f);
        encoding.push_back(is_minor_phrase_boundary ? 1.0f : 0.0f);
        
        return encoding;
    }

    // PhonemeTimingInfo Implementation
    PhonemeTimingInfo::PhonemeTimingInfo()
        : start_time_ms(0.0), duration_ms(0.0), end_time_ms(0.0)
        , consonant_start_ms(0.0), consonant_end_ms(0.0), transition_duration_ms(0.0)
        , timing_confidence(1.0), is_valid(true) {
    }

    // ContextFeatures Implementation
    ContextFeatures::ContextFeatures()
        : frame_time_ms(0.0), frame_index(0) {
        phoneme_context.resize(CONTEXT_WINDOW_SIZE);
        position_context.resize(CONTEXT_WINDOW_SIZE);
    }

    Eigen::VectorXd ContextFeatures::toFeatureVector() const {
        std::vector<float> all_features;
        
        // Add phoneme context features
        for (const auto& phoneme_feat : phoneme_context) {
            auto phoneme_vec = phoneme_feat.toBinaryVector();
            all_features.insert(all_features.end(), phoneme_vec.begin(), phoneme_vec.end());
        }
        
        // Add position context features
        for (const auto& pos_feat : position_context) {
            auto pos_vec = pos_feat.toVector();
            all_features.insert(all_features.end(), pos_vec.begin(), pos_vec.end());
        }
        
        // Add timing features
        all_features.push_back(static_cast<float>(current_timing.duration_ms));
        all_features.push_back(static_cast<float>(current_timing.timing_confidence));
        all_features.push_back(current_timing.is_valid ? 1.0f : 0.0f);
        
        // Add MIDI features if available
        if (current_midi.note_number > 0) {
            all_features.push_back(static_cast<float>(current_midi.note_number) / 127.0f);
            all_features.push_back(static_cast<float>(current_midi.velocity) / 127.0f);
            all_features.push_back(static_cast<float>(current_midi.frequency_hz) / 1000.0f);
        } else {
            all_features.push_back(0.0f);
            all_features.push_back(0.0f);
            all_features.push_back(0.0f);
        }
        
        // Add VCV features if available
        if (current_vcv.is_valid) {
            all_features.push_back(static_cast<float>(current_vcv.boundary_confidence));
            all_features.push_back(current_vcv.vowel1.empty() ? 0.0f : 1.0f);
            all_features.push_back(current_vcv.consonant.empty() ? 0.0f : 1.0f);
            all_features.push_back(current_vcv.vowel2.empty() ? 0.0f : 1.0f);
        } else {
            all_features.push_back(0.0f);
            all_features.push_back(0.0f);
            all_features.push_back(0.0f);
            all_features.push_back(0.0f);
        }
        
        // Convert to Eigen vector
        Eigen::VectorXd feature_vector(all_features.size());
        for (size_t i = 0; i < all_features.size(); ++i) {
            feature_vector[i] = static_cast<double>(all_features[i]);
        }
        
        return feature_vector;
    }

    size_t ContextFeatures::getTotalDimension() {
        return CONTEXT_WINDOW_SIZE * (PhonemeFeatures::FEATURE_SIZE + PositionEncoding::ENCODING_SIZE) 
               + 3 + 3 + 4; // timing + MIDI + VCV features
    }

    // JapanesePhonemeClassifier Implementation
    JapanesePhonemeClassifier::JapanesePhonemeClassifier() {
        initializePhonemeFeatures();
    }

    void JapanesePhonemeClassifier::initializePhonemeFeatures() {
        initializeVowelFeatures();
        initializeConsonantFeatures();
        initializeSpecialPhonemes();
    }

    void JapanesePhonemeClassifier::initializeVowelFeatures() {
        // Japanese vowels: /a/, /i/, /u/, /e/, /o/
        
        // /a/ - low, central, unrounded
        PhonemeFeatures a_features;
        a_features.is_vowel = true;
        a_features.vowel_central = true;
        a_features.vowel_low = true;
        a_features.vowel_unrounded = true;
        phoneme_features_["a"] = a_features;
        
        // /i/ - high, front, unrounded
        PhonemeFeatures i_features;
        i_features.is_vowel = true;
        i_features.vowel_front = true;
        i_features.vowel_high = true;
        i_features.vowel_unrounded = true;
        phoneme_features_["i"] = i_features;
        
        // /u/ - high, back, rounded (slightly)
        PhonemeFeatures u_features;
        u_features.is_vowel = true;
        u_features.vowel_back = true;
        u_features.vowel_high = true;
        u_features.vowel_rounded = true;
        phoneme_features_["u"] = u_features;
        
        // /e/ - mid, front, unrounded
        PhonemeFeatures e_features;
        e_features.is_vowel = true;
        e_features.vowel_front = true;
        e_features.vowel_mid = true;
        e_features.vowel_unrounded = true;
        phoneme_features_["e"] = e_features;
        
        // /o/ - mid, back, rounded
        PhonemeFeatures o_features;
        o_features.is_vowel = true;
        o_features.vowel_back = true;
        o_features.vowel_mid = true;
        o_features.vowel_rounded = true;
        phoneme_features_["o"] = o_features;
        
        // Long vowels
        auto a_long = a_features;
        a_long.is_long_vowel = a_long.vowel_long = true;
        phoneme_features_["aa"] = phoneme_features_["a:"] = a_long;
        
        auto i_long = i_features;
        i_long.is_long_vowel = i_long.vowel_long = true;
        phoneme_features_["ii"] = phoneme_features_["i:"] = i_long;
        
        auto u_long = u_features;
        u_long.is_long_vowel = u_long.vowel_long = true;
        phoneme_features_["uu"] = phoneme_features_["u:"] = u_long;
        
        auto e_long = e_features;
        e_long.is_long_vowel = e_long.vowel_long = true;
        phoneme_features_["ee"] = phoneme_features_["e:"] = e_long;
        
        auto o_long = o_features;
        o_long.is_long_vowel = o_long.vowel_long = true;
        phoneme_features_["oo"] = phoneme_features_["o:"] = o_long;
    }

    void JapanesePhonemeClassifier::initializeConsonantFeatures() {
        // Plosives
        PhonemeFeatures p_feat;
        p_feat.is_consonant = p_feat.is_plosive = p_feat.manner_stop = true;
        p_feat.place_bilabial = true;
        phoneme_features_["p"] = p_feat;
        
        auto b_feat = p_feat;
        b_feat.voiced = true;
        phoneme_features_["b"] = b_feat;
        
        PhonemeFeatures t_feat;
        t_feat.is_consonant = t_feat.is_plosive = t_feat.manner_stop = true;
        t_feat.place_alveolar = true;
        phoneme_features_["t"] = t_feat;
        
        auto d_feat = t_feat;
        d_feat.voiced = true;
        phoneme_features_["d"] = d_feat;
        
        PhonemeFeatures k_feat;
        k_feat.is_consonant = k_feat.is_plosive = k_feat.manner_stop = true;
        k_feat.place_velar = true;
        phoneme_features_["k"] = k_feat;
        
        auto g_feat = k_feat;
        g_feat.voiced = true;
        phoneme_features_["g"] = g_feat;
        
        // Fricatives
        PhonemeFeatures s_feat;
        s_feat.is_consonant = s_feat.is_fricative = s_feat.manner_fricative = true;
        s_feat.place_alveolar = true;
        phoneme_features_["s"] = s_feat;
        
        auto z_feat = s_feat;
        z_feat.voiced = true;
        phoneme_features_["z"] = z_feat;
        
        PhonemeFeatures sh_feat;
        sh_feat.is_consonant = sh_feat.is_fricative = sh_feat.manner_fricative = true;
        sh_feat.place_palatal = sh_feat.palatalized = true;
        phoneme_features_["sh"] = phoneme_features_["ʃ"] = sh_feat;
        
        auto zh_feat = sh_feat;
        zh_feat.voiced = true;
        phoneme_features_["zh"] = phoneme_features_["ʒ"] = zh_feat;
        
        PhonemeFeatures h_feat;
        h_feat.is_consonant = h_feat.is_fricative = h_feat.manner_fricative = true;
        h_feat.place_glottal = true;
        phoneme_features_["h"] = h_feat;
        
        // Nasals
        PhonemeFeatures m_feat;
        m_feat.is_consonant = m_feat.is_nasal = m_feat.manner_nasal = true;
        m_feat.place_bilabial = m_feat.voiced = true;
        phoneme_features_["m"] = m_feat;
        
        PhonemeFeatures n_feat;
        n_feat.is_consonant = n_feat.is_nasal = n_feat.manner_nasal = true;
        n_feat.place_alveolar = n_feat.voiced = true;
        phoneme_features_["n"] = n_feat;
        
        PhonemeFeatures ng_feat;
        ng_feat.is_consonant = ng_feat.is_nasal = ng_feat.manner_nasal = true;
        ng_feat.place_velar = ng_feat.voiced = true;
        phoneme_features_["ng"] = phoneme_features_["ŋ"] = ng_feat;
        
        // Liquids
        PhonemeFeatures r_feat;
        r_feat.is_consonant = r_feat.manner_liquid = true;
        r_feat.place_alveolar = r_feat.voiced = true;
        phoneme_features_["r"] = r_feat;
        
        // Semivowels
        PhonemeFeatures y_feat;
        y_feat.is_consonant = y_feat.is_semivowel = y_feat.manner_glide = true;
        y_feat.place_palatal = y_feat.voiced = true;
        phoneme_features_["y"] = y_feat;
        
        PhonemeFeatures w_feat;
        w_feat.is_consonant = w_feat.is_semivowel = w_feat.manner_glide = true;
        w_feat.place_velar = w_feat.voiced = true;
        phoneme_features_["w"] = w_feat;
        
        // Affricates
        PhonemeFeatures ts_feat;
        ts_feat.is_consonant = ts_feat.is_plosive = true;
        ts_feat.place_alveolar = true;
        phoneme_features_["ts"] = ts_feat;
        
        auto dz_feat = ts_feat;
        dz_feat.voiced = true;
        phoneme_features_["dz"] = dz_feat;
        
        PhonemeFeatures ch_feat;
        ch_feat.is_consonant = ch_feat.is_plosive = true;
        ch_feat.place_palatal = ch_feat.palatalized = true;
        phoneme_features_["ch"] = phoneme_features_["tʃ"] = ch_feat;
        
        auto j_feat = ch_feat;
        j_feat.voiced = true;
        phoneme_features_["j"] = phoneme_features_["dʒ"] = j_feat;
    }

    void JapanesePhonemeClassifier::initializeSpecialPhonemes() {
        // Silence/pause
        PhonemeFeatures sil_feat;
        sil_feat.is_silence = true;
        phoneme_features_["sil"] = phoneme_features_["<SIL>"] = phoneme_features_["pau"] = sil_feat;
        
        // Special Japanese phonemes
        PhonemeFeatures Q_feat; // Geminate (っ)
        Q_feat.is_consonant = Q_feat.is_plosive = true;
        phoneme_features_["Q"] = phoneme_features_["っ"] = Q_feat;
        
        PhonemeFeatures N_feat; // Syllabic nasal (ん)
        N_feat.is_consonant = N_feat.is_nasal = N_feat.manner_nasal = true;
        N_feat.voiced = true;
        phoneme_features_["N"] = phoneme_features_["ん"] = N_feat;
    }

    PhonemeFeatures JapanesePhonemeClassifier::classifyPhoneme(const std::string& phoneme) const {
        auto it = phoneme_features_.find(phoneme);
        if (it != phoneme_features_.end()) {
            return it->second;
        }
        
        // Try lowercase
        std::string lower_phoneme = phoneme;
        std::transform(lower_phoneme.begin(), lower_phoneme.end(), lower_phoneme.begin(), ::tolower);
        it = phoneme_features_.find(lower_phoneme);
        if (it != phoneme_features_.end()) {
            return it->second;
        }
        
        // Return default (silence) if not found
        PhonemeFeatures default_feat;
        default_feat.is_silence = true;
        return default_feat;
    }

    bool JapanesePhonemeClassifier::isJapaneseVowel(const std::string& phoneme) const {
        auto features = classifyPhoneme(phoneme);
        return features.is_vowel;
    }

    bool JapanesePhonemeClassifier::isJapaneseConsonant(const std::string& phoneme) const {
        auto features = classifyPhoneme(phoneme);
        return features.is_consonant;
    }

    bool JapanesePhonemeClassifier::isValidJapanesePhoneme(const std::string& phoneme) const {
        auto features = classifyPhoneme(phoneme);
        return features.is_vowel || features.is_consonant || features.is_silence;
    }

    std::string JapanesePhonemeClassifier::getPhonemeCategory(const std::string& phoneme) const {
        auto features = classifyPhoneme(phoneme);
        
        if (features.is_silence) return "silence";
        if (features.is_vowel) {
            if (features.is_long_vowel) return "long_vowel";
            return "vowel";
        }
        if (features.is_consonant) {
            if (features.is_nasal) return "nasal";
            if (features.is_plosive) return "plosive";
            if (features.is_fricative) return "fricative";
            if (features.is_semivowel) return "semivowel";
            return "consonant";
        }
        
        return "unknown";
    }

    double JapanesePhonemeClassifier::calculatePhonemeDistance(
        const std::string& phoneme1, 
        const std::string& phoneme2) const {
        
        auto feat1 = classifyPhoneme(phoneme1);
        auto feat2 = classifyPhoneme(phoneme2);
        
        auto vec1 = feat1.toBinaryVector();
        auto vec2 = feat2.toBinaryVector();
        
        double distance = 0.0;
        for (size_t i = 0; i < vec1.size() && i < vec2.size(); ++i) {
            double diff = vec1[i] - vec2[i];
            distance += diff * diff;
        }
        
        return std::sqrt(distance);
    }

    std::vector<std::string> JapanesePhonemeClassifier::findSimilarPhonemes(
        const std::string& phoneme, 
        double threshold) const {
        
        std::vector<std::string> similar_phonemes;
        
        for (const auto& [ph, features] : phoneme_features_) {
            double distance = calculatePhonemeDistance(phoneme, ph);
            if (distance <= threshold && ph != phoneme) {
                similar_phonemes.push_back(ph);
            }
        }
        
        // Sort by distance
        std::sort(similar_phonemes.begin(), similar_phonemes.end(),
            [this, &phoneme](const std::string& a, const std::string& b) {
                return calculatePhonemeDistance(phoneme, a) < calculatePhonemeDistance(phoneme, b);
            });
        
        return similar_phonemes;
    }

    // ContextWindowExtractor Implementation
    ContextWindowExtractor::ContextWindowExtractor() {
        // Use default config
    }

    ContextWindowExtractor::ContextWindowExtractor(const WindowConfig& config) : config_(config) {
    }

    std::vector<PhonemeFeatures> ContextWindowExtractor::extractPhonemeContext(
        const std::vector<PhonemeTimingInfo>& phonemes,
        size_t current_index) const {
        
        std::vector<PhonemeFeatures> context_features;
        auto indices = getContextIndices(current_index, phonemes.size(), config_.phoneme_window);
        
        for (size_t idx : indices) {
            if (idx == SIZE_MAX) { // Padding index
                context_features.push_back(getPaddingFeatures());
            } else {
                context_features.push_back(classifier_.classifyPhoneme(phonemes[idx].phoneme));
            }
        }
        
        return context_features;
    }

    std::vector<PositionEncoding> ContextWindowExtractor::extractPositionContext(
        const std::vector<PhonemeTimingInfo>& phonemes,
        size_t current_index) const {
        
        std::vector<PositionEncoding> position_context;
        auto indices = getContextIndices(current_index, phonemes.size(), config_.phoneme_window);
        
        PositionEncoder encoder;
        
        for (size_t idx : indices) {
            if (idx == SIZE_MAX) { // Padding index
                position_context.push_back(getPaddingPosition());
            } else {
                position_context.push_back(encoder.encodePosition(phonemes, idx));
            }
        }
        
        return position_context;
    }

    std::vector<size_t> ContextWindowExtractor::getContextIndices(
        size_t current_index, 
        size_t sequence_length, 
        size_t window_size) const {
        
        std::vector<size_t> indices;
        
        for (int offset = -static_cast<int>(window_size); 
             offset <= static_cast<int>(window_size); 
             ++offset) {
            
            int target_idx = static_cast<int>(current_index) + offset;
            
            if (target_idx < 0 || target_idx >= static_cast<int>(sequence_length)) {
                if (config_.enable_padding) {
                    indices.push_back(SIZE_MAX); // Special padding index
                }
            } else {
                indices.push_back(static_cast<size_t>(target_idx));
            }
        }
        
        return indices;
    }

    PhonemeFeatures ContextWindowExtractor::getPaddingFeatures() const {
        return classifier_.classifyPhoneme(config_.padding_symbol);
    }

    PositionEncoding ContextWindowExtractor::getPaddingPosition() const {
        return PositionEncoding(); // Default-initialized (all zeros)
    }

    // PositionEncoder Implementation
    PositionEncoder::PositionEncoder() {
    }

    PositionEncoding PositionEncoder::encodePosition(
        const std::vector<PhonemeTimingInfo>& phonemes,
        size_t phoneme_index,
        const AccentInfo& accent_info) const {
        
        PositionEncoding encoding;
        
        if (phoneme_index >= phonemes.size()) {
            return encoding; // Return default-initialized encoding
        }
        
        // Extract syllables and mora
        auto syllables = extractSyllables(phonemes);
        auto mora = extractMora(phonemes);
        
        // Find which syllable and mora this phoneme belongs to
        size_t syllable_idx = 0, mora_idx = 0;
        for (size_t i = 0; i < syllables.size(); ++i) {
            auto& syllable = syllables[i];
            if (std::find(syllable.begin(), syllable.end(), phoneme_index) != syllable.end()) {
                syllable_idx = i;
                break;
            }
        }
        
        for (size_t i = 0; i < mora.size(); ++i) {
            auto& m = mora[i];
            if (std::find(m.begin(), m.end(), phoneme_index) != m.end()) {
                mora_idx = i;
                break;
            }
        }
        
        // Calculate positions within units
        if (syllable_idx < syllables.size()) {
            auto& syllable = syllables[syllable_idx];
            auto pos_in_syl = std::find(syllable.begin(), syllable.end(), phoneme_index);
            if (pos_in_syl != syllable.end()) {
                size_t position = std::distance(syllable.begin(), pos_in_syl);
                encoding.position_in_syllable = static_cast<float>(position) / syllable.size();
                encoding.is_syllable_initial = (position == 0);
                encoding.is_syllable_final = (position == syllable.size() - 1);
            }
        }
        
        if (mora_idx < mora.size()) {
            auto& m = mora[mora_idx];
            auto pos_in_mora = std::find(m.begin(), m.end(), phoneme_index);
            if (pos_in_mora != m.end()) {
                size_t position = std::distance(m.begin(), pos_in_mora);
                encoding.position_in_mora = static_cast<float>(position) / m.size();
            }
        }
        
        // Word and phrase positions (simplified - would need more linguistic analysis)
        encoding.position_in_word = static_cast<float>(syllable_idx) / syllables.size();
        encoding.position_in_phrase = encoding.position_in_word; // Simplified
        encoding.position_in_utterance = static_cast<float>(phoneme_index) / phonemes.size();
        
        // Accent information
        encoding.accent_strength = calculateAccentStrength(accent_info, mora_idx);
        encoding.has_accent = (accent_info.accent_position == static_cast<int>(mora_idx));
        encoding.accent_position = accent_info.accent_position;
        
        // Boundary detection (simplified)
        if (syllable_idx == 0) encoding.is_word_initial = true;
        if (syllable_idx == syllables.size() - 1) encoding.is_word_final = true;
        
        return encoding;
    }

    std::vector<std::vector<size_t>> PositionEncoder::extractSyllables(
        const std::vector<PhonemeTimingInfo>& phonemes) const {
        
        std::vector<std::vector<size_t>> syllables;
        std::vector<size_t> current_syllable;
        
        for (size_t i = 0; i < phonemes.size(); ++i) {
            current_syllable.push_back(i);
            
            // Simple syllable boundary detection - break after vowels
            JapanesePhonemeClassifier classifier;
            if (classifier.isJapaneseVowel(phonemes[i].phoneme)) {
                if (!current_syllable.empty()) {
                    syllables.push_back(current_syllable);
                    current_syllable.clear();
                }
            }
        }
        
        // Add remaining phonemes
        if (!current_syllable.empty()) {
            syllables.push_back(current_syllable);
        }
        
        return syllables;
    }

    std::vector<std::vector<size_t>> PositionEncoder::extractMora(
        const std::vector<PhonemeTimingInfo>& phonemes) const {
        
        // For Japanese, mora are similar to syllables but with special handling
        // This is a simplified implementation
        return extractSyllables(phonemes);
    }

    float PositionEncoder::calculateRelativePosition(size_t index, size_t start, size_t end) const {
        if (end <= start) return 0.0f;
        if (index < start) return 0.0f;
        if (index >= end) return 1.0f;
        return static_cast<float>(index - start) / static_cast<float>(end - start);
    }

    float PositionEncoder::calculateAccentStrength(const AccentInfo& info, size_t mora_index) const {
        if (info.accent_position < 0) return 0.0f;
        
        int distance = std::abs(static_cast<int>(mora_index) - info.accent_position);
        
        // Accent strength decreases with distance
        float strength = info.accent_strength * std::exp(-distance * 0.5f);
        return std::max(0.0f, std::min(1.0f, strength));
    }

    // FeatureNormalizer Implementation
    FeatureNormalizer::FeatureNormalizer() 
        : type_(NormalizationType::Z_SCORE), sample_count_(0) {
    }

    FeatureNormalizer::FeatureNormalizer(NormalizationType type) 
        : type_(type), sample_count_(0) {
    }

    void FeatureNormalizer::fit(const std::vector<Eigen::VectorXd>& training_data) {
        if (training_data.empty()) return;
        
        calculateStatistics(training_data);
        params_.is_fitted = true;
    }

    void FeatureNormalizer::fitIncremental(const Eigen::VectorXd& sample) {
        updateIncrementalStats(sample);
        params_.is_fitted = true;
    }

    Eigen::VectorXd FeatureNormalizer::normalize(const Eigen::VectorXd& features) const {
        if (!params_.is_fitted) {
            return features; // Return unchanged if not fitted
        }
        
        switch (type_) {
            case NormalizationType::NONE:
                return features;
            case NormalizationType::Z_SCORE:
                return zScoreNormalize(features);
            case NormalizationType::MIN_MAX:
                return minMaxNormalize(features);
            case NormalizationType::ROBUST_SCALING:
                return robustScaleNormalize(features);
            case NormalizationType::QUANTILE_UNIFORM:
                return quantileNormalize(features);
            case NormalizationType::LOG_SCALING:
                return logScaleNormalize(features);
            default:
                return features;
        }
    }

    Eigen::VectorXd FeatureNormalizer::zScoreNormalize(const Eigen::VectorXd& features) const {
        Eigen::VectorXd result = features;
        for (int i = 0; i < features.size() && i < params_.std.size(); ++i) {
            if (params_.std[i] > 1e-8) {
                result[i] = (features[i] - params_.mean[i]) / params_.std[i];
            } else {
                result[i] = 0.0;
            }
        }
        return result;
    }

    Eigen::VectorXd FeatureNormalizer::minMaxNormalize(const Eigen::VectorXd& features) const {
        Eigen::VectorXd result = features;
        for (int i = 0; i < features.size() && i < params_.min.size(); ++i) {
            double range = params_.max[i] - params_.min[i];
            if (range > 1e-8) {
                result[i] = (features[i] - params_.min[i]) / range;
            } else {
                result[i] = 0.0;
            }
        }
        return result;
    }

    Eigen::VectorXd FeatureNormalizer::robustScaleNormalize(const Eigen::VectorXd& features) const {
        Eigen::VectorXd result = features;
        for (int i = 0; i < features.size() && i < params_.median.size(); ++i) {
            double iqr = params_.q75[i] - params_.q25[i];
            if (iqr > 1e-8) {
                result[i] = (features[i] - params_.median[i]) / iqr;
            } else {
                result[i] = 0.0;
            }
        }
        return result;
    }

    Eigen::VectorXd FeatureNormalizer::logScaleNormalize(const Eigen::VectorXd& features) const {
        Eigen::VectorXd result(features.size());
        for (int i = 0; i < features.size(); ++i) {
            result[i] = std::log(std::max(features[i], 1e-8) + 1.0);
        }
        return result;
    }

    void FeatureNormalizer::calculateStatistics(const std::vector<Eigen::VectorXd>& data) {
        if (data.empty()) return;
        
        int feature_dim = data[0].size();
        params_.mean = Eigen::VectorXd::Zero(feature_dim);
        params_.std = Eigen::VectorXd::Zero(feature_dim);
        params_.min = Eigen::VectorXd::Constant(feature_dim, std::numeric_limits<double>::max());
        params_.max = Eigen::VectorXd::Constant(feature_dim, std::numeric_limits<double>::lowest());
        
        // Calculate mean, min, max
        for (const auto& sample : data) {
            params_.mean += sample;
            for (int i = 0; i < feature_dim; ++i) {
                params_.min[i] = std::min(params_.min[i], sample[i]);
                params_.max[i] = std::max(params_.max[i], sample[i]);
            }
        }
        params_.mean /= data.size();
        
        // Calculate standard deviation
        for (const auto& sample : data) {
            Eigen::VectorXd diff = sample - params_.mean;
            for (int i = 0; i < feature_dim; ++i) {
                params_.std[i] += diff[i] * diff[i];
            }
        }
        for (int i = 0; i < feature_dim; ++i) {
            params_.std[i] = std::sqrt(params_.std[i] / data.size());
        }
        
        // Calculate quantiles for robust scaling
        params_.median = Eigen::VectorXd::Zero(feature_dim);
        params_.q25 = Eigen::VectorXd::Zero(feature_dim);
        params_.q75 = Eigen::VectorXd::Zero(feature_dim);
        
        for (int i = 0; i < feature_dim; ++i) {
            std::vector<double> values;
            for (const auto& sample : data) {
                values.push_back(sample[i]);
            }
            std::sort(values.begin(), values.end());
            
            size_t n = values.size();
            params_.q25[i] = values[n / 4];
            params_.median[i] = values[n / 2];
            params_.q75[i] = values[3 * n / 4];
        }
    }

    void FeatureNormalizer::updateIncrementalStats(const Eigen::VectorXd& sample) {
        if (sample_count_ == 0) {
            running_mean_ = sample;
            running_m2_ = Eigen::VectorXd::Zero(sample.size());
        } else {
            // Welford's algorithm for online mean and variance
            sample_count_++;
            Eigen::VectorXd delta = sample - running_mean_;
            running_mean_ += delta / sample_count_;
            Eigen::VectorXd delta2 = sample - running_mean_;
            running_m2_ += delta.cwiseProduct(delta2);
            
            // Update normalization parameters
            params_.mean = running_mean_;
            params_.std = (running_m2_ / sample_count_).cwiseSqrt();
        }
        sample_count_++;
    }

    // ContextFeatureExtractor Implementation
    ContextFeatureExtractor::ContextFeatureExtractor() : use_normalization_(false) {
    }

    ContextFeatureExtractor::ContextFeatureExtractor(const ExtractionConfig& config) 
        : config_(config), use_normalization_(false) {
        
        window_extractor_.setConfig(config.window_config);
        normalizer_ = FeatureNormalizer(config.normalization_type);
    }

    ContextFeatures ContextFeatureExtractor::extractFeatures(
        const std::vector<midi::MusicalPhoneme>& musical_phonemes,
        size_t current_index) const {
        
        // Convert to phoneme timing info
        auto phoneme_timings = convertFromMusicalPhonemes(musical_phonemes);
        
        ContextFeatures features;
        
        if (current_index >= phoneme_timings.size()) {
            return features; // Return empty features
        }
        
        // Extract phoneme and position context
        features.phoneme_context = window_extractor_.extractPhonemeContext(phoneme_timings, current_index);
        features.position_context = window_extractor_.extractPositionContext(phoneme_timings, current_index);
        
        // Set current timing information
        features.current_timing = phoneme_timings[current_index];
        
        // Set frame information
        features.frame_index = current_index;
        features.frame_time_ms = features.current_timing.start_time_ms;
        
        // Add MIDI features if available and enabled
        if (config_.include_midi_features && current_index < musical_phonemes.size()) {
            features.current_midi = musical_phonemes[current_index].midi_note;
        }
        
        // Add VCV features if available and enabled
        if (config_.include_vcv_features && current_index < musical_phonemes.size()) {
            features.current_vcv = musical_phonemes[current_index].vcv_info;
        }
        
        return features;
    }

    std::vector<ContextFeatures> ContextFeatureExtractor::extractBatch(
        const std::vector<midi::MusicalPhoneme>& musical_phonemes) const {
        
        std::vector<ContextFeatures> batch_features;
        batch_features.reserve(musical_phonemes.size());
        
        for (size_t i = 0; i < musical_phonemes.size(); ++i) {
            batch_features.push_back(extractFeatures(musical_phonemes, i));
        }
        
        return batch_features;
    }

    ContextFeatures ContextFeatureExtractor::extractFromOtoEntries(
        const std::vector<utau::OtoEntry>& oto_entries,
        size_t current_index,
        const midi::MidiParser::ParseResult& midi_data) const {
        
        // Convert oto entries to phoneme timing info
        auto phoneme_timings = convertFromOtoEntries(oto_entries);
        
        ContextFeatures features;
        
        if (current_index >= phoneme_timings.size()) {
            return features;
        }
        
        // Extract context features
        features.phoneme_context = window_extractor_.extractPhonemeContext(phoneme_timings, current_index);
        
        // Extract position context with accent info if MIDI data is available
        PositionEncoder::AccentInfo accent_info;
        if (!midi_data.notes.empty()) {
            accent_info = extractAccentInfo(midi_data, phoneme_timings);
        }
        
        features.position_context = window_extractor_.extractPositionContext(phoneme_timings, current_index);
        
        // Set current timing
        features.current_timing = phoneme_timings[current_index];
        features.frame_index = current_index;
        features.frame_time_ms = features.current_timing.start_time_ms;
        
        // Add MIDI features if available
        if (config_.include_midi_features && current_index < midi_data.notes.size()) {
            features.current_midi = midi_data.notes[current_index];
        }
        
        return features;
    }

    void ContextFeatureExtractor::trainNormalizer(const std::vector<ContextFeatures>& training_data) {
        std::vector<Eigen::VectorXd> feature_vectors;
        feature_vectors.reserve(training_data.size());
        
        for (const auto& features : training_data) {
            feature_vectors.push_back(features.toFeatureVector());
        }
        
        normalizer_.fit(feature_vectors);
        use_normalization_ = true;
    }

    void ContextFeatureExtractor::setConfig(const ExtractionConfig& config) {
        config_ = config;
        window_extractor_.setConfig(config.window_config);
        normalizer_ = FeatureNormalizer(config.normalization_type);
    }

    void ContextFeatureExtractor::clearCache() {
        feature_cache_.clear();
    }

    size_t ContextFeatureExtractor::getCacheSize() const {
        return feature_cache_.size();
    }

    std::vector<PhonemeTimingInfo> ContextFeatureExtractor::convertFromMusicalPhonemes(
        const std::vector<midi::MusicalPhoneme>& musical_phonemes) {
        
        std::vector<PhonemeTimingInfo> timing_infos;
        timing_infos.reserve(musical_phonemes.size());
        
        for (const auto& musical_phoneme : musical_phonemes) {
            PhonemeTimingInfo timing;
            timing.phoneme = musical_phoneme.timing.phoneme;
            timing.start_time_ms = musical_phoneme.timing.start_time_ms;
            timing.duration_ms = musical_phoneme.timing.duration_ms;
            timing.end_time_ms = timing.start_time_ms + timing.duration_ms;
            timing.timing_confidence = musical_phoneme.timing.timing_confidence;
            timing.is_valid = musical_phoneme.timing.is_valid;
            
            timing_infos.push_back(timing);
        }
        
        return timing_infos;
    }

    std::vector<PhonemeTimingInfo> ContextFeatureExtractor::convertFromOtoEntries(
        const std::vector<utau::OtoEntry>& oto_entries) {
        
        std::vector<PhonemeTimingInfo> timing_infos;
        timing_infos.reserve(oto_entries.size());
        
        for (const auto& entry : oto_entries) {
            PhonemeTimingInfo timing;
            timing.phoneme = entry.alias;
            timing.start_time_ms = entry.offset;
            timing.duration_ms = entry.consonant + entry.blank; // Simplified
            timing.end_time_ms = timing.start_time_ms + timing.duration_ms;
            timing.consonant_start_ms = entry.offset + entry.preutterance;
            timing.consonant_end_ms = timing.consonant_start_ms + entry.consonant;
            timing.timing_confidence = 1.0; // Default
            timing.is_valid = true;
            
            timing_infos.push_back(timing);
        }
        
        return timing_infos;
    }

    std::string ContextFeatureExtractor::generateCacheKey(
        const std::vector<PhonemeTimingInfo>& phonemes,
        size_t index) const {
        
        std::ostringstream oss;
        oss << "idx:" << index << "_size:" << phonemes.size();
        
        // Add phoneme context for uniqueness
        int start = std::max(0, static_cast<int>(index) - 2);
        int end = std::min(static_cast<int>(phonemes.size()), static_cast<int>(index) + 3);
        
        for (int i = start; i < end; ++i) {
            oss << "_" << phonemes[i].phoneme;
        }
        
        return oss.str();
    }

    PositionEncoder::AccentInfo ContextFeatureExtractor::extractAccentInfo(
        const midi::MidiParser::ParseResult& midi_data,
        const std::vector<PhonemeTimingInfo>& phonemes) const {
        
        PositionEncoder::AccentInfo accent_info;
        
        // Simple accent detection based on MIDI pitch peaks
        if (!midi_data.notes.empty()) {
            // Find highest pitch note as accent position
            auto max_pitch_it = std::max_element(midi_data.notes.begin(), midi_data.notes.end(),
                [](const midi::MidiNote& a, const midi::MidiNote& b) {
                    return a.note_number < b.note_number;
                });
            
            if (max_pitch_it != midi_data.notes.end()) {
                size_t accent_note_idx = std::distance(midi_data.notes.begin(), max_pitch_it);
                accent_info.accent_position = static_cast<int>(accent_note_idx);
                accent_info.accent_strength = max_pitch_it->velocity / 127.0f;
            }
        }
        
        return accent_info;
    }

    // Context utilities implementation
    namespace context_utils {
        
        std::vector<float> concatenateFeatures(const ContextFeatures& features) {
            auto feature_vector = features.toFeatureVector();
            std::vector<float> result;
            result.reserve(feature_vector.size());
            
            for (int i = 0; i < feature_vector.size(); ++i) {
                result.push_back(static_cast<float>(feature_vector[i]));
            }
            
            return result;
        }

        bool validateContextFeatures(const ContextFeatures& features) {
            // Check if context windows have correct size
            if (features.phoneme_context.size() != ContextFeatures::CONTEXT_WINDOW_SIZE) {
                return false;
            }
            
            if (features.position_context.size() != ContextFeatures::CONTEXT_WINDOW_SIZE) {
                return false;
            }
            
            // Check timing validity
            if (!features.current_timing.is_valid) {
                return false;
            }
            
            // Check for reasonable timing values
            if (features.current_timing.duration_ms < 0.0 || features.current_timing.duration_ms > 10000.0) {
                return false;
            }
            
            return true;
        }

        FeatureStatistics analyzeFeatures(const std::vector<ContextFeatures>& features) {
            FeatureStatistics stats;
            stats.total_features = features.size();
            
            if (features.empty()) {
                return stats;
            }
            
            // Count feature types
            stats.phoneme_features = features[0].phoneme_context.size() * PhonemeFeatures::FEATURE_SIZE;
            stats.position_features = features[0].position_context.size() * PositionEncoding::ENCODING_SIZE;
            stats.timing_features = 3; // duration, confidence, validity
            
            // Calculate dimension statistics
            std::vector<size_t> dimensions;
            std::unordered_set<std::string> unique_phonemes;
            
            for (const auto& feature : features) {
                auto vec = feature.toFeatureVector();
                dimensions.push_back(vec.size());
                unique_phonemes.insert(feature.current_timing.phoneme);
            }
            
            if (!dimensions.empty()) {
                double sum = std::accumulate(dimensions.begin(), dimensions.end(), 0.0);
                stats.mean_dimension = sum / dimensions.size();
                
                double var = 0.0;
                for (size_t dim : dimensions) {
                    var += (dim - stats.mean_dimension) * (dim - stats.mean_dimension);
                }
                stats.std_dimension = std::sqrt(var / dimensions.size());
            }
            
            stats.unique_phonemes.assign(unique_phonemes.begin(), unique_phonemes.end());
            
            return stats;
        }

        double assessFeatureQuality(const std::vector<ContextFeatures>& features) {
            if (features.empty()) return 0.0;
            
            double quality_score = 1.0;
            size_t valid_count = 0;
            double avg_confidence = 0.0;
            
            for (const auto& feature : features) {
                if (validateContextFeatures(feature)) {
                    valid_count++;
                }
                avg_confidence += feature.current_timing.timing_confidence;
            }
            
            double validity_ratio = static_cast<double>(valid_count) / features.size();
            avg_confidence /= features.size();
            
            quality_score = (validity_ratio + avg_confidence) * 0.5;
            
            return std::max(0.0, std::min(1.0, quality_score));
        }

    } // namespace context_utils

} // namespace context
} // namespace nexussynth
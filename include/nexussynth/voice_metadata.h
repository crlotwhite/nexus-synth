#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include "cJSON.h"

namespace nexussynth {
namespace metadata {

    /**
     * @brief Version information for NexusSynth models
     * 
     * Semantic versioning system for .nvm model files
     * to ensure compatibility and migration support
     */
    struct Version {
        int major;          // Breaking changes
        int minor;          // New features (backward compatible)
        int patch;          // Bug fixes (backward compatible)
        std::string build;  // Optional build identifier
        
        Version() : major(1), minor(0), patch(0) {}
        Version(int maj, int min, int pat, const std::string& b = "")
            : major(maj), minor(min), patch(pat), build(b) {}
        
        // Version comparison
        bool operator==(const Version& other) const;
        bool operator<(const Version& other) const;
        bool operator>(const Version& other) const;
        bool is_compatible_with(const Version& other) const;
        
        // String representation
        std::string to_string() const;
        static Version from_string(const std::string& version_str);
        
        // Predefined versions
        static const Version NEXUS_SYNTH_1_0;
        static const Version CURRENT;
    };

    /**
     * @brief Audio format specifications for voice models
     * 
     * Defines the audio parameters used for model training
     * and synthesis, ensuring consistent quality settings
     */
    struct AudioFormat {
        int sample_rate;        // Sample rate in Hz (e.g., 44100, 48000)
        double frame_period;    // Frame period in milliseconds (e.g., 5.0)
        int bit_depth;          // Bit depth (16, 24, 32)
        int channels;           // Number of channels (1=mono, 2=stereo)
        std::string format;     // Format name ("PCM", "FLAC", etc.)
        
        AudioFormat() 
            : sample_rate(44100)
            , frame_period(5.0)
            , bit_depth(16)
            , channels(1)
            , format("PCM") {}
        
        AudioFormat(int sr, double fp, int bd = 16, int ch = 1, const std::string& fmt = "PCM")
            : sample_rate(sr), frame_period(fp), bit_depth(bd), channels(ch), format(fmt) {}
        
        // Validation
        bool is_valid() const;
        
        // Common presets
        static AudioFormat utau_standard();    // 44.1kHz, 5ms, 16-bit mono
        static AudioFormat high_quality();     // 48kHz, 5ms, 24-bit mono
        static AudioFormat low_latency();      // 44.1kHz, 2.5ms, 16-bit mono
    };

    /**
     * @brief License information for voice models
     * 
     * Comprehensive licensing system supporting various
     * usage rights and distribution terms
     */
    struct LicenseInfo {
        std::string name;           // License name (e.g., "CC BY-SA 4.0")
        std::string url;            // URL to full license text
        std::string summary;        // Brief description of terms
        bool commercial_use;        // Commercial usage allowed
        bool modification;          // Modification allowed
        bool redistribution;        // Redistribution allowed
        bool attribution_required;  // Attribution required
        std::string attribution;    // How to attribute
        
        LicenseInfo()
            : commercial_use(false)
            , modification(false)
            , redistribution(false)
            , attribution_required(true) {}
        
        // Common licenses
        static LicenseInfo creative_commons_by_sa();
        static LicenseInfo creative_commons_by_nc_sa();
        static LicenseInfo utau_standard();
        static LicenseInfo proprietary();
    };

    /**
     * @brief Statistical information about the voice model
     * 
     * Training statistics and model characteristics
     * for quality assessment and optimization
     */
    struct ModelStatistics {
        size_t total_phonemes;          // Number of phoneme models
        size_t total_contexts;          // Number of context-dependent models
        size_t total_states;            // Total HMM states
        size_t total_gaussians;         // Total Gaussian components
        double model_size_mb;           // Compressed model size in MB
        double training_time_hours;     // Training time in hours
        size_t training_utterances;     // Number of training utterances
        double average_f0_hz;           // Average fundamental frequency
        double f0_range_semitones;      // F0 range in semitones
        
        ModelStatistics()
            : total_phonemes(0), total_contexts(0), total_states(0)
            , total_gaussians(0), model_size_mb(0.0), training_time_hours(0.0)
            , training_utterances(0), average_f0_hz(0.0), f0_range_semitones(0.0) {}
    };

    /**
     * @brief Comprehensive voice bank metadata
     * 
     * Complete metadata structure for NexusSynth voice models
     * including identification, technical specs, and licensing
     */
    struct VoiceMetadata {
        // Core identification
        std::string name;               // Voice bank name
        std::string display_name;       // Human-readable display name
        std::string author;             // Creator/author name
        std::string contact;            // Contact information
        Version version;                // Model version
        
        // Descriptive information
        std::string description;        // Voice description
        std::string language;           // Primary language (ISO 639-1)
        std::string accent;             // Accent/dialect
        std::string voice_type;         // "male", "female", "neutral", "child"
        std::vector<std::string> tags;  // Search tags
        
        // Technical specifications
        AudioFormat audio_format;       // Audio format specifications
        std::string model_type;         // "hmm", "neural", "hybrid"
        Version nexussynth_version;     // Engine version used
        std::string phoneme_set;        // Phoneme set identifier
        
        // Temporal information
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point modified_time;
        std::optional<std::chrono::system_clock::time_point> trained_time;
        
        // Legal and usage information
        LicenseInfo license;            // Licensing terms
        std::string copyright;          // Copyright notice
        std::vector<std::string> credits; // Additional credits
        
        // Model statistics
        ModelStatistics statistics;     // Training and model stats
        
        // Custom metadata
        std::unordered_map<std::string, std::string> custom_fields;
        
        VoiceMetadata();
        explicit VoiceMetadata(const std::string& voice_name);
        
        // Validation
        bool is_valid() const;
        std::vector<std::string> validate_and_get_errors() const;
        
        // JSON serialization
        std::string to_json() const;
        bool from_json(const std::string& json_str);
        cJSON* to_cjson() const;
        bool from_cjson(const cJSON* json);
        
        // File I/O
        bool save_to_file(const std::string& filename) const;
        bool load_from_file(const std::string& filename);
        
        // Utility methods
        std::string get_full_name() const;
        std::string get_version_string() const;
        bool is_compatible_with_engine(const Version& engine_version) const;
        
        // UTAU compatibility
        std::string to_utau_character_txt() const;
        bool from_utau_character_txt(const std::string& content);
        
    private:
        // Helper methods for JSON conversion
        void add_string_to_json(cJSON* json, const char* key, const std::string& value) const;
        void add_time_to_json(cJSON* json, const char* key, 
                             const std::chrono::system_clock::time_point& time) const;
        std::string get_string_from_json(const cJSON* json, const char* key, 
                                        const std::string& default_value = "") const;
        std::chrono::system_clock::time_point get_time_from_json(const cJSON* json, 
                                                                const char* key) const;
        
        // Default values
        static constexpr const char* DEFAULT_MODEL_TYPE = "hmm";
        static constexpr const char* DEFAULT_LANGUAGE = "ja";
        static constexpr const char* DEFAULT_PHONEME_SET = "japanese-cv";
    };

    /**
     * @brief Metadata manager for voice model collections
     * 
     * Manages multiple voice metadata instances and provides
     * search, validation, and batch operations
     */
    class MetadataManager {
    public:
        MetadataManager();
        
        // Voice management
        bool add_voice(const VoiceMetadata& metadata);
        bool remove_voice(const std::string& name);
        VoiceMetadata* get_voice(const std::string& name);
        const VoiceMetadata* get_voice(const std::string& name) const;
        
        // Search and filtering
        std::vector<VoiceMetadata*> find_by_author(const std::string& author);
        std::vector<VoiceMetadata*> find_by_language(const std::string& language);
        std::vector<VoiceMetadata*> find_by_tag(const std::string& tag);
        std::vector<VoiceMetadata*> find_compatible(const Version& engine_version);
        
        // Batch operations
        std::vector<std::string> validate_all();
        bool save_all_to_directory(const std::string& directory);
        bool load_all_from_directory(const std::string& directory);
        
        // Statistics
        size_t count() const { return voices_.size(); }
        std::vector<std::string> get_all_names() const;
        ModelStatistics get_aggregate_statistics() const;
        
        // Utility
        void clear();
        
    private:
        std::unordered_map<std::string, std::unique_ptr<VoiceMetadata>> voices_;
        
        // Helper methods
        bool is_valid_voice_name(const std::string& name) const;
    };

    /**
     * @brief Utility functions for metadata operations
     */
    namespace utils {
        
        // Time conversion utilities
        std::string time_to_iso8601(const std::chrono::system_clock::time_point& time);
        std::chrono::system_clock::time_point time_from_iso8601(const std::string& iso_str);
        
        // String utilities
        std::string utf8_validate_and_clean(const std::string& input);
        bool is_valid_language_code(const std::string& code);
        bool is_valid_voice_name(const std::string& name);
        
        // File utilities
        std::string generate_metadata_filename(const std::string& voice_name);
        std::vector<std::string> find_metadata_files(const std::string& directory);
        
        // Validation
        bool validate_audio_format(const AudioFormat& format);
        bool validate_version(const Version& version);
        
    } // namespace utils

} // namespace metadata
} // namespace nexussynth
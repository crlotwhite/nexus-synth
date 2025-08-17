#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <fstream>

namespace nexussynth {
namespace utau {

    /**
     * @brief UTAU oto.ini entry structure
     * 
     * Represents a single entry in the oto.ini file with all timing parameters
     * Format: filename=alias,offset,consonant,blank,preutterance,overlap
     */
    struct OtoEntry {
        std::string filename;       // Audio filename (.wav)
        std::string alias;          // Phoneme alias (romaji or phonetic)
        double offset;              // Start position in milliseconds
        double consonant;           // Consonant length in milliseconds
        double blank;               // End blank in milliseconds
        double preutterance;        // Pre-utterance timing in milliseconds
        double overlap;             // Overlap with previous phoneme in milliseconds
        
        OtoEntry() 
            : offset(0.0), consonant(0.0), blank(0.0), preutterance(0.0), overlap(0.0) {}
        
        OtoEntry(const std::string& fname, const std::string& alias_name,
                 double off, double cons, double bl, double pre, double over)
            : filename(fname), alias(alias_name), offset(off), consonant(cons)
            , blank(bl), preutterance(pre), overlap(over) {}
        
        // Validation
        bool is_valid() const;
        
        // Utility methods
        double get_phoneme_start() const { return offset; }
        double get_phoneme_end() const { return offset + consonant + blank; }
        double get_consonant_end() const { return offset + consonant; }
        double get_effective_start() const { return offset - preutterance; }
        
        // String representation for debugging
        std::string to_string() const;
    };

    /**
     * @brief UTAU voicebank metadata extracted from oto.ini
     * 
     * Contains information about the voicebank derived from parsing
     */
    struct VoicebankInfo {
        std::string path;                    // Voicebank directory path
        std::string name;                    // Voicebank name (derived from path)
        std::string encoding_detected;       // Detected file encoding
        size_t total_entries;               // Total number of oto entries
        std::vector<std::string> phonemes;  // Unique phonemes found
        std::vector<std::string> filenames; // Audio files referenced
        
        // Quality metrics
        size_t entries_with_timing;         // Entries with non-zero timing
        size_t duplicate_aliases;           // Duplicate alias count
        size_t missing_files;               // Files not found on disk
        
        VoicebankInfo() 
            : total_entries(0), entries_with_timing(0)
            , duplicate_aliases(0), missing_files(0) {}
    };

    /**
     * @brief Character encoding detection and conversion utility
     * 
     * Handles the complex encoding requirements of UTAU files:
     * - Shift-JIS (Windows UTAU default)
     * - UTF-8 (UTAU-Synth, modern)
     * - UTF-8 with BOM
     * - ASCII (basic compatibility)
     */
    class EncodingDetector {
    public:
        enum class Encoding {
            UNKNOWN,
            ASCII,
            UTF8,
            UTF8_BOM,
            SHIFT_JIS,
            GB2312      // Chinese support
        };
        
        // Detection methods
        static Encoding detect_encoding(const std::string& filename);
        static Encoding detect_encoding(const std::vector<uint8_t>& data);
        
        // Conversion methods
        static std::string convert_to_utf8(const std::string& input, Encoding source_encoding);
        static std::string convert_from_utf8(const std::string& input, Encoding target_encoding);
        
        // File operations with encoding
        static std::vector<std::string> read_lines_with_encoding(
            const std::string& filename, Encoding encoding = Encoding::UNKNOWN);
        
        static bool write_lines_with_encoding(
            const std::string& filename, const std::vector<std::string>& lines, 
            Encoding encoding = Encoding::UTF8);
        
        // Encoding information
        static std::string encoding_to_string(Encoding encoding);
        static bool is_japanese_encoding(Encoding encoding);
        
    private:
        // Helper methods
        static bool has_utf8_bom(const std::vector<uint8_t>& data);
        static bool is_valid_utf8(const std::vector<uint8_t>& data);
        static bool has_shift_jis_markers(const std::vector<uint8_t>& data);
        static bool is_ascii_only(const std::vector<uint8_t>& data);
    };

    /**
     * @brief UTAU oto.ini file parser with encoding support
     * 
     * Comprehensive parser that handles:
     * - Multiple encoding formats (Shift-JIS, UTF-8)
     * - Cross-platform compatibility (Windows/Mac UTAU)
     * - Error recovery and validation
     * - Performance optimization for large voicebanks
     */
    class OtoIniParser {
    public:
        // Parsing options
        struct ParseOptions {
            bool strict_validation;        // Strict field validation
            bool auto_detect_encoding;     // Automatic encoding detection
            bool skip_invalid_entries;     // Continue on parse errors
            bool validate_audio_files;     // Check if audio files exist
            double default_preutterance;   // Default preutterance if missing
            double default_overlap;        // Default overlap if missing
            
            ParseOptions()
                : strict_validation(false)
                , auto_detect_encoding(true)
                , skip_invalid_entries(true)
                , validate_audio_files(true)
                , default_preutterance(0.0)
                , default_overlap(0.0) {}
        };
        
        // Parsing results
        struct ParseResult {
            bool success;
            std::vector<OtoEntry> entries;
            VoicebankInfo voicebank_info;
            std::vector<std::string> errors;
            std::vector<std::string> warnings;
            
            ParseResult() : success(false) {}
        };
        
        OtoIniParser();
        explicit OtoIniParser(const ParseOptions& options);
        
        // Main parsing methods
        ParseResult parse_file(const std::string& filename);
        ParseResult parse_directory(const std::string& directory_path);
        ParseResult parse_string(const std::string& content, 
                                const std::string& source_path = "");
        
        // Utility methods
        std::vector<OtoEntry> get_entries_for_phoneme(const std::string& phoneme) const;
        std::vector<OtoEntry> get_entries_for_file(const std::string& filename) const;
        std::optional<OtoEntry> find_best_match(const std::string& phoneme) const;
        
        // Validation and analysis
        std::vector<std::string> validate_entries(const std::vector<OtoEntry>& entries) const;
        VoicebankInfo analyze_voicebank(const std::vector<OtoEntry>& entries,
                                       const std::string& base_path = "") const;
        
        // Configuration
        void set_options(const ParseOptions& options) { options_ = options; }
        const ParseOptions& get_options() const { return options_; }
        
        // Export functionality
        bool export_to_file(const std::vector<OtoEntry>& entries,
                           const std::string& filename,
                           EncodingDetector::Encoding encoding = EncodingDetector::Encoding::UTF8) const;
        
        std::string export_to_string(const std::vector<OtoEntry>& entries) const;
        
    private:
        ParseOptions options_;
        mutable std::vector<OtoEntry> cached_entries_;
        mutable std::unordered_map<std::string, std::vector<size_t>> phoneme_index_;
        
        // Internal parsing methods
        ParseResult parse_lines(const std::vector<std::string>& lines,
                               const std::string& source_path);
        
        std::optional<OtoEntry> parse_oto_line(const std::string& line,
                                              size_t line_number,
                                              std::vector<std::string>& errors) const;
        
        std::vector<std::string> tokenize_oto_line(const std::string& line) const;
        double parse_double_field(const std::string& field, double default_value = 0.0) const;
        
        // Validation helpers
        bool validate_oto_entry(const OtoEntry& entry, std::vector<std::string>& errors) const;
        bool validate_timing_parameters(const OtoEntry& entry, std::vector<std::string>& errors) const;
        bool audio_file_exists(const std::string& base_path, const std::string& filename) const;
        
        // Index building for fast lookup
        void build_phoneme_index() const;
        void clear_cache() const;
        
        // String utilities
        std::string trim_whitespace(const std::string& str) const;
        std::string normalize_path_separators(const std::string& path) const;
        
        // Error handling
        void add_error(std::vector<std::string>& errors, const std::string& message,
                      size_t line_number = 0, const std::string& context = "") const;
        void add_warning(std::vector<std::string>& warnings, const std::string& message,
                        size_t line_number = 0, const std::string& context = "") const;
    };

    /**
     * @brief UTAU oto.ini format writer
     * 
     * Handles writing oto.ini files with proper encoding and formatting
     */
    class OtoIniWriter {
    public:
        struct WriteOptions {
            EncodingDetector::Encoding encoding;   // Target encoding
            bool include_utf8_marker;              // Add #Charset:UTF-8 if UTF-8
            bool preserve_precision;               // High precision for timing values
            std::string line_ending;               // "\r\n" for Windows, "\n" for Unix
            
            WriteOptions()
                : encoding(EncodingDetector::Encoding::UTF8)
                , include_utf8_marker(true)
                , preserve_precision(true)
                , line_ending("\n") {}
        };
        
        OtoIniWriter();
        explicit OtoIniWriter(const WriteOptions& options);
        
        // Writing methods
        bool write_to_file(const std::vector<OtoEntry>& entries, const std::string& filename);
        std::string write_to_string(const std::vector<OtoEntry>& entries);
        
        // Utility methods
        static std::string format_oto_entry(const OtoEntry& entry, bool preserve_precision = true);
        static std::string format_timing_value(double value, bool preserve_precision = true);
        
        // Configuration
        void set_options(const WriteOptions& options) { options_ = options; }
        const WriteOptions& get_options() const { return options_; }
        
    private:
        WriteOptions options_;
        
        // Internal methods
        std::vector<std::string> entries_to_lines(const std::vector<OtoEntry>& entries) const;
        std::string create_utf8_header() const;
    };

    /**
     * @brief Utility functions for UTAU oto.ini processing
     */
    namespace utils {
        
        // File system utilities
        std::vector<std::string> find_oto_files(const std::string& directory_path);
        std::vector<std::string> find_audio_files(const std::string& directory_path);
        bool is_utau_voicebank_directory(const std::string& directory_path);
        
        // Phoneme analysis
        std::vector<std::string> extract_unique_phonemes(const std::vector<OtoEntry>& entries);
        std::unordered_map<std::string, size_t> count_phoneme_usage(const std::vector<OtoEntry>& entries);
        
        // Timing analysis
        struct TimingStats {
            double min_offset, max_offset, avg_offset;
            double min_consonant, max_consonant, avg_consonant;
            double min_preutterance, max_preutterance, avg_preutterance;
            double min_overlap, max_overlap, avg_overlap;
            size_t total_entries;
        };
        
        TimingStats analyze_timing_distribution(const std::vector<OtoEntry>& entries);
        
        // Compatibility helpers
        std::string convert_alias_to_romaji(const std::string& alias);
        std::string convert_romaji_to_hiragana(const std::string& romaji);
        bool is_japanese_phoneme(const std::string& phoneme);
        
        // Validation utilities
        std::vector<std::string> find_duplicate_aliases(const std::vector<OtoEntry>& entries);
        std::vector<std::string> find_missing_audio_files(const std::vector<OtoEntry>& entries,
                                                          const std::string& base_path);
        std::vector<OtoEntry> find_overlapping_entries(const std::vector<OtoEntry>& entries);
        
    } // namespace utils

} // namespace utau
} // namespace nexussynth
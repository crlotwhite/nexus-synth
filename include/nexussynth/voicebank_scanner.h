#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <filesystem>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include "utau_oto_parser.h"
// #include "voice_metadata.h"  // Temporarily disabled due to cJSON path issue
#include "utau_logger.h"

namespace nexussynth {
namespace conditioning {

    /**
     * @brief Audio file validation result
     * 
     * Contains detailed information about audio file validation
     * including format, sample rate, and integrity checks
     */
    struct AudioFileInfo {
        std::string filename;           // Audio file name
        std::string full_path;          // Complete file path
        bool exists;                    // File exists on disk
        bool is_valid;                  // File is valid audio
        size_t file_size;               // File size in bytes
        
        // Audio properties
        int sample_rate;                // Sample rate in Hz
        int bit_depth;                  // Bit depth (16, 24, 32)
        int channels;                   // Number of channels
        double duration_ms;             // Duration in milliseconds
        std::string format;             // Audio format (WAV, FLAC, etc.)
        
        // Quality indicators
        bool has_clipping;              // Audio clipping detected
        double rms_level;               // RMS audio level
        double peak_level;              // Peak audio level
        double snr_estimate;            // Signal-to-noise ratio estimate
        
        AudioFileInfo() 
            : exists(false), is_valid(false), file_size(0)
            , sample_rate(0), bit_depth(0), channels(0)
            , duration_ms(0.0), has_clipping(false)
            , rms_level(0.0), peak_level(0.0), snr_estimate(0.0) {}
    };

    /**
     * @brief UTAU voice bank structure validation result
     * 
     * Comprehensive validation result for a complete UTAU voice bank
     * including directory structure, files, and metadata
     */
    struct VoicebankValidation {
        std::string path;                           // Voice bank root path
        std::string name;                           // Voice bank name
        bool is_valid;                              // Overall validity
        
        // Structure validation
        bool has_oto_ini;                           // Has oto.ini file
        bool has_audio_files;                       // Has audio files
        bool has_character_txt;                     // Has character.txt
        bool has_readme;                            // Has readme file
        
        // File counts
        size_t total_oto_entries;                   // Total OTO entries
        size_t total_audio_files;                   // Total audio files found
        size_t referenced_audio_files;              // Audio files referenced in OTO
        size_t missing_audio_files;                 // Missing audio file count
        size_t orphaned_audio_files;                // Audio files not in OTO
        
        // Quality metrics
        size_t duplicate_aliases;                   // Duplicate alias count
        size_t invalid_timing_entries;              // Invalid timing parameters
        size_t encoding_issues;                     // Text encoding problems
        
        // Audio quality summary
        size_t valid_audio_files;                   // Valid audio files
        size_t audio_format_issues;                 // Format compatibility issues
        size_t audio_quality_warnings;              // Quality warnings (clipping, etc.)
        
        // Detailed results
        std::vector<std::string> errors;            // Validation errors
        std::vector<std::string> warnings;          // Validation warnings
        std::vector<std::string> suggestions;       // Improvement suggestions
        std::unordered_map<std::string, AudioFileInfo> audio_info; // Audio file details
        
        VoicebankValidation() 
            : is_valid(false), has_oto_ini(false), has_audio_files(false)
            , has_character_txt(false), has_readme(false)
            , total_oto_entries(0), total_audio_files(0)
            , referenced_audio_files(0), missing_audio_files(0)
            , orphaned_audio_files(0), duplicate_aliases(0)
            , invalid_timing_entries(0), encoding_issues(0)
            , valid_audio_files(0), audio_format_issues(0)
            , audio_quality_warnings(0) {}
    };

    /**
     * @brief Voice bank discovery result
     * 
     * Result of scanning a directory tree for UTAU voice banks
     */
    struct VoicebankDiscovery {
        std::string search_path;                    // Root search path
        std::vector<std::string> voicebank_paths;   // Found voice bank paths
        size_t directories_scanned;                 // Total directories scanned
        size_t files_scanned;                       // Total files scanned
        std::chrono::milliseconds scan_duration;    // Time taken to scan
        
        // Quality summary
        size_t valid_voicebanks;                    // Number of valid voice banks
        size_t invalid_voicebanks;                  // Number of invalid voice banks
        size_t partial_voicebanks;                  // Partially valid voice banks
        
        std::vector<std::string> scan_errors;       // Scanning errors
        std::vector<std::string> scan_warnings;     // Scanning warnings
        
        VoicebankDiscovery() 
            : directories_scanned(0), files_scanned(0)
            , scan_duration(0), valid_voicebanks(0)
            , invalid_voicebanks(0), partial_voicebanks(0) {}
    };

    /**
     * @brief Voice bank scanner configuration
     * 
     * Configuration options for customizing voice bank scanning behavior
     */
    struct ScannerConfig {
        // Scanning options
        bool recursive_search;                      // Search subdirectories recursively
        bool validate_audio_files;                  // Perform audio file validation
        bool validate_timing_parameters;            // Validate OTO timing parameters
        bool detect_encoding_issues;                // Detect text encoding problems
        bool analyze_audio_quality;                 // Perform audio quality analysis
        
        // Performance options
        int max_scan_depth;                         // Maximum recursion depth
        size_t max_files_per_directory;             // Maximum files to scan per directory
        bool parallel_scanning;                     // Use multi-threading
        int max_threads;                            // Maximum worker threads
        
        // Filtering options
        std::unordered_set<std::string> supported_audio_formats;  // Supported audio formats
        std::vector<std::string> excluded_directories;            // Directories to skip
        std::vector<std::string> excluded_files;                 // Files to skip
        
        // Validation thresholds
        double min_audio_duration_ms;               // Minimum audio duration
        double max_audio_duration_ms;               // Maximum audio duration
        int preferred_sample_rate;                  // Preferred sample rate
        int preferred_bit_depth;                    // Preferred bit depth
        
        ScannerConfig() 
            : recursive_search(true)
            , validate_audio_files(true)
            , validate_timing_parameters(true)
            , detect_encoding_issues(true)
            , analyze_audio_quality(false)
            , max_scan_depth(5)
            , max_files_per_directory(1000)
            , parallel_scanning(true)
            , max_threads(std::thread::hardware_concurrency())
            , min_audio_duration_ms(100.0)
            , max_audio_duration_ms(30000.0)
            , preferred_sample_rate(44100)
            , preferred_bit_depth(16) {
            
            // Default supported audio formats
            supported_audio_formats = {".wav", ".flac", ".aiff", ".aif"};
            
            // Common directories to exclude
            excluded_directories = {".git", ".svn", "_cache", "backup", "temp"};
            
            // Common files to exclude
            excluded_files = {".DS_Store", "Thumbs.db", "desktop.ini"};
        }
    };

    /**
     * @brief Progress callback interface for voice bank scanning
     * 
     * Provides real-time feedback during scanning operations
     */
    class ScanProgressCallback {
    public:
        virtual ~ScanProgressCallback() = default;
        
        // Progress notifications
        virtual void on_scan_started(const std::string& path) {}
        virtual void on_directory_entered(const std::string& path, size_t depth) {}
        virtual void on_voicebank_found(const std::string& path) {}
        virtual void on_voicebank_validated(const std::string& path, bool is_valid) {}
        virtual void on_scan_progress(size_t current, size_t total) {}
        virtual void on_scan_completed(const VoicebankDiscovery& result) {}
        
        // Error notifications
        virtual void on_scan_error(const std::string& path, const std::string& error) {}
        virtual void on_validation_warning(const std::string& path, const std::string& warning) {}
    };

    /**
     * @brief UTAU voice bank directory scanner
     * 
     * Comprehensive scanner that recursively explores directory trees to find
     * and validate UTAU voice banks. Integrates with existing OTO parser and
     * provides detailed validation results for voice bank conditioning.
     * 
     * Key features:
     * - Recursive directory tree traversal
     * - Multi-threaded scanning for performance
     * - Comprehensive voice bank structure validation
     * - Audio file format and quality analysis
     * - Text encoding detection and validation
     * - Progress reporting and cancellation support
     * - Detailed error reporting and suggestions
     */
    class VoicebankScanner {
    public:
        VoicebankScanner();
        explicit VoicebankScanner(const ScannerConfig& config);
        
        // Configuration
        void set_config(const ScannerConfig& config) { config_ = config; }
        const ScannerConfig& get_config() const { return config_; }
        
        void set_progress_callback(std::shared_ptr<ScanProgressCallback> callback) { 
            progress_callback_ = callback; 
        }
        
        // Main scanning operations
        VoicebankDiscovery scan_directory(const std::string& path);
        VoicebankDiscovery scan_multiple_directories(const std::vector<std::string>& paths);
        
        // Individual voice bank operations
        VoicebankValidation validate_voicebank(const std::string& path);
        AudioFileInfo validate_audio_file(const std::string& file_path);
        
        // Utility operations
        bool is_voicebank_directory(const std::string& path);
        std::vector<std::string> find_voicebank_candidates(const std::string& search_path);
        
        // Analysis operations
        std::vector<std::string> get_supported_formats() const;
        std::unordered_map<std::string, size_t> analyze_format_distribution(
            const VoicebankDiscovery& discovery);
        
        // Cancellation support
        void cancel_scan() { cancel_requested_ = true; }
        bool is_cancelled() const { return cancel_requested_; }
        void reset_cancellation() { cancel_requested_ = false; }
        
    private:
        ScannerConfig config_;
        std::shared_ptr<ScanProgressCallback> progress_callback_;
        std::atomic<bool> cancel_requested_;
        
        // Core scanning implementation
        VoicebankDiscovery scan_directory_impl(const std::string& path);
        void scan_directory_recursive(const std::filesystem::path& current_path,
                                     int current_depth,
                                     VoicebankDiscovery& result);
        
        // Voice bank validation implementation
        VoicebankValidation validate_voicebank_impl(const std::string& path);
        bool validate_directory_structure(const std::string& path, VoicebankValidation& validation);
        bool validate_oto_files(const std::string& path, VoicebankValidation& validation);
        bool validate_audio_files(const std::string& path, VoicebankValidation& validation);
        bool validate_metadata_files(const std::string& path, VoicebankValidation& validation);
        
        // Audio file validation implementation
        AudioFileInfo validate_audio_file_impl(const std::string& file_path);
        bool read_audio_properties(const std::string& file_path, AudioFileInfo& info);
        bool analyze_audio_quality(const std::string& file_path, AudioFileInfo& info);
        
        // Utility methods
        bool should_skip_directory(const std::string& directory_name) const;
        bool should_skip_file(const std::string& filename) const;
        bool is_supported_audio_format(const std::string& file_extension) const;
        std::string extract_voicebank_name(const std::string& path) const;
        
        // Progress reporting
        void report_progress(const std::string& operation, 
                           const std::string& path = "", 
                           size_t current = 0, 
                           size_t total = 0);
        void report_error(const std::string& path, const std::string& error);
        void report_warning(const std::string& path, const std::string& warning);
        
        // Threading support
        void process_voicebank_parallel(const std::vector<std::string>& paths,
                                      VoicebankDiscovery& result);
        
        // File system utilities
        bool is_directory_accessible(const std::filesystem::path& path) const;
        size_t count_files_in_directory(const std::filesystem::path& path) const;
        std::vector<std::filesystem::path> get_audio_files_in_directory(
            const std::filesystem::path& path) const;
    };

    /**
     * @brief Simple console progress reporter
     * 
     * Basic implementation of ScanProgressCallback that prints to console
     */
    class ConsoleProgressReporter : public ScanProgressCallback {
    public:
        ConsoleProgressReporter(bool verbose = false) : verbose_(verbose) {}
        
        void on_scan_started(const std::string& path) override;
        void on_directory_entered(const std::string& path, size_t depth) override;
        void on_voicebank_found(const std::string& path) override;
        void on_voicebank_validated(const std::string& path, bool is_valid) override;
        void on_scan_progress(size_t current, size_t total) override;
        void on_scan_completed(const VoicebankDiscovery& result) override;
        void on_scan_error(const std::string& path, const std::string& error) override;
        void on_validation_warning(const std::string& path, const std::string& warning) override;
        
    private:
        bool verbose_;
        std::chrono::steady_clock::time_point last_progress_time_;
        
        void print_progress_bar(size_t current, size_t total, int width = 50);
        std::string format_file_size(size_t bytes);
        std::string format_duration(std::chrono::milliseconds duration);
    };

    /**
     * @brief Voice bank scanning utility functions
     */
    namespace scanner_utils {
        
        // File format detection
        bool is_wav_file(const std::string& file_path);
        bool is_flac_file(const std::string& file_path);
        bool is_audio_file(const std::string& file_path);
        
        // Audio property extraction
        std::optional<int> get_wav_sample_rate(const std::string& file_path);
        std::optional<int> get_wav_bit_depth(const std::string& file_path);
        std::optional<double> get_audio_duration_ms(const std::string& file_path);
        
        // Directory utilities
        std::vector<std::string> find_files_by_extension(
            const std::string& directory, 
            const std::string& extension,
            bool recursive = false);
        
        size_t count_files_by_pattern(
            const std::string& directory,
            const std::string& pattern,
            bool recursive = false);
        
        // Path utilities
        std::string normalize_path(const std::string& path);
        std::string get_relative_path(const std::string& base_path, const std::string& full_path);
        bool is_subdirectory(const std::string& parent, const std::string& child);
        
        // Validation helpers
        bool validate_oto_timing_consistency(const std::vector<utau::OtoEntry>& entries);
        std::vector<std::string> find_encoding_inconsistencies(
            const std::string& directory_path);
        
        // Report generation
        std::string generate_validation_report(const VoicebankValidation& validation);
        std::string generate_discovery_summary(const VoicebankDiscovery& discovery);
        bool export_validation_json(const VoicebankValidation& validation, 
                                   const std::string& output_path);
        
    } // namespace scanner_utils

} // namespace conditioning
} // namespace nexussynth
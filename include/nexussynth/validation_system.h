#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <optional>
#include <set>
#include "nvm_format.h"
#include "utau_oto_parser.h"
#include "conditioning_config.h"

namespace nexussynth {
namespace validation {

    /**
     * @brief Validation severity levels
     */
    enum class ValidationSeverity {
        INFO = 0,       // Informational messages
        WARNING = 1,    // Potential issues that don't prevent usage
        ERROR = 2,      // Serious issues that affect functionality
        CRITICAL = 3    // Fatal errors that prevent file usage
    };

    /**
     * @brief Validation issue category
     */
    enum class ValidationCategory {
        FILE_STRUCTURE,     // File format and structure issues
        NVM_INTEGRITY,      // .nvm file specific validation
        PARAMETER_RANGE,    // Parameter value validation
        PHONEME_COVERAGE,   // Phoneme completeness analysis
        MODEL_CONSISTENCY,  // HMM model validation
        METADATA_VALIDITY,  // Metadata validation
        COMPRESSION_ISSUES, // Compression-related problems
        CHECKSUM_ERRORS,    // Data integrity issues
        VERSION_COMPAT,     // Version compatibility issues
        CONVERSION_QUALITY  // Voice bank conversion quality
    };

    /**
     * @brief Individual validation issue
     */
    struct ValidationIssue {
        std::string id;                         // Unique issue identifier
        ValidationSeverity severity;            // Issue severity level
        ValidationCategory category;            // Issue category
        std::string title;                      // Short issue description
        std::string description;                // Detailed issue description
        std::string location;                   // Where the issue was found
        std::optional<std::string> suggestion;  // Suggested fix
        std::unordered_map<std::string, std::string> metadata; // Additional context
        
        // Location details
        std::optional<size_t> file_offset;     // Byte offset in file
        std::optional<std::string> chunk_type; // NVM chunk where issue occurred
        std::optional<std::string> model_name; // Model name if applicable
        std::optional<std::string> phoneme;    // Phoneme if applicable
        
        ValidationIssue(const std::string& issue_id, ValidationSeverity sev, 
                       ValidationCategory cat, const std::string& title_text)
            : id(issue_id), severity(sev), category(cat), title(title_text) {}
    };

    /**
     * @brief Comprehensive validation report
     */
    struct ValidationReport {
        std::string file_path;                  // Path to validated file
        std::string validation_id;              // Unique validation session ID
        std::chrono::system_clock::time_point validation_time; // When validation occurred
        std::chrono::milliseconds validation_duration; // Time taken
        
        // Overall status
        bool is_valid;                          // Overall file validity
        bool is_usable;                         // Whether file can be used despite issues
        size_t total_issues;                    // Total number of issues found
        
        // Issue breakdown by severity
        size_t info_count;
        size_t warning_count;
        size_t error_count;
        size_t critical_count;
        
        // Issue breakdown by category
        std::unordered_map<ValidationCategory, size_t> category_counts;
        
        // Detailed issues
        std::vector<ValidationIssue> issues;
        
        // File analysis summary
        struct FileAnalysis {
            std::optional<nvm::SemanticVersion> file_version;
            std::optional<size_t> file_size;
            std::optional<size_t> model_count;
            std::optional<size_t> phoneme_count;
            std::optional<bool> has_compression;
            std::optional<bool> has_checksum;
            std::optional<double> compression_ratio;
            std::string file_format;            // "nvm", "utau", "unknown"
        } file_analysis;
        
        // Quality metrics
        struct QualityMetrics {
            double overall_score;               // 0.0 - 1.0 overall quality
            double completeness_score;          // Phoneme coverage completeness
            double consistency_score;           // Model consistency
            double integrity_score;             // File integrity
            std::vector<std::string> missing_phonemes;
            std::vector<std::string> duplicate_models;
            std::vector<std::string> corrupted_models;
        } quality_metrics;
        
        ValidationReport() 
            : validation_time(std::chrono::system_clock::now())
            , is_valid(false), is_usable(false), total_issues(0)
            , info_count(0), warning_count(0), error_count(0), critical_count(0) {
            quality_metrics.overall_score = 0.0;
            quality_metrics.completeness_score = 0.0;
            quality_metrics.consistency_score = 0.0;
            quality_metrics.integrity_score = 0.0;
        }
    };

    /**
     * @brief Phoneme coverage analysis result
     */
    struct PhonemeAnalysis {
        std::set<std::string> required_phonemes;    // Expected phonemes for language
        std::set<std::string> found_phonemes;       // Phonemes found in voice bank
        std::set<std::string> missing_phonemes;     // Required but missing phonemes
        std::set<std::string> extra_phonemes;       // Found but not required phonemes
        std::set<std::string> duplicate_phonemes;   // Duplicate phoneme entries
        
        // Coverage statistics
        size_t total_required;
        size_t total_found;
        size_t total_missing;
        double coverage_percentage;
        
        // Quality indicators
        bool has_basic_vowels;
        bool has_basic_consonants;
        bool has_diphthongs;
        bool has_special_phonemes;
        
        PhonemeAnalysis() 
            : total_required(0), total_found(0), total_missing(0)
            , coverage_percentage(0.0), has_basic_vowels(false)
            , has_basic_consonants(false), has_diphthongs(false)
            , has_special_phonemes(false) {}
    };

    /**
     * @brief Parameter validation rules and ranges
     */
    struct ParameterValidationRules {
        // F0 parameter ranges
        double min_f0_hz = 50.0;
        double max_f0_hz = 800.0;
        double typical_min_f0_hz = 80.0;
        double typical_max_f0_hz = 400.0;
        
        // Spectral parameter ranges
        double min_spectral_peak = -100.0;  // dB
        double max_spectral_peak = 20.0;    // dB
        double min_spectral_power = -120.0; // dB
        double max_spectral_power = 0.0;    // dB
        
        // Timing parameter ranges
        double min_segment_duration_ms = 10.0;
        double max_segment_duration_ms = 5000.0;
        double min_frame_period_ms = 1.0;
        double max_frame_period_ms = 20.0;
        
        // HMM parameter ranges
        size_t min_hmm_states = 3;
        size_t max_hmm_states = 10;
        size_t min_gaussians_per_state = 1;
        size_t max_gaussians_per_state = 64;
        
        // Model consistency thresholds
        double max_model_variance_ratio = 10.0;
        double min_transition_probability = 0.001;
        double max_covariance_determinant = 1e10;
        
        // File size limits
        size_t max_model_size_bytes = 100 * 1024 * 1024; // 100MB per model
        size_t max_total_file_size_bytes = 1024 * 1024 * 1024; // 1GB total
        size_t max_models_per_file = 10000; // Maximum models per NVM file
    };

    /**
     * @brief Validation progress callback interface
     */
    class ValidationProgressCallback {
    public:
        virtual ~ValidationProgressCallback() = default;
        
        // Progress notifications
        virtual void on_validation_started(const std::string& file_path) {}
        virtual void on_validation_progress(size_t current_step, size_t total_steps, const std::string& current_task) {}
        virtual void on_validation_completed(const ValidationReport& report) {}
        
        // Issue notifications
        virtual void on_issue_found(const ValidationIssue& issue) {}
        virtual void on_critical_error(const std::string& error_message) {}
        
        // Analysis notifications
        virtual void on_file_analysis_completed(const ValidationReport::FileAnalysis& analysis) {}
        virtual void on_phoneme_analysis_completed(const PhonemeAnalysis& analysis) {}
    };

    /**
     * @brief Main validation engine
     * 
     * Comprehensive validation system for .nvm files and voice bank conversions
     * providing detailed error analysis, quality metrics, and improvement suggestions
     */
    class ValidationEngine {
    public:
        ValidationEngine();
        explicit ValidationEngine(const ParameterValidationRules& rules);
        
        // Configuration
        void set_validation_rules(const ParameterValidationRules& rules) { rules_ = rules; }
        const ParameterValidationRules& get_validation_rules() const { return rules_; }
        
        void set_progress_callback(std::shared_ptr<ValidationProgressCallback> callback) { 
            progress_callback_ = callback; 
        }
        
        // Main validation operations
        ValidationReport validate_nvm_file(const std::string& file_path);
        ValidationReport validate_utau_voicebank(const std::string& voicebank_path);
        ValidationReport validate_conversion_result(const std::string& source_path, 
                                                   const std::string& target_path);
        
        // Specific validation components
        std::vector<ValidationIssue> validate_file_structure(const std::string& file_path);
        std::vector<ValidationIssue> validate_nvm_integrity(const nvm::NvmFile& nvm_file);
        std::vector<ValidationIssue> validate_parameter_ranges(const nvm::NvmFile& nvm_file);
        std::vector<ValidationIssue> validate_model_consistency(const nvm::NvmFile& nvm_file);
        
        // Phoneme analysis
        PhonemeAnalysis analyze_phoneme_coverage(const nvm::NvmFile& nvm_file, 
                                                const std::string& target_language = "japanese");
        PhonemeAnalysis analyze_utau_phoneme_coverage(const std::string& voicebank_path,
                                                     const std::string& target_language = "japanese");
        
        // Quality assessment
        double calculate_overall_quality_score(const ValidationReport& report);
        std::vector<std::string> generate_improvement_suggestions(const ValidationReport& report);
        
        // Batch validation
        std::vector<ValidationReport> validate_multiple_files(const std::vector<std::string>& file_paths);
        ValidationReport validate_batch_conversion_results(const std::vector<std::string>& source_paths,
                                                          const std::vector<std::string>& target_paths);
        
        // Report generation
        std::string generate_json_report(const ValidationReport& report);
        std::string generate_html_report(const ValidationReport& report);
        std::string generate_markdown_report(const ValidationReport& report);
        bool export_report(const ValidationReport& report, const std::string& output_path, 
                          const std::string& format = "json");
        
        // Comparison and analysis
        ValidationReport compare_files(const std::string& original_path, const std::string& converted_path);
        std::vector<ValidationIssue> analyze_conversion_quality(const std::string& source_path,
                                                               const std::string& target_path);
        
    private:
        ParameterValidationRules rules_;
        std::shared_ptr<ValidationProgressCallback> progress_callback_;
        
        // Internal validation components
        std::vector<ValidationIssue> validate_file_format(const std::string& file_path);
        std::vector<ValidationIssue> validate_nvm_header(const nvm::FileHeader& header);
        std::vector<ValidationIssue> validate_nvm_chunks(const nvm::NvmFile& nvm_file);
        std::vector<ValidationIssue> validate_metadata(const metadata::VoiceMetadata& metadata);
        std::vector<ValidationIssue> validate_compression_integrity(const nvm::NvmFile& nvm_file);
        std::vector<ValidationIssue> validate_checksum_integrity(const nvm::NvmFile& nvm_file);
        std::vector<ValidationIssue> validate_version_compatibility(const nvm::SemanticVersion& version);
        
        // Model validation
        std::vector<ValidationIssue> validate_hmm_models(const std::vector<hmm::PhonemeHmm>& models);
        std::vector<ValidationIssue> validate_gaussian_mixtures(const hmm::PhonemeHmm& model);
        std::vector<ValidationIssue> validate_transition_matrices(const hmm::PhonemeHmm& model);
        std::vector<ValidationIssue> validate_context_features(const hmm::ContextFeature& context);
        
        // UTAU validation
        std::vector<ValidationIssue> validate_utau_structure(const std::string& voicebank_path);
        std::vector<ValidationIssue> validate_oto_entries(const std::vector<utau::OtoEntry>& entries);
        std::vector<ValidationIssue> validate_audio_files(const std::string& voicebank_path, 
                                                          const std::vector<utau::OtoEntry>& entries);
        
        // Phoneme analysis helpers
        std::set<std::string> get_required_phonemes(const std::string& language);
        std::set<std::string> extract_phonemes_from_nvm(const nvm::NvmFile& nvm_file);
        std::set<std::string> extract_phonemes_from_utau(const std::string& voicebank_path);
        bool is_basic_vowel(const std::string& phoneme);
        bool is_basic_consonant(const std::string& phoneme);
        bool is_diphthong(const std::string& phoneme);
        bool is_special_phoneme(const std::string& phoneme);
        
        // Quality calculation helpers
        double calculate_completeness_score(const PhonemeAnalysis& analysis);
        double calculate_consistency_score(const std::vector<ValidationIssue>& issues);
        double calculate_integrity_score(const std::vector<ValidationIssue>& issues);
        
        // Report generation helpers
        std::string severity_to_string(ValidationSeverity severity);
        std::string category_to_string(ValidationCategory category);
        std::string format_issue_as_json(const ValidationIssue& issue);
        std::string format_issue_as_html(const ValidationIssue& issue);
        std::string format_issue_as_markdown(const ValidationIssue& issue);
        
        // Progress reporting
        void report_progress(size_t current, size_t total, const std::string& task);
        void report_issue(const ValidationIssue& issue);
        void report_critical_error(const std::string& error);
        
        // Utility methods
        std::string generate_unique_id();
        std::chrono::milliseconds get_elapsed_time(const std::chrono::steady_clock::time_point& start);
        bool is_file_accessible(const std::string& file_path);
        size_t get_file_size(const std::string& file_path);
    };

    /**
     * @brief Console validation progress reporter
     */
    class ConsoleValidationProgressCallback : public ValidationProgressCallback {
    public:
        ConsoleValidationProgressCallback(bool verbose = false) : verbose_(verbose) {}
        
        void on_validation_started(const std::string& file_path) override;
        void on_validation_progress(size_t current_step, size_t total_steps, const std::string& current_task) override;
        void on_validation_completed(const ValidationReport& report) override;
        void on_issue_found(const ValidationIssue& issue) override;
        void on_critical_error(const std::string& error_message) override;
        
    private:
        bool verbose_;
        std::chrono::steady_clock::time_point start_time_;
        
        void print_progress_bar(size_t current, size_t total, int width = 50);
        std::string severity_color(ValidationSeverity severity);
        std::string category_icon(ValidationCategory category);
    };

    /**
     * @brief Validation utility functions
     */
    namespace validation_utils {
        
        // Report analysis
        bool has_critical_issues(const ValidationReport& report);
        bool is_file_usable(const ValidationReport& report);
        size_t count_issues_by_severity(const ValidationReport& report, ValidationSeverity severity);
        size_t count_issues_by_category(const ValidationReport& report, ValidationCategory category);
        
        // Issue filtering and sorting
        std::vector<ValidationIssue> filter_issues_by_severity(const std::vector<ValidationIssue>& issues, 
                                                              ValidationSeverity min_severity);
        std::vector<ValidationIssue> filter_issues_by_category(const std::vector<ValidationIssue>& issues,
                                                              ValidationCategory category);
        std::vector<ValidationIssue> sort_issues_by_severity(const std::vector<ValidationIssue>& issues);
        
        // Suggestion generation
        std::vector<std::string> generate_fix_suggestions(const ValidationIssue& issue);
        std::string generate_summary_suggestion(const ValidationReport& report);
        
        // Report comparison
        std::vector<ValidationIssue> compare_reports(const ValidationReport& before, 
                                                    const ValidationReport& after);
        bool has_improvement(const ValidationReport& before, const ValidationReport& after);
        
        // Standard phoneme sets
        std::set<std::string> get_japanese_phoneme_set();
        std::set<std::string> get_english_phoneme_set();
        std::set<std::string> get_basic_utau_phoneme_set();
        
        // File format detection
        std::string detect_file_format(const std::string& file_path);
        bool is_nvm_file(const std::string& file_path);
        bool is_utau_voicebank(const std::string& directory_path);
        
    } // namespace validation_utils

} // namespace validation
} // namespace nexussynth
#pragma once

#include "context_features.h"
#include "context_feature_extractor.h"
#include <string>
#include <vector>
#include <fstream>
#include <optional>

namespace nexussynth {
namespace context {

    /**
     * @brief Label file generation and validation for HMM training
     * 
     * Generates standard .lab files from context feature vectors
     * Compatible with HTS-style label format
     */
    class LabelFileGenerator {
    public:
        struct LabelEntry {
            double start_time_ms;       // Start time in milliseconds
            double end_time_ms;         // End time in milliseconds
            std::string hts_label;      // HTS-formatted context label
            
            LabelEntry() : start_time_ms(0.0), end_time_ms(0.0) {}
            LabelEntry(double start, double end, const std::string& label)
                : start_time_ms(start), end_time_ms(end), hts_label(label) {}
        };

        struct ValidationResult {
            bool is_valid;
            std::vector<std::string> errors;
            std::vector<std::string> warnings;
            size_t total_entries;
            double total_duration_ms;
            
            ValidationResult() : is_valid(false), total_entries(0), total_duration_ms(0.0) {}
        };

        struct GenerationConfig {
            bool include_timing;            // Include time stamps in output
            bool validate_timing;           // Validate time alignment
            bool sort_by_time;             // Sort entries by time
            double time_precision_ms;       // Time precision (default: 0.1ms)
            std::string time_format;        // "ms" or "seconds" or "frames"
            
            GenerationConfig() 
                : include_timing(true), validate_timing(true), sort_by_time(true)
                , time_precision_ms(0.1), time_format("ms") {}
        };

        LabelFileGenerator();
        explicit LabelFileGenerator(const GenerationConfig& config);

        // Main generation methods
        bool generateFromContextFeatures(
            const std::vector<ContextFeatures>& features,
            const std::string& output_filename);

        bool generateFromHMMFeatures(
            const std::vector<hmm::ContextFeatureVector>& features,
            const std::vector<PhonemeTimingInfo>& timing_info,
            const std::string& output_filename);

        // Label entry generation
        std::vector<LabelEntry> createLabelEntries(
            const std::vector<ContextFeatures>& features) const;

        std::vector<LabelEntry> createLabelEntries(
            const std::vector<hmm::ContextFeatureVector>& features,
            const std::vector<PhonemeTimingInfo>& timing_info) const;

        // File I/O operations
        bool writeLabelFile(
            const std::vector<LabelEntry>& entries,
            const std::string& filename) const;

        std::optional<std::vector<LabelEntry>> readLabelFile(
            const std::string& filename) const;

        // Validation
        ValidationResult validateLabelFile(const std::string& filename) const;
        ValidationResult validateLabelEntries(const std::vector<LabelEntry>& entries) const;

        // Utilities
        std::string formatTimeStamp(double time_ms) const;
        double parseTimeStamp(const std::string& time_str) const;
        
        // Statistics
        struct FileStatistics {
            size_t total_entries;
            double total_duration_ms;
            double avg_phoneme_duration_ms;
            double min_phoneme_duration_ms;
            double max_phoneme_duration_ms;
            std::vector<std::string> unique_phonemes;
        };
        
        FileStatistics analyzeLabFile(const std::string& filename) const;
        
        // Configuration
        void setConfig(const GenerationConfig& config) { config_ = config; }
        const GenerationConfig& getConfig() const { return config_; }

    private:
        GenerationConfig config_;
        
        // Helper methods
        std::string formatLabelLine(const LabelEntry& entry) const;
        std::optional<LabelEntry> parseLabelLine(const std::string& line) const;
        bool validateTimingConsistency(const std::vector<LabelEntry>& entries) const;
        void sortEntriesByTime(std::vector<LabelEntry>& entries) const;
        
        // Time format conversion
        std::string convertTimeFormat(double time_ms, const std::string& format) const;
        double parseTimeFormat(const std::string& time_str, const std::string& format) const;
    };

    /**
     * @brief Batch label file processor for large datasets
     */
    class LabelFileBatchProcessor {
    public:
        struct BatchConfig {
            size_t max_files_per_batch;     // Maximum files to process at once
            bool parallel_processing;       // Enable multi-threading
            bool continue_on_error;         // Continue processing if one file fails
            std::string output_directory;   // Output directory for processed files
            std::string file_extension;     // Output file extension (default: ".lab")
            
            BatchConfig() 
                : max_files_per_batch(100), parallel_processing(true)
                , continue_on_error(true), file_extension(".lab") {}
        };

        struct BatchResult {
            size_t total_files;
            size_t successful_files;
            size_t failed_files;
            std::vector<std::string> error_files;
            std::vector<std::string> error_messages;
            double total_processing_time_ms;
        };

        LabelFileBatchProcessor();
        explicit LabelFileBatchProcessor(const BatchConfig& config);

        // Batch processing
        BatchResult processDirectory(const std::string& input_directory);
        BatchResult processFileList(const std::vector<std::string>& input_files);

        // Configuration
        void setConfig(const BatchConfig& config) { config_ = config; }
        const BatchConfig& getConfig() const { return config_; }

    private:
        BatchConfig config_;
        LabelFileGenerator generator_;
        
        void processFile(const std::string& input_file, const std::string& output_file);
    };

    /**
     * @brief Label file format utilities
     */
    namespace label_utils {
        
        // Format detection
        enum class LabelFormat {
            HTS_STANDARD,       // Standard HTS format
            NEXUS_EXTENDED,     // Extended format with additional features
            UTAU_TIMING,        // UTAU-compatible timing format
            AUTO_DETECT         // Auto-detect format
        };
        
        LabelFormat detectFormat(const std::string& filename);
        bool convertFormat(const std::string& input_file, const std::string& output_file,
                          LabelFormat input_format, LabelFormat output_format);
        
        // Quality assessment
        struct QualityMetrics {
            double timing_accuracy;         // Timing alignment accuracy
            double label_consistency;       // Label format consistency
            double feature_completeness;    // Feature vector completeness
            double overall_quality;         // Overall quality score [0-1]
        };
        
        QualityMetrics assessQuality(const std::string& label_file);
        QualityMetrics assessQuality(const std::vector<LabelFileGenerator::LabelEntry>& entries);
        
        // Comparison utilities
        struct ComparisonResult {
            double similarity_score;        // Overall similarity [0-1]
            size_t matching_entries;        // Number of matching entries
            size_t total_entries;           // Total entries compared
            std::vector<std::string> differences;  // Detailed differences
        };
        
        ComparisonResult compareLabFiles(const std::string& file1, const std::string& file2);
        
    } // namespace label_utils

} // namespace context
} // namespace nexussynth
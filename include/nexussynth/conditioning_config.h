#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <cJSON.h>
#include "world_wrapper.h"
#include "voicebank_scanner.h"

namespace nexussynth {
namespace conditioning {

    /**
     * @brief Logging configuration for conditioning operations
     */
    struct LoggingConfig {
        enum Level {
            SILENT = 0,
            ERROR = 1,
            WARNING = 2,
            INFO = 3,
            DEBUG = 4,
            TRACE = 5
        };
        
        Level console_level;            // Console logging level
        Level file_level;               // File logging level
        std::string log_file_path;      // Log file path (empty = no file logging)
        bool timestamp_enabled;         // Include timestamps in logs
        bool thread_id_enabled;         // Include thread IDs in logs
        size_t max_log_file_size_mb;    // Maximum log file size before rotation
        int max_log_files;              // Maximum number of rotated log files
        
        LoggingConfig()
            : console_level(INFO)
            , file_level(DEBUG)
            , log_file_path("")
            , timestamp_enabled(true)
            , thread_id_enabled(false)
            , max_log_file_size_mb(10)
            , max_log_files(5) {}
    };

    /**
     * @brief Audio quality and processing configuration
     */
    struct AudioProcessingConfig {
        // Target audio specifications
        int target_sample_rate;         // Target sample rate for processing
        int target_bit_depth;           // Target bit depth
        bool force_mono;                // Force mono conversion
        bool normalize_audio;           // Normalize audio levels
        
        // Quality settings
        double noise_threshold_db;      // Noise floor threshold in dB
        double silence_threshold_db;    // Silence detection threshold in dB
        double max_duration_seconds;    // Maximum audio file duration
        double min_duration_seconds;    // Minimum audio file duration
        
        // Resampling options
        enum ResampleMethod {
            LINEAR = 0,
            CUBIC = 1,
            SINC_FAST = 2,
            SINC_BEST = 3
        };
        ResampleMethod resample_method;
        
        // Audio enhancement
        bool apply_preemphasis;         // Apply pre-emphasis filter
        double preemphasis_coefficient; // Pre-emphasis coefficient
        bool apply_dc_removal;          // Remove DC component
        
        AudioProcessingConfig()
            : target_sample_rate(44100)
            , target_bit_depth(16)
            , force_mono(true)
            , normalize_audio(true)
            , noise_threshold_db(-40.0)
            , silence_threshold_db(-30.0)
            , max_duration_seconds(30.0)
            , min_duration_seconds(0.1)
            , resample_method(SINC_FAST)
            , apply_preemphasis(false)
            , preemphasis_coefficient(0.97)
            , apply_dc_removal(true) {}
    };

    /**
     * @brief Model training and optimization configuration
     */
    struct ModelTrainingConfig {
        // Training parameters
        int max_training_iterations;   // Maximum training iterations
        double convergence_threshold;  // Convergence threshold for early stopping
        int convergence_patience;      // Patience for convergence detection
        
        // Model complexity
        int min_gaussian_components;   // Minimum Gaussian components per state
        int max_gaussian_components;   // Maximum Gaussian components per state
        bool auto_component_selection; // Automatic component selection
        
        // Data augmentation
        bool enable_pitch_augmentation;    // Enable pitch shifting augmentation
        double pitch_shift_range_cents;   // Pitch shift range in cents
        bool enable_tempo_augmentation;    // Enable tempo stretching
        double tempo_stretch_range;       // Tempo stretch range (0.5 = ±50%)
        
        // Optimization levels
        enum OptimizationLevel {
            FAST = 0,       // Fast processing, lower quality
            BALANCED = 1,   // Balance between speed and quality
            QUALITY = 2,    // High quality, slower processing
            MAXIMUM = 3     // Maximum quality, slowest processing
        };
        OptimizationLevel optimization_level;
        
        ModelTrainingConfig()
            : max_training_iterations(100)
            , convergence_threshold(0.001)
            , convergence_patience(5)
            , min_gaussian_components(1)
            , max_gaussian_components(8)
            , auto_component_selection(true)
            , enable_pitch_augmentation(false)
            , pitch_shift_range_cents(200.0)
            , enable_tempo_augmentation(false)
            , tempo_stretch_range(0.2)
            , optimization_level(BALANCED) {}
    };

    /**
     * @brief Batch processing configuration
     */
    struct BatchProcessingConfig {
        // Threading and parallelism
        int num_worker_threads;         // Number of worker threads (0 = auto)
        size_t queue_size_limit;        // Maximum items in processing queue
        size_t batch_size;              // Number of files per batch
        
        // Memory management
        size_t max_memory_usage_mb;     // Maximum memory usage in MB
        bool enable_memory_mapping;     // Use memory mapping for large files
        bool cache_processed_files;     // Cache processed file data
        
        // Progress reporting
        bool enable_progress_reporting; // Enable progress callbacks
        int progress_update_interval_ms; // Progress update interval
        bool show_eta;                  // Show estimated time to completion
        
        // Error handling
        bool continue_on_error;         // Continue processing on individual errors
        int max_consecutive_errors;     // Maximum consecutive errors before stopping
        bool save_error_files;          // Save list of failed files
        
        // Output options
        bool preserve_directory_structure; // Preserve input directory structure
        bool compress_output;               // Compress output .nvm files
        
        BatchProcessingConfig()
            : num_worker_threads(0)
            , queue_size_limit(1000)
            , batch_size(10)
            , max_memory_usage_mb(2048)
            , enable_memory_mapping(true)
            , cache_processed_files(false)
            , enable_progress_reporting(true)
            , progress_update_interval_ms(100)
            , show_eta(true)
            , continue_on_error(true)
            , max_consecutive_errors(10)
            , save_error_files(true)
            , preserve_directory_structure(true)
            , compress_output(true) {}
    };

    /**
     * @brief Output and file management configuration
     */
    struct OutputConfig {
        // Output paths
        std::string output_directory;       // Base output directory
        std::string model_file_extension;   // Model file extension (.nvm)
        std::string metadata_file_extension; // Metadata file extension (.json)
        
        // File naming
        enum NamingScheme {
            PRESERVE_ORIGINAL = 0,  // Keep original voice bank names
            SANITIZE_NAMES = 1,     // Sanitize names for filesystem safety
            CUSTOM_PREFIX = 2       // Add custom prefix to names
        };
        NamingScheme naming_scheme;
        std::string custom_prefix;
        
        // File organization
        bool create_subdirectories;     // Create subdirectories by author/language
        bool generate_index_file;       // Generate voice bank index file
        bool backup_original_files;     // Backup original files before processing
        
        // Quality assurance
        bool validate_output_files;     // Validate generated .nvm files
        bool generate_quality_reports;  // Generate quality analysis reports
        std::string quality_report_format; // Quality report format (json/txt/html)
        
        OutputConfig()
            : output_directory("./output")
            , model_file_extension(".nvm")
            , metadata_file_extension(".json")
            , naming_scheme(SANITIZE_NAMES)
            , custom_prefix("")
            , create_subdirectories(false)
            , generate_index_file(true)
            , backup_original_files(false)
            , validate_output_files(true)
            , generate_quality_reports(true)
            , quality_report_format("json") {}
    };

    /**
     * @brief Main configuration structure for voice bank conditioning
     * 
     * Comprehensive configuration system that controls all aspects of
     * the UTAU voice bank to NexusSynth .nvm conversion process
     */
    struct ConditioningConfig {
        // Configuration metadata
        std::string config_version;     // Configuration schema version
        std::string config_name;        // Configuration name/profile
        std::string description;        // Configuration description
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point modified_time;
        
        // Component configurations
        WorldConfig world_config;               // WORLD vocoder parameters
        ScannerConfig scanner_config;           // Voice bank scanning options
        AudioProcessingConfig audio_config;     // Audio processing settings
        ModelTrainingConfig training_config;    // Model training parameters
        BatchProcessingConfig batch_config;     // Batch processing options
        OutputConfig output_config;             // Output and file management
        LoggingConfig logging_config;           // Logging configuration
        
        // Custom settings
        std::unordered_map<std::string, std::string> custom_settings;
        
        ConditioningConfig()
            : config_version("1.0")
            , config_name("default")
            , description("Default NexusSynth conditioning configuration")
            , created_time(std::chrono::system_clock::now())
            , modified_time(std::chrono::system_clock::now()) {}
        
        explicit ConditioningConfig(const std::string& name)
            : config_version("1.0")
            , config_name(name)
            , description("Custom NexusSynth conditioning configuration")
            , created_time(std::chrono::system_clock::now())
            , modified_time(std::chrono::system_clock::now()) {}
    };

    /**
     * @brief Configuration validation result
     */
    struct ConfigValidationResult {
        bool is_valid;                          // Overall validation result
        std::vector<std::string> errors;        // Validation errors
        std::vector<std::string> warnings;      // Validation warnings
        std::vector<std::string> suggestions;   // Configuration suggestions
        
        ConfigValidationResult() : is_valid(true) {}
    };

    /**
     * @brief Configuration file manager
     * 
     * Handles loading, saving, validation, and management of
     * conditioning configuration files
     */
    class ConfigManager {
    public:
        ConfigManager();
        ~ConfigManager();
        
        // Configuration loading and saving
        bool load_config(const std::string& file_path, ConditioningConfig& config);
        bool save_config(const std::string& file_path, const ConditioningConfig& config);
        
        // JSON serialization
        std::string config_to_json(const ConditioningConfig& config);
        bool config_from_json(const std::string& json_str, ConditioningConfig& config);
        
        // Configuration validation
        ConfigValidationResult validate_config(const ConditioningConfig& config);
        
        // Default configurations
        ConditioningConfig get_default_config();
        ConditioningConfig get_fast_config();      // Fast processing preset
        ConditioningConfig get_quality_config();   // High quality preset
        ConditioningConfig get_batch_config();     // Batch processing preset
        
        // Configuration templates
        bool create_config_template(const std::string& file_path, const std::string& template_name);
        std::vector<std::string> get_available_templates();
        
        // Utility methods
        bool config_file_exists(const std::string& file_path);
        std::string get_config_directory();
        bool ensure_config_directory_exists();
        
        // Migration support
        bool migrate_config(ConditioningConfig& config, const std::string& target_version);
        std::string get_supported_config_version();
        
    private:
        // JSON conversion helpers
        cJSON* world_config_to_json(const WorldConfig& config);
        bool world_config_from_json(const cJSON* json, WorldConfig& config);
        
        cJSON* scanner_config_to_json(const ScannerConfig& config);
        bool scanner_config_from_json(const cJSON* json, ScannerConfig& config);
        
        cJSON* audio_config_to_json(const AudioProcessingConfig& config);
        bool audio_config_from_json(const cJSON* json, AudioProcessingConfig& config);
        
        cJSON* training_config_to_json(const ModelTrainingConfig& config);
        bool training_config_from_json(const cJSON* json, ModelTrainingConfig& config);
        
        cJSON* batch_config_to_json(const BatchProcessingConfig& config);
        bool batch_config_from_json(const cJSON* json, BatchProcessingConfig& config);
        
        cJSON* output_config_to_json(const OutputConfig& config);
        bool output_config_from_json(const cJSON* json, OutputConfig& config);
        
        cJSON* logging_config_to_json(const LoggingConfig& config);
        bool logging_config_from_json(const cJSON* json, LoggingConfig& config);
        
        // Validation helpers
        bool validate_world_config(const WorldConfig& config, std::vector<std::string>& errors);
        bool validate_audio_config(const AudioProcessingConfig& config, std::vector<std::string>& errors);
        bool validate_paths(const ConditioningConfig& config, std::vector<std::string>& errors);
        
        // Utility helpers
        std::string time_to_iso8601(const std::chrono::system_clock::time_point& time);
        std::chrono::system_clock::time_point time_from_iso8601(const std::string& iso_str);
        
        // Default configuration directory
        static constexpr const char* DEFAULT_CONFIG_DIR = ".nexussynth";
        static constexpr const char* CONFIG_FILE_EXTENSION = ".json";
        static constexpr const char* CURRENT_CONFIG_VERSION = "1.0";
    };

    /**
     * @brief Configuration utility functions
     */
    namespace config_utils {
        
        // Configuration presets
        ConditioningConfig create_utau_compatible_config();
        ConditioningConfig create_high_quality_config();
        ConditioningConfig create_fast_processing_config();
        ConditioningConfig create_batch_processing_config();
        
        // Configuration comparison
        bool configs_equal(const ConditioningConfig& a, const ConditioningConfig& b);
        std::vector<std::string> get_config_differences(const ConditioningConfig& a, const ConditioningConfig& b);
        
        // Environment variable support
        bool apply_environment_overrides(ConditioningConfig& config);
        std::vector<std::string> get_supported_env_variables();
        
        // Command line argument support
        bool apply_command_line_overrides(ConditioningConfig& config, int argc, char** argv);
        std::string get_command_line_help();
        
    } // namespace config_utils

} // namespace conditioning
} // namespace nexussynth
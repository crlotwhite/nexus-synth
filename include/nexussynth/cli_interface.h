#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <iostream>
#include <memory>
#include <functional>
#include "conditioning_config.h"
#include "batch_processor.h"
#include "validation_system.h"
#include "voicebank_scanner.h"

namespace nexussynth {
namespace cli {

    /**
     * @brief CLI command types supported by NexusSynth conditioning tool
     */
    enum class CliCommand {
        CONVERT,        // Convert single voice bank
        BATCH,          // Batch convert multiple voice banks
        VALIDATE,       // Validate voice bank or .nvm file
        SCAN,           // Scan directories for voice banks
        CONFIG,         // Manage configuration files
        INFO,           // Display system/file information
        HELP,           // Show help information
        VERSION         // Show version information
    };

    /**
     * @brief CLI argument parsing and validation
     */
    struct CliArguments {
        CliCommand command = CliCommand::HELP;
        
        // Input/Output paths
        std::string input_path;
        std::string output_path;
        std::string config_path;
        std::string log_file;
        
        // Processing options
        bool recursive = false;
        bool force_overwrite = false;
        bool validate_output = true;
        bool generate_report = false;
        bool verbose = false;
        bool quiet = false;
        bool dry_run = false;
        
        // Performance options
        int threads = 0;  // 0 = auto-detect
        size_t memory_limit_mb = 0;  // 0 = no limit
        
        // Validation options
        bool skip_validation = false;
        bool strict_validation = false;
        std::string validation_report_format = "json";
        
        // Batch processing options
        std::vector<std::string> batch_inputs;
        std::string batch_output_dir;
        bool preserve_structure = true;
        
        // Configuration options
        std::string preset = "default";
        std::unordered_map<std::string, std::string> config_overrides;
        
        // Display options
        bool show_progress = true;
        bool show_stats = true;
        bool no_color = false;
    };

    /**
     * @brief CLI operation result
     */
    struct CliResult {
        bool success = false;
        int exit_code = 0;
        std::string message;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        
        // Statistics
        size_t files_processed = 0;
        size_t files_succeeded = 0;
        size_t files_failed = 0;
        std::chrono::duration<double> total_time{0};
        size_t total_output_size = 0;
    };

    /**
     * @brief Main CLI interface class for NexusSynth voice bank conditioning
     * 
     * Provides command-line interface for voice bank conversion, validation,
     * and batch processing operations with comprehensive progress reporting
     * and error handling.
     */
    class CliInterface {
    public:
        CliInterface();
        ~CliInterface() = default;
        
        // Main entry point
        CliResult run(int argc, char* argv[]);
        
        // Command execution
        CliResult execute_convert(const CliArguments& args);
        CliResult execute_batch(const CliArguments& args);
        CliResult execute_validate(const CliArguments& args);
        CliResult execute_scan(const CliArguments& args);
        CliResult execute_config(const CliArguments& args);
        CliResult execute_info(const CliArguments& args);
        CliResult execute_help(const CliArguments& args);
        CliResult execute_version(const CliArguments& args);
        
        // Configuration management
        void load_config(const std::string& config_path);
        void save_config(const std::string& config_path) const;
        
        // Progress and logging
        void set_verbose(bool verbose) { verbose_ = verbose; }
        void set_quiet(bool quiet) { quiet_ = quiet; }
        void set_no_color(bool no_color) { no_color_ = no_color; }
        
    private:
        // Configuration
        conditioning::ConditioningConfig default_config_;
        bool verbose_ = false;
        bool quiet_ = false;
        bool no_color_ = false;
        
        // Component instances
        std::unique_ptr<conditioning::BatchProcessor> batch_processor_;
        std::unique_ptr<validation::ValidationEngine> validation_engine_;
        std::unique_ptr<conditioning::VoicebankScanner> voicebank_scanner_;
        
        // Argument parsing
        CliArguments parse_arguments(int argc, char* argv[]);
        CliCommand parse_command(const std::string& command_str);
        bool parse_flag(const std::string& arg, const std::string& next_arg, 
                       size_t& index, CliArguments& args);
        
        // Validation
        bool validate_arguments(const CliArguments& args);
        bool validate_paths(const CliArguments& args);
        bool validate_config_options(const CliArguments& args);
        
        // Utility methods
        std::string resolve_output_path(const std::string& input_path, 
                                      const std::string& output_dir = "",
                                      bool preserve_structure = true);
        conditioning::ConditioningConfig create_config_from_args(const CliArguments& args);
        void setup_components(const CliArguments& args);
        
        // Progress reporting
        void print_banner();
        void print_help();
        void print_version();
        void print_progress_header(const std::string& operation);
        void print_result_summary(const CliResult& result);
        
        // Colored output
        std::string color_text(const std::string& text, const std::string& color);
        std::string format_file_size(size_t bytes);
        std::string format_duration(std::chrono::duration<double> duration);
        
        // Error handling
        void log_error(const std::string& message);
        void log_warning(const std::string& message);
        void log_info(const std::string& message);
        void log_verbose(const std::string& message);
        
        // Configuration presets
        conditioning::ConditioningConfig get_preset_config(const std::string& preset);
        std::vector<std::string> get_available_presets();
    };

    /**
     * @brief CLI progress callback for batch processing
     */
    class CliProgressCallback : public conditioning::BatchProgressCallback {
    public:
        CliProgressCallback(bool verbose = false, bool no_color = false);
        
        void on_batch_started(size_t total_jobs) override;
        void on_batch_completed(const conditioning::BatchProcessingStats& stats) override;
        void on_batch_progress(const conditioning::BatchProcessingStats& stats) override;
        void on_job_started(const conditioning::BatchJob& job) override;
        void on_job_completed(const conditioning::BatchJob& job, 
                            const conditioning::JobResult& result) override;
        void on_job_failed(const conditioning::BatchJob& job, 
                          const std::string& error) override;
        void on_eta_updated(const std::chrono::system_clock::time_point& estimated_completion) override;
        
    private:
        bool verbose_;
        bool no_color_;
        std::chrono::steady_clock::time_point last_update_;
        size_t current_job_ = 0;
        size_t total_jobs_ = 0;
        
        void print_progress_bar(size_t current, size_t total, int width = 50);
        std::string color_text(const std::string& text, const std::string& color);
        std::string format_duration(std::chrono::duration<double> duration);
        std::string format_file_size(size_t bytes);
    };

    /**
     * @brief CLI validation progress callback
     */
    class CliValidationCallback : public validation::ValidationProgressCallback {
    public:
        CliValidationCallback(bool verbose = false, bool no_color = false);
        
        void on_validation_started(const std::string& file_path) override;
        void on_validation_progress(size_t current_step, size_t total_steps, 
                                  const std::string& current_task) override;
        void on_validation_completed(const validation::ValidationReport& report) override;
        void on_issue_found(const validation::ValidationIssue& issue) override;
        void on_critical_error(const std::string& error_message) override;
        
    private:
        bool verbose_;
        bool no_color_;
        size_t issue_count_ = 0;
        
        std::string color_text(const std::string& text, const std::string& color);
        std::string severity_color(validation::ValidationSeverity severity);
        std::string format_file_size(size_t bytes);
    };

    /**
     * @brief CLI scanning progress callback
     */
    class CliScanCallback : public conditioning::ScanProgressCallback {
    public:
        CliScanCallback(bool verbose = false, bool no_color = false);
        
        void on_scan_started(const std::string& path) override;
        void on_directory_entered(const std::string& path, size_t depth) override;
        void on_voicebank_found(const std::string& path) override;
        void on_voicebank_validated(const std::string& path, bool is_valid) override;
        void on_scan_progress(size_t current, size_t total) override;
        void on_scan_completed(const conditioning::VoicebankDiscovery& result) override;
        void on_scan_error(const std::string& path, const std::string& error) override;
        
    private:
        bool verbose_;
        bool no_color_;
        size_t found_count_ = 0;
        size_t valid_count_ = 0;
        
        std::string color_text(const std::string& text, const std::string& color);
        void print_progress_bar(size_t current, size_t total, int width = 50);
    };

    /**
     * @brief CLI utility functions
     */
    namespace cli_utils {
        
        // Command line parsing utilities
        std::vector<std::string> split_arguments(const std::string& args_string);
        std::string join_paths(const std::vector<std::string>& paths);
        bool is_valid_path(const std::string& path);
        bool is_directory(const std::string& path);
        bool is_utau_voicebank(const std::string& path);
        bool is_nvm_file(const std::string& path);
        
        // File system utilities
        std::string get_absolute_path(const std::string& path);
        std::string get_parent_directory(const std::string& path);
        std::string get_filename_without_extension(const std::string& path);
        bool create_directories_recursive(const std::string& path);
        
        // Configuration utilities
        std::string find_config_file(const std::string& start_path = ".");
        bool validate_config_file(const std::string& config_path);
        std::vector<std::string> expand_glob_patterns(const std::vector<std::string>& patterns);
        
        // Output formatting
        std::string center_text(const std::string& text, size_t width);
        std::string format_table_row(const std::vector<std::string>& columns, 
                                   const std::vector<size_t>& widths);
        void print_table(const std::vector<std::vector<std::string>>& data,
                        const std::vector<std::string>& headers = {});
        
        // Error handling
        std::string get_error_suggestion(int exit_code);
        std::string format_error_context(const std::string& operation, 
                                       const std::string& file_path,
                                       const std::string& error_message);
        
        // System information
        std::string get_system_info();
        std::string get_dependency_versions();
        size_t get_available_memory_mb();
        int get_optimal_thread_count();
        
    } // namespace cli_utils

} // namespace cli
} // namespace nexussynth
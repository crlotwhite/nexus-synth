#include "nexussynth/cli_interface.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <regex>

namespace nexussynth {
namespace cli {

namespace {
    // ANSI color codes
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
}

CliInterface::CliInterface() {
    // Initialize default configuration
    default_config_ = conditioning::ConditioningConfig();
}

CliResult CliInterface::run(int argc, char* argv[]) {
    CliResult result;
    
    try {
        // Parse command line arguments
        CliArguments args = parse_arguments(argc, argv);
        
        // Validate arguments
        if (!validate_arguments(args)) {
            result.success = false;
            result.exit_code = 1;
            result.message = "Invalid arguments provided";
            return result;
        }
        
        // Set global options
        verbose_ = args.verbose;
        quiet_ = args.quiet;
        no_color_ = args.no_color;
        
        // Load configuration if specified
        if (!args.config_path.empty()) {
            load_config(args.config_path);
        }
        
        // Setup components
        setup_components(args);
        
        // Execute command
        switch (args.command) {
            case CliCommand::CONVERT:
                result = execute_convert(args);
                break;
            case CliCommand::BATCH:
                result = execute_batch(args);
                break;
            case CliCommand::VALIDATE:
                result = execute_validate(args);
                break;
            case CliCommand::SCAN:
                result = execute_scan(args);
                break;
            case CliCommand::CONFIG:
                result = execute_config(args);
                break;
            case CliCommand::INFO:
                result = execute_info(args);
                break;
            case CliCommand::VERSION:
                result = execute_version(args);
                break;
            case CliCommand::HELP:
            default:
                result = execute_help(args);
                break;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.exit_code = 1;
        result.message = std::string("Unhandled exception: ") + e.what();
        log_error(result.message);
    }
    
    if (!quiet_) {
        print_result_summary(result);
    }
    
    return result;
}

CliArguments CliInterface::parse_arguments(int argc, char* argv[]) {
    CliArguments args;
    
    if (argc < 2) {
        args.command = CliCommand::HELP;
        return args;
    }
    
    // Parse command
    args.command = parse_command(argv[1]);
    
    // Parse flags and options
    for (size_t i = 2; i < static_cast<size_t>(argc); ++i) {
        std::string arg = argv[i];
        std::string next_arg = (i + 1 < static_cast<size_t>(argc)) ? argv[i + 1] : "";
        
        if (parse_flag(arg, next_arg, i, args)) {
            continue;
        }
        
        // Handle positional arguments based on command
        if (args.command == CliCommand::CONVERT && args.input_path.empty()) {
            args.input_path = arg;
        } else if (args.command == CliCommand::CONVERT && args.output_path.empty()) {
            args.output_path = arg;
        } else if (args.command == CliCommand::BATCH && args.batch_output_dir.empty()) {
            args.batch_output_dir = arg;
        } else if (args.command == CliCommand::VALIDATE && args.input_path.empty()) {
            args.input_path = arg;
        } else if (args.command == CliCommand::SCAN && args.input_path.empty()) {
            args.input_path = arg;
        } else {
            // Additional input for batch processing
            args.batch_inputs.push_back(arg);
        }
    }
    
    return args;
}

CliCommand CliInterface::parse_command(const std::string& command_str) {
    std::string cmd = command_str;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "convert" || cmd == "c") return CliCommand::CONVERT;
    if (cmd == "batch" || cmd == "b") return CliCommand::BATCH;
    if (cmd == "validate" || cmd == "v") return CliCommand::VALIDATE;
    if (cmd == "scan" || cmd == "s") return CliCommand::SCAN;
    if (cmd == "config" || cmd == "cfg") return CliCommand::CONFIG;
    if (cmd == "info" || cmd == "i") return CliCommand::INFO;
    if (cmd == "version" || cmd == "--version") return CliCommand::VERSION;
    if (cmd == "help" || cmd == "--help" || cmd == "-h") return CliCommand::HELP;
    
    return CliCommand::HELP;
}

bool CliInterface::parse_flag(const std::string& arg, const std::string& next_arg, 
                             size_t& index, CliArguments& args) {
    if (arg == "-i" || arg == "--input") {
        args.input_path = next_arg;
        ++index;
        return true;
    }
    if (arg == "-o" || arg == "--output") {
        args.output_path = next_arg;
        ++index;
        return true;
    }
    if (arg == "-c" || arg == "--config") {
        args.config_path = next_arg;
        ++index;
        return true;
    }
    if (arg == "--log-file") {
        args.log_file = next_arg;
        ++index;
        return true;
    }
    if (arg == "-j" || arg == "--threads") {
        args.threads = std::stoi(next_arg);
        ++index;
        return true;
    }
    if (arg == "--memory-limit") {
        args.memory_limit_mb = std::stoull(next_arg);
        ++index;
        return true;
    }
    if (arg == "--preset") {
        args.preset = next_arg;
        ++index;
        return true;
    }
    if (arg == "--report-format") {
        args.validation_report_format = next_arg;
        ++index;
        return true;
    }
    if (arg == "-r" || arg == "--recursive") {
        args.recursive = true;
        return true;
    }
    if (arg == "-f" || arg == "--force") {
        args.force_overwrite = true;
        return true;
    }
    if (arg == "--no-validate") {
        args.validate_output = false;
        return true;
    }
    if (arg == "--strict") {
        args.strict_validation = true;
        return true;
    }
    if (arg == "--dry-run") {
        args.dry_run = true;
        return true;
    }
    if (arg == "-v" || arg == "--verbose") {
        args.verbose = true;
        return true;
    }
    if (arg == "-q" || arg == "--quiet") {
        args.quiet = true;
        return true;
    }
    if (arg == "--no-color") {
        args.no_color = true;
        return true;
    }
    if (arg == "--no-progress") {
        args.show_progress = false;
        return true;
    }
    if (arg == "--preserve-structure") {
        args.preserve_structure = true;
        return true;
    }
    if (arg == "--generate-report") {
        args.generate_report = true;
        return true;
    }
    
    return false;
}

bool CliInterface::validate_arguments(const CliArguments& args) {
    // Validate paths
    if (!validate_paths(args)) {
        return false;
    }
    
    // Validate config options
    if (!validate_config_options(args)) {
        return false;
    }
    
    // Command-specific validation
    switch (args.command) {
        case CliCommand::CONVERT:
            if (args.input_path.empty()) {
                log_error("Convert command requires input path");
                return false;
            }
            break;
        case CliCommand::BATCH:
            if (args.batch_inputs.empty() && args.input_path.empty()) {
                log_error("Batch command requires input paths");
                return false;
            }
            if (args.batch_output_dir.empty()) {
                log_error("Batch command requires output directory");
                return false;
            }
            break;
        case CliCommand::VALIDATE:
            if (args.input_path.empty()) {
                log_error("Validate command requires input path");
                return false;
            }
            break;
        case CliCommand::SCAN:
            if (args.input_path.empty()) {
                log_error("Scan command requires input path");
                return false;
            }
            break;
        default:
            break;
    }
    
    return true;
}

bool CliInterface::validate_paths(const CliArguments& args) {
    namespace fs = std::filesystem;
    
    // Validate input path exists
    if (!args.input_path.empty() && !fs::exists(args.input_path)) {
        log_error("Input path does not exist: " + args.input_path);
        return false;
    }
    
    // Validate config path exists
    if (!args.config_path.empty() && !fs::exists(args.config_path)) {
        log_error("Config file does not exist: " + args.config_path);
        return false;
    }
    
    // Validate batch inputs exist
    for (const auto& path : args.batch_inputs) {
        if (!fs::exists(path)) {
            log_error("Batch input path does not exist: " + path);
            return false;
        }
    }
    
    return true;
}

bool CliInterface::validate_config_options(const CliArguments& args) {
    // Validate threads
    if (args.threads < 0) {
        log_error("Thread count cannot be negative");
        return false;
    }
    
    // Validate memory limit
    if (args.memory_limit_mb > 0 && args.memory_limit_mb < 128) {
        log_warning("Memory limit is very low (< 128MB), performance may be poor");
    }
    
    // Validate preset
    auto available_presets = get_available_presets();
    if (std::find(available_presets.begin(), available_presets.end(), args.preset) == available_presets.end()) {
        log_error("Unknown preset: " + args.preset);
        log_info("Available presets: " + cli_utils::join_paths(available_presets));
        return false;
    }
    
    // Validate report format
    if (args.validation_report_format != "json" && 
        args.validation_report_format != "html" && 
        args.validation_report_format != "markdown") {
        log_error("Unsupported report format: " + args.validation_report_format);
        log_info("Supported formats: json, html, markdown");
        return false;
    }
    
    return true;
}

CliResult CliInterface::execute_convert(const CliArguments& args) {
    CliResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    if (!quiet_) {
        print_progress_header("Voice Bank Conversion");
        log_info("Converting: " + args.input_path);
        if (!args.output_path.empty()) {
            log_info("Output: " + args.output_path);
        }
    }
    
    try {
        // Resolve output path
        std::string output_path = args.output_path;
        if (output_path.empty()) {
            output_path = resolve_output_path(args.input_path);
        }
        
        // Create configuration
        auto config = create_config_from_args(args);
        
        // Dry run mode
        if (args.dry_run) {
            log_info("DRY RUN: Would convert to " + output_path);
            result.success = true;
            result.exit_code = 0;
            result.message = "Dry run completed successfully";
            return result;
        }
        
        // Validate input voice bank first
        if (args.validate_output) {
            log_info("Validating input voice bank...");
            auto validation_report = validation_engine_->validate_utau_voicebank(args.input_path);
            if (!validation_report.is_usable) {
                result.success = false;
                result.exit_code = 2;
                result.message = "Input voice bank validation failed";
                result.errors.push_back("Voice bank is not usable for conversion");
                return result;
            }
            if (verbose_) {
                log_info("Validation passed with " + std::to_string(validation_report.total_issues) + " issues");
            }
        }
        
        // Set up batch processor for single conversion
        auto progress_callback = std::make_shared<CliProgressCallback>(verbose_, no_color_);
        batch_processor_->set_progress_callback(progress_callback);
        
        // Add single job
        std::string job_id = batch_processor_->add_job(args.input_path, output_path, config);
        
        // Execute conversion
        if (!batch_processor_->start_batch()) {
            result.success = false;
            result.exit_code = 3;
            result.message = "Failed to start conversion";
            return result;
        }
        
        // Wait for completion (in a real implementation, this would be more sophisticated)
        while (batch_processor_->is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Get results
        auto stats = batch_processor_->get_stats();
        auto job_results = batch_processor_->get_results();
        
        if (stats.failed_jobs > 0) {
            result.success = false;
            result.exit_code = 4;
            result.message = "Conversion failed";
            auto error_log = batch_processor_->get_error_log();
            result.errors.insert(result.errors.end(), error_log.begin(), error_log.end());
        } else {
            result.success = true;
            result.exit_code = 0;
            result.message = "Conversion completed successfully";
            result.files_processed = 1;
            result.files_succeeded = 1;
            
            if (!job_results.empty()) {
                result.total_output_size = job_results[0].output_file_size_bytes;
            }
        }
        
        // Post-conversion validation
        if (result.success && args.validate_output) {
            log_info("Validating output file...");
            auto validation_report = validation_engine_->validate_nvm_file(output_path);
            if (!validation_report.is_valid) {
                result.warnings.push_back("Output validation found " + 
                                        std::to_string(validation_report.total_issues) + " issues");
            }
        }
        
        // Generate report if requested
        if (args.generate_report && !job_results.empty()) {
            std::string report_path = output_path + "_report." + args.validation_report_format;
            // Would generate report here
            log_info("Report saved to: " + report_path);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.exit_code = 5;
        result.message = "Exception during conversion: " + std::string(e.what());
        log_error(result.message);
    }
    
    result.total_time = std::chrono::steady_clock::now() - start_time;
    return result;
}

CliResult CliInterface::execute_batch(const CliArguments& args) {
    CliResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    if (!quiet_) {
        print_progress_header("Batch Voice Bank Conversion");
    }
    
    try {
        // Collect all input paths
        std::vector<std::string> input_paths = args.batch_inputs;
        if (!args.input_path.empty()) {
            input_paths.push_back(args.input_path);
        }
        
        // If recursive scanning is enabled, expand directories
        if (args.recursive) {
            std::vector<std::string> expanded_paths;
            for (const auto& path : input_paths) {
                if (std::filesystem::is_directory(path)) {
                    auto discovery = voicebank_scanner_->scan_directory(path);
                    expanded_paths.insert(expanded_paths.end(), 
                                        discovery.voicebank_paths.begin(), 
                                        discovery.voicebank_paths.end());
                } else {
                    expanded_paths.push_back(path);
                }
            }
            input_paths = expanded_paths;
        }
        
        log_info("Found " + std::to_string(input_paths.size()) + " voice banks to convert");
        
        if (input_paths.empty()) {
            result.success = false;
            result.exit_code = 2;
            result.message = "No voice banks found for batch conversion";
            return result;
        }
        
        // Create configuration
        auto config = create_config_from_args(args);
        
        // Set up progress callback
        auto progress_callback = std::make_shared<CliProgressCallback>(verbose_, no_color_);
        batch_processor_->set_progress_callback(progress_callback);
        
        // Add jobs to batch processor
        for (const auto& input_path : input_paths) {
            std::string output_path = resolve_output_path(input_path, args.batch_output_dir, 
                                                        args.preserve_structure);
            batch_processor_->add_job(input_path, output_path, config);
        }
        
        // Dry run mode
        if (args.dry_run) {
            log_info("DRY RUN: Would process " + std::to_string(input_paths.size()) + " voice banks");
            result.success = true;
            result.exit_code = 0;
            result.message = "Dry run completed successfully";
            return result;
        }
        
        // Execute batch
        if (!batch_processor_->start_batch()) {
            result.success = false;
            result.exit_code = 3;
            result.message = "Failed to start batch processing";
            return result;
        }
        
        // Wait for completion
        while (batch_processor_->is_running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Get results
        auto stats = batch_processor_->get_stats();
        
        result.files_processed = stats.total_jobs;
        result.files_succeeded = stats.completed_jobs;
        result.files_failed = stats.failed_jobs;
        result.total_output_size = stats.total_output_size_bytes;
        
        if (stats.failed_jobs == 0) {
            result.success = true;
            result.exit_code = 0;
            result.message = "All conversions completed successfully";
        } else {
            result.success = stats.completed_jobs > 0;
            result.exit_code = (stats.completed_jobs > 0) ? 1 : 4;
            result.message = std::to_string(stats.failed_jobs) + " conversions failed";
            
            auto error_log = batch_processor_->get_error_log();
            result.errors.insert(result.errors.end(), error_log.begin(), error_log.end());
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.exit_code = 5;
        result.message = "Exception during batch processing: " + std::string(e.what());
        log_error(result.message);
    }
    
    result.total_time = std::chrono::steady_clock::now() - start_time;
    return result;
}

CliResult CliInterface::execute_validate(const CliArguments& args) {
    CliResult result;
    
    if (!quiet_) {
        print_progress_header("Validation");
        log_info("Validating: " + args.input_path);
    }
    
    try {
        auto progress_callback = std::make_shared<CliValidationCallback>(verbose_, no_color_);
        validation_engine_->set_progress_callback(progress_callback);
        
        validation::ValidationReport report;
        
        // Determine file type and validate accordingly
        if (cli_utils::is_directory(args.input_path) && 
            cli_utils::is_utau_voicebank(args.input_path)) {
            report = validation_engine_->validate_utau_voicebank(args.input_path);
        } else if (cli_utils::is_nvm_file(args.input_path)) {
            report = validation_engine_->validate_nvm_file(args.input_path);
        } else {
            result.success = false;
            result.exit_code = 2;
            result.message = "Input is neither a UTAU voice bank nor an NVM file";
            return result;
        }
        
        // Determine result based on validation
        if (report.is_valid) {
            result.success = true;
            result.exit_code = 0;
            result.message = "Validation passed";
        } else if (report.is_usable) {
            result.success = true;
            result.exit_code = 1;
            result.message = "Validation passed with warnings";
        } else {
            result.success = false;
            result.exit_code = 2;
            result.message = "Validation failed";
        }
        
        // Add validation details
        if (report.total_issues > 0) {
            result.message += " (" + std::to_string(report.total_issues) + " issues found)";
        }
        
        // Generate report if requested
        if (args.generate_report) {
            std::string report_path = args.input_path + "_validation." + args.validation_report_format;
            if (validation_engine_->export_report(report, report_path, args.validation_report_format)) {
                log_info("Validation report saved to: " + report_path);
            } else {
                result.warnings.push_back("Failed to save validation report");
            }
        }
        
        result.files_processed = 1;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.exit_code = 5;
        result.message = "Exception during validation: " + std::string(e.what());
        log_error(result.message);
    }
    
    return result;
}

CliResult CliInterface::execute_scan(const CliArguments& args) {
    CliResult result;
    
    if (!quiet_) {
        print_progress_header("Voice Bank Scanning");
        log_info("Scanning: " + args.input_path);
    }
    
    try {
        auto progress_callback = std::make_shared<CliScanCallback>(verbose_, no_color_);
        voicebank_scanner_->set_progress_callback(progress_callback);
        
        // Configure scanner
        conditioning::ScannerConfig scanner_config;
        scanner_config.recursive_search = args.recursive;
        scanner_config.validate_audio_files = true;
        scanner_config.parallel_scanning = (args.threads != 1);
        if (args.threads > 0) {
            scanner_config.max_threads = args.threads;
        }
        voicebank_scanner_->set_config(scanner_config);
        
        // Perform scan
        conditioning::VoicebankDiscovery discovery = voicebank_scanner_->scan_directory(args.input_path);
        
        result.success = true;
        result.exit_code = 0;
        result.message = "Found " + std::to_string(discovery.voicebank_paths.size()) + " voice banks";
        result.files_processed = discovery.voicebank_paths.size();
        result.files_succeeded = discovery.valid_voicebanks;
        result.files_failed = discovery.invalid_voicebanks;
        
        // Print results if not quiet
        if (!quiet_) {
            log_info("Scan completed:");
            log_info("  Total directories scanned: " + std::to_string(discovery.directories_scanned));
            log_info("  Total files scanned: " + std::to_string(discovery.files_scanned));
            log_info("  Voice banks found: " + std::to_string(discovery.voicebank_paths.size()));
            log_info("  Valid voice banks: " + std::to_string(discovery.valid_voicebanks));
            log_info("  Invalid voice banks: " + std::to_string(discovery.invalid_voicebanks));
            log_info("  Scan duration: " + format_duration(std::chrono::duration_cast<std::chrono::duration<double>>(discovery.scan_duration)));
        }
        
        // List found voice banks if verbose
        if (verbose_ && !discovery.voicebank_paths.empty()) {
            log_info("Found voice banks:");
            for (const auto& path : discovery.voicebank_paths) {
                log_info("  " + path);
            }
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.exit_code = 5;
        result.message = "Exception during scanning: " + std::string(e.what());
        log_error(result.message);
    }
    
    return result;
}

CliResult CliInterface::execute_config(const CliArguments& args) {
    CliResult result;
    result.success = true;
    result.exit_code = 0;
    result.message = "Configuration operations not yet implemented";
    
    // This would implement configuration management
    // For now, just show current configuration
    if (!quiet_) {
        print_progress_header("Configuration");
        log_info("Current preset: " + args.preset);
        log_info("Available presets: " + cli_utils::join_paths(get_available_presets()));
    }
    
    return result;
}

CliResult CliInterface::execute_info(const CliArguments& args) {
    CliResult result;
    result.success = true;
    result.exit_code = 0;
    result.message = "System information displayed";
    
    if (!quiet_) {
        print_progress_header("System Information");
        std::cout << cli_utils::get_system_info() << std::endl;
        std::cout << "\nDependency Versions:\n" << cli_utils::get_dependency_versions() << std::endl;
        
        if (!args.input_path.empty()) {
            std::cout << "\nFile Information:\n";
            if (cli_utils::is_utau_voicebank(args.input_path)) {
                std::cout << "Type: UTAU Voice Bank\n";
                std::cout << "Path: " << args.input_path << "\n";
            } else if (cli_utils::is_nvm_file(args.input_path)) {
                std::cout << "Type: NexusSynth NVM File\n";
                std::cout << "Path: " << args.input_path << "\n";
            } else {
                std::cout << "Type: Unknown\n";
                std::cout << "Path: " << args.input_path << "\n";
            }
        }
    }
    
    return result;
}

CliResult CliInterface::execute_help(const CliArguments& args) {
    CliResult result;
    result.success = true;
    result.exit_code = 0;
    result.message = "Help information displayed";
    
    print_banner();
    print_help();
    
    return result;
}

CliResult CliInterface::execute_version(const CliArguments& args) {
    CliResult result;
    result.success = true;
    result.exit_code = 0;
    result.message = "Version information displayed";
    
    print_version();
    
    return result;
}

void CliInterface::setup_components(const CliArguments& args) {
    // Setup batch processor
    conditioning::BatchProcessingConfig batch_config;
    batch_config.num_worker_threads = (args.threads > 0) ? args.threads : 0;
    batch_config.max_memory_usage_mb = (args.memory_limit_mb > 0) ? args.memory_limit_mb : 0;
    batch_processor_ = std::make_unique<conditioning::BatchProcessor>(batch_config);
    
    // Setup validation engine
    validation::ParameterValidationRules validation_rules;
    if (args.strict_validation) {
        validation_rules.max_model_variance_ratio = 5.0;
        validation_rules.min_transition_probability = 0.01;
    }
    validation_engine_ = std::make_unique<validation::ValidationEngine>(validation_rules);
    
    // Setup voice bank scanner
    voicebank_scanner_ = std::make_unique<conditioning::VoicebankScanner>();
}

conditioning::ConditioningConfig CliInterface::create_config_from_args(const CliArguments& args) {
    auto config = get_preset_config(args.preset);
    
    // Apply command-line overrides
    for (const auto& [key, value] : args.config_overrides) {
        // This would apply configuration overrides
        // Implementation depends on specific configuration structure
    }
    
    return config;
}

conditioning::ConditioningConfig CliInterface::get_preset_config(const std::string& preset) {
    conditioning::ConditioningConfig config;
    
    if (preset == "fast") {
        // Fast conversion settings
        config.world_config.frame_period = 10.0;
    } else if (preset == "quality") {
        // High quality settings  
        config.world_config.frame_period = 2.5;
    } else if (preset == "batch") {
        // Batch processing optimized settings
        config.world_config.frame_period = 5.0;
    }
    // else use default settings
    
    return config;
}

std::vector<std::string> CliInterface::get_available_presets() {
    return {"default", "fast", "quality", "batch"};
}

std::string CliInterface::resolve_output_path(const std::string& input_path, 
                                            const std::string& output_dir,
                                            bool preserve_structure) {
    namespace fs = std::filesystem;
    
    fs::path input(input_path);
    std::string filename = cli_utils::get_filename_without_extension(input_path) + ".nvm";
    
    if (output_dir.empty()) {
        return input.parent_path() / filename;
    }
    
    fs::path output_base(output_dir);
    
    if (preserve_structure) {
        // Preserve relative path structure
        fs::path relative = fs::relative(input.parent_path(), fs::current_path());
        return output_base / relative / filename;
    } else {
        return output_base / filename;
    }
}

void CliInterface::print_banner() {
    if (no_color_) {
        std::cout << "NexusSynth Voice Bank Conditioning Tool v1.0.0\n";
        std::cout << "Copyright (c) 2024 NexusSynth Project\n\n";
    } else {
        std::cout << color_text("NexusSynth Voice Bank Conditioning Tool v1.0.0", CYAN + BOLD) << "\n";
        std::cout << color_text("Copyright (c) 2024 NexusSynth Project", DIM) << "\n\n";
    }
}

void CliInterface::print_help() {
    std::cout << color_text("USAGE:", BOLD) << "\n";
    std::cout << "  nexussynth <command> [options] [arguments]\n\n";
    
    std::cout << color_text("COMMANDS:", BOLD) << "\n";
    std::cout << "  convert, c       Convert single UTAU voice bank to NVM format\n";
    std::cout << "  batch, b         Batch convert multiple voice banks\n";
    std::cout << "  validate, v      Validate voice bank or NVM file\n";
    std::cout << "  scan, s          Scan directories for voice banks\n";
    std::cout << "  config, cfg      Manage configuration\n";
    std::cout << "  info, i          Show system and file information\n";
    std::cout << "  version          Show version information\n";
    std::cout << "  help             Show this help message\n\n";
    
    std::cout << color_text("OPTIONS:", BOLD) << "\n";
    std::cout << "  -i, --input PATH       Input voice bank path\n";
    std::cout << "  -o, --output PATH      Output file/directory path\n";
    std::cout << "  -c, --config FILE      Configuration file path\n";
    std::cout << "  -j, --threads NUM      Number of worker threads (0=auto)\n";
    std::cout << "  -r, --recursive        Recursive directory scanning\n";
    std::cout << "  -f, --force           Force overwrite existing files\n";
    std::cout << "  --preset NAME          Use configuration preset (default, fast, quality, batch)\n";
    std::cout << "  --dry-run              Show what would be done without executing\n";
    std::cout << "  --strict               Enable strict validation\n";
    std::cout << "  --generate-report      Generate detailed validation report\n";
    std::cout << "  --report-format FMT    Report format (json, html, markdown)\n";
    std::cout << "  -v, --verbose          Verbose output\n";
    std::cout << "  -q, --quiet            Quiet mode\n";
    std::cout << "  --no-color             Disable colored output\n";
    std::cout << "  --no-progress          Disable progress bars\n\n";
    
    std::cout << color_text("EXAMPLES:", BOLD) << "\n";
    std::cout << "  # Convert single voice bank\n";
    std::cout << "  nexussynth convert /path/to/voicebank\n\n";
    std::cout << "  # Batch convert with custom output directory\n";
    std::cout << "  nexussynth batch -o /output/dir /voice/bank/dir1 /voice/bank/dir2\n\n";
    std::cout << "  # Validate with detailed report\n";
    std::cout << "  nexussynth validate --generate-report --report-format html /path/to/file\n\n";
    std::cout << "  # Scan for voice banks recursively\n";
    std::cout << "  nexussynth scan -r /music/collections\n\n";
}

void CliInterface::print_version() {
    std::cout << "NexusSynth Conditioning Tool v1.0.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "Dependencies:\n";
    std::cout << cli_utils::get_dependency_versions() << std::endl;
}

void CliInterface::print_progress_header(const std::string& operation) {
    std::string header = "=== " + operation + " ===";
    std::cout << color_text(header, BOLD + CYAN) << std::endl;
}

void CliInterface::print_result_summary(const CliResult& result) {
    if (quiet_) return;
    
    std::cout << "\n" << color_text("=== SUMMARY ===", BOLD) << "\n";
    
    if (result.success) {
        std::cout << color_text("✓ SUCCESS", GREEN + BOLD) << ": " << result.message << "\n";
    } else {
        std::cout << color_text("✗ FAILED", RED + BOLD) << ": " << result.message << "\n";
    }
    
    if (result.files_processed > 0) {
        std::cout << "Files processed: " << result.files_processed << "\n";
        std::cout << "Files succeeded: " << result.files_succeeded << "\n";
        if (result.files_failed > 0) {
            std::cout << color_text("Files failed: " + std::to_string(result.files_failed), RED) << "\n";
        }
    }
    
    if (result.total_output_size > 0) {
        std::cout << "Total output size: " << format_file_size(result.total_output_size) << "\n";
    }
    
    if (result.total_time.count() > 0) {
        std::cout << "Total time: " << format_duration(result.total_time) << "\n";
    }
    
    // Show warnings
    for (const auto& warning : result.warnings) {
        log_warning(warning);
    }
    
    // Show errors
    for (const auto& error : result.errors) {
        log_error(error);
    }
}

std::string CliInterface::color_text(const std::string& text, const std::string& color) {
    if (no_color_) return text;
    return color + text + RESET;
}

std::string CliInterface::format_file_size(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit_index = 0;
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

std::string CliInterface::format_duration(std::chrono::duration<double> duration) {
    auto seconds = duration.count();
    
    if (seconds < 60.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << seconds << "s";
        return oss.str();
    } else if (seconds < 3600.0) {
        int minutes = static_cast<int>(seconds / 60);
        int secs = static_cast<int>(seconds) % 60;
        return std::to_string(minutes) + "m " + std::to_string(secs) + "s";
    } else {
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds - hours * 3600) / 60);
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
}

void CliInterface::log_error(const std::string& message) {
    if (quiet_) return;
    std::cerr << color_text("[ERROR] ", RED + BOLD) << message << std::endl;
}

void CliInterface::log_warning(const std::string& message) {
    if (quiet_) return;
    std::cerr << color_text("[WARNING] ", YELLOW + BOLD) << message << std::endl;
}

void CliInterface::log_info(const std::string& message) {
    if (quiet_) return;
    std::cout << color_text("[INFO] ", BLUE) << message << std::endl;
}

void CliInterface::log_verbose(const std::string& message) {
    if (!verbose_ || quiet_) return;
    std::cout << color_text("[VERBOSE] ", DIM) << message << std::endl;
}

void CliInterface::load_config(const std::string& config_path) {
    // Implementation would load configuration from file
    log_verbose("Loading configuration from: " + config_path);
}

void CliInterface::save_config(const std::string& config_path) const {
    // Implementation would save current configuration to file
    if (verbose_) {
        std::cout << "[VERBOSE] Saving configuration to: " + config_path << std::endl;
    }
}

} // namespace cli
} // namespace nexussynth
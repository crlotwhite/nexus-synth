#include "nexussynth/cli_interface.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>

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

// CliProgressCallback implementation
CliProgressCallback::CliProgressCallback(bool verbose, bool no_color)
    : verbose_(verbose), no_color_(no_color), current_job_(0), total_jobs_(0) {
    last_update_ = std::chrono::steady_clock::now();
}

void CliProgressCallback::on_batch_started(size_t total_jobs) {
    total_jobs_ = total_jobs;
    current_job_ = 0;
    last_update_ = std::chrono::steady_clock::now();
    
    std::cout << color_text("Starting batch processing of " + std::to_string(total_jobs) + " jobs...", BOLD) << std::endl;
}

void CliProgressCallback::on_batch_completed(const conditioning::BatchProcessingStats& stats) {
    std::cout << "\n" << color_text("Batch processing completed!", GREEN + BOLD) << std::endl;
    std::cout << "Total jobs: " << stats.total_jobs << std::endl;
    std::cout << "Completed: " << color_text(std::to_string(stats.completed_jobs), GREEN) << std::endl;
    if (stats.failed_jobs > 0) {
        std::cout << "Failed: " << color_text(std::to_string(stats.failed_jobs), RED) << std::endl;
    }
    std::cout << "Total processing time: " << format_duration(std::chrono::duration<double, std::milli>(stats.total_processing_time_ms)) << std::endl;
    std::cout << "Average per job: " << format_duration(std::chrono::duration<double, std::milli>(stats.average_processing_time_ms)) << std::endl;
    std::cout << "Total output size: " << format_file_size(stats.total_output_size_bytes) << std::endl;
    std::cout << "Peak memory usage: " << std::fixed << std::setprecision(1) << stats.peak_memory_usage_mb << " MB" << std::endl;
}

void CliProgressCallback::on_batch_progress(const conditioning::BatchProcessingStats& stats) {
    // Only update progress every 500ms to avoid spam
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_).count() < 500) {
        return;
    }
    last_update_ = now;
    
    size_t total_processed = stats.completed_jobs + stats.failed_jobs;
    print_progress_bar(total_processed, stats.total_jobs);
    
    if (verbose_) {
        std::cout << " [" << stats.active_jobs << " active, " << stats.queued_jobs << " queued]";
        std::cout << " Memory: " << std::fixed << std::setprecision(1) << stats.current_memory_usage_mb << " MB";
    }
    
    std::cout << "\r" << std::flush;
}

void CliProgressCallback::on_job_started(const conditioning::BatchJob& job) {
    current_job_++;
    
    if (verbose_) {
        std::cout << "\n" << color_text("[" + std::to_string(current_job_) + "/" + std::to_string(total_jobs_) + "]", CYAN) 
                  << " Starting: " << job.voice_bank_name << std::endl;
        std::cout << "  Input: " << job.input_path << std::endl;
        std::cout << "  Output: " << job.output_path << std::endl;
    }
}

void CliProgressCallback::on_job_completed(const conditioning::BatchJob& job, const conditioning::JobResult& result) {
    if (verbose_) {
        std::cout << "\n" << color_text("✓", GREEN) << " Completed: " << job.voice_bank_name;
        std::cout << " (" << format_duration(result.processing_time) << ")";
        std::cout << " -> " << format_file_size(result.output_file_size_bytes);
        
        if (result.compression_ratio > 0) {
            std::cout << " (compression: " << std::fixed << std::setprecision(1) << result.compression_ratio << "x)";
        }
        
        if (result.estimated_quality_score > 0) {
            std::cout << " quality: " << std::fixed << std::setprecision(2) << result.estimated_quality_score;
        }
        
        std::cout << std::endl;
        
        if (!result.warnings.empty()) {
            for (const auto& warning : result.warnings) {
                std::cout << "  " << color_text("Warning:", YELLOW) << " " << warning << std::endl;
            }
        }
    }
}

void CliProgressCallback::on_job_failed(const conditioning::BatchJob& job, const std::string& error) {
    std::cout << "\n" << color_text("✗", RED + BOLD) << " Failed: " << job.voice_bank_name << std::endl;
    std::cout << "  " << color_text("Error:", RED) << " " << error << std::endl;
}

void CliProgressCallback::on_eta_updated(const std::chrono::system_clock::time_point& estimated_completion) {
    if (!verbose_) return;
    
    auto now = std::chrono::system_clock::now();
    auto remaining = estimated_completion - now;
    
    if (remaining > std::chrono::seconds(0)) {
        auto remaining_duration = std::chrono::duration_cast<std::chrono::duration<double>>(remaining);
        std::cout << " ETA: " << format_duration(remaining_duration);
    }
}

void CliProgressCallback::print_progress_bar(size_t current, size_t total, int width) {
    if (total == 0) return;
    
    float progress = static_cast<float>(current) / static_cast<float>(total);
    int filled_width = static_cast<int>(progress * width);
    
    std::cout << "\r[";
    for (int i = 0; i < width; ++i) {
        if (i < filled_width) {
            std::cout << color_text("█", GREEN);
        } else {
            std::cout << color_text("░", DIM);
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0f) << "%";
    std::cout << " (" << current << "/" << total << ")";
}

std::string CliProgressCallback::color_text(const std::string& text, const std::string& color) {
    if (no_color_) return text;
    return color + text + RESET;
}

std::string CliProgressCallback::format_duration(std::chrono::duration<double> duration) {
    auto seconds = duration.count();
    
    if (seconds < 1.0) {
        return std::to_string(static_cast<int>(seconds * 1000)) + "ms";
    } else if (seconds < 60.0) {
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

std::string CliProgressCallback::format_file_size(size_t bytes) {
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

// CliValidationCallback implementation
CliValidationCallback::CliValidationCallback(bool verbose, bool no_color)
    : verbose_(verbose), no_color_(no_color), issue_count_(0) {}

void CliValidationCallback::on_validation_started(const std::string& file_path) {
    issue_count_ = 0;
    std::cout << color_text("Validating: ", BOLD) << file_path << std::endl;
}

void CliValidationCallback::on_validation_progress(size_t current_step, size_t total_steps, 
                                                  const std::string& current_task) {
    if (!verbose_) return;
    
    float progress = static_cast<float>(current_step) / static_cast<float>(total_steps);
    std::cout << "\r" << color_text("Progress: ", DIM) << std::fixed << std::setprecision(1) 
              << (progress * 100.0f) << "% - " << current_task << std::flush;
}

void CliValidationCallback::on_validation_completed(const validation::ValidationReport& report) {
    if (verbose_) {
        std::cout << "\n";
    }
    
    std::cout << color_text("Validation completed:", BOLD) << std::endl;
    
    if (report.is_valid) {
        std::cout << color_text("✓ VALID", GREEN + BOLD) << " - File passed all validation checks" << std::endl;
    } else if (report.is_usable) {
        std::cout << color_text("⚠ USABLE", YELLOW + BOLD) << " - File has issues but can be used" << std::endl;
    } else {
        std::cout << color_text("✗ INVALID", RED + BOLD) << " - File has critical issues" << std::endl;
    }
    
    std::cout << "Total issues: " << report.total_issues << std::endl;
    
    if (report.info_count > 0) {
        std::cout << "  " << color_text("Info: " + std::to_string(report.info_count), BLUE) << std::endl;
    }
    if (report.warning_count > 0) {
        std::cout << "  " << color_text("Warnings: " + std::to_string(report.warning_count), YELLOW) << std::endl;
    }
    if (report.error_count > 0) {
        std::cout << "  " << color_text("Errors: " + std::to_string(report.error_count), RED) << std::endl;
    }
    if (report.critical_count > 0) {
        std::cout << "  " << color_text("Critical: " + std::to_string(report.critical_count), RED + BOLD) << std::endl;
    }
    
    // Show quality metrics
    if (report.quality_metrics.overall_score > 0) {
        std::cout << "Quality Score: " << std::fixed << std::setprecision(2) 
                  << (report.quality_metrics.overall_score * 100.0) << "%" << std::endl;
    }
    
    // Show file analysis
    if (report.file_analysis.model_count.has_value()) {
        std::cout << "Models: " << report.file_analysis.model_count.value() << std::endl;
    }
    if (report.file_analysis.phoneme_count.has_value()) {
        std::cout << "Phonemes: " << report.file_analysis.phoneme_count.value() << std::endl;
    }
    if (report.file_analysis.file_size.has_value()) {
        std::cout << "File size: " << format_file_size(report.file_analysis.file_size.value()) << std::endl;
    }
}

void CliValidationCallback::on_issue_found(const validation::ValidationIssue& issue) {
    issue_count_++;
    
    if (!verbose_ && issue.severity < validation::ValidationSeverity::ERROR) {
        return; // Only show errors and critical issues in non-verbose mode
    }
    
    std::string severity_str;
    switch (issue.severity) {
        case validation::ValidationSeverity::INFO:
            severity_str = color_text("[INFO]", BLUE);
            break;
        case validation::ValidationSeverity::WARNING:
            severity_str = color_text("[WARNING]", YELLOW);
            break;
        case validation::ValidationSeverity::ERROR:
            severity_str = color_text("[ERROR]", RED);
            break;
        case validation::ValidationSeverity::CRITICAL:
            severity_str = color_text("[CRITICAL]", RED + BOLD);
            break;
    }
    
    std::cout << severity_str << " " << issue.title;
    
    if (!issue.location.empty()) {
        std::cout << " (" << issue.location << ")";
    }
    
    std::cout << std::endl;
    
    if (verbose_ && !issue.description.empty()) {
        std::cout << "  " << issue.description << std::endl;
    }
    
    if (issue.suggestion.has_value()) {
        std::cout << "  " << color_text("Suggestion:", CYAN) << " " << issue.suggestion.value() << std::endl;
    }
}

void CliValidationCallback::on_critical_error(const std::string& error_message) {
    std::cout << color_text("[CRITICAL ERROR]", RED + BOLD) << " " << error_message << std::endl;
}

std::string CliValidationCallback::color_text(const std::string& text, const std::string& color) {
    if (no_color_) return text;
    return color + text + RESET;
}

std::string CliValidationCallback::severity_color(validation::ValidationSeverity severity) {
    if (no_color_) return "";
    
    switch (severity) {
        case validation::ValidationSeverity::INFO: return BLUE;
        case validation::ValidationSeverity::WARNING: return YELLOW;
        case validation::ValidationSeverity::ERROR: return RED;
        case validation::ValidationSeverity::CRITICAL: return RED + BOLD;
        default: return "";
    }
}

std::string CliValidationCallback::format_file_size(size_t bytes) {
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

// CliScanCallback implementation
CliScanCallback::CliScanCallback(bool verbose, bool no_color)
    : verbose_(verbose), no_color_(no_color), found_count_(0), valid_count_(0) {}

void CliScanCallback::on_scan_started(const std::string& path) {
    found_count_ = 0;
    valid_count_ = 0;
    std::cout << color_text("Scanning directory: ", BOLD) << path << std::endl;
}

void CliScanCallback::on_directory_entered(const std::string& path, size_t depth) {
    if (!verbose_) return;
    
    std::string indent(depth * 2, ' ');
    std::cout << color_text(indent + "Entering: ", DIM) << path << std::endl;
}

void CliScanCallback::on_voicebank_found(const std::string& path) {
    found_count_++;
    
    if (verbose_) {
        std::cout << color_text("Found voice bank: ", GREEN) << path << std::endl;
    }
}

void CliScanCallback::on_voicebank_validated(const std::string& path, bool is_valid) {
    if (is_valid) {
        valid_count_++;
        if (verbose_) {
            std::cout << color_text("✓ Valid: ", GREEN) << path << std::endl;
        }
    } else if (verbose_) {
        std::cout << color_text("✗ Invalid: ", RED) << path << std::endl;
    }
}

void CliScanCallback::on_scan_progress(size_t current, size_t total) {
    if (total > 0) {
        print_progress_bar(current, total);
        std::cout << " [" << found_count_ << " found, " << valid_count_ << " valid]";
        std::cout << "\r" << std::flush;
    }
}

void CliScanCallback::on_scan_completed(const conditioning::VoicebankDiscovery& result) {
    std::cout << "\n" << color_text("Scan completed!", GREEN + BOLD) << std::endl;
    std::cout << "Directories scanned: " << result.directories_scanned << std::endl;
    std::cout << "Files scanned: " << result.files_scanned << std::endl;
    std::cout << "Voice banks found: " << result.voicebank_paths.size() << std::endl;
    std::cout << "Valid voice banks: " << color_text(std::to_string(result.valid_voicebanks), GREEN) << std::endl;
    if (result.invalid_voicebanks > 0) {
        std::cout << "Invalid voice banks: " << color_text(std::to_string(result.invalid_voicebanks), RED) << std::endl;
    }
    if (result.partial_voicebanks > 0) {
        std::cout << "Partial voice banks: " << color_text(std::to_string(result.partial_voicebanks), YELLOW) << std::endl;
    }
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::duration<double>>(result.scan_duration);
    std::cout << "Scan duration: " << std::fixed << std::setprecision(2) << duration_ms.count() << "s" << std::endl;
}

void CliScanCallback::on_scan_error(const std::string& path, const std::string& error) {
    std::cout << color_text("Scan error: ", RED) << path << " - " << error << std::endl;
}

void CliScanCallback::print_progress_bar(size_t current, size_t total, int width) {
    if (total == 0) return;
    
    float progress = static_cast<float>(current) / static_cast<float>(total);
    int filled_width = static_cast<int>(progress * width);
    
    std::cout << "\r[";
    for (int i = 0; i < width; ++i) {
        if (i < filled_width) {
            std::cout << color_text("█", GREEN);
        } else {
            std::cout << color_text("░", DIM);
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0f) << "%";
}

std::string CliScanCallback::color_text(const std::string& text, const std::string& color) {
    if (no_color_) return text;
    return color + text + RESET;
}

} // namespace cli
} // namespace nexussynth
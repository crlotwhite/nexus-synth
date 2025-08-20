#include "nexussynth/validation_system.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <regex>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace nexussynth {
namespace validation {

// ValidationEngine implementation
ValidationEngine::ValidationEngine() {
    // Initialize with default rules
}

ValidationEngine::ValidationEngine(const ParameterValidationRules& rules) 
    : rules_(rules) {
}

ValidationReport ValidationEngine::validate_nvm_file(const std::string& file_path) {
    ValidationReport report;
    report.file_path = file_path;
    report.validation_id = generate_unique_id();
    report.validation_time = std::chrono::system_clock::now();
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (progress_callback_) {
            progress_callback_->on_validation_started(file_path);
        }
        
        // Basic file existence and format checks
        auto file_issues = validate_file_structure(file_path);
        report.issues.insert(report.issues.end(), file_issues.begin(), file_issues.end());
        
        // Update progress
        if (progress_callback_) {
            progress_callback_->on_validation_progress(1, 3, "Checking file structure");
        }
        
        // Check file size and basic properties
        namespace fs = std::filesystem;
        if (fs::exists(file_path)) {
            auto file_size = fs::file_size(file_path);
            report.file_analysis.file_size = file_size;
            report.file_analysis.file_format = "nvm";
            
            // Basic size validation
            if (file_size == 0) {
                ValidationIssue issue("EMPTY_FILE", ValidationSeverity::CRITICAL,
                                    ValidationCategory::FILE_STRUCTURE, "File is empty");
                issue.description = "The NVM file has zero size";
                report.issues.push_back(issue);
            } else if (file_size > rules_.max_total_file_size_bytes) {
                ValidationIssue issue("FILE_TOO_LARGE", ValidationSeverity::WARNING,
                                    ValidationCategory::FILE_STRUCTURE, "File is very large");
                issue.description = "File size exceeds recommended maximum";
                report.issues.push_back(issue);
            }
        }
        
        // Update progress
        if (progress_callback_) {
            progress_callback_->on_validation_progress(2, 3, "Validating file content");
        }
        
        // Basic header validation
        std::ifstream file(file_path, std::ios::binary);
        if (file.is_open()) {
            uint32_t magic;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            
            if (magic != 0x314D564E) { // 'NVM1' magic number
                ValidationIssue issue("INVALID_MAGIC", ValidationSeverity::CRITICAL,
                                    ValidationCategory::NVM_INTEGRITY, "Invalid file format");
                issue.description = "File does not have valid NVM magic number";
                report.issues.push_back(issue);
            }
        } else {
            ValidationIssue issue("FILE_READ_ERROR", ValidationSeverity::CRITICAL,
                                ValidationCategory::FILE_STRUCTURE, "Cannot read file");
            issue.description = "Unable to open file for reading";
            report.issues.push_back(issue);
        }
        
    } catch (const std::exception& e) {
        ValidationIssue issue("VALIDATION_ERROR", ValidationSeverity::CRITICAL,
                            ValidationCategory::NVM_INTEGRITY, "Validation failed");
        issue.description = std::string("Exception during validation: ") + e.what();
        report.issues.push_back(issue);
    }
    
    // Calculate summary statistics
    report.total_issues = report.issues.size();
    for (const auto& issue : report.issues) {
        switch (issue.severity) {
            case ValidationSeverity::INFO: report.info_count++; break;
            case ValidationSeverity::WARNING: report.warning_count++; break;
            case ValidationSeverity::ERROR: report.error_count++; break;
            case ValidationSeverity::CRITICAL: report.critical_count++; break;
        }
    }
    
    // Determine overall validity
    report.is_valid = (report.critical_count == 0 && report.error_count == 0);
    report.is_usable = (report.critical_count == 0);
    
    // Calculate basic quality score
    if (report.total_issues == 0) {
        report.quality_metrics.overall_score = 1.0;
    } else {
        double penalty = 0.0;
        penalty += report.critical_count * 0.5;
        penalty += report.error_count * 0.2;
        penalty += report.warning_count * 0.05;
        penalty += report.info_count * 0.01;
        report.quality_metrics.overall_score = std::max(0.0, 1.0 - penalty);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    report.validation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (progress_callback_) {
        progress_callback_->on_validation_completed(report);
    }
    
    return report;
}

ValidationReport ValidationEngine::validate_utau_voicebank(const std::string& voicebank_path) {
    ValidationReport report;
    report.file_path = voicebank_path;
    report.validation_id = generate_unique_id();
    report.validation_time = std::chrono::system_clock::now();
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (progress_callback_) {
            progress_callback_->on_validation_started(voicebank_path);
        }
        
        // Validate UTAU structure
        auto structure_issues = validate_utau_structure(voicebank_path);
        report.issues.insert(report.issues.end(), structure_issues.begin(), structure_issues.end());
        
        // Set file analysis info
        report.file_analysis.file_format = "utau";
        
        namespace fs = std::filesystem;
        if (fs::exists(voicebank_path) && fs::is_directory(voicebank_path)) {
            // Count files
            size_t audio_count = 0;
            size_t total_size = 0;
            
            try {
                for (const auto& entry : fs::recursive_directory_iterator(voicebank_path)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        
                        if (ext == ".wav" || ext == ".flac" || ext == ".aif" || ext == ".aiff") {
                            audio_count++;
                        }
                        
                        total_size += entry.file_size();
                    }
                }
            } catch (const std::exception&) {
                // Continue validation even if file counting fails
            }
            
            report.file_analysis.file_size = total_size;
            
            // Estimate model count based on audio files
            if (audio_count > 0) {
                report.file_analysis.model_count = audio_count;
                report.file_analysis.phoneme_count = std::min(audio_count, static_cast<size_t>(100)); // Rough estimate
            }
        }
        
    } catch (const std::exception& e) {
        ValidationIssue issue("VALIDATION_ERROR", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "Validation failed");
        issue.description = std::string("Exception during validation: ") + e.what();
        report.issues.push_back(issue);
    }
    
    // Calculate summary statistics
    report.total_issues = report.issues.size();
    for (const auto& issue : report.issues) {
        switch (issue.severity) {
            case ValidationSeverity::INFO: report.info_count++; break;
            case ValidationSeverity::WARNING: report.warning_count++; break;
            case ValidationSeverity::ERROR: report.error_count++; break;
            case ValidationSeverity::CRITICAL: report.critical_count++; break;
        }
    }
    
    // Determine overall validity
    report.is_valid = (report.critical_count == 0 && report.error_count == 0);
    report.is_usable = (report.critical_count == 0);
    
    // Calculate basic quality score
    if (report.total_issues == 0) {
        report.quality_metrics.overall_score = 1.0;
    } else {
        double penalty = 0.0;
        penalty += report.critical_count * 0.5;
        penalty += report.error_count * 0.2;
        penalty += report.warning_count * 0.05;
        penalty += report.info_count * 0.01;
        report.quality_metrics.overall_score = std::max(0.0, 1.0 - penalty);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    report.validation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (progress_callback_) {
        progress_callback_->on_validation_completed(report);
    }
    
    return report;
}

std::vector<ValidationIssue> ValidationEngine::validate_file_structure(const std::string& file_path) {
    std::vector<ValidationIssue> issues;
    
    namespace fs = std::filesystem;
    
    // Check if file exists
    if (!fs::exists(file_path)) {
        ValidationIssue issue("FILE_NOT_FOUND", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "File not found");
        issue.description = "The specified file does not exist";
        issue.location = file_path;
        issues.push_back(issue);
        return issues;
    }
    
    // Check if it's a regular file
    if (fs::is_directory(file_path)) {
        ValidationIssue issue("IS_DIRECTORY", ValidationSeverity::ERROR,
                            ValidationCategory::FILE_STRUCTURE, "Path is a directory");
        issue.description = "Expected a file but found a directory";
        issue.location = file_path;
        issues.push_back(issue);
        return issues;
    }
    
    // Check file permissions
    if (!is_file_accessible(file_path)) {
        ValidationIssue issue("FILE_ACCESS_DENIED", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "Cannot access file");
        issue.description = "File exists but cannot be read";
        issue.location = file_path;
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<ValidationIssue> ValidationEngine::validate_utau_structure(const std::string& voicebank_path) {
    std::vector<ValidationIssue> issues;
    
    namespace fs = std::filesystem;
    
    // Check if directory exists
    if (!fs::exists(voicebank_path)) {
        ValidationIssue issue("VOICEBANK_NOT_FOUND", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "Voice bank directory not found");
        issue.description = "The specified voice bank directory does not exist";
        issue.location = voicebank_path;
        issues.push_back(issue);
        return issues;
    }
    
    if (!fs::is_directory(voicebank_path)) {
        ValidationIssue issue("NOT_DIRECTORY", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "Path is not a directory");
        issue.description = "Expected a directory but found a file";
        issue.location = voicebank_path;
        issues.push_back(issue);
        return issues;
    }
    
    fs::path voice_path(voicebank_path);
    
    // Check for oto.ini
    fs::path oto_path = voice_path / "oto.ini";
    if (!fs::exists(oto_path)) {
        ValidationIssue issue("MISSING_OTO_INI", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "Missing oto.ini file");
        issue.description = "UTAU voice banks require an oto.ini file";
        issue.location = voicebank_path;
        issue.suggestion = "Create an oto.ini file with timing information for audio files";
        issues.push_back(issue);
    }
    
    // Check for audio files
    bool has_audio = false;
    std::vector<std::string> audio_extensions = {".wav", ".flac", ".aif", ".aiff"};
    
    try {
        for (const auto& entry : fs::directory_iterator(voice_path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (std::find(audio_extensions.begin(), audio_extensions.end(), ext) != audio_extensions.end()) {
                    has_audio = true;
                    break;
                }
            }
        }
    } catch (const std::exception&) {
        ValidationIssue issue("DIRECTORY_READ_ERROR", ValidationSeverity::ERROR,
                            ValidationCategory::FILE_STRUCTURE, "Cannot read directory");
        issue.description = "Unable to scan voice bank directory";
        issue.location = voicebank_path;
        issues.push_back(issue);
    }
    
    if (!has_audio) {
        ValidationIssue issue("NO_AUDIO_FILES", ValidationSeverity::CRITICAL,
                            ValidationCategory::FILE_STRUCTURE, "No audio files found");
        issue.description = "Voice bank contains no supported audio files";
        issue.location = voicebank_path;
        issue.suggestion = "Add WAV or FLAC audio files to the voice bank directory";
        issues.push_back(issue);
    }
    
    // Check for character.txt (optional but recommended)
    fs::path char_path = voice_path / "character.txt";
    if (!fs::exists(char_path)) {
        ValidationIssue issue("MISSING_CHARACTER_TXT", ValidationSeverity::INFO,
                            ValidationCategory::METADATA_VALIDITY, "Missing character.txt");
        issue.description = "character.txt file provides voice bank metadata";
        issue.location = voicebank_path;
        issue.suggestion = "Add character.txt with voice bank information";
        issues.push_back(issue);
    }
    
    return issues;
}

bool ValidationEngine::export_report(const ValidationReport& report, 
                                   const std::string& output_path,
                                   const std::string& format) {
    try {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            return false;
        }
        
        if (format == "json") {
            file << generate_json_report(report);
        } else if (format == "html") {
            file << generate_html_report(report);
        } else if (format == "markdown") {
            file << generate_markdown_report(report);
        } else {
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string ValidationEngine::generate_json_report(const ValidationReport& report) {
    // Simple JSON generation (would use proper JSON library in production)
    std::ostringstream json;
    json << "{\n";
    json << "  \"file_path\": \"" << report.file_path << "\",\n";
    json << "  \"validation_time\": \"" << std::chrono::duration_cast<std::chrono::seconds>(
        report.validation_time.time_since_epoch()).count() << "\",\n";
    json << "  \"is_valid\": " << (report.is_valid ? "true" : "false") << ",\n";
    json << "  \"is_usable\": " << (report.is_usable ? "true" : "false") << ",\n";
    json << "  \"total_issues\": " << report.total_issues << ",\n";
    json << "  \"critical_count\": " << report.critical_count << ",\n";
    json << "  \"error_count\": " << report.error_count << ",\n";
    json << "  \"warning_count\": " << report.warning_count << ",\n";
    json << "  \"info_count\": " << report.info_count << ",\n";
    json << "  \"quality_score\": " << report.quality_metrics.overall_score << "\n";
    json << "}\n";
    return json.str();
}

std::string ValidationEngine::generate_html_report(const ValidationReport& report) {
    std::ostringstream html;
    html << "<html><head><title>Validation Report</title></head><body>\n";
    html << "<h1>Validation Report</h1>\n";
    html << "<p><strong>File:</strong> " << report.file_path << "</p>\n";
    html << "<p><strong>Status:</strong> " << (report.is_valid ? "Valid" : "Invalid") << "</p>\n";
    html << "<p><strong>Total Issues:</strong> " << report.total_issues << "</p>\n";
    html << "<p><strong>Quality Score:</strong> " << (report.quality_metrics.overall_score * 100) << "%</p>\n";
    html << "</body></html>\n";
    return html.str();
}

std::string ValidationEngine::generate_markdown_report(const ValidationReport& report) {
    std::ostringstream md;
    md << "# Validation Report\n\n";
    md << "**File:** " << report.file_path << "\n\n";
    md << "**Status:** " << (report.is_valid ? "✅ Valid" : "❌ Invalid") << "\n\n";
    md << "**Total Issues:** " << report.total_issues << "\n\n";
    md << "**Quality Score:** " << (report.quality_metrics.overall_score * 100) << "%\n\n";
    
    if (!report.issues.empty()) {
        md << "## Issues\n\n";
        for (const auto& issue : report.issues) {
            md << "- **" << issue.title << "**: " << issue.description << "\n";
        }
    }
    
    return md.str();
}

// Utility methods
std::string ValidationEngine::generate_unique_id() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "val_" << time_t;
    return oss.str();
}

bool ValidationEngine::is_file_accessible(const std::string& file_path) {
    std::ifstream file(file_path);
    return file.is_open();
}

size_t ValidationEngine::get_file_size(const std::string& file_path) {
    try {
        return std::filesystem::file_size(file_path);
    } catch (const std::exception&) {
        return 0;
    }
}

// Progress callback implementations
void ConsoleValidationProgressCallback::on_validation_started(const std::string& file_path) {
    if (!verbose_) return;
    std::cout << "Starting validation of: " << file_path << std::endl;
}

void ConsoleValidationProgressCallback::on_validation_progress(size_t current_step, size_t total_steps, 
                                                             const std::string& current_task) {
    if (!verbose_) return;
    double progress = static_cast<double>(current_step) / total_steps * 100.0;
    std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "% - " << current_task << std::flush;
}

void ConsoleValidationProgressCallback::on_validation_completed(const ValidationReport& report) {
    if (verbose_) std::cout << std::endl;
    
    std::string status = report.is_valid ? "VALID" : (report.is_usable ? "USABLE" : "INVALID");
    std::cout << "Validation completed: " << status;
    
    if (report.total_issues > 0) {
        std::cout << " (" << report.total_issues << " issues)";
    }
    
    std::cout << std::endl;
}

void ConsoleValidationProgressCallback::on_issue_found(const ValidationIssue& issue) {
    if (!verbose_ && issue.severity < ValidationSeverity::ERROR) return;
    
    std::string severity_str;
    switch (issue.severity) {
        case ValidationSeverity::INFO: severity_str = "[INFO]"; break;
        case ValidationSeverity::WARNING: severity_str = "[WARNING]"; break;
        case ValidationSeverity::ERROR: severity_str = "[ERROR]"; break;
        case ValidationSeverity::CRITICAL: severity_str = "[CRITICAL]"; break;
    }
    
    std::cout << severity_str << " " << issue.title;
    if (!issue.location.empty()) {
        std::cout << " (" << issue.location << ")";
    }
    std::cout << std::endl;
}

void ConsoleValidationProgressCallback::on_critical_error(const std::string& error_message) {
    std::cout << "[CRITICAL ERROR] " << error_message << std::endl;
}

} // namespace validation
} // namespace nexussynth
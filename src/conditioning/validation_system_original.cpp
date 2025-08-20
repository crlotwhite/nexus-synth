#include "nexussynth/validation_system.h"
#include "nexussynth/utau_logger.h"
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cmath>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace nexussynth {
namespace validation {

    // =============================================================================
    // ValidationEngine Implementation
    // =============================================================================

    ValidationEngine::ValidationEngine() {
        // Initialize with default validation rules
        rules_ = ParameterValidationRules();
        LOG_INFO("ValidationEngine initialized with default rules");
    }

    ValidationEngine::ValidationEngine(const ParameterValidationRules& rules) 
        : rules_(rules) {
        LOG_INFO("ValidationEngine initialized with custom rules");
    }

    ValidationReport ValidationEngine::validate_nvm_file(const std::string& file_path) {
        auto start_time = std::chrono::steady_clock::now();
        ValidationReport report;
        report.file_path = file_path;
        report.validation_id = generate_unique_id();
        
        if (progress_callback_) {
            progress_callback_->on_validation_started(file_path);
        }
        
        LOG_INFO("Starting validation of NVM file: " + file_path);
        
        try {
            // Step 1: Basic file accessibility and format check
            report_progress(1, 8, "Checking file accessibility");
            auto file_issues = validate_file_format(file_path);
            report.issues.insert(report.issues.end(), file_issues.begin(), file_issues.end());
            
            if (!is_file_accessible(file_path)) {
                ValidationIssue critical_issue("FILE_NOT_ACCESSIBLE", ValidationSeverity::CRITICAL,
                                              ValidationCategory::FILE_STRUCTURE, "File not accessible");
                critical_issue.description = "Cannot access file at path: " + file_path;
                critical_issue.location = file_path;
                report.issues.push_back(critical_issue);
                report.is_valid = false;
                report.is_usable = false;
                return report;
            }
            
            // Step 2: Open and analyze NVM file
            report_progress(2, 8, "Opening NVM file");
            nvm::NvmFile nvm_file;
            if (!nvm_file.open(file_path)) {
                ValidationIssue critical_issue("NVM_OPEN_FAILED", ValidationSeverity::CRITICAL,
                                              ValidationCategory::NVM_INTEGRITY, "Failed to open NVM file");
                critical_issue.description = "Could not open or parse NVM file structure";
                critical_issue.location = file_path;
                report.issues.push_back(critical_issue);
                report.is_valid = false;
                report.is_usable = false;
                return report;
            }
            
            // Step 3: File structure validation
            report_progress(3, 8, "Validating file structure");
            auto structure_issues = validate_file_structure(file_path);
            report.issues.insert(report.issues.end(), structure_issues.begin(), structure_issues.end());
            
            // Step 4: NVM integrity validation
            report_progress(4, 8, "Validating NVM integrity");
            auto integrity_issues = validate_nvm_integrity(nvm_file);
            report.issues.insert(report.issues.end(), integrity_issues.begin(), integrity_issues.end());
            
            // Step 5: Parameter range validation
            report_progress(5, 8, "Validating parameter ranges");
            auto parameter_issues = validate_parameter_ranges(nvm_file);
            report.issues.insert(report.issues.end(), parameter_issues.begin(), parameter_issues.end());
            
            // Step 6: Model consistency validation
            report_progress(6, 8, "Validating model consistency");
            auto consistency_issues = validate_model_consistency(nvm_file);
            report.issues.insert(report.issues.end(), consistency_issues.begin(), consistency_issues.end());
            
            // Step 7: Phoneme coverage analysis
            report_progress(7, 8, "Analyzing phoneme coverage");
            auto phoneme_analysis = analyze_phoneme_coverage(nvm_file);
            if (progress_callback_) {
                progress_callback_->on_phoneme_analysis_completed(phoneme_analysis);
            }
            
            // Convert phoneme analysis to validation issues
            if (phoneme_analysis.coverage_percentage < 80.0) {
                ValidationIssue coverage_issue("LOW_PHONEME_COVERAGE", ValidationSeverity::WARNING,
                                             ValidationCategory::PHONEME_COVERAGE, "Low phoneme coverage");
                coverage_issue.description = "Phoneme coverage is " + 
                    std::to_string(phoneme_analysis.coverage_percentage) + "% (recommended: >80%)";
                coverage_issue.metadata["coverage_percentage"] = std::to_string(phoneme_analysis.coverage_percentage);
                coverage_issue.metadata["missing_count"] = std::to_string(phoneme_analysis.total_missing);
                report.issues.push_back(coverage_issue);
            }
            
            // Step 8: Calculate quality metrics and finalize report
            report_progress(8, 8, "Calculating quality metrics");
            
            // File analysis
            report.file_analysis.file_version = nvm::SemanticVersion(nvm_file.get_file_version());
            report.file_analysis.file_size = get_file_size(file_path);
            report.file_analysis.model_count = nvm_file.get_model_count();
            report.file_analysis.phoneme_count = phoneme_analysis.total_found;
            report.file_analysis.file_format = "nvm";
            
            // Quality metrics
            report.quality_metrics.completeness_score = calculate_completeness_score(phoneme_analysis);
            report.quality_metrics.consistency_score = calculate_consistency_score(report.issues);
            report.quality_metrics.integrity_score = calculate_integrity_score(report.issues);
            report.quality_metrics.overall_score = calculate_overall_quality_score(report);
            report.quality_metrics.missing_phonemes = 
                std::vector<std::string>(phoneme_analysis.missing_phonemes.begin(), 
                                       phoneme_analysis.missing_phonemes.end());
            
            // Count issues by severity and category
            for (const auto& issue : report.issues) {
                switch (issue.severity) {
                    case ValidationSeverity::INFO: report.info_count++; break;
                    case ValidationSeverity::WARNING: report.warning_count++; break;
                    case ValidationSeverity::ERROR: report.error_count++; break;
                    case ValidationSeverity::CRITICAL: report.critical_count++; break;
                }
                report.category_counts[issue.category]++;
            }
            
            report.total_issues = report.issues.size();
            report.is_valid = (report.critical_count == 0 && report.error_count == 0);
            report.is_usable = (report.critical_count == 0);
            
        } catch (const std::exception& e) {
            ValidationIssue exception_issue("VALIDATION_EXCEPTION", ValidationSeverity::CRITICAL,
                                           ValidationCategory::FILE_STRUCTURE, "Validation exception");
            exception_issue.description = "Exception during validation: " + std::string(e.what());
            exception_issue.location = file_path;
            report.issues.push_back(exception_issue);
            report.is_valid = false;
            report.is_usable = false;
            
            if (progress_callback_) {
                progress_callback_->on_critical_error("Validation exception: " + std::string(e.what()));
            }
        }
        
        report.validation_duration = get_elapsed_time(start_time);
        
        if (progress_callback_) {
            progress_callback_->on_validation_completed(report);
        }
        
        LOG_INFO("Validation completed for: " + file_path + 
                " (Issues: " + std::to_string(report.total_issues) + 
                ", Valid: " + (report.is_valid ? "Yes" : "No") + ")");
        
        return report;
    }

    ValidationReport ValidationEngine::validate_utau_voicebank(const std::string& voicebank_path) {
        auto start_time = std::chrono::steady_clock::now();
        ValidationReport report;
        report.file_path = voicebank_path;
        report.validation_id = generate_unique_id();
        
        if (progress_callback_) {
            progress_callback_->on_validation_started(voicebank_path);
        }
        
        LOG_INFO("Starting validation of UTAU voicebank: " + voicebank_path);
        
        try {
            // Step 1: Check if directory exists and is accessible
            report_progress(1, 6, "Checking voicebank directory");
            if (!std::filesystem::exists(voicebank_path) || !std::filesystem::is_directory(voicebank_path)) {
                ValidationIssue critical_issue("VOICEBANK_NOT_FOUND", ValidationSeverity::CRITICAL,
                                              ValidationCategory::FILE_STRUCTURE, "Voicebank directory not found");
                critical_issue.description = "Cannot access voicebank directory at: " + voicebank_path;
                critical_issue.location = voicebank_path;
                report.issues.push_back(critical_issue);
                report.is_valid = false;
                report.is_usable = false;
                return report;
            }
            
            // Step 2: Validate UTAU structure
            report_progress(2, 6, "Validating UTAU structure");
            auto structure_issues = validate_utau_structure(voicebank_path);
            report.issues.insert(report.issues.end(), structure_issues.begin(), structure_issues.end());
            
            // Step 3: Parse and validate oto.ini
            report_progress(3, 6, "Validating oto.ini entries");
            std::string oto_path = voicebank_path + "/oto.ini";
            if (std::filesystem::exists(oto_path)) {
                utau::UtauOtoParser parser;
                auto oto_entries = parser.parse_oto_file(oto_path);
                auto oto_issues = validate_oto_entries(oto_entries);
                report.issues.insert(report.issues.end(), oto_issues.begin(), oto_issues.end());
                
                // Step 4: Validate audio files
                report_progress(4, 6, "Validating audio files");
                auto audio_issues = validate_audio_files(voicebank_path, oto_entries);
                report.issues.insert(report.issues.end(), audio_issues.begin(), audio_issues.end());
                
                report.file_analysis.model_count = oto_entries.size();
            } else {
                ValidationIssue oto_missing("OTO_INI_MISSING", ValidationSeverity::CRITICAL,
                                          ValidationCategory::FILE_STRUCTURE, "oto.ini file missing");
                oto_missing.description = "Required oto.ini file not found in voicebank directory";
                oto_missing.location = oto_path;
                report.issues.push_back(oto_missing);
            }
            
            // Step 5: Phoneme coverage analysis
            report_progress(5, 6, "Analyzing phoneme coverage");
            auto phoneme_analysis = analyze_utau_phoneme_coverage(voicebank_path);
            if (progress_callback_) {
                progress_callback_->on_phoneme_analysis_completed(phoneme_analysis);
            }
            
            // Step 6: Finalize report
            report_progress(6, 6, "Finalizing report");
            
            // File analysis
            report.file_analysis.file_format = "utau";
            report.file_analysis.phoneme_count = phoneme_analysis.total_found;
            
            // Quality metrics
            report.quality_metrics.completeness_score = calculate_completeness_score(phoneme_analysis);
            report.quality_metrics.consistency_score = calculate_consistency_score(report.issues);
            report.quality_metrics.integrity_score = calculate_integrity_score(report.issues);
            report.quality_metrics.overall_score = calculate_overall_quality_score(report);
            
            // Count issues
            for (const auto& issue : report.issues) {
                switch (issue.severity) {
                    case ValidationSeverity::INFO: report.info_count++; break;
                    case ValidationSeverity::WARNING: report.warning_count++; break;
                    case ValidationSeverity::ERROR: report.error_count++; break;
                    case ValidationSeverity::CRITICAL: report.critical_count++; break;
                }
                report.category_counts[issue.category]++;
            }
            
            report.total_issues = report.issues.size();
            report.is_valid = (report.critical_count == 0 && report.error_count == 0);
            report.is_usable = (report.critical_count == 0);
            
        } catch (const std::exception& e) {
            ValidationIssue exception_issue("VALIDATION_EXCEPTION", ValidationSeverity::CRITICAL,
                                           ValidationCategory::FILE_STRUCTURE, "Validation exception");
            exception_issue.description = "Exception during validation: " + std::string(e.what());
            exception_issue.location = voicebank_path;
            report.issues.push_back(exception_issue);
            report.is_valid = false;
            report.is_usable = false;
        }
        
        report.validation_duration = get_elapsed_time(start_time);
        
        if (progress_callback_) {
            progress_callback_->on_validation_completed(report);
        }
        
        LOG_INFO("UTAU validation completed for: " + voicebank_path + 
                " (Issues: " + std::to_string(report.total_issues) + 
                ", Valid: " + (report.is_valid ? "Yes" : "No") + ")");
        
        return report;
    }

    std::vector<ValidationIssue> ValidationEngine::validate_file_structure(const std::string& file_path) {
        std::vector<ValidationIssue> issues;
        
        try {
            // Check file accessibility
            if (!is_file_accessible(file_path)) {
                ValidationIssue issue("FILE_NOT_ACCESSIBLE", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "File not accessible");
                issue.description = "Cannot read file at: " + file_path;
                issue.location = file_path;
                issues.push_back(issue);
                return issues;
            }
            
            // Check file size
            size_t file_size = get_file_size(file_path);
            if (file_size == 0) {
                ValidationIssue issue("EMPTY_FILE", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "File is empty");
                issue.description = "File has zero bytes";
                issue.location = file_path;
                issues.push_back(issue);
            } else if (file_size > rules_.max_total_file_size_bytes) {
                ValidationIssue issue("FILE_TOO_LARGE", ValidationSeverity::WARNING,
                                     ValidationCategory::FILE_STRUCTURE, "File is very large");
                issue.description = "File size (" + std::to_string(file_size / (1024*1024)) + 
                                   "MB) exceeds recommended maximum (" + 
                                   std::to_string(rules_.max_total_file_size_bytes / (1024*1024)) + "MB)";
                issue.location = file_path;
                issue.metadata["file_size_bytes"] = std::to_string(file_size);
                issues.push_back(issue);
            }
            
            // Check file extension
            std::filesystem::path path(file_path);
            std::string extension = path.extension().string();
            if (extension != ".nvm") {
                ValidationIssue issue("UNEXPECTED_EXTENSION", ValidationSeverity::WARNING,
                                     ValidationCategory::FILE_STRUCTURE, "Unexpected file extension");
                issue.description = "File has extension '" + extension + "' instead of '.nvm'";
                issue.location = file_path;
                issue.suggestion = "Consider renaming file to have .nvm extension";
                issues.push_back(issue);
            }
            
        } catch (const std::exception& e) {
            ValidationIssue issue("FILE_STRUCTURE_EXCEPTION", ValidationSeverity::ERROR,
                                 ValidationCategory::FILE_STRUCTURE, "Exception during file structure validation");
            issue.description = "Exception: " + std::string(e.what());
            issue.location = file_path;
            issues.push_back(issue);
        }
        
        return issues;
    }

    std::vector<ValidationIssue> ValidationEngine::validate_nvm_integrity(const nvm::NvmFile& nvm_file) {
        std::vector<ValidationIssue> issues;
        
        try {
            // Verify file integrity
            if (!nvm_file.verify_integrity()) {
                ValidationIssue issue("NVM_INTEGRITY_FAILED", ValidationSeverity::CRITICAL,
                                     ValidationCategory::NVM_INTEGRITY, "NVM file integrity check failed");
                issue.description = "File integrity verification failed - file may be corrupted";
                issue.suggestion = "Try re-creating the file from original source";
                issues.push_back(issue);
            }
            
            // Verify checksums if enabled
            if (!nvm_file.verify_checksums()) {
                ValidationIssue issue("CHECKSUM_VERIFICATION_FAILED", ValidationSeverity::ERROR,
                                     ValidationCategory::CHECKSUM_ERRORS, "Checksum verification failed");
                issue.description = "One or more checksums do not match - data corruption detected";
                issue.suggestion = "Regenerate file from original source";
                issues.push_back(issue);
            }
            
            // Check version compatibility
            auto file_version = nvm::SemanticVersion(nvm_file.get_file_version());
            auto version_issues = validate_version_compatibility(file_version);
            issues.insert(issues.end(), version_issues.begin(), version_issues.end());
            
            // Check model count
            size_t model_count = nvm_file.get_model_count();
            if (model_count == 0) {
                ValidationIssue issue("NO_MODELS", ValidationSeverity::CRITICAL,
                                     ValidationCategory::NVM_INTEGRITY, "No models in file");
                issue.description = "NVM file contains no voice models";
                issues.push_back(issue);
            } else if (model_count > rules_.max_models_per_file) {
                ValidationIssue issue("TOO_MANY_MODELS", ValidationSeverity::WARNING,
                                     ValidationCategory::NVM_INTEGRITY, "Very large number of models");
                issue.description = "File contains " + std::to_string(model_count) + 
                                   " models (maximum recommended: " + std::to_string(rules_.max_models_per_file) + ")";
                issue.metadata["model_count"] = std::to_string(model_count);
                issues.push_back(issue);
            }
            
        } catch (const std::exception& e) {
            ValidationIssue issue("NVM_INTEGRITY_EXCEPTION", ValidationSeverity::ERROR,
                                 ValidationCategory::NVM_INTEGRITY, "Exception during integrity validation");
            issue.description = "Exception: " + std::string(e.what());
            issues.push_back(issue);
        }
        
        return issues;
    }

    // Placeholder implementations for remaining methods
    std::vector<ValidationIssue> ValidationEngine::validate_parameter_ranges(const nvm::NvmFile& nvm_file) {
        std::vector<ValidationIssue> issues;
        
        try {
            auto all_models = nvm_file.get_all_models();
            
            for (const auto& model : all_models) {
                std::string model_name = model.phoneme + "_" + std::to_string(model.context_hash);
                
                // Validate HMM states and parameters
                for (size_t state_idx = 0; state_idx < model.states.size(); ++state_idx) {
                    const auto& state = model.states[state_idx];
                    
                    // Check state count is within reasonable bounds
                    if (model.states.size() < rules_.min_hmm_states) {
                        ValidationIssue issue("TOO_FEW_HMM_STATES", ValidationSeverity::ERROR,
                                             ValidationCategory::PARAMETER_RANGE, "Too few HMM states");
                        issue.description = "Model " + model_name + " has " + std::to_string(model.states.size()) + 
                                           " states (minimum: " + std::to_string(rules_.min_hmm_states) + ")";
                        issue.model_name = model_name;
                        issue.metadata["state_count"] = std::to_string(model.states.size());
                        issues.push_back(issue);
                    }
                    
                    if (model.states.size() > rules_.max_hmm_states) {
                        ValidationIssue issue("TOO_MANY_HMM_STATES", ValidationSeverity::WARNING,
                                             ValidationCategory::PARAMETER_RANGE, "Too many HMM states");
                        issue.description = "Model " + model_name + " has " + std::to_string(model.states.size()) + 
                                           " states (maximum recommended: " + std::to_string(rules_.max_hmm_states) + ")";
                        issue.model_name = model_name;
                        issue.metadata["state_count"] = std::to_string(model.states.size());
                        issues.push_back(issue);
                    }
                    
                    // Validate Gaussian mixture components
                    if (state.mixture.components.size() < rules_.min_gaussians_per_state) {
                        ValidationIssue issue("TOO_FEW_GAUSSIANS", ValidationSeverity::WARNING,
                                             ValidationCategory::PARAMETER_RANGE, "Too few Gaussian components");
                        issue.description = "State " + std::to_string(state_idx) + " in model " + model_name + 
                                           " has " + std::to_string(state.mixture.components.size()) + " components (minimum: " + 
                                           std::to_string(rules_.min_gaussians_per_state) + ")";
                        issue.model_name = model_name;
                        issue.metadata["state_index"] = std::to_string(state_idx);
                        issues.push_back(issue);
                    }
                    
                    if (state.mixture.components.size() > rules_.max_gaussians_per_state) {
                        ValidationIssue issue("TOO_MANY_GAUSSIANS", ValidationSeverity::WARNING,
                                             ValidationCategory::PARAMETER_RANGE, "Too many Gaussian components");
                        issue.description = "State " + std::to_string(state_idx) + " in model " + model_name + 
                                           " has " + std::to_string(state.mixture.components.size()) + " components (maximum: " + 
                                           std::to_string(rules_.max_gaussians_per_state) + ")";
                        issue.model_name = model_name;
                        issue.metadata["state_index"] = std::to_string(state_idx);
                        issues.push_back(issue);
                    }
                    
                    // Validate individual Gaussian components
                    for (size_t comp_idx = 0; comp_idx < state.mixture.components.size(); ++comp_idx) {
                        const auto& component = state.mixture.components[comp_idx];
                        
                        // Check mean values are within reasonable ranges
                        for (int i = 0; i < component.mean.size(); ++i) {
                            double mean_val = component.mean[i];
                            
                            // F0 parameter validation (assuming first dimension is F0-related)
                            if (i == 0) {
                                if (mean_val < rules_.min_f0_hz || mean_val > rules_.max_f0_hz) {
                                    ValidationIssue issue("F0_OUT_OF_RANGE", ValidationSeverity::WARNING,
                                                         ValidationCategory::PARAMETER_RANGE, "F0 parameter out of range");
                                    issue.description = "F0 mean " + std::to_string(mean_val) + "Hz in model " + model_name + 
                                                       " is outside typical range (" + std::to_string(rules_.min_f0_hz) + "-" + 
                                                       std::to_string(rules_.max_f0_hz) + "Hz)";
                                    issue.model_name = model_name;
                                    issue.metadata["f0_value"] = std::to_string(mean_val);
                                    issues.push_back(issue);
                                }
                            }
                            
                            // Check for extreme values that might indicate corruption
                            if (std::isnan(mean_val) || std::isinf(mean_val)) {
                                ValidationIssue issue("INVALID_MEAN_VALUE", ValidationSeverity::CRITICAL,
                                                     ValidationCategory::PARAMETER_RANGE, "Invalid mean value");
                                issue.description = "NaN or infinite mean value in model " + model_name + 
                                                   ", state " + std::to_string(state_idx) + ", component " + std::to_string(comp_idx);
                                issue.model_name = model_name;
                                issues.push_back(issue);
                            }
                        }
                        
                        // Check covariance matrix
                        double det = component.covariance.determinant();
                        if (det <= 0) {
                            ValidationIssue issue("NON_POSITIVE_COVARIANCE", ValidationSeverity::CRITICAL,
                                                 ValidationCategory::PARAMETER_RANGE, "Non-positive definite covariance");
                            issue.description = "Covariance matrix in model " + model_name + 
                                               " is not positive definite (determinant: " + std::to_string(det) + ")";
                            issue.model_name = model_name;
                            issue.suggestion = "Check training data and convergence";
                            issues.push_back(issue);
                        }
                        
                        if (det > rules_.max_covariance_determinant) {
                            ValidationIssue issue("COVARIANCE_TOO_LARGE", ValidationSeverity::WARNING,
                                                 ValidationCategory::PARAMETER_RANGE, "Very large covariance determinant");
                            issue.description = "Covariance determinant " + std::to_string(det) + " in model " + model_name + 
                                               " is very large (may indicate poor training)";
                            issue.model_name = model_name;
                            issue.suggestion = "Consider retraining with more data or regularization";
                            issues.push_back(issue);
                        }
                        
                        // Validate mixture weight
                        if (component.weight <= 0.0 || component.weight > 1.0) {
                            ValidationIssue issue("INVALID_MIXTURE_WEIGHT", ValidationSeverity::ERROR,
                                                 ValidationCategory::PARAMETER_RANGE, "Invalid mixture weight");
                            issue.description = "Mixture weight " + std::to_string(component.weight) + " in model " + model_name + 
                                               " is outside valid range (0.0, 1.0]";
                            issue.model_name = model_name;
                            issues.push_back(issue);
                        }
                    }
                    
                    // Check that mixture weights sum to approximately 1.0
                    double weight_sum = 0.0;
                    for (const auto& component : state.mixture.components) {
                        weight_sum += component.weight;
                    }
                    if (std::abs(weight_sum - 1.0) > 1e-6) {
                        ValidationIssue issue("MIXTURE_WEIGHTS_NOT_NORMALIZED", ValidationSeverity::ERROR,
                                             ValidationCategory::PARAMETER_RANGE, "Mixture weights not normalized");
                        issue.description = "Mixture weights sum to " + std::to_string(weight_sum) + 
                                           " instead of 1.0 in model " + model_name + ", state " + std::to_string(state_idx);
                        issue.model_name = model_name;
                        issue.suggestion = "Renormalize mixture weights";
                        issues.push_back(issue);
                    }
                }
                
                // Validate transition probabilities
                for (size_t i = 0; i < model.transition_matrix.rows(); ++i) {
                    double row_sum = 0.0;
                    for (size_t j = 0; j < model.transition_matrix.cols(); ++j) {
                        double prob = model.transition_matrix(i, j);
                        row_sum += prob;
                        
                        if (prob < 0.0 || prob > 1.0) {
                            ValidationIssue issue("INVALID_TRANSITION_PROB", ValidationSeverity::ERROR,
                                                 ValidationCategory::PARAMETER_RANGE, "Invalid transition probability");
                            issue.description = "Transition probability " + std::to_string(prob) + 
                                               " from state " + std::to_string(i) + " to " + std::to_string(j) + 
                                               " in model " + model_name + " is outside [0,1]";
                            issue.model_name = model_name;
                            issues.push_back(issue);
                        }
                        
                        if (prob > 0.0 && prob < rules_.min_transition_probability) {
                            ValidationIssue issue("VERY_LOW_TRANSITION_PROB", ValidationSeverity::WARNING,
                                                 ValidationCategory::PARAMETER_RANGE, "Very low transition probability");
                            issue.description = "Transition probability " + std::to_string(prob) + 
                                               " in model " + model_name + " is very small (may cause numerical issues)";
                            issue.model_name = model_name;
                            issues.push_back(issue);
                        }
                    }
                    
                    // Check row normalization
                    if (std::abs(row_sum - 1.0) > 1e-6) {
                        ValidationIssue issue("TRANSITION_MATRIX_NOT_NORMALIZED", ValidationSeverity::ERROR,
                                             ValidationCategory::PARAMETER_RANGE, "Transition matrix row not normalized");
                        issue.description = "Transition matrix row " + std::to_string(i) + " sums to " + 
                                           std::to_string(row_sum) + " instead of 1.0 in model " + model_name;
                        issue.model_name = model_name;
                        issue.suggestion = "Renormalize transition matrix rows";
                        issues.push_back(issue);
                    }
                }
            }
            
        } catch (const std::exception& e) {
            ValidationIssue issue("PARAMETER_VALIDATION_EXCEPTION", ValidationSeverity::ERROR,
                                 ValidationCategory::PARAMETER_RANGE, "Exception during parameter validation");
            issue.description = "Exception: " + std::string(e.what());
            issues.push_back(issue);
            LOG_ERROR("Exception in parameter range validation: " + std::string(e.what()));
        }
        
        LOG_DEBUG("Parameter range validation completed with " + std::to_string(issues.size()) + " issues");
        return issues;
    }

    std::vector<ValidationIssue> ValidationEngine::validate_model_consistency(const nvm::NvmFile& nvm_file) {
        std::vector<ValidationIssue> issues;
        
        try {
            auto all_models = nvm_file.get_all_models();
            
            if (all_models.empty()) {
                ValidationIssue issue("NO_MODELS_FOR_CONSISTENCY", ValidationSeverity::CRITICAL,
                                     ValidationCategory::MODEL_CONSISTENCY, "No models available for consistency check");
                issue.description = "Cannot perform consistency validation on empty model set";
                issues.push_back(issue);
                return issues;
            }
            
            // Group models by phoneme for consistency checking
            std::unordered_map<std::string, std::vector<const hmm::PhonemeHmm*>> phoneme_groups;
            std::unordered_map<std::string, size_t> phoneme_counts;
            std::set<std::string> all_phonemes;
            
            for (const auto& model : all_models) {
                phoneme_groups[model.phoneme].push_back(&model);
                phoneme_counts[model.phoneme]++;
                all_phonemes.insert(model.phoneme);
            }
            
            // Check for duplicate models (same phoneme + context)
            std::unordered_map<std::string, std::vector<std::string>> context_map;
            for (const auto& model : all_models) {
                std::string key = model.phoneme + "_" + std::to_string(model.context_hash);
                std::string model_name = model.phoneme + "_" + std::to_string(model.context_hash);
                
                if (context_map.find(key) != context_map.end()) {
                    ValidationIssue issue("DUPLICATE_MODEL", ValidationSeverity::ERROR,
                                         ValidationCategory::MODEL_CONSISTENCY, "Duplicate model found");
                    issue.description = "Model " + model_name + " appears multiple times with same context";
                    issue.model_name = model_name;
                    issue.suggestion = "Remove duplicate models or differentiate contexts";
                    issues.push_back(issue);
                }
                context_map[key].push_back(model_name);
            }
            
            // Check dimensional consistency within phoneme groups
            for (const auto& [phoneme, models] : phoneme_groups) {
                if (models.size() <= 1) continue;
                
                // Check state count consistency
                size_t expected_states = models[0]->states.size();
                for (size_t i = 1; i < models.size(); ++i) {
                    if (models[i]->states.size() != expected_states) {
                        ValidationIssue issue("INCONSISTENT_STATE_COUNT", ValidationSeverity::WARNING,
                                             ValidationCategory::MODEL_CONSISTENCY, "Inconsistent state count");
                        issue.description = "Phoneme " + phoneme + " has models with different state counts: " +
                                           std::to_string(expected_states) + " vs " + std::to_string(models[i]->states.size());
                        issue.phoneme = phoneme;
                        issue.suggestion = "Consider standardizing state counts across contexts";
                        issues.push_back(issue);
                    }
                }
                
                // Check feature dimension consistency
                if (!models.empty() && !models[0]->states.empty() && !models[0]->states[0].mixture.components.empty()) {
                    int expected_dim = models[0]->states[0].mixture.components[0].mean.size();
                    
                    for (const auto* model : models) {
                        for (const auto& state : model->states) {
                            for (const auto& component : state.mixture.components) {
                                if (component.mean.size() != expected_dim) {
                                    ValidationIssue issue("INCONSISTENT_FEATURE_DIM", ValidationSeverity::CRITICAL,
                                                         ValidationCategory::MODEL_CONSISTENCY, "Inconsistent feature dimensions");
                                    issue.description = "Feature dimension mismatch in phoneme " + phoneme + 
                                                       ": expected " + std::to_string(expected_dim) + 
                                                       ", found " + std::to_string(component.mean.size());
                                    issue.phoneme = phoneme;
                                    issues.push_back(issue);
                                }
                            }
                        }
                    }
                }
            }
            
            // Check for statistical outliers in model parameters
            std::vector<double> all_mixture_weights;
            std::vector<double> all_transition_probs;
            std::vector<double> all_covariance_dets;
            
            for (const auto& model : all_models) {
                for (const auto& state : model.states) {
                    for (const auto& component : state.mixture.components) {
                        all_mixture_weights.push_back(component.weight);
                        double det = component.covariance.determinant();
                        if (det > 0) all_covariance_dets.push_back(det);
                    }
                }
                
                for (int i = 0; i < model.transition_matrix.rows(); ++i) {
                    for (int j = 0; j < model.transition_matrix.cols(); ++j) {
                        double prob = model.transition_matrix(i, j);
                        if (prob > 0) all_transition_probs.push_back(prob);
                    }
                }
            }
            
            // Calculate statistics and detect outliers
            auto detect_outliers = [&](const std::vector<double>& values, const std::string& param_name) {
                if (values.empty()) return;
                
                double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
                double variance = 0.0;
                for (double val : values) {
                    variance += (val - mean) * (val - mean);
                }
                variance /= values.size();
                double stddev = std::sqrt(variance);
                
                // Detect values more than 3 standard deviations from mean
                for (double val : values) {
                    if (std::abs(val - mean) > 3.0 * stddev) {
                        ValidationIssue issue("STATISTICAL_OUTLIER", ValidationSeverity::WARNING,
                                             ValidationCategory::MODEL_CONSISTENCY, "Statistical outlier detected");
                        issue.description = param_name + " value " + std::to_string(val) + 
                                           " is a statistical outlier (mean: " + std::to_string(mean) + 
                                           ", stddev: " + std::to_string(stddev) + ")";
                        issue.suggestion = "Review model training or data quality";
                        issues.push_back(issue);
                    }
                }
            };
            
            detect_outliers(all_mixture_weights, "Mixture weight");
            detect_outliers(all_covariance_dets, "Covariance determinant");
            detect_outliers(all_transition_probs, "Transition probability");
            
            // Check for missing phoneme pairs (common in Japanese UTAU)
            std::set<std::string> expected_japanese_phonemes = {
                "a", "i", "u", "e", "o", "ka", "ki", "ku", "ke", "ko", "n"
            };
            
            for (const std::string& expected : expected_japanese_phonemes) {
                if (all_phonemes.find(expected) == all_phonemes.end()) {
                    ValidationIssue issue("MISSING_COMMON_PHONEME", ValidationSeverity::WARNING,
                                         ValidationCategory::MODEL_CONSISTENCY, "Missing common phoneme");
                    issue.description = "Common phoneme '" + expected + "' is missing from model set";
                    issue.phoneme = expected;
                    issue.suggestion = "Consider adding model for phoneme '" + expected + "'";
                    issues.push_back(issue);
                }
            }
            
            // Check transition matrix structure consistency
            for (const auto& model : all_models) {
                if (model.transition_matrix.rows() != model.transition_matrix.cols()) {
                    ValidationIssue issue("NON_SQUARE_TRANSITION_MATRIX", ValidationSeverity::CRITICAL,
                                         ValidationCategory::MODEL_CONSISTENCY, "Non-square transition matrix");
                    issue.description = "Transition matrix is not square (" + 
                                       std::to_string(model.transition_matrix.rows()) + "x" + 
                                       std::to_string(model.transition_matrix.cols()) + ") in model " + model.phoneme;
                    issue.model_name = model.phoneme;
                    issues.push_back(issue);
                }
                
                if (model.transition_matrix.rows() != model.states.size()) {
                    ValidationIssue issue("TRANSITION_STATE_MISMATCH", ValidationSeverity::CRITICAL,
                                         ValidationCategory::MODEL_CONSISTENCY, "Transition matrix size mismatch");
                    issue.description = "Transition matrix size (" + std::to_string(model.transition_matrix.rows()) + 
                                       ") doesn't match state count (" + std::to_string(model.states.size()) + 
                                       ") in model " + model.phoneme;
                    issue.model_name = model.phoneme;
                    issues.push_back(issue);
                }
            }
            
            // Check model variance ratios for consistency
            for (const auto& [phoneme, models] : phoneme_groups) {
                if (models.size() <= 1) continue;
                
                std::vector<double> model_variances;
                for (const auto* model : models) {
                    double total_variance = 0.0;
                    size_t component_count = 0;
                    
                    for (const auto& state : model->states) {
                        for (const auto& component : state.mixture.components) {
                            total_variance += component.covariance.trace();
                            component_count++;
                        }
                    }
                    
                    if (component_count > 0) {
                        model_variances.push_back(total_variance / component_count);
                    }
                }
                
                if (model_variances.size() >= 2) {
                    auto minmax = std::minmax_element(model_variances.begin(), model_variances.end());
                    double variance_ratio = *minmax.second / std::max(*minmax.first, 1e-10);
                    
                    if (variance_ratio > rules_.max_model_variance_ratio) {
                        ValidationIssue issue("HIGH_VARIANCE_RATIO", ValidationSeverity::WARNING,
                                             ValidationCategory::MODEL_CONSISTENCY, "High variance ratio between models");
                        issue.description = "Phoneme " + phoneme + " has high variance ratio (" + 
                                           std::to_string(variance_ratio) + ") between different contexts";
                        issue.phoneme = phoneme;
                        issue.suggestion = "Consider retraining with consistent data normalization";
                        issues.push_back(issue);
                    }
                }
            }
            
        } catch (const std::exception& e) {
            ValidationIssue issue("MODEL_CONSISTENCY_EXCEPTION", ValidationSeverity::ERROR,
                                 ValidationCategory::MODEL_CONSISTENCY, "Exception during model consistency validation");
            issue.description = "Exception: " + std::string(e.what());
            issues.push_back(issue);
            LOG_ERROR("Exception in model consistency validation: " + std::string(e.what()));
        }
        
        LOG_DEBUG("Model consistency validation completed with " + std::to_string(issues.size()) + " issues");
        return issues;
    }

    PhonemeAnalysis ValidationEngine::analyze_phoneme_coverage(const nvm::NvmFile& nvm_file, 
                                                             const std::string& target_language) {
        PhonemeAnalysis analysis;
        
        // Get required phonemes for target language
        analysis.required_phonemes = get_required_phonemes(target_language);
        analysis.total_required = analysis.required_phonemes.size();
        
        // Extract phonemes from NVM file
        analysis.found_phonemes = extract_phonemes_from_nvm(nvm_file);
        analysis.total_found = analysis.found_phonemes.size();
        
        // Calculate missing and extra phonemes
        std::set_difference(analysis.required_phonemes.begin(), analysis.required_phonemes.end(),
                          analysis.found_phonemes.begin(), analysis.found_phonemes.end(),
                          std::inserter(analysis.missing_phonemes, analysis.missing_phonemes.begin()));
        
        std::set_difference(analysis.found_phonemes.begin(), analysis.found_phonemes.end(),
                          analysis.required_phonemes.begin(), analysis.required_phonemes.end(),
                          std::inserter(analysis.extra_phonemes, analysis.extra_phonemes.begin()));
        
        analysis.total_missing = analysis.missing_phonemes.size();
        
        // Calculate coverage percentage
        if (analysis.total_required > 0) {
            analysis.coverage_percentage = 
                100.0 * (analysis.total_required - analysis.total_missing) / analysis.total_required;
        }
        
        // Analyze phoneme types
        for (const auto& phoneme : analysis.found_phonemes) {
            if (is_basic_vowel(phoneme)) analysis.has_basic_vowels = true;
            if (is_basic_consonant(phoneme)) analysis.has_basic_consonants = true;
            if (is_diphthong(phoneme)) analysis.has_diphthongs = true;
            if (is_special_phoneme(phoneme)) analysis.has_special_phonemes = true;
        }
        
        return analysis;
    }

    PhonemeAnalysis ValidationEngine::analyze_utau_phoneme_coverage(const std::string& voicebank_path,
                                                                   const std::string& target_language) {
        PhonemeAnalysis analysis;
        
        // Get required phonemes for target language
        analysis.required_phonemes = get_required_phonemes(target_language);
        analysis.total_required = analysis.required_phonemes.size();
        
        // Extract phonemes from UTAU voicebank
        analysis.found_phonemes = extract_phonemes_from_utau(voicebank_path);
        analysis.total_found = analysis.found_phonemes.size();
        
        // Calculate missing and extra phonemes
        std::set_difference(analysis.required_phonemes.begin(), analysis.required_phonemes.end(),
                          analysis.found_phonemes.begin(), analysis.found_phonemes.end(),
                          std::inserter(analysis.missing_phonemes, analysis.missing_phonemes.begin()));
        
        std::set_difference(analysis.found_phonemes.begin(), analysis.found_phonemes.end(),
                          analysis.required_phonemes.begin(), analysis.required_phonemes.end(),
                          std::inserter(analysis.extra_phonemes, analysis.extra_phonemes.begin()));
        
        analysis.total_missing = analysis.missing_phonemes.size();
        
        // Calculate coverage percentage
        if (analysis.total_required > 0) {
            analysis.coverage_percentage = 
                100.0 * (analysis.total_required - analysis.total_missing) / analysis.total_required;
        }
        
        return analysis;
    }

    std::string ValidationEngine::generate_json_report(const ValidationReport& report) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"validation_id\": \"" << report.validation_id << "\",\n";
        json << "  \"file_path\": \"" << report.file_path << "\",\n";
        json << "  \"is_valid\": " << (report.is_valid ? "true" : "false") << ",\n";
        json << "  \"is_usable\": " << (report.is_usable ? "true" : "false") << ",\n";
        json << "  \"total_issues\": " << report.total_issues << ",\n";
        json << "  \"severity_counts\": {\n";
        json << "    \"info\": " << report.info_count << ",\n";
        json << "    \"warning\": " << report.warning_count << ",\n";
        json << "    \"error\": " << report.error_count << ",\n";
        json << "    \"critical\": " << report.critical_count << "\n";
        json << "  },\n";
        json << "  \"quality_metrics\": {\n";
        json << "    \"overall_score\": " << std::fixed << std::setprecision(3) << report.quality_metrics.overall_score << ",\n";
        json << "    \"completeness_score\": " << std::fixed << std::setprecision(3) << report.quality_metrics.completeness_score << ",\n";
        json << "    \"consistency_score\": " << std::fixed << std::setprecision(3) << report.quality_metrics.consistency_score << ",\n";
        json << "    \"integrity_score\": " << std::fixed << std::setprecision(3) << report.quality_metrics.integrity_score << "\n";
        json << "  },\n";
        json << "  \"issues\": [\n";
        
        for (size_t i = 0; i < report.issues.size(); ++i) {
            json << "    " << format_issue_as_json(report.issues[i]);
            if (i < report.issues.size() - 1) json << ",";
            json << "\n";
        }
        
        json << "  ]\n";
        json << "}\n";
        
        return json.str();
    }

    // =============================================================================
    // Helper method implementations
    // =============================================================================

    std::vector<ValidationIssue> ValidationEngine::validate_file_format(const std::string& file_path) {
        std::vector<ValidationIssue> issues;
        
        try {
            std::ifstream file(file_path, std::ios::binary);
            if (!file.is_open()) {
                ValidationIssue issue("FILE_OPEN_FAILED", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "Cannot open file");
                issue.description = "Failed to open file for reading";
                issue.location = file_path;
                issues.push_back(issue);
                return issues;
            }
            
            // Read first few bytes to check magic number
            uint32_t magic = 0;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            
            if (magic != nvm::constants::MAGIC_NUMBER) {
                ValidationIssue issue("INVALID_MAGIC_NUMBER", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "Invalid file format");
                issue.description = "File does not have valid NVM magic number";
                issue.location = file_path;
                issue.metadata["expected_magic"] = std::to_string(nvm::constants::MAGIC_NUMBER);
                issue.metadata["found_magic"] = std::to_string(magic);
                issues.push_back(issue);
            }
            
        } catch (const std::exception& e) {
            ValidationIssue issue("FILE_FORMAT_EXCEPTION", ValidationSeverity::ERROR,
                                 ValidationCategory::FILE_STRUCTURE, "Exception during format validation");
            issue.description = "Exception: " + std::string(e.what());
            issue.location = file_path;
            issues.push_back(issue);
        }
        
        return issues;
    }

    std::vector<ValidationIssue> ValidationEngine::validate_version_compatibility(const nvm::SemanticVersion& version) {
        std::vector<ValidationIssue> issues;
        
        auto current_version = nvm::SemanticVersion(nvm::constants::CURRENT_VERSION);
        auto min_version = nvm::SemanticVersion(nvm::constants::MIN_SUPPORTED_VERSION);
        
        if (version < min_version) {
            ValidationIssue issue("VERSION_TOO_OLD", ValidationSeverity::ERROR,
                                 ValidationCategory::VERSION_COMPAT, "Version too old");
            issue.description = "File version " + version.to_string() + 
                               " is older than minimum supported version " + min_version.to_string();
            issue.suggestion = "Convert file to newer version using migration tools";
            issues.push_back(issue);
        } else if (version > current_version) {
            ValidationIssue issue("VERSION_TOO_NEW", ValidationSeverity::WARNING,
                                 ValidationCategory::VERSION_COMPAT, "Version newer than current");
            issue.description = "File version " + version.to_string() + 
                               " is newer than current version " + current_version.to_string();
            issue.suggestion = "Update NexusSynth to support this file version";
            issues.push_back(issue);
        }
        
        return issues;
    }

    std::set<std::string> ValidationEngine::get_required_phonemes(const std::string& language) {
        if (language == "japanese") {
            return validation_utils::get_japanese_phoneme_set();
        } else if (language == "english") {
            return validation_utils::get_english_phoneme_set();
        } else {
            return validation_utils::get_basic_utau_phoneme_set();
        }
    }

    std::set<std::string> ValidationEngine::extract_phonemes_from_nvm(const nvm::NvmFile& nvm_file) {
        std::set<std::string> phonemes;
        
        try {
            auto model_names = nvm_file.get_model_names();
            for (const auto& name : model_names) {
                // Extract phoneme from model name (simplified)
                size_t underscore_pos = name.find('_');
                if (underscore_pos != std::string::npos) {
                    std::string phoneme = name.substr(0, underscore_pos);
                    phonemes.insert(phoneme);
                } else {
                    phonemes.insert(name);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception extracting phonemes from NVM: " + std::string(e.what()));
        }
        
        return phonemes;
    }

    std::set<std::string> ValidationEngine::extract_phonemes_from_utau(const std::string& voicebank_path) {
        std::set<std::string> phonemes;
        
        try {
            std::string oto_path = voicebank_path + "/oto.ini";
            if (std::filesystem::exists(oto_path)) {
                utau::UtauOtoParser parser;
                auto oto_entries = parser.parse_oto_file(oto_path);
                for (const auto& entry : oto_entries) {
                    phonemes.insert(entry.alias);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception extracting phonemes from UTAU: " + std::string(e.what()));
        }
        
        return phonemes;
    }

    std::string ValidationEngine::format_issue_as_json(const ValidationIssue& issue) {
        std::ostringstream json;
        json << "{\n";
        json << "      \"id\": \"" << issue.id << "\",\n";
        json << "      \"severity\": \"" << severity_to_string(issue.severity) << "\",\n";
        json << "      \"category\": \"" << category_to_string(issue.category) << "\",\n";
        json << "      \"title\": \"" << issue.title << "\",\n";
        json << "      \"description\": \"" << issue.description << "\",\n";
        json << "      \"location\": \"" << issue.location << "\"";
        if (issue.suggestion.has_value()) {
            json << ",\n      \"suggestion\": \"" << issue.suggestion.value() << "\"";
        }
        json << "\n    }";
        return json.str();
    }

    std::string ValidationEngine::severity_to_string(ValidationSeverity severity) {
        switch (severity) {
            case ValidationSeverity::INFO: return "info";
            case ValidationSeverity::WARNING: return "warning";
            case ValidationSeverity::ERROR: return "error";
            case ValidationSeverity::CRITICAL: return "critical";
            default: return "unknown";
        }
    }

    std::string ValidationEngine::category_to_string(ValidationCategory category) {
        switch (category) {
            case ValidationCategory::FILE_STRUCTURE: return "file_structure";
            case ValidationCategory::NVM_INTEGRITY: return "nvm_integrity";
            case ValidationCategory::PARAMETER_RANGE: return "parameter_range";
            case ValidationCategory::PHONEME_COVERAGE: return "phoneme_coverage";
            case ValidationCategory::MODEL_CONSISTENCY: return "model_consistency";
            case ValidationCategory::METADATA_VALIDITY: return "metadata_validity";
            case ValidationCategory::COMPRESSION_ISSUES: return "compression_issues";
            case ValidationCategory::CHECKSUM_ERRORS: return "checksum_errors";
            case ValidationCategory::VERSION_COMPAT: return "version_compat";
            case ValidationCategory::CONVERSION_QUALITY: return "conversion_quality";
            default: return "unknown";
        }
    }

    // Additional UTAU validation implementations
    std::vector<ValidationIssue> ValidationEngine::validate_utau_structure(const std::string& voicebank_path) {
        std::vector<ValidationIssue> issues;
        
        try {
            namespace fs = std::filesystem;
            
            // Check if directory exists
            if (!fs::exists(voicebank_path) || !fs::is_directory(voicebank_path)) {
                ValidationIssue issue("UTAU_DIR_NOT_FOUND", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "UTAU directory not found");
                issue.description = "Voicebank directory does not exist: " + voicebank_path;
                issue.location = voicebank_path;
                issues.push_back(issue);
                return issues;
            }
            
            // Check for essential files
            std::string oto_path = voicebank_path + "/oto.ini";
            if (!fs::exists(oto_path)) {
                ValidationIssue issue("MISSING_OTO_INI", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "Missing oto.ini file");
                issue.description = "Required oto.ini file not found in voicebank directory";
                issue.location = oto_path;
                issue.suggestion = "Create oto.ini file with phoneme timing information";
                issues.push_back(issue);
            }
            
            // Check for character.txt (voice bank info)
            std::string character_path = voicebank_path + "/character.txt";
            if (!fs::exists(character_path)) {
                ValidationIssue issue("MISSING_CHARACTER_TXT", ValidationSeverity::WARNING,
                                     ValidationCategory::FILE_STRUCTURE, "Missing character.txt file");
                issue.description = "character.txt file not found (voice bank metadata)";
                issue.location = character_path;
                issue.suggestion = "Add character.txt for voice bank metadata";
                issues.push_back(issue);
            }
            
            // Check for readme.txt
            std::string readme_path = voicebank_path + "/readme.txt";
            if (!fs::exists(readme_path)) {
                ValidationIssue issue("MISSING_README", ValidationSeverity::INFO,
                                     ValidationCategory::FILE_STRUCTURE, "Missing readme.txt file");
                issue.description = "readme.txt file not found (documentation)";
                issue.location = readme_path;
                issue.suggestion = "Consider adding readme.txt with usage information";
                issues.push_back(issue);
            }
            
            // Scan for audio files
            std::vector<std::string> audio_files;
            std::vector<std::string> supported_extensions = {".wav", ".WAV"};
            
            for (const auto& entry : fs::directory_iterator(voicebank_path)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    if (std::find(supported_extensions.begin(), supported_extensions.end(), extension) != supported_extensions.end()) {
                        audio_files.push_back(entry.path().filename().string());
                    }
                }
            }
            
            if (audio_files.empty()) {
                ValidationIssue issue("NO_AUDIO_FILES", ValidationSeverity::CRITICAL,
                                     ValidationCategory::FILE_STRUCTURE, "No audio files found");
                issue.description = "No WAV audio files found in voicebank directory";
                issue.location = voicebank_path;
                issue.suggestion = "Add WAV audio files with recorded phonemes";
                issues.push_back(issue);
            } else if (audio_files.size() < 50) {
                ValidationIssue issue("FEW_AUDIO_FILES", ValidationSeverity::WARNING,
                                     ValidationCategory::FILE_STRUCTURE, "Very few audio files");
                issue.description = "Only " + std::to_string(audio_files.size()) + 
                                   " audio files found (typical UTAU voicebanks have 100+)";
                issue.location = voicebank_path;
                issue.metadata["audio_file_count"] = std::to_string(audio_files.size());
                issues.push_back(issue);
            }
            
            // Check directory permissions
            try {
                fs::permissions permission = fs::status(voicebank_path).permissions();
                if ((permission & fs::perms::owner_read) == fs::perms::none) {
                    ValidationIssue issue("NO_READ_PERMISSION", ValidationSeverity::CRITICAL,
                                         ValidationCategory::FILE_STRUCTURE, "No read permission");
                    issue.description = "Cannot read voicebank directory (permission denied)";
                    issue.location = voicebank_path;
                    issues.push_back(issue);
                }
            } catch (const std::exception& e) {
                ValidationIssue issue("PERMISSION_CHECK_FAILED", ValidationSeverity::WARNING,
                                     ValidationCategory::FILE_STRUCTURE, "Permission check failed");
                issue.description = "Could not check directory permissions: " + std::string(e.what());
                issue.location = voicebank_path;
                issues.push_back(issue);
            }
            
            // Check for nested directories (multi-pitch banks)
            bool has_subdirs = false;
            for (const auto& entry : fs::directory_iterator(voicebank_path)) {
                if (entry.is_directory()) {
                    has_subdirs = true;
                    
                    // Check if subdirectory has its own oto.ini
                    std::string sub_oto_path = entry.path().string() + "/oto.ini";
                    if (fs::exists(sub_oto_path)) {
                        ValidationIssue issue("MULTI_PITCH_DETECTED", ValidationSeverity::INFO,
                                             ValidationCategory::FILE_STRUCTURE, "Multi-pitch voicebank detected");
                        issue.description = "Found subdirectory with oto.ini: " + entry.path().filename().string();
                        issue.location = sub_oto_path;
                        issue.suggestion = "Ensure all pitch directories are properly configured";
                        issues.push_back(issue);
                    }
                }
            }
            
            // Check file encoding issues (common with UTAU files)
            if (fs::exists(oto_path)) {
                std::ifstream oto_file(oto_path, std::ios::binary);
                if (oto_file.is_open()) {
                    // Read first few bytes to detect encoding
                    std::vector<char> buffer(1024);
                    oto_file.read(buffer.data(), buffer.size());
                    size_t bytes_read = oto_file.gcount();
                    
                    // Check for BOM or non-ASCII characters
                    bool has_non_ascii = false;
                    bool has_bom = false;
                    
                    if (bytes_read >= 3 && 
                        static_cast<unsigned char>(buffer[0]) == 0xEF &&
                        static_cast<unsigned char>(buffer[1]) == 0xBB &&
                        static_cast<unsigned char>(buffer[2]) == 0xBF) {
                        has_bom = true;
                    }
                    
                    for (size_t i = (has_bom ? 3 : 0); i < bytes_read; ++i) {
                        if (static_cast<unsigned char>(buffer[i]) > 127) {
                            has_non_ascii = true;
                            break;
                        }
                    }
                    
                    if (has_non_ascii && !has_bom) {
                        ValidationIssue issue("ENCODING_ISSUE_SUSPECTED", ValidationSeverity::WARNING,
                                             ValidationCategory::FILE_STRUCTURE, "Potential encoding issue");
                        issue.description = "oto.ini contains non-ASCII characters without UTF-8 BOM (may be Shift-JIS)";
                        issue.location = oto_path;
                        issue.suggestion = "Verify file encoding is compatible with UTAU standards";
                        issues.push_back(issue);
                    }
                }
            }
            
        } catch (const std::exception& e) {
            ValidationIssue issue("UTAU_STRUCTURE_EXCEPTION", ValidationSeverity::ERROR,
                                 ValidationCategory::FILE_STRUCTURE, "Exception during UTAU structure validation");
            issue.description = "Exception: " + std::string(e.what());
            issue.location = voicebank_path;
            issues.push_back(issue);
        }
        
        return issues;
    }

    std::vector<ValidationIssue> ValidationEngine::validate_oto_entries(const std::vector<utau::OtoEntry>& entries) {
        std::vector<ValidationIssue> issues;
        // TODO: Implement OTO entry validation
        return issues;
    }

    std::vector<ValidationIssue> ValidationEngine::validate_audio_files(const std::string& voicebank_path,
                                                                        const std::vector<utau::OtoEntry>& entries) {
        std::vector<ValidationIssue> issues;
        // TODO: Implement audio file validation
        return issues;
    }

    double ValidationEngine::calculate_completeness_score(const PhonemeAnalysis& analysis) {
        if (analysis.total_required == 0) return 1.0;
        return static_cast<double>(analysis.total_required - analysis.total_missing) / analysis.total_required;
    }

    double ValidationEngine::calculate_consistency_score(const std::vector<ValidationIssue>& issues) {
        // Simple scoring based on issue counts
        size_t error_weight = 0;
        for (const auto& issue : issues) {
            switch (issue.severity) {
                case ValidationSeverity::CRITICAL: error_weight += 10; break;
                case ValidationSeverity::ERROR: error_weight += 5; break;
                case ValidationSeverity::WARNING: error_weight += 2; break;
                case ValidationSeverity::INFO: error_weight += 1; break;
            }
        }
        return std::max(0.0, 1.0 - (error_weight / 100.0));
    }

    double ValidationEngine::calculate_integrity_score(const std::vector<ValidationIssue>& issues) {
        // Focus on integrity-related issues
        size_t integrity_errors = 0;
        for (const auto& issue : issues) {
            if (issue.category == ValidationCategory::NVM_INTEGRITY ||
                issue.category == ValidationCategory::CHECKSUM_ERRORS ||
                issue.category == ValidationCategory::FILE_STRUCTURE) {
                integrity_errors++;
            }
        }
        return std::max(0.0, 1.0 - (integrity_errors / 10.0));
    }

    double ValidationEngine::calculate_overall_quality_score(const ValidationReport& report) {
        double weighted_score = 
            0.4 * report.quality_metrics.completeness_score +
            0.3 * report.quality_metrics.consistency_score +
            0.3 * report.quality_metrics.integrity_score;
        return std::max(0.0, std::min(1.0, weighted_score));
    }

    // Utility method implementations
    bool ValidationEngine::is_basic_vowel(const std::string& phoneme) {
        std::set<std::string> basic_vowels = {"a", "i", "u", "e", "o"};
        return basic_vowels.count(phoneme) > 0;
    }

    bool ValidationEngine::is_basic_consonant(const std::string& phoneme) {
        std::set<std::string> basic_consonants = {"k", "s", "t", "n", "h", "m", "y", "r", "w", "g", "z", "d", "b", "p"};
        return basic_consonants.count(phoneme) > 0;
    }

    bool ValidationEngine::is_diphthong(const std::string& phoneme) {
        return phoneme.length() > 1 && 
               (phoneme.find("ai") != std::string::npos || 
                phoneme.find("ou") != std::string::npos ||
                phoneme.find("ei") != std::string::npos);
    }

    bool ValidationEngine::is_special_phoneme(const std::string& phoneme) {
        std::set<std::string> special = {"br", "cl", "sil", "pau"};
        return special.count(phoneme) > 0;
    }

    std::string ValidationEngine::generate_unique_id() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        return "VAL_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }

    std::chrono::milliseconds ValidationEngine::get_elapsed_time(const std::chrono::steady_clock::time_point& start) {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
    }

    bool ValidationEngine::is_file_accessible(const std::string& file_path) {
        std::ifstream file(file_path, std::ios::binary);
        return file.good();
    }

    size_t ValidationEngine::get_file_size(const std::string& file_path) {
        try {
            return std::filesystem::file_size(file_path);
        } catch (const std::exception&) {
            return 0;
        }
    }

    void ValidationEngine::report_progress(size_t current, size_t total, const std::string& task) {
        if (progress_callback_) {
            progress_callback_->on_validation_progress(current, total, task);
        }
    }

    void ValidationEngine::report_issue(const ValidationIssue& issue) {
        if (progress_callback_) {
            progress_callback_->on_issue_found(issue);
        }
    }

    void ValidationEngine::report_critical_error(const std::string& error) {
        if (progress_callback_) {
            progress_callback_->on_critical_error(error);
        }
    }

    // =============================================================================
    // ConsoleValidationProgressCallback Implementation
    // =============================================================================

    void ConsoleValidationProgressCallback::on_validation_started(const std::string& file_path) {
        start_time_ = std::chrono::steady_clock::now();
        std::cout << "\n🔍 Starting validation of: " << file_path << std::endl;
    }

    void ConsoleValidationProgressCallback::on_validation_progress(size_t current_step, size_t total_steps, 
                                                                  const std::string& current_task) {
        print_progress_bar(current_step, total_steps);
        std::cout << " [" << current_step << "/" << total_steps << "] " << current_task << "\r" << std::flush;
    }

    void ConsoleValidationProgressCallback::on_validation_completed(const ValidationReport& report) {
        auto duration = std::chrono::steady_clock::now() - start_time_;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        std::cout << "\n\n✅ Validation completed in " << duration_ms.count() << "ms" << std::endl;
        std::cout << "   Result: " << (report.is_valid ? "✅ Valid" : "❌ Invalid") 
                  << " (" << (report.is_usable ? "Usable" : "Not usable") << ")" << std::endl;
        std::cout << "   Issues: " << report.total_issues 
                  << " (Critical: " << report.critical_count 
                  << ", Errors: " << report.error_count 
                  << ", Warnings: " << report.warning_count << ")" << std::endl;
        std::cout << "   Quality Score: " << std::fixed << std::setprecision(1) 
                  << (report.quality_metrics.overall_score * 100) << "%" << std::endl;
    }

    void ConsoleValidationProgressCallback::on_issue_found(const ValidationIssue& issue) {
        if (verbose_) {
            std::cout << "\n" << severity_color(issue.severity) << category_icon(issue.category) 
                      << " " << issue.title << ": " << issue.description << "\033[0m" << std::endl;
        }
    }

    void ConsoleValidationProgressCallback::on_critical_error(const std::string& error_message) {
        std::cout << "\n🚨 CRITICAL ERROR: " << error_message << std::endl;
    }

    void ConsoleValidationProgressCallback::print_progress_bar(size_t current, size_t total, int width) {
        if (total == 0) return;
        
        float progress = static_cast<float>(current) / static_cast<float>(total);
        int pos = static_cast<int>(width * progress);
        
        std::cout << "[";
        for (int i = 0; i < width; ++i) {
            if (i < pos) std::cout << "█";
            else if (i == pos) std::cout << "▏";
            else std::cout << " ";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0f) << "%";
    }

    std::string ConsoleValidationProgressCallback::severity_color(ValidationSeverity severity) {
        switch (severity) {
            case ValidationSeverity::INFO: return "\033[36m";      // Cyan
            case ValidationSeverity::WARNING: return "\033[33m";   // Yellow
            case ValidationSeverity::ERROR: return "\033[31m";     // Red
            case ValidationSeverity::CRITICAL: return "\033[35m";  // Magenta
            default: return "\033[0m";
        }
    }

    std::string ConsoleValidationProgressCallback::category_icon(ValidationCategory category) {
        switch (category) {
            case ValidationCategory::FILE_STRUCTURE: return "📁";
            case ValidationCategory::NVM_INTEGRITY: return "🔒";
            case ValidationCategory::PARAMETER_RANGE: return "📊";
            case ValidationCategory::PHONEME_COVERAGE: return "🗣️";
            case ValidationCategory::MODEL_CONSISTENCY: return "🧠";
            case ValidationCategory::METADATA_VALIDITY: return "📋";
            case ValidationCategory::COMPRESSION_ISSUES: return "🗜️";
            case ValidationCategory::CHECKSUM_ERRORS: return "✅";
            case ValidationCategory::VERSION_COMPAT: return "🔄";
            case ValidationCategory::CONVERSION_QUALITY: return "⚡";
            default: return "❓";
        }
    }

    // =============================================================================
    // Validation utilities implementation
    // =============================================================================

    namespace validation_utils {
        
        std::set<std::string> get_japanese_phoneme_set() {
            return {
                // Basic vowels
                "a", "i", "u", "e", "o",
                // Basic consonants
                "k", "s", "t", "n", "h", "m", "y", "r", "w", "g", "z", "d", "b", "p",
                // Consonant-vowel combinations
                "ka", "ki", "ku", "ke", "ko",
                "sa", "si", "su", "se", "so",
                "ta", "ti", "tu", "te", "to",
                "na", "ni", "nu", "ne", "no",
                "ha", "hi", "hu", "he", "ho",
                "ma", "mi", "mu", "me", "mo",
                "ya", "yu", "yo",
                "ra", "ri", "ru", "re", "ro",
                "wa", "wo", "n",
                // Special phonemes
                "sil", "pau", "br", "cl"
            };
        }

        std::set<std::string> get_english_phoneme_set() {
            return {
                // Basic vowels
                "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER", "EY", "IH", "IY", "OW", "OY", "UH", "UW",
                // Basic consonants
                "B", "CH", "D", "DH", "F", "G", "HH", "JH", "K", "L", "M", "N", "NG", "P", "R", "S", "SH", "T", "TH", "V", "W", "Y", "Z", "ZH",
                // Special phonemes
                "sil", "pau", "br", "cl"
            };
        }

        std::set<std::string> get_basic_utau_phoneme_set() {
            return {
                "a", "i", "u", "e", "o", "n",
                "ka", "ki", "ku", "ke", "ko",
                "sa", "si", "su", "se", "so",
                "ta", "ti", "tu", "te", "to",
                "na", "ni", "nu", "ne", "no",
                "ha", "hi", "hu", "he", "ho",
                "ma", "mi", "mu", "me", "mo",
                "ya", "yu", "yo",
                "ra", "ri", "ru", "re", "ro",
                "wa", "wo"
            };
        }

        std::string detect_file_format(const std::string& file_path) {
            if (std::filesystem::is_directory(file_path)) {
                if (std::filesystem::exists(file_path + "/oto.ini")) {
                    return "utau";
                }
                return "directory";
            }
            
            std::filesystem::path path(file_path);
            std::string extension = path.extension().string();
            
            if (extension == ".nvm") {
                return "nvm";
            } else if (extension == ".wav" || extension == ".flac") {
                return "audio";
            }
            
            return "unknown";
        }

        bool is_nvm_file(const std::string& file_path) {
            return detect_file_format(file_path) == "nvm";
        }

        bool is_utau_voicebank(const std::string& directory_path) {
            return detect_file_format(directory_path) == "utau";
        }

    } // namespace validation_utils

} // namespace validation
} // namespace nexussynth
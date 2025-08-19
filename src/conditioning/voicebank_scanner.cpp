#include "nexussynth/voicebank_scanner.h"
#include "nexussynth/utau_oto_parser.h"
// #include "nexussynth/voice_metadata.h"  // Temporarily disabled due to cJSON path issue
#include "nexussynth/utau_logger.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <regex>
#include <thread>
#include <future>
#include <iomanip>
#include <cmath>
#include <atomic>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nexussynth {
namespace conditioning {

// VoicebankScanner implementation
VoicebankScanner::VoicebankScanner() 
    : cancel_requested_(false) {
}

VoicebankScanner::VoicebankScanner(const ScannerConfig& config) 
    : config_(config), cancel_requested_(false) {
}

VoicebankDiscovery VoicebankScanner::scan_directory(const std::string& path) {
    LOG_INFO("Starting voice bank directory scan: " + path);
    
    reset_cancellation();
    auto start_time = std::chrono::steady_clock::now();
    
    if (progress_callback_) {
        progress_callback_->on_scan_started(path);
    }
    
    VoicebankDiscovery result = scan_directory_impl(path);
    
    auto end_time = std::chrono::steady_clock::now();
    result.scan_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    LOG_INFO_F("Voice bank scan completed in %d ms. Found %zu voice banks",
               (int)result.scan_duration.count(), result.voicebank_paths.size());
    
    if (progress_callback_) {
        progress_callback_->on_scan_completed(result);
    }
    
    return result;
}

VoicebankDiscovery VoicebankScanner::scan_multiple_directories(const std::vector<std::string>& paths) {
    VoicebankDiscovery combined_result;
    combined_result.search_path = "Multiple paths";
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (const auto& path : paths) {
        if (cancel_requested_) break;
        
        LOG_INFO("Scanning directory: " + path);
        VoicebankDiscovery single_result = scan_directory(path);
        
        // Combine results
        combined_result.voicebank_paths.insert(
            combined_result.voicebank_paths.end(),
            single_result.voicebank_paths.begin(),
            single_result.voicebank_paths.end());
        
        combined_result.directories_scanned += single_result.directories_scanned;
        combined_result.files_scanned += single_result.files_scanned;
        combined_result.valid_voicebanks += single_result.valid_voicebanks;
        combined_result.invalid_voicebanks += single_result.invalid_voicebanks;
        combined_result.partial_voicebanks += single_result.partial_voicebanks;
        
        combined_result.scan_errors.insert(
            combined_result.scan_errors.end(),
            single_result.scan_errors.begin(),
            single_result.scan_errors.end());
        
        combined_result.scan_warnings.insert(
            combined_result.scan_warnings.end(),
            single_result.scan_warnings.begin(),
            single_result.scan_warnings.end());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    combined_result.scan_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return combined_result;
}

VoicebankValidation VoicebankScanner::validate_voicebank(const std::string& path) {
    LOG_DEBUG("Validating voice bank: " + path);
    return validate_voicebank_impl(path);
}

AudioFileInfo VoicebankScanner::validate_audio_file(const std::string& file_path) {
    LOG_DEBUG("Validating audio file: " + file_path);
    return validate_audio_file_impl(file_path);
}

bool VoicebankScanner::is_voicebank_directory(const std::string& path) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return false;
    }
    
    // Check for oto.ini file
    fs::path oto_path = fs::path(path) / "oto.ini";
    if (!fs::exists(oto_path)) {
        return false;
    }
    
    // Check for at least one audio file
    auto audio_files = get_audio_files_in_directory(fs::path(path));
    return !audio_files.empty();
}

std::vector<std::string> VoicebankScanner::find_voicebank_candidates(const std::string& search_path) {
    std::vector<std::string> candidates;
    namespace fs = std::filesystem;
    
    if (!fs::exists(search_path) || !fs::is_directory(search_path)) {
        return candidates;
    }
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(search_path)) {
            if (cancel_requested_) break;
            
            if (entry.is_directory()) {
                std::string dir_path = entry.path().string();
                if (is_voicebank_directory(dir_path)) {
                    candidates.push_back(dir_path);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error scanning for voice bank candidates: " + std::string(e.what()));
    }
    
    return candidates;
}

std::vector<std::string> VoicebankScanner::get_supported_formats() const {
    std::vector<std::string> formats;
    for (const auto& format : config_.supported_audio_formats) {
        formats.push_back(format);
    }
    return formats;
}

std::unordered_map<std::string, size_t> VoicebankScanner::analyze_format_distribution(
    const VoicebankDiscovery& discovery) {
    
    std::unordered_map<std::string, size_t> distribution;
    namespace fs = std::filesystem;
    
    for (const auto& vb_path : discovery.voicebank_paths) {
        auto audio_files = get_audio_files_in_directory(fs::path(vb_path));
        for (const auto& file : audio_files) {
            std::string extension = file.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            distribution[extension]++;
        }
    }
    
    return distribution;
}

// Private implementation methods
VoicebankDiscovery VoicebankScanner::scan_directory_impl(const std::string& path) {
    VoicebankDiscovery result;
    result.search_path = path;
    
    namespace fs = std::filesystem;
    
    if (!fs::exists(path) || !fs::is_directory(path)) {
        result.scan_errors.push_back("Path does not exist or is not a directory: " + path);
        return result;
    }
    
    if (!is_directory_accessible(fs::path(path))) {
        result.scan_errors.push_back("Cannot access directory: " + path);
        return result;
    }
    
    try {
        scan_directory_recursive(fs::path(path), 0, result);
    } catch (const std::exception& e) {
        result.scan_errors.push_back("Scanning error: " + std::string(e.what()));
    }
    
    // Validate found voice banks
    if (config_.parallel_scanning && result.voicebank_paths.size() > 1) {
        process_voicebank_parallel(result.voicebank_paths, result);
    } else {
        for (const auto& vb_path : result.voicebank_paths) {
            if (cancel_requested_) break;
            
            VoicebankValidation validation = validate_voicebank_impl(vb_path);
            if (validation.is_valid) {
                result.valid_voicebanks++;
            } else if (validation.has_oto_ini || validation.has_audio_files) {
                result.partial_voicebanks++;
            } else {
                result.invalid_voicebanks++;
            }
            
            if (progress_callback_) {
                progress_callback_->on_voicebank_validated(vb_path, validation.is_valid);
            }
        }
    }
    
    return result;
}

void VoicebankScanner::scan_directory_recursive(const std::filesystem::path& current_path,
                                               int current_depth,
                                               VoicebankDiscovery& result) {
    namespace fs = std::filesystem;
    
    if (cancel_requested_ || current_depth > config_.max_scan_depth) {
        return;
    }
    
    if (progress_callback_) {
        progress_callback_->on_directory_entered(current_path.string(), current_depth);
    }
    
    result.directories_scanned++;
    
    // Check if current directory is a voice bank
    if (is_voicebank_directory(current_path.string())) {
        result.voicebank_paths.push_back(current_path.string());
        if (progress_callback_) {
            progress_callback_->on_voicebank_found(current_path.string());
        }
        LOG_DEBUG("Found voice bank: " + current_path.string());
    }
    
    // Count files in current directory
    size_t file_count = count_files_in_directory(current_path);
    result.files_scanned += file_count;
    
    if (file_count > config_.max_files_per_directory) {
        result.scan_warnings.push_back(
            "Large directory skipped: " + current_path.string() + 
            " (" + std::to_string(file_count) + " files)");
        return;
    }
    
    // Continue recursive search if enabled
    if (config_.recursive_search && current_depth < config_.max_scan_depth) {
        try {
            for (const auto& entry : fs::directory_iterator(current_path)) {
                if (cancel_requested_) break;
                
                if (entry.is_directory()) {
                    std::string dir_name = entry.path().filename().string();
                    if (!should_skip_directory(dir_name)) {
                        scan_directory_recursive(entry.path(), current_depth + 1, result);
                    }
                }
            }
        } catch (const std::exception& e) {
            result.scan_errors.push_back(
                "Error scanning directory " + current_path.string() + ": " + e.what());
        }
    }
}

VoicebankValidation VoicebankScanner::validate_voicebank_impl(const std::string& path) {
    VoicebankValidation validation;
    validation.path = path;
    validation.name = extract_voicebank_name(path);
    
    // Validate directory structure
    bool structure_valid = validate_directory_structure(path, validation);
    
    // Validate OTO files if present
    bool oto_valid = true;
    if (validation.has_oto_ini) {
        oto_valid = validate_oto_files(path, validation);
    }
    
    // Validate audio files if enabled
    bool audio_valid = true;
    if (config_.validate_audio_files && validation.has_audio_files) {
        audio_valid = validate_audio_files(path, validation);
    }
    
    // Validate metadata files
    bool metadata_valid = validate_metadata_files(path, validation);
    
    // Determine overall validity
    validation.is_valid = structure_valid && oto_valid && audio_valid && metadata_valid &&
                         validation.has_oto_ini && validation.has_audio_files &&
                         validation.missing_audio_files == 0;
    
    // Generate suggestions
    if (!validation.is_valid) {
        if (!validation.has_oto_ini) {
            validation.suggestions.push_back("Create oto.ini file with phoneme timing data");
        }
        if (!validation.has_audio_files) {
            validation.suggestions.push_back("Add WAV audio files to voice bank directory");
        }
        if (validation.missing_audio_files > 0) {
            validation.suggestions.push_back(
                "Fix " + std::to_string(validation.missing_audio_files) + 
                " missing audio files referenced in oto.ini");
        }
        if (validation.duplicate_aliases > 0) {
            validation.suggestions.push_back(
                "Remove " + std::to_string(validation.duplicate_aliases) + 
                " duplicate aliases from oto.ini");
        }
    }
    
    return validation;
}

bool VoicebankScanner::validate_directory_structure(const std::string& path, VoicebankValidation& validation) {
    namespace fs = std::filesystem;
    
    fs::path base_path(path);
    
    // Check for oto.ini
    fs::path oto_path = base_path / "oto.ini";
    validation.has_oto_ini = fs::exists(oto_path) && fs::is_regular_file(oto_path);
    
    // Check for character.txt
    fs::path char_path = base_path / "character.txt";
    validation.has_character_txt = fs::exists(char_path) && fs::is_regular_file(char_path);
    
    // Check for readme files
    std::vector<std::string> readme_names = {"readme.txt", "README.txt", "readme.md", "README.md"};
    for (const auto& readme_name : readme_names) {
        fs::path readme_path = base_path / readme_name;
        if (fs::exists(readme_path) && fs::is_regular_file(readme_path)) {
            validation.has_readme = true;
            break;
        }
    }
    
    // Count audio files
    auto audio_files = get_audio_files_in_directory(base_path);
    validation.total_audio_files = audio_files.size();
    validation.has_audio_files = !audio_files.empty();
    
    return validation.has_oto_ini && validation.has_audio_files;
}

bool VoicebankScanner::validate_oto_files(const std::string& path, VoicebankValidation& validation) {
    namespace fs = std::filesystem;
    
    fs::path oto_path = fs::path(path) / "oto.ini";
    if (!fs::exists(oto_path)) {
        validation.errors.push_back("oto.ini file not found");
        return false;
    }
    
    try {
        utau::OtoIniParser parser;
        utau::OtoIniParser::ParseOptions options;
        options.validate_audio_files = false;  // We'll validate separately
        options.auto_detect_encoding = true;
        parser.set_options(options);
        
        auto parse_result = parser.parse_file(oto_path.string());
        
        if (!parse_result.success) {
            validation.errors.push_back("Failed to parse oto.ini file");
            for (const auto& error : parse_result.errors) {
                validation.errors.push_back("OTO Error: " + error);
            }
            return false;
        }
        
        // Store parsing results
        validation.total_oto_entries = parse_result.entries.size();
        
        // Count referenced audio files
        std::unordered_set<std::string> referenced_files;
        for (const auto& entry : parse_result.entries) {
            referenced_files.insert(entry.filename);
        }
        validation.referenced_audio_files = referenced_files.size();
        
        // Find duplicate aliases
        std::unordered_map<std::string, int> alias_count;
        for (const auto& entry : parse_result.entries) {
            alias_count[entry.alias]++;
        }
        for (const auto& [alias, count] : alias_count) {
            if (count > 1) {
                validation.duplicate_aliases++;
            }
        }
        
        // Validate timing parameters if enabled
        if (config_.validate_timing_parameters) {
            for (const auto& entry : parse_result.entries) {
                if (!entry.is_valid()) {
                    validation.invalid_timing_entries++;
                }
            }
        }
        
        // Check for missing audio files
        fs::path base_path(path);
        for (const auto& filename : referenced_files) {
            fs::path audio_path = base_path / filename;
            if (!fs::exists(audio_path)) {
                validation.missing_audio_files++;
            }
        }
        
        // Find orphaned audio files
        auto all_audio_files = get_audio_files_in_directory(base_path);
        for (const auto& audio_file : all_audio_files) {
            std::string filename = audio_file.filename().string();
            if (referenced_files.find(filename) == referenced_files.end()) {
                validation.orphaned_audio_files++;
            }
        }
        
        // Copy warnings
        for (const auto& warning : parse_result.warnings) {
            validation.warnings.push_back("OTO Warning: " + warning);
        }
        
        // Check encoding issues
        if (config_.detect_encoding_issues) {
            if (parse_result.voicebank_info.encoding_detected == "UNKNOWN") {
                validation.encoding_issues++;
                validation.warnings.push_back("Could not detect oto.ini encoding");
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        validation.errors.push_back("Exception while parsing oto.ini: " + std::string(e.what()));
        return false;
    }
}

bool VoicebankScanner::validate_audio_files(const std::string& path, VoicebankValidation& validation) {
    namespace fs = std::filesystem;
    
    auto audio_files = get_audio_files_in_directory(fs::path(path));
    
    for (const auto& audio_file : audio_files) {
        std::string file_path = audio_file.string();
        AudioFileInfo info = validate_audio_file_impl(file_path);
        
        validation.audio_info[audio_file.filename().string()] = info;
        
        if (info.is_valid) {
            validation.valid_audio_files++;
        }
        
        // Check format compatibility
        if (!is_supported_audio_format(audio_file.extension().string())) {
            validation.audio_format_issues++;
            validation.warnings.push_back(
                "Unsupported audio format: " + audio_file.filename().string());
        }
        
        // Check quality issues
        if (info.has_clipping) {
            validation.audio_quality_warnings++;
            validation.warnings.push_back(
                "Audio clipping detected: " + audio_file.filename().string());
        }
        
        if (info.sample_rate != config_.preferred_sample_rate) {
            validation.warnings.push_back(
                "Non-standard sample rate in " + audio_file.filename().string() + 
                ": " + std::to_string(info.sample_rate) + " Hz");
        }
    }
    
    return validation.audio_format_issues == 0;
}

bool VoicebankScanner::validate_metadata_files(const std::string& path, VoicebankValidation& validation) {
    namespace fs = std::filesystem;
    
    // Validate character.txt if present
    fs::path char_path = fs::path(path) / "character.txt";
    if (fs::exists(char_path)) {
        try {
            std::ifstream char_file(char_path);
            if (!char_file.is_open()) {
                validation.warnings.push_back("Cannot read character.txt file");
            } else {
                // Basic validation - check if file is readable and not empty
                char_file.seekg(0, std::ios::end);
                if (char_file.tellg() == 0) {
                    validation.warnings.push_back("character.txt file is empty");
                }
            }
        } catch (const std::exception& e) {
            validation.warnings.push_back(
                "Error reading character.txt: " + std::string(e.what()));
        }
    }
    
    return true;  // Metadata validation is non-critical
}

AudioFileInfo VoicebankScanner::validate_audio_file_impl(const std::string& file_path) {
    AudioFileInfo info;
    info.filename = std::filesystem::path(file_path).filename().string();
    info.full_path = file_path;
    
    namespace fs = std::filesystem;
    
    // Check if file exists
    if (!fs::exists(file_path)) {
        info.exists = false;
        return info;
    }
    
    info.exists = true;
    info.file_size = fs::file_size(file_path);
    
    // Read basic audio properties
    bool props_read = read_audio_properties(file_path, info);
    if (!props_read) {
        info.is_valid = false;
        return info;
    }
    
    info.is_valid = true;
    
    // Perform quality analysis if enabled
    if (config_.analyze_audio_quality) {
        analyze_audio_quality(file_path, info);
    }
    
    return info;
}

bool VoicebankScanner::read_audio_properties(const std::string& file_path, AudioFileInfo& info) {
    // This is a simplified implementation - in a real application,
    // you would use a proper audio library like libsndfile, FMOD, or similar
    
    std::string extension = std::filesystem::path(file_path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".wav") {
        // Basic WAV header reading
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Read WAV header (simplified)
        char header[44];
        file.read(header, 44);
        
        if (file.gcount() != 44) return false;
        
        // Check RIFF header
        if (std::string(header, 4) != "RIFF") return false;
        if (std::string(header + 8, 4) != "WAVE") return false;
        
        // Extract basic properties from header
        info.sample_rate = *reinterpret_cast<const uint32_t*>(header + 24);
        info.bit_depth = *reinterpret_cast<const uint16_t*>(header + 34);
        info.channels = *reinterpret_cast<const uint16_t*>(header + 22);
        info.format = "WAV";
        
        // Calculate duration (simplified)
        uint32_t data_size = *reinterpret_cast<const uint32_t*>(header + 40);
        if (info.sample_rate > 0 && info.bit_depth > 0 && info.channels > 0) {
            double bytes_per_sample = (info.bit_depth / 8.0) * info.channels;
            double total_samples = data_size / bytes_per_sample;
            info.duration_ms = (total_samples / info.sample_rate) * 1000.0;
        }
        
        return true;
    }
    
    // For other formats, use default values
    info.sample_rate = 44100;  // Assume standard values
    info.bit_depth = 16;
    info.channels = 1;
    info.format = extension.substr(1);  // Remove the dot
    info.duration_ms = 1000.0;  // Default 1 second
    
    return true;
}

bool VoicebankScanner::analyze_audio_quality(const std::string& file_path, AudioFileInfo& info) {
    // Simplified quality analysis - in practice, you'd use proper audio analysis
    
    // Estimate quality based on file size and duration
    if (info.duration_ms > 0 && info.file_size > 0) {
        double bytes_per_ms = info.file_size / info.duration_ms;
        
        // Very rough quality estimates
        if (bytes_per_ms < 50) {
            info.rms_level = 0.3;  // Low quality
            info.snr_estimate = 30.0;
        } else if (bytes_per_ms > 200) {
            info.rms_level = 0.7;  // High quality
            info.snr_estimate = 60.0;
        } else {
            info.rms_level = 0.5;  // Medium quality
            info.snr_estimate = 45.0;
        }
        
        info.peak_level = info.rms_level * 1.4;  // Rough estimate
        info.has_clipping = info.peak_level > 0.95;
    }
    
    return true;
}

// Utility method implementations
bool VoicebankScanner::should_skip_directory(const std::string& directory_name) const {
    // Check against excluded directories
    for (const auto& excluded : config_.excluded_directories) {
        if (directory_name == excluded) {
            return true;
        }
    }
    
    // Skip hidden directories (starting with .)
    if (!directory_name.empty() && directory_name[0] == '.') {
        return true;
    }
    
    return false;
}

bool VoicebankScanner::should_skip_file(const std::string& filename) const {
    // Check against excluded files
    for (const auto& excluded : config_.excluded_files) {
        if (filename == excluded) {
            return true;
        }
    }
    
    return false;
}

bool VoicebankScanner::is_supported_audio_format(const std::string& file_extension) const {
    std::string ext = file_extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return config_.supported_audio_formats.find(ext) != config_.supported_audio_formats.end();
}

std::string VoicebankScanner::extract_voicebank_name(const std::string& path) const {
    namespace fs = std::filesystem;
    return fs::path(path).filename().string();
}

bool VoicebankScanner::is_directory_accessible(const std::filesystem::path& path) const {
    try {
        std::filesystem::directory_iterator iter(path);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

size_t VoicebankScanner::count_files_in_directory(const std::filesystem::path& path) const {
    size_t count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                count++;
            }
        }
    } catch (const std::exception&) {
        // Ignore errors and return 0
    }
    return count;
}

std::vector<std::filesystem::path> VoicebankScanner::get_audio_files_in_directory(
    const std::filesystem::path& path) const {
    
    std::vector<std::filesystem::path> audio_files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (is_supported_audio_format(extension)) {
                    audio_files.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception&) {
        // Ignore errors
    }
    
    return audio_files;
}

void VoicebankScanner::process_voicebank_parallel(const std::vector<std::string>& paths,
                                                 VoicebankDiscovery& result) {
    
    int num_threads = std::min(config_.max_threads, (int)paths.size());
    std::vector<std::future<void>> futures;
    
    std::mutex result_mutex;
    std::atomic<size_t> completed_count(0);
    
    auto worker = [this, &result, &result_mutex, &completed_count, &paths](const std::vector<std::string>& worker_paths) {
        for (const auto& path : worker_paths) {
            if (cancel_requested_) break;
            
            VoicebankValidation validation = validate_voicebank_impl(path);
            
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                if (validation.is_valid) {
                    result.valid_voicebanks++;
                } else if (validation.has_oto_ini || validation.has_audio_files) {
                    result.partial_voicebanks++;
                } else {
                    result.invalid_voicebanks++;
                }
            }
            
            completed_count++;
            
            if (progress_callback_) {
                progress_callback_->on_voicebank_validated(path, validation.is_valid);
                progress_callback_->on_scan_progress(completed_count, paths.size());
            }
        }
    };
    
    // Distribute paths among threads
    size_t paths_per_thread = (paths.size() + num_threads - 1) / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        size_t start_idx = i * paths_per_thread;
        size_t end_idx = std::min(start_idx + paths_per_thread, paths.size());
        
        if (start_idx < end_idx) {
            std::vector<std::string> worker_paths(
                paths.begin() + start_idx, 
                paths.begin() + end_idx);
            
            futures.emplace_back(
                std::async(std::launch::async, worker, std::move(worker_paths)));
        }
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
}

void VoicebankScanner::report_progress(const std::string& operation, 
                                      const std::string& path, 
                                      size_t current, 
                                      size_t total) {
    if (progress_callback_) {
        if (total > 0) {
            progress_callback_->on_scan_progress(current, total);
        }
    }
}

void VoicebankScanner::report_error(const std::string& path, const std::string& error) {
    if (progress_callback_) {
        progress_callback_->on_scan_error(path, error);
    }
    LOG_ERROR("Scanner error at " + path + ": " + error);
}

void VoicebankScanner::report_warning(const std::string& path, const std::string& warning) {
    if (progress_callback_) {
        progress_callback_->on_validation_warning(path, warning);
    }
    LOG_WARN("Scanner warning at " + path + ": " + warning);
}

// ConsoleProgressReporter implementation
void ConsoleProgressReporter::on_scan_started(const std::string& path) {
    std::cout << "🔍 Starting voice bank scan: " << path << std::endl;
    last_progress_time_ = std::chrono::steady_clock::now();
}

void ConsoleProgressReporter::on_directory_entered(const std::string& path, size_t depth) {
    if (verbose_) {
        std::cout << std::string(depth * 2, ' ') << "📁 " << path << std::endl;
    }
}

void ConsoleProgressReporter::on_voicebank_found(const std::string& path) {
    std::cout << "🎤 Found voice bank: " << std::filesystem::path(path).filename().string() << std::endl;
}

void ConsoleProgressReporter::on_voicebank_validated(const std::string& path, bool is_valid) {
    std::string status = is_valid ? "✅" : "❌";
    std::cout << status << " Validated: " << std::filesystem::path(path).filename().string() << std::endl;
}

void ConsoleProgressReporter::on_scan_progress(size_t current, size_t total) {
    auto now = std::chrono::steady_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress_time_);
    
    // Only update progress every 100ms to avoid spam
    if (time_diff.count() >= 100) {
        print_progress_bar(current, total);
        last_progress_time_ = now;
    }
}

void ConsoleProgressReporter::on_scan_completed(const VoicebankDiscovery& result) {
    std::cout << "\n🎉 Scan completed!" << std::endl;
    std::cout << "   Duration: " << format_duration(result.scan_duration) << std::endl;
    std::cout << "   Directories scanned: " << result.directories_scanned << std::endl;
    std::cout << "   Files scanned: " << result.files_scanned << std::endl;
    std::cout << "   Voice banks found: " << result.voicebank_paths.size() << std::endl;
    std::cout << "   ├─ Valid: " << result.valid_voicebanks << std::endl;
    std::cout << "   ├─ Partial: " << result.partial_voicebanks << std::endl;
    std::cout << "   └─ Invalid: " << result.invalid_voicebanks << std::endl;
    
    if (!result.scan_errors.empty()) {
        std::cout << "   Errors: " << result.scan_errors.size() << std::endl;
    }
    if (!result.scan_warnings.empty()) {
        std::cout << "   Warnings: " << result.scan_warnings.size() << std::endl;
    }
}

void ConsoleProgressReporter::on_scan_error(const std::string& path, const std::string& error) {
    std::cerr << "❌ Error at " << path << ": " << error << std::endl;
}

void ConsoleProgressReporter::on_validation_warning(const std::string& path, const std::string& warning) {
    if (verbose_) {
        std::cout << "⚠️  Warning at " << path << ": " << warning << std::endl;
    }
}

void ConsoleProgressReporter::print_progress_bar(size_t current, size_t total, int width) {
    if (total == 0) return;
    
    double progress = (double)current / total;
    int filled = (int)(progress * width);
    
    std::cout << "\r[";
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            std::cout << "█";
        } else {
            std::cout << "░";
        }
    }
    std::cout << "] " << std::setprecision(1) << std::fixed << (progress * 100) << "% ";
    std::cout << "(" << current << "/" << total << ")";
    std::cout.flush();
}

std::string ConsoleProgressReporter::format_file_size(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::setprecision(1) << std::fixed << size << " " << units[unit];
    return oss.str();
}

std::string ConsoleProgressReporter::format_duration(std::chrono::milliseconds duration) {
    auto total_ms = duration.count();
    
    if (total_ms < 1000) {
        return std::to_string(total_ms) + "ms";
    }
    
    auto seconds = total_ms / 1000;
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }
    
    auto minutes = seconds / 60;
    seconds %= 60;
    return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
}

} // namespace conditioning
} // namespace nexussynth
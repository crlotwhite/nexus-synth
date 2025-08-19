#include "nexussynth/label_file_generator.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <chrono>
#include <thread>
#include <filesystem>
#include <set>

namespace nexussynth {
namespace context {

    // LabelFileGenerator Implementation
    LabelFileGenerator::LabelFileGenerator() {
        // Use default config
    }

    LabelFileGenerator::LabelFileGenerator(const GenerationConfig& config) : config_(config) {
    }

    bool LabelFileGenerator::generateFromContextFeatures(
        const std::vector<ContextFeatures>& features,
        const std::string& output_filename) {
        
        auto entries = createLabelEntries(features);
        
        if (config_.sort_by_time) {
            sortEntriesByTime(entries);
        }
        
        if (config_.validate_timing && !validateTimingConsistency(entries)) {
            return false;
        }
        
        return writeLabelFile(entries, output_filename);
    }

    bool LabelFileGenerator::generateFromHMMFeatures(
        const std::vector<hmm::ContextFeatureVector>& features,
        const std::vector<PhonemeTimingInfo>& timing_info,
        const std::string& output_filename) {
        
        auto entries = createLabelEntries(features, timing_info);
        
        if (config_.sort_by_time) {
            sortEntriesByTime(entries);
        }
        
        if (config_.validate_timing && !validateTimingConsistency(entries)) {
            return false;
        }
        
        return writeLabelFile(entries, output_filename);
    }

    std::vector<LabelFileGenerator::LabelEntry> LabelFileGenerator::createLabelEntries(
        const std::vector<ContextFeatures>& features) const {
        
        std::vector<LabelEntry> entries;
        entries.reserve(features.size());
        
        for (const auto& feature : features) {
            LabelEntry entry;
            entry.start_time_ms = feature.current_timing.start_time_ms;
            entry.end_time_ms = feature.current_timing.end_time_ms;
            
            // Convert ContextFeatures to HTS label format
            // Create a temporary HMM ContextFeatureVector for label generation
            hmm::ContextFeatureVector hmm_context;
            hmm_context.current_phoneme = feature.current_timing.phoneme;
            
            // Extract context phonemes from phoneme_context if available
            if (feature.phoneme_context.size() >= 7) { // CONTEXT_WINDOW_SIZE
                // Map from context window to quinphone context
                size_t center_idx = feature.phoneme_context.size() / 2; // Center of window
                
                // For simplification, use center phoneme as current
                // In a full implementation, would need phoneme name extraction from features
                hmm_context.left_left_phoneme = (center_idx >= 2) ? "sil" : "sil";  // Simplified
                hmm_context.left_phoneme = (center_idx >= 1) ? "sil" : "sil";       // Simplified
                hmm_context.right_phoneme = (center_idx + 1 < feature.phoneme_context.size()) ? "sil" : "sil";
                hmm_context.right_right_phoneme = (center_idx + 2 < feature.phoneme_context.size()) ? "sil" : "sil";
            }
            
            // Set timing and prosodic information
            hmm_context.note_duration_ms = feature.current_timing.duration_ms;
            
            // Add MIDI information if available
            if (feature.current_midi.note_number > 0) {
                // Convert MIDI note to pitch in cents (relative to A4=440Hz)
                double pitch_hz = 440.0 * std::pow(2.0, (feature.current_midi.note_number - 69) / 12.0);
                hmm_context.pitch_cents = 1200.0 * std::log2(pitch_hz / 440.0);
            }
            
            entry.hts_label = hmm_context.to_hts_label();
            entries.push_back(entry);
        }
        
        return entries;
    }

    std::vector<LabelFileGenerator::LabelEntry> LabelFileGenerator::createLabelEntries(
        const std::vector<hmm::ContextFeatureVector>& features,
        const std::vector<PhonemeTimingInfo>& timing_info) const {
        
        std::vector<LabelEntry> entries;
        entries.reserve(std::min(features.size(), timing_info.size()));
        
        for (size_t i = 0; i < features.size() && i < timing_info.size(); ++i) {
            LabelEntry entry;
            entry.start_time_ms = timing_info[i].start_time_ms;
            entry.end_time_ms = timing_info[i].end_time_ms;
            entry.hts_label = features[i].to_hts_label();
            entries.push_back(entry);
        }
        
        return entries;
    }

    bool LabelFileGenerator::writeLabelFile(
        const std::vector<LabelEntry>& entries,
        const std::string& filename) const {
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        for (const auto& entry : entries) {
            file << formatLabelLine(entry) << "\n";
        }
        
        file.close();
        return true;
    }

    std::optional<std::vector<LabelFileGenerator::LabelEntry>> LabelFileGenerator::readLabelFile(
        const std::string& filename) const {
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        std::vector<LabelEntry> entries;
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments
            
            auto entry = parseLabelLine(line);
            if (entry) {
                entries.push_back(*entry);
            }
        }
        
        file.close();
        return entries;
    }

    LabelFileGenerator::ValidationResult LabelFileGenerator::validateLabelFile(
        const std::string& filename) const {
        
        ValidationResult result;
        
        auto entries_opt = readLabelFile(filename);
        if (!entries_opt) {
            result.errors.push_back("Could not read label file: " + filename);
            return result;
        }
        
        return validateLabelEntries(*entries_opt);
    }

    LabelFileGenerator::ValidationResult LabelFileGenerator::validateLabelEntries(
        const std::vector<LabelEntry>& entries) const {
        
        ValidationResult result;
        result.total_entries = entries.size();
        result.is_valid = true;
        
        if (entries.empty()) {
            result.warnings.push_back("Label file is empty");
            return result;
        }
        
        // Check timing consistency
        double total_duration = 0.0;
        double prev_end_time = -1.0;
        
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            
            // Check time order
            if (entry.start_time_ms < 0) {
                result.errors.push_back("Entry " + std::to_string(i) + ": Negative start time");
                result.is_valid = false;
            }
            
            if (entry.end_time_ms <= entry.start_time_ms) {
                result.errors.push_back("Entry " + std::to_string(i) + ": End time <= start time");
                result.is_valid = false;
            }
            
            // Check continuity
            if (config_.validate_timing && prev_end_time >= 0) {
                double gap = entry.start_time_ms - prev_end_time;
                if (std::abs(gap) > config_.time_precision_ms) {
                    if (gap > 0) {
                        result.warnings.push_back("Entry " + std::to_string(i) + ": Gap in timing (" + 
                                                 std::to_string(gap) + "ms)");
                    } else {
                        result.warnings.push_back("Entry " + std::to_string(i) + ": Overlap in timing (" + 
                                                 std::to_string(-gap) + "ms)");
                    }
                }
            }
            
            // Check label format
            if (entry.hts_label.empty()) {
                result.errors.push_back("Entry " + std::to_string(i) + ": Empty label");
                result.is_valid = false;
            }
            
            total_duration += (entry.end_time_ms - entry.start_time_ms);
            prev_end_time = entry.end_time_ms;
        }
        
        result.total_duration_ms = total_duration;
        
        // Check overall consistency
        if (entries.front().start_time_ms > config_.time_precision_ms) {
            result.warnings.push_back("First entry does not start at time 0");
        }
        
        return result;
    }

    std::string LabelFileGenerator::formatTimeStamp(double time_ms) const {
        if (config_.time_format == "seconds") {
            return std::to_string(time_ms / 1000.0);
        } else if (config_.time_format == "frames") {
            // Assume 16kHz sample rate, 5ms frame shift (typical for HTS)
            int frame_number = static_cast<int>(time_ms / 5.0);
            return std::to_string(frame_number);
        } else { // "ms" format
            return std::to_string(static_cast<long long>(time_ms * 10000)); // HTS uses 10ns units
        }
    }

    double LabelFileGenerator::parseTimeStamp(const std::string& time_str) const {
        double value = std::stod(time_str);
        
        if (config_.time_format == "seconds") {
            return value * 1000.0;
        } else if (config_.time_format == "frames") {
            return value * 5.0; // Convert frame to ms (5ms frame shift)
        } else { // HTS format (10ns units)
            return value / 10000.0;
        }
    }

    LabelFileGenerator::FileStatistics LabelFileGenerator::analyzeLabFile(
        const std::string& filename) const {
        
        FileStatistics stats;
        
        auto entries_opt = readLabelFile(filename);
        if (!entries_opt) {
            return stats;
        }
        
        const auto& entries = *entries_opt;
        stats.total_entries = entries.size();
        
        if (entries.empty()) {
            return stats;
        }
        
        // Calculate timing statistics
        std::vector<double> durations;
        std::set<std::string> phonemes;
        
        for (const auto& entry : entries) {
            double duration = entry.end_time_ms - entry.start_time_ms;
            durations.push_back(duration);
            stats.total_duration_ms += duration;
            
            // Extract phoneme from HTS label (simplified extraction)
            std::regex phoneme_regex(R"(\+([^+]+)\+)");
            std::smatch match;
            if (std::regex_search(entry.hts_label, match, phoneme_regex)) {
                phonemes.insert(match[1].str());
            }
        }
        
        if (!durations.empty()) {
            stats.avg_phoneme_duration_ms = stats.total_duration_ms / durations.size();
            stats.min_phoneme_duration_ms = *std::min_element(durations.begin(), durations.end());
            stats.max_phoneme_duration_ms = *std::max_element(durations.begin(), durations.end());
        }
        
        stats.unique_phonemes.assign(phonemes.begin(), phonemes.end());
        
        return stats;
    }

    std::string LabelFileGenerator::formatLabelLine(const LabelEntry& entry) const {
        std::ostringstream oss;
        
        if (config_.include_timing) {
            oss << formatTimeStamp(entry.start_time_ms) << " "
                << formatTimeStamp(entry.end_time_ms) << " ";
        }
        
        oss << entry.hts_label;
        
        return oss.str();
    }

    std::optional<LabelFileGenerator::LabelEntry> LabelFileGenerator::parseLabelLine(
        const std::string& line) const {
        
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        
        while (iss >> token) {
            tokens.push_back(token);
        }
        
        if (tokens.empty()) {
            return std::nullopt;
        }
        
        LabelEntry entry;
        
        if (config_.include_timing && tokens.size() >= 3) {
            // Format: start_time end_time label
            entry.start_time_ms = parseTimeStamp(tokens[0]);
            entry.end_time_ms = parseTimeStamp(tokens[1]);
            entry.hts_label = tokens[2];
            
            // Combine remaining tokens if label contains spaces
            for (size_t i = 3; i < tokens.size(); ++i) {
                entry.hts_label += " " + tokens[i];
            }
        } else {
            // Label only format
            entry.hts_label = line;
        }
        
        return entry;
    }

    bool LabelFileGenerator::validateTimingConsistency(const std::vector<LabelEntry>& entries) const {
        if (entries.size() < 2) return true;
        
        for (size_t i = 1; i < entries.size(); ++i) {
            double gap = entries[i].start_time_ms - entries[i-1].end_time_ms;
            if (std::abs(gap) > config_.time_precision_ms) {
                return false;
            }
        }
        
        return true;
    }

    void LabelFileGenerator::sortEntriesByTime(std::vector<LabelEntry>& entries) const {
        std::sort(entries.begin(), entries.end(),
            [](const LabelEntry& a, const LabelEntry& b) {
                return a.start_time_ms < b.start_time_ms;
            });
    }

    // LabelFileBatchProcessor Implementation
    LabelFileBatchProcessor::LabelFileBatchProcessor() {
        // Use default config
    }

    LabelFileBatchProcessor::LabelFileBatchProcessor(const BatchConfig& config) : config_(config) {
    }

    LabelFileBatchProcessor::BatchResult LabelFileBatchProcessor::processDirectory(
        const std::string& input_directory) {
        
        BatchResult result;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Scan directory for compatible files
        std::vector<std::string> input_files;
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(input_directory)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    // Look for context feature files or other compatible formats
                    if (filename.find(".ctx") != std::string::npos || 
                        filename.find(".features") != std::string::npos) {
                        input_files.push_back(entry.path().string());
                    }
                }
            }
        } catch (const std::exception& e) {
            result.error_messages.push_back("Directory scan failed: " + std::string(e.what()));
            return result;
        }
        
        result = processFileList(input_files);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.total_processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        return result;
    }

    LabelFileBatchProcessor::BatchResult LabelFileBatchProcessor::processFileList(
        const std::vector<std::string>& input_files) {
        
        BatchResult result;
        result.total_files = input_files.size();
        
        for (const auto& input_file : input_files) {
            try {
                std::filesystem::path input_path(input_file);
                std::string output_file = config_.output_directory + "/" + 
                                         input_path.stem().string() + config_.file_extension;
                
                processFile(input_file, output_file);
                result.successful_files++;
                
            } catch (const std::exception& e) {
                result.failed_files++;
                result.error_files.push_back(input_file);
                result.error_messages.push_back("File: " + input_file + " Error: " + e.what());
                
                if (!config_.continue_on_error) {
                    break;
                }
            }
        }
        
        return result;
    }

    void LabelFileBatchProcessor::processFile(
        const std::string& input_file, 
        const std::string& output_file) {
        
        // This is a placeholder implementation
        // In a full implementation, would load context features from input_file
        // and generate labels using the generator_
        
        std::vector<hmm::ContextFeatureVector> dummy_features;
        std::vector<PhonemeTimingInfo> dummy_timing;
        
        generator_.generateFromHMMFeatures(dummy_features, dummy_timing, output_file);
    }

    // label_utils namespace implementation
    namespace label_utils {
        
        LabelFormat detectFormat(const std::string& filename) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                return LabelFormat::AUTO_DETECT;
            }
            
            std::string first_line;
            std::getline(file, first_line);
            
            // Simple format detection based on line structure
            if (first_line.find("/A:") != std::string::npos) {
                return LabelFormat::HTS_STANDARD;
            } else if (first_line.find("ms") != std::string::npos) {
                return LabelFormat::UTAU_TIMING;
            } else {
                return LabelFormat::NEXUS_EXTENDED;
            }
        }
        
        bool convertFormat(const std::string& input_file, const std::string& output_file,
                          LabelFormat input_format, LabelFormat output_format) {
            // Placeholder implementation
            return false;
        }
        
        QualityMetrics assessQuality(const std::string& label_file) {
            LabelFileGenerator generator;
            auto entries_opt = generator.readLabelFile(label_file);
            
            if (!entries_opt) {
                return QualityMetrics(); // Return default (poor quality)
            }
            
            return assessQuality(*entries_opt);
        }
        
        QualityMetrics assessQuality(const std::vector<LabelFileGenerator::LabelEntry>& entries) {
            QualityMetrics metrics;
            
            if (entries.empty()) {
                return metrics;
            }
            
            // Assess timing accuracy
            double timing_score = 1.0;
            for (size_t i = 1; i < entries.size(); ++i) {
                double gap = entries[i].start_time_ms - entries[i-1].end_time_ms;
                if (std::abs(gap) > 1.0) { // 1ms tolerance
                    timing_score *= 0.9; // Penalize gaps
                }
            }
            metrics.timing_accuracy = timing_score;
            
            // Assess label consistency
            size_t valid_labels = 0;
            for (const auto& entry : entries) {
                if (!entry.hts_label.empty() && entry.hts_label.find("/") != std::string::npos) {
                    valid_labels++;
                }
            }
            metrics.label_consistency = static_cast<double>(valid_labels) / entries.size();
            
            // Assess feature completeness (simplified)
            metrics.feature_completeness = 0.8; // Default assumption
            
            // Overall quality
            metrics.overall_quality = (metrics.timing_accuracy + metrics.label_consistency + 
                                     metrics.feature_completeness) / 3.0;
            
            return metrics;
        }
        
        ComparisonResult compareLabFiles(const std::string& file1, const std::string& file2) {
            ComparisonResult result;
            
            LabelFileGenerator generator;
            auto entries1_opt = generator.readLabelFile(file1);
            auto entries2_opt = generator.readLabelFile(file2);
            
            if (!entries1_opt || !entries2_opt) {
                return result; // Return default (no similarity)
            }
            
            const auto& entries1 = *entries1_opt;
            const auto& entries2 = *entries2_opt;
            
            result.total_entries = std::max(entries1.size(), entries2.size());
            size_t min_size = std::min(entries1.size(), entries2.size());
            
            for (size_t i = 0; i < min_size; ++i) {
                if (entries1[i].hts_label == entries2[i].hts_label) {
                    result.matching_entries++;
                } else {
                    result.differences.push_back("Entry " + std::to_string(i) + ": " + 
                                                entries1[i].hts_label + " vs " + entries2[i].hts_label);
                }
            }
            
            if (result.total_entries > 0) {
                result.similarity_score = static_cast<double>(result.matching_entries) / result.total_entries;
            }
            
            return result;
        }
        
    } // namespace label_utils

} // namespace context
} // namespace nexussynth
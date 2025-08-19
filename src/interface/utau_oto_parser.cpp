#include "nexussynth/utau_oto_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <regex>

namespace nexussynth {
namespace utau {

    // OtoEntry implementation
    bool OtoEntry::is_valid() const {
        return !filename.empty() && !alias.empty() && 
               std::isfinite(offset) && std::isfinite(consonant) && 
               std::isfinite(blank) && std::isfinite(preutterance) && 
               std::isfinite(overlap);
    }

    std::string OtoEntry::to_string() const {
        std::ostringstream oss;
        oss << filename << "=" << alias << "," << offset << "," << consonant << ","
            << blank << "," << preutterance << "," << overlap;
        return oss.str();
    }

    // EncodingDetector implementation
    EncodingDetector::Encoding EncodingDetector::detect_encoding(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return Encoding::UNKNOWN;
        }
        
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        return detect_encoding(data);
    }

    EncodingDetector::Encoding EncodingDetector::detect_encoding(const std::vector<uint8_t>& data) {
        if (data.empty()) {
            return Encoding::UNKNOWN;
        }
        
        // Check for UTF-8 BOM
        if (has_utf8_bom(data)) {
            return Encoding::UTF8_BOM;
        }
        
        // Check if ASCII only
        if (is_ascii_only(data)) {
            return Encoding::ASCII;
        }
        
        // Check for valid UTF-8
        if (is_valid_utf8(data)) {
            return Encoding::UTF8;
        }
        
        // Check for Shift-JIS markers
        if (has_shift_jis_markers(data)) {
            return Encoding::SHIFT_JIS;
        }
        
        // Default to Shift-JIS for compatibility with most UTAU files
        return Encoding::SHIFT_JIS;
    }

    std::string EncodingDetector::convert_to_utf8(const std::string& input, Encoding source_encoding) {
        switch (source_encoding) {
            case Encoding::ASCII:
            case Encoding::UTF8:
                return input;
                
            case Encoding::UTF8_BOM:
                // Remove BOM if present
                if (input.size() >= 3 && 
                    static_cast<uint8_t>(input[0]) == 0xEF &&
                    static_cast<uint8_t>(input[1]) == 0xBB &&
                    static_cast<uint8_t>(input[2]) == 0xBF) {
                    return input.substr(3);
                }
                return input;
                
            case Encoding::SHIFT_JIS:
                // Simplified Shift-JIS to UTF-8 conversion
                // For production use, would integrate ICU or similar library
                // For now, return as-is since most modern systems handle it
                return input;
                
            default:
                return input;
        }
    }

    std::vector<std::string> EncodingDetector::read_lines_with_encoding(
        const std::string& filename, Encoding encoding) {
        
        std::vector<std::string> lines;
        
        if (encoding == Encoding::UNKNOWN) {
            encoding = detect_encoding(filename);
        }
        
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return lines;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // Convert to UTF-8 if needed
        std::string utf8_content = convert_to_utf8(content, encoding);
        
        // Split into lines
        std::istringstream iss(utf8_content);
        std::string line;
        while (std::getline(iss, line)) {
            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        
        return lines;
    }

    std::string EncodingDetector::encoding_to_string(Encoding encoding) {
        switch (encoding) {
            case Encoding::ASCII: return "ASCII";
            case Encoding::UTF8: return "UTF-8";
            case Encoding::UTF8_BOM: return "UTF-8 with BOM";
            case Encoding::SHIFT_JIS: return "Shift-JIS";
            case Encoding::GB2312: return "GB2312";
            default: return "Unknown";
        }
    }

    bool EncodingDetector::has_utf8_bom(const std::vector<uint8_t>& data) {
        return data.size() >= 3 && 
               data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF;
    }

    bool EncodingDetector::is_valid_utf8(const std::vector<uint8_t>& data) {
        size_t i = 0;
        while (i < data.size()) {
            uint8_t byte = data[i];
            
            if (byte < 0x80) {
                // ASCII character
                i++;
            } else if ((byte >> 5) == 0x06) {
                // 110xxxxx - 2 byte sequence
                if (i + 1 >= data.size() || (data[i + 1] >> 6) != 0x02) {
                    return false;
                }
                i += 2;
            } else if ((byte >> 4) == 0x0E) {
                // 1110xxxx - 3 byte sequence
                if (i + 2 >= data.size() || 
                    (data[i + 1] >> 6) != 0x02 || (data[i + 2] >> 6) != 0x02) {
                    return false;
                }
                i += 3;
            } else if ((byte >> 3) == 0x1E) {
                // 11110xxx - 4 byte sequence
                if (i + 3 >= data.size() || 
                    (data[i + 1] >> 6) != 0x02 || (data[i + 2] >> 6) != 0x02 || 
                    (data[i + 3] >> 6) != 0x02) {
                    return false;
                }
                i += 4;
            } else {
                return false;
            }
        }
        return true;
    }

    bool EncodingDetector::has_shift_jis_markers(const std::vector<uint8_t>& data) {
        // Look for common Shift-JIS character patterns
        for (size_t i = 0; i < data.size() - 1; i++) {
            uint8_t first = data[i];
            uint8_t second = data[i + 1];
            
            // Check for Shift-JIS two-byte character ranges
            if ((first >= 0x81 && first <= 0x9F) || (first >= 0xE0 && first <= 0xFC)) {
                if ((second >= 0x40 && second <= 0x7E) || (second >= 0x80 && second <= 0xFC)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool EncodingDetector::is_ascii_only(const std::vector<uint8_t>& data) {
        return std::all_of(data.begin(), data.end(), [](uint8_t byte) {
            return byte < 0x80;
        });
    }

    // OtoIniParser implementation
    OtoIniParser::OtoIniParser() = default;

    OtoIniParser::OtoIniParser(const ParseOptions& options) : options_(options) {}

    OtoIniParser::ParseResult OtoIniParser::parse_file(const std::string& filename) {
        ParseResult result;
        
        if (!std::filesystem::exists(filename)) {
            result.errors.push_back("File not found: " + filename);
            return result;
        }
        
        EncodingDetector::Encoding encoding = EncodingDetector::Encoding::UNKNOWN;
        if (options_.auto_detect_encoding) {
            encoding = EncodingDetector::detect_encoding(filename);
            result.voicebank_info.encoding_detected = EncodingDetector::encoding_to_string(encoding);
        }
        
        auto lines = EncodingDetector::read_lines_with_encoding(filename, encoding);
        if (lines.empty()) {
            result.errors.push_back("Failed to read file or file is empty: " + filename);
            return result;
        }
        
        return parse_lines(lines, filename);
    }

    OtoIniParser::ParseResult OtoIniParser::parse_string(const std::string& content, 
                                                         const std::string& source_path) {
        std::vector<std::string> lines;
        std::istringstream iss(content);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        
        return parse_lines(lines, source_path);
    }

    OtoIniParser::ParseResult OtoIniParser::parse_lines(const std::vector<std::string>& lines,
                                                        const std::string& source_path) {
        ParseResult result;
        result.success = true;
        
        for (size_t i = 0; i < lines.size(); i++) {
            const std::string& line = lines[i];
            
            // Skip empty lines and comments
            std::string trimmed = trim_whitespace(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }
            
            auto entry_opt = parse_oto_line(line, i + 1, result.errors);
            if (entry_opt.has_value()) {
                if (options_.strict_validation) {
                    if (validate_oto_entry(entry_opt.value(), result.errors)) {
                        result.entries.push_back(entry_opt.value());
                    } else if (!options_.skip_invalid_entries) {
                        result.success = false;
                        break;
                    }
                } else {
                    result.entries.push_back(entry_opt.value());
                }
            } else if (!options_.skip_invalid_entries) {
                result.success = false;
                break;
            }
        }
        
        // Analyze voicebank info
        result.voicebank_info = analyze_voicebank(result.entries, source_path);
        
        // Cache entries for fast lookup
        cached_entries_ = result.entries;
        build_phoneme_index();
        
        return result;
    }

    std::optional<OtoEntry> OtoIniParser::parse_oto_line(const std::string& line,
                                                         size_t line_number,
                                                         std::vector<std::string>& errors) const {
        // Find the equals sign that separates filename from parameters
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            add_error(errors, "Missing '=' separator", line_number, line);
            return std::nullopt;
        }
        
        std::string filename = trim_whitespace(line.substr(0, equals_pos));
        std::string params = line.substr(equals_pos + 1);
        
        // Split parameters by comma
        auto tokens = tokenize_oto_line(params);
        if (tokens.size() < 6) {
            add_error(errors, "Insufficient parameters (expected 6)", line_number, line);
            return std::nullopt;
        }
        
        OtoEntry entry;
        entry.filename = filename;
        entry.alias = trim_whitespace(tokens[0]);
        entry.offset = parse_double_field(tokens[1]);
        entry.consonant = parse_double_field(tokens[2]);
        entry.blank = parse_double_field(tokens[3]);
        entry.preutterance = parse_double_field(tokens[4], options_.default_preutterance);
        entry.overlap = parse_double_field(tokens[5], options_.default_overlap);
        
        return entry;
    }

    std::vector<std::string> OtoIniParser::tokenize_oto_line(const std::string& line) const {
        std::vector<std::string> tokens;
        std::stringstream ss(line);
        std::string token;
        
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        return tokens;
    }

    double OtoIniParser::parse_double_field(const std::string& field, double default_value) const {
        std::string trimmed = trim_whitespace(field);
        if (trimmed.empty()) {
            return default_value;
        }
        
        try {
            return std::stod(trimmed);
        } catch (const std::exception&) {
            return default_value;
        }
    }

    bool OtoIniParser::validate_oto_entry(const OtoEntry& entry, std::vector<std::string>& errors) const {
        bool valid = true;
        
        if (entry.filename.empty()) {
            errors.push_back("Empty filename in oto entry");
            valid = false;
        }
        
        if (entry.alias.empty()) {
            errors.push_back("Empty alias in oto entry: " + entry.filename);
            valid = false;
        }
        
        if (!validate_timing_parameters(entry, errors)) {
            valid = false;
        }
        
        return valid;
    }

    bool OtoIniParser::validate_timing_parameters(const OtoEntry& entry, std::vector<std::string>& errors) const {
        bool valid = true;
        
        if (!std::isfinite(entry.offset) || entry.offset < 0) {
            errors.push_back("Invalid offset value in entry: " + entry.alias);
            valid = false;
        }
        
        if (!std::isfinite(entry.consonant) || entry.consonant < 0) {
            errors.push_back("Invalid consonant value in entry: " + entry.alias);
            valid = false;
        }
        
        if (!std::isfinite(entry.blank) || entry.blank < 0) {
            errors.push_back("Invalid blank value in entry: " + entry.alias);
            valid = false;
        }
        
        if (!std::isfinite(entry.preutterance)) {
            errors.push_back("Invalid preutterance value in entry: " + entry.alias);
            valid = false;
        }
        
        if (!std::isfinite(entry.overlap)) {
            errors.push_back("Invalid overlap value in entry: " + entry.alias);
            valid = false;
        }
        
        return valid;
    }

    VoicebankInfo OtoIniParser::analyze_voicebank(const std::vector<OtoEntry>& entries,
                                                  const std::string& base_path) const {
        VoicebankInfo info;
        info.path = base_path;
        info.total_entries = entries.size();
        
        std::unordered_map<std::string, size_t> alias_count;
        std::unordered_map<std::string, bool> file_exists;
        
        for (const auto& entry : entries) {
            // Count unique phonemes and aliases
            if (std::find(info.phonemes.begin(), info.phonemes.end(), entry.alias) == info.phonemes.end()) {
                info.phonemes.push_back(entry.alias);
            }
            
            if (std::find(info.filenames.begin(), info.filenames.end(), entry.filename) == info.filenames.end()) {
                info.filenames.push_back(entry.filename);
            }
            
            // Count timing information
            if (entry.offset != 0.0 || entry.consonant != 0.0 || entry.preutterance != 0.0 || entry.overlap != 0.0) {
                info.entries_with_timing++;
            }
            
            // Check for duplicates
            alias_count[entry.alias]++;
            if (alias_count[entry.alias] > 1) {
                info.duplicate_aliases++;
            }
            
            // Check file existence (if validation enabled)
            if (options_.validate_audio_files && !base_path.empty()) {
                std::string full_path = normalize_path_separators(base_path + "/" + entry.filename);
                if (file_exists.find(full_path) == file_exists.end()) {
                    file_exists[full_path] = audio_file_exists(base_path, entry.filename);
                }
                if (!file_exists[full_path]) {
                    info.missing_files++;
                }
            }
        }
        
        return info;
    }

    bool OtoIniParser::audio_file_exists(const std::string& base_path, const std::string& filename) const {
        std::string full_path = normalize_path_separators(base_path + "/" + filename);
        return std::filesystem::exists(full_path);
    }

    void OtoIniParser::build_phoneme_index() const {
        phoneme_index_.clear();
        for (size_t i = 0; i < cached_entries_.size(); i++) {
            const std::string& phoneme = cached_entries_[i].alias;
            phoneme_index_[phoneme].push_back(i);
        }
    }

    std::string OtoIniParser::trim_whitespace(const std::string& str) const {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    std::string OtoIniParser::normalize_path_separators(const std::string& path) const {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized;
    }

    void OtoIniParser::add_error(std::vector<std::string>& errors, const std::string& message,
                                size_t line_number, const std::string& context) const {
        std::ostringstream oss;
        if (line_number > 0) {
            oss << "Line " << line_number << ": ";
        }
        oss << message;
        if (!context.empty()) {
            oss << " [" << context << "]";
        }
        errors.push_back(oss.str());
    }

    std::vector<OtoEntry> OtoIniParser::get_entries_for_phoneme(const std::string& phoneme) const {
        std::vector<OtoEntry> results;
        auto it = phoneme_index_.find(phoneme);
        if (it != phoneme_index_.end()) {
            for (size_t index : it->second) {
                if (index < cached_entries_.size()) {
                    results.push_back(cached_entries_[index]);
                }
            }
        }
        return results;
    }

    // Utility functions
    namespace utils {
        
        std::vector<std::string> find_oto_files(const std::string& directory_path) {
            std::vector<std::string> oto_files;
            
            if (!std::filesystem::exists(directory_path)) {
                return oto_files;
            }
            
            for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename == "oto.ini" || filename == "oto_ini.txt") {
                        oto_files.push_back(entry.path().string());
                    }
                }
            }
            
            return oto_files;
        }
        
        std::vector<std::string> extract_unique_phonemes(const std::vector<OtoEntry>& entries) {
            std::vector<std::string> phonemes;
            for (const auto& entry : entries) {
                if (std::find(phonemes.begin(), phonemes.end(), entry.alias) == phonemes.end()) {
                    phonemes.push_back(entry.alias);
                }
            }
            std::sort(phonemes.begin(), phonemes.end());
            return phonemes;
        }
        
        bool is_utau_voicebank_directory(const std::string& directory_path) {
            auto oto_files = find_oto_files(directory_path);
            return !oto_files.empty();
        }
        
        std::vector<std::string> find_duplicate_aliases(const std::vector<OtoEntry>& entries) {
            std::unordered_map<std::string, size_t> alias_count;
            std::vector<std::string> duplicates;
            
            // Count occurrences of each alias
            for (const auto& entry : entries) {
                alias_count[entry.alias]++;
            }
            
            // Find aliases that appear more than once
            for (const auto& pair : alias_count) {
                if (pair.second > 1) {
                    duplicates.push_back(pair.first);
                }
            }
            
            std::sort(duplicates.begin(), duplicates.end());
            return duplicates;
        }
        
    } // namespace utils

} // namespace utau
} // namespace nexussynth
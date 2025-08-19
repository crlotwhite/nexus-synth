#include "nexussynth/voice_metadata.h"
#include <sstream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace nexussynth {
namespace metadata {

// Version implementation
const Version Version::NEXUS_SYNTH_1_0(1, 0, 0);
const Version Version::CURRENT(1, 0, 0);

bool Version::operator==(const Version& other) const {
    return major == other.major && minor == other.minor && patch == other.patch;
}

bool Version::operator<(const Version& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    return patch < other.patch;
}

bool Version::operator>(const Version& other) const {
    return other < *this;
}

bool Version::is_compatible_with(const Version& other) const {
    // Compatible if major version matches and this version >= other version
    return major == other.major && !(*this < other);
}

std::string Version::to_string() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    if (!build.empty()) {
        oss << "-" << build;
    }
    return oss.str();
}

Version Version::from_string(const std::string& version_str) {
    std::regex version_regex(R"((\d+)\.(\d+)\.(\d+)(?:-(.+))?)");
    std::smatch match;
    
    if (std::regex_match(version_str, match, version_regex)) {
        int major = std::stoi(match[1].str());
        int minor = std::stoi(match[2].str());
        int patch = std::stoi(match[3].str());
        std::string build = match[4].str();
        return Version(major, minor, patch, build);
    }
    
    return Version(); // Default version on parse failure
}

// AudioFormat implementation
bool AudioFormat::is_valid() const {
    return sample_rate > 0 && 
           frame_period > 0.0 && 
           bit_depth > 0 && 
           channels > 0 &&
           !format.empty();
}

AudioFormat AudioFormat::utau_standard() {
    return AudioFormat(44100, 5.0, 16, 1, "PCM");
}

AudioFormat AudioFormat::high_quality() {
    return AudioFormat(48000, 5.0, 24, 1, "PCM");
}

AudioFormat AudioFormat::low_latency() {
    return AudioFormat(44100, 2.5, 16, 1, "PCM");
}

// LicenseInfo implementation
LicenseInfo LicenseInfo::creative_commons_by_sa() {
    LicenseInfo license;
    license.name = "CC BY-SA 4.0";
    license.url = "https://creativecommons.org/licenses/by-sa/4.0/";
    license.summary = "Creative Commons Attribution-ShareAlike 4.0 International";
    license.commercial_use = true;
    license.modification = true;
    license.redistribution = true;
    license.attribution_required = true;
    return license;
}

LicenseInfo LicenseInfo::creative_commons_by_nc_sa() {
    LicenseInfo license;
    license.name = "CC BY-NC-SA 4.0";
    license.url = "https://creativecommons.org/licenses/by-nc-sa/4.0/";
    license.summary = "Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International";
    license.commercial_use = false;
    license.modification = true;
    license.redistribution = true;
    license.attribution_required = true;
    return license;
}

LicenseInfo LicenseInfo::utau_standard() {
    LicenseInfo license;
    license.name = "UTAU Standard";
    license.summary = "Standard UTAU voice bank license";
    license.commercial_use = false;
    license.modification = true;
    license.redistribution = false;
    license.attribution_required = true;
    return license;
}

LicenseInfo LicenseInfo::proprietary() {
    LicenseInfo license;
    license.name = "Proprietary";
    license.summary = "All rights reserved";
    license.commercial_use = false;
    license.modification = false;
    license.redistribution = false;
    license.attribution_required = true;
    return license;
}

// VoiceMetadata implementation
VoiceMetadata::VoiceMetadata() 
    : version(1, 0, 0)
    , language(DEFAULT_LANGUAGE)
    , model_type(DEFAULT_MODEL_TYPE)
    , nexussynth_version(Version::CURRENT)
    , phoneme_set(DEFAULT_PHONEME_SET)
    , created_time(std::chrono::system_clock::now())
    , modified_time(std::chrono::system_clock::now())
    , license(LicenseInfo::utau_standard()) {
}

VoiceMetadata::VoiceMetadata(const std::string& voice_name) : VoiceMetadata() {
    name = voice_name;
    display_name = voice_name;
}

bool VoiceMetadata::is_valid() const {
    return validate_and_get_errors().empty();
}

std::vector<std::string> VoiceMetadata::validate_and_get_errors() const {
    std::vector<std::string> errors;
    
    if (name.empty()) {
        errors.push_back("Voice name cannot be empty");
    }
    
    if (author.empty()) {
        errors.push_back("Author name cannot be empty");
    }
    
    if (!audio_format.is_valid()) {
        errors.push_back("Invalid audio format specification");
    }
    
    if (language.empty() || !utils::is_valid_language_code(language)) {
        errors.push_back("Invalid or missing language code");
    }
    
    if (model_type.empty()) {
        errors.push_back("Model type cannot be empty");
    }
    
    return errors;
}

std::string VoiceMetadata::to_json() const {
    cJSON* json = to_cjson();
    char* json_string = cJSON_Print(json);
    std::string result(json_string);
    free(json_string);
    cJSON_Delete(json);
    return result;
}

bool VoiceMetadata::from_json(const std::string& json_str) {
    cJSON* json = cJSON_Parse(json_str.c_str());
    if (!json) return false;
    
    bool result = from_cjson(json);
    cJSON_Delete(json);
    return result;
}

cJSON* VoiceMetadata::to_cjson() const {
    cJSON* json = cJSON_CreateObject();
    
    // Core identification
    add_string_to_json(json, "name", name);
    add_string_to_json(json, "display_name", display_name);
    add_string_to_json(json, "author", author);
    add_string_to_json(json, "contact", contact);
    add_string_to_json(json, "version", version.to_string());
    
    // Descriptive information
    add_string_to_json(json, "description", description);
    add_string_to_json(json, "language", language);
    add_string_to_json(json, "accent", accent);
    add_string_to_json(json, "voice_type", voice_type);
    
    // Tags array
    if (!tags.empty()) {
        cJSON* tags_array = cJSON_CreateArray();
        for (const auto& tag : tags) {
            cJSON_AddItemToArray(tags_array, cJSON_CreateString(tag.c_str()));
        }
        cJSON_AddItemToObject(json, "tags", tags_array);
    }
    
    // Technical specifications
    cJSON* audio_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(audio_json, "sample_rate", audio_format.sample_rate);
    cJSON_AddNumberToObject(audio_json, "frame_period", audio_format.frame_period);
    cJSON_AddNumberToObject(audio_json, "bit_depth", audio_format.bit_depth);
    cJSON_AddNumberToObject(audio_json, "channels", audio_format.channels);
    add_string_to_json(audio_json, "format", audio_format.format);
    cJSON_AddItemToObject(json, "audio_format", audio_json);
    
    add_string_to_json(json, "model_type", model_type);
    add_string_to_json(json, "nexussynth_version", nexussynth_version.to_string());
    add_string_to_json(json, "phoneme_set", phoneme_set);
    
    // Temporal information
    add_time_to_json(json, "created_time", created_time);
    add_time_to_json(json, "modified_time", modified_time);
    if (trained_time.has_value()) {
        add_time_to_json(json, "trained_time", trained_time.value());
    }
    
    // License information
    cJSON* license_json = cJSON_CreateObject();
    add_string_to_json(license_json, "name", license.name);
    add_string_to_json(license_json, "url", license.url);
    add_string_to_json(license_json, "summary", license.summary);
    cJSON_AddBoolToObject(license_json, "commercial_use", license.commercial_use);
    cJSON_AddBoolToObject(license_json, "modification", license.modification);
    cJSON_AddBoolToObject(license_json, "redistribution", license.redistribution);
    cJSON_AddBoolToObject(license_json, "attribution_required", license.attribution_required);
    add_string_to_json(license_json, "attribution", license.attribution);
    cJSON_AddItemToObject(json, "license", license_json);
    
    add_string_to_json(json, "copyright", copyright);
    
    // Model statistics
    cJSON* stats_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats_json, "total_phonemes", statistics.total_phonemes);
    cJSON_AddNumberToObject(stats_json, "total_contexts", statistics.total_contexts);
    cJSON_AddNumberToObject(stats_json, "total_states", statistics.total_states);
    cJSON_AddNumberToObject(stats_json, "total_gaussians", statistics.total_gaussians);
    cJSON_AddNumberToObject(stats_json, "model_size_mb", statistics.model_size_mb);
    cJSON_AddNumberToObject(stats_json, "training_time_hours", statistics.training_time_hours);
    cJSON_AddNumberToObject(stats_json, "training_utterances", statistics.training_utterances);
    cJSON_AddNumberToObject(stats_json, "average_f0_hz", statistics.average_f0_hz);
    cJSON_AddNumberToObject(stats_json, "f0_range_semitones", statistics.f0_range_semitones);
    cJSON_AddItemToObject(json, "statistics", stats_json);
    
    // Custom fields
    if (!custom_fields.empty()) {
        cJSON* custom_json = cJSON_CreateObject();
        for (const auto& [key, value] : custom_fields) {
            add_string_to_json(custom_json, key.c_str(), value);
        }
        cJSON_AddItemToObject(json, "custom_fields", custom_json);
    }
    
    return json;
}

bool VoiceMetadata::from_cjson(const cJSON* json) {
    if (!json) return false;
    
    // Core identification
    name = get_string_from_json(json, "name");
    display_name = get_string_from_json(json, "display_name", name);
    author = get_string_from_json(json, "author");
    contact = get_string_from_json(json, "contact");
    version = Version::from_string(get_string_from_json(json, "version", "1.0.0"));
    
    // Descriptive information
    description = get_string_from_json(json, "description");
    language = get_string_from_json(json, "language", DEFAULT_LANGUAGE);
    accent = get_string_from_json(json, "accent");
    voice_type = get_string_from_json(json, "voice_type");
    
    // Tags
    tags.clear();
    const cJSON* tags_array = cJSON_GetObjectItem(json, "tags");
    if (cJSON_IsArray(tags_array)) {
        cJSON* tag = nullptr;
        cJSON_ArrayForEach(tag, tags_array) {
            if (cJSON_IsString(tag)) {
                tags.emplace_back(tag->valuestring);
            }
        }
    }
    
    // Audio format
    const cJSON* audio_json = cJSON_GetObjectItem(json, "audio_format");
    if (audio_json) {
        const cJSON* sample_rate_json = cJSON_GetObjectItem(audio_json, "sample_rate");
        if (cJSON_IsNumber(sample_rate_json)) {
            audio_format.sample_rate = sample_rate_json->valueint;
        }
        
        const cJSON* frame_period_json = cJSON_GetObjectItem(audio_json, "frame_period");
        if (cJSON_IsNumber(frame_period_json)) {
            audio_format.frame_period = frame_period_json->valuedouble;
        }
        
        const cJSON* bit_depth_json = cJSON_GetObjectItem(audio_json, "bit_depth");
        if (cJSON_IsNumber(bit_depth_json)) {
            audio_format.bit_depth = bit_depth_json->valueint;
        }
        
        const cJSON* channels_json = cJSON_GetObjectItem(audio_json, "channels");
        if (cJSON_IsNumber(channels_json)) {
            audio_format.channels = channels_json->valueint;
        }
        
        audio_format.format = get_string_from_json(audio_json, "format", "PCM");
    }
    
    // Technical specifications
    model_type = get_string_from_json(json, "model_type", DEFAULT_MODEL_TYPE);
    nexussynth_version = Version::from_string(get_string_from_json(json, "nexussynth_version", "1.0.0"));
    phoneme_set = get_string_from_json(json, "phoneme_set", DEFAULT_PHONEME_SET);
    
    // Temporal information
    created_time = get_time_from_json(json, "created_time");
    modified_time = get_time_from_json(json, "modified_time");
    
    const cJSON* trained_time_json = cJSON_GetObjectItem(json, "trained_time");
    if (trained_time_json && cJSON_IsString(trained_time_json)) {
        trained_time = get_time_from_json(json, "trained_time");
    }
    
    return true;
}

bool VoiceMetadata::save_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    file << to_json();
    return file.good();
}

bool VoiceMetadata::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    return from_json(content);
}

std::string VoiceMetadata::get_full_name() const {
    if (!display_name.empty()) {
        return display_name + " (" + name + ")";
    }
    return name;
}

std::string VoiceMetadata::get_version_string() const {
    return version.to_string();
}

bool VoiceMetadata::is_compatible_with_engine(const Version& engine_version) const {
    return nexussynth_version.is_compatible_with(engine_version);
}

// Helper methods
void VoiceMetadata::add_string_to_json(cJSON* json, const char* key, const std::string& value) const {
    if (!value.empty()) {
        cJSON_AddStringToObject(json, key, value.c_str());
    }
}

void VoiceMetadata::add_time_to_json(cJSON* json, const char* key, 
                                    const std::chrono::system_clock::time_point& time) const {
    std::string time_str = utils::time_to_iso8601(time);
    if (!time_str.empty()) {
        cJSON_AddStringToObject(json, key, time_str.c_str());
    }
}

std::string VoiceMetadata::get_string_from_json(const cJSON* json, const char* key, 
                                               const std::string& default_value) const {
    const cJSON* item = cJSON_GetObjectItem(json, key);
    if (item && cJSON_IsString(item)) {
        return std::string(item->valuestring);
    }
    return default_value;
}

std::chrono::system_clock::time_point VoiceMetadata::get_time_from_json(const cJSON* json, 
                                                                       const char* key) const {
    std::string time_str = get_string_from_json(json, key);
    if (!time_str.empty()) {
        return utils::time_from_iso8601(time_str);
    }
    return std::chrono::system_clock::now();
}

// MetadataManager implementation
MetadataManager::MetadataManager() {
}

bool MetadataManager::add_voice(const VoiceMetadata& metadata) {
    if (!metadata.is_valid() || !is_valid_voice_name(metadata.name)) {
        return false;
    }
    
    voices_[metadata.name] = std::make_unique<VoiceMetadata>(metadata);
    return true;
}

bool MetadataManager::remove_voice(const std::string& name) {
    return voices_.erase(name) > 0;
}

VoiceMetadata* MetadataManager::get_voice(const std::string& name) {
    auto it = voices_.find(name);
    return (it != voices_.end()) ? it->second.get() : nullptr;
}

const VoiceMetadata* MetadataManager::get_voice(const std::string& name) const {
    auto it = voices_.find(name);
    return (it != voices_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> MetadataManager::get_all_names() const {
    std::vector<std::string> names;
    names.reserve(voices_.size());
    
    for (const auto& [name, metadata] : voices_) {
        names.push_back(name);
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

bool MetadataManager::is_valid_voice_name(const std::string& name) const {
    return utils::is_valid_voice_name(name);
}

// Utility functions
namespace utils {

std::string time_to_iso8601(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::chrono::system_clock::time_point time_from_iso8601(const std::string& iso_str) {
    std::tm tm = {};
    std::istringstream ss(iso_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    
    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string utf8_validate_and_clean(const std::string& input) {
    // Simple UTF-8 validation - in production, use proper UTF-8 library
    return input;
}

bool is_valid_language_code(const std::string& code) {
    // Simple validation for ISO 639-1 codes
    std::regex lang_regex("[a-z]{2}");
    return std::regex_match(code, lang_regex);
}

bool is_valid_voice_name(const std::string& name) {
    if (name.empty() || name.length() > 64) return false;
    
    // Allow alphanumeric, underscore, hyphen, and spaces
    std::regex name_regex("[a-zA-Z0-9_\\-\\s]+");
    return std::regex_match(name, name_regex);
}

std::string generate_metadata_filename(const std::string& voice_name) {
    return voice_name + "_metadata.json";
}

bool validate_audio_format(const AudioFormat& format) {
    return format.is_valid();
}

bool validate_version(const Version& version) {
    return version.major >= 0 && version.minor >= 0 && version.patch >= 0;
}

} // namespace utils

} // namespace metadata
} // namespace nexussynth
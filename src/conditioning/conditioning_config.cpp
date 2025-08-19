#include "nexussynth/conditioning_config.h"
#include "nexussynth/utau_logger.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#endif

namespace nexussynth {
namespace conditioning {

// ConfigManager implementation
ConfigManager::ConfigManager() {
    // Ensure configuration directory exists
    ensure_config_directory_exists();
}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::load_config(const std::string& file_path, ConditioningConfig& config) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open configuration file: " + file_path);
            return false;
        }
        
        // Read entire file content
        std::string json_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        file.close();
        
        if (json_content.empty()) {
            LOG_ERROR("Configuration file is empty: " + file_path);
            return false;
        }
        
        // Parse JSON and populate config
        bool success = config_from_json(json_content, config);
        if (success) {
            LOG_INFO("Successfully loaded configuration from: " + file_path);
        } else {
            LOG_ERROR("Failed to parse configuration file: " + file_path);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception loading configuration: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::save_config(const std::string& file_path, const ConditioningConfig& config) {
    try {
        // Validate config before saving
        auto validation = validate_config(config);
        if (!validation.is_valid) {
            LOG_ERROR("Cannot save invalid configuration");
            for (const auto& error : validation.errors) {
                LOG_ERROR("Validation error: " + error);
            }
            return false;
        }
        
        // Convert to JSON
        std::string json_str = config_to_json(config);
        if (json_str.empty()) {
            LOG_ERROR("Failed to serialize configuration to JSON");
            return false;
        }
        
        // Ensure output directory exists
        std::filesystem::path file_path_obj(file_path);
        std::filesystem::path parent_dir = file_path_obj.parent_path();
        if (!parent_dir.empty()) {
            std::filesystem::create_directories(parent_dir);
        }
        
        // Write to file
        std::ofstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR("Cannot create configuration file: " + file_path);
            return false;
        }
        
        file << json_str;
        file.close();
        
        LOG_INFO("Configuration saved to: " + file_path);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception saving configuration: " + std::string(e.what()));
        return false;
    }
}

std::string ConfigManager::config_to_json(const ConditioningConfig& config) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return "";
    
    try {
        // Metadata
        cJSON_AddStringToObject(root, "config_version", config.config_version.c_str());
        cJSON_AddStringToObject(root, "config_name", config.config_name.c_str());
        cJSON_AddStringToObject(root, "description", config.description.c_str());
        cJSON_AddStringToObject(root, "created_time", time_to_iso8601(config.created_time).c_str());
        cJSON_AddStringToObject(root, "modified_time", time_to_iso8601(config.modified_time).c_str());
        
        // Component configurations
        cJSON_AddItemToObject(root, "world_config", world_config_to_json(config.world_config));
        cJSON_AddItemToObject(root, "scanner_config", scanner_config_to_json(config.scanner_config));
        cJSON_AddItemToObject(root, "audio_config", audio_config_to_json(config.audio_config));
        cJSON_AddItemToObject(root, "training_config", training_config_to_json(config.training_config));
        cJSON_AddItemToObject(root, "batch_config", batch_config_to_json(config.batch_config));
        cJSON_AddItemToObject(root, "output_config", output_config_to_json(config.output_config));
        cJSON_AddItemToObject(root, "logging_config", logging_config_to_json(config.logging_config));
        
        // Custom settings
        if (!config.custom_settings.empty()) {
            cJSON* custom_obj = cJSON_CreateObject();
            for (const auto& [key, value] : config.custom_settings) {
                cJSON_AddStringToObject(custom_obj, key.c_str(), value.c_str());
            }
            cJSON_AddItemToObject(root, "custom_settings", custom_obj);
        }
        
        // Convert to string with formatting
        char* json_string = cJSON_Print(root);
        std::string result = json_string ? json_string : "";
        
        // Cleanup
        if (json_string) free(json_string);
        cJSON_Delete(root);
        
        return result;
        
    } catch (const std::exception& e) {
        cJSON_Delete(root);
        LOG_ERROR("Error serializing configuration: " + std::string(e.what()));
        return "";
    }
}

bool ConfigManager::config_from_json(const std::string& json_str, ConditioningConfig& config) {
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) {
        LOG_ERROR("Invalid JSON format in configuration");
        return false;
    }
    
    try {
        // Parse metadata
        cJSON* item = cJSON_GetObjectItem(root, "config_version");
        if (item && cJSON_IsString(item)) {
            config.config_version = item->valuestring;
        }
        
        item = cJSON_GetObjectItem(root, "config_name");
        if (item && cJSON_IsString(item)) {
            config.config_name = item->valuestring;
        }
        
        item = cJSON_GetObjectItem(root, "description");
        if (item && cJSON_IsString(item)) {
            config.description = item->valuestring;
        }
        
        item = cJSON_GetObjectItem(root, "created_time");
        if (item && cJSON_IsString(item)) {
            config.created_time = time_from_iso8601(item->valuestring);
        }
        
        item = cJSON_GetObjectItem(root, "modified_time");
        if (item && cJSON_IsString(item)) {
            config.modified_time = time_from_iso8601(item->valuestring);
        }
        
        // Parse component configurations
        item = cJSON_GetObjectItem(root, "world_config");
        if (item) world_config_from_json(item, config.world_config);
        
        item = cJSON_GetObjectItem(root, "scanner_config");
        if (item) scanner_config_from_json(item, config.scanner_config);
        
        item = cJSON_GetObjectItem(root, "audio_config");
        if (item) audio_config_from_json(item, config.audio_config);
        
        item = cJSON_GetObjectItem(root, "training_config");
        if (item) training_config_from_json(item, config.training_config);
        
        item = cJSON_GetObjectItem(root, "batch_config");
        if (item) batch_config_from_json(item, config.batch_config);
        
        item = cJSON_GetObjectItem(root, "output_config");
        if (item) output_config_from_json(item, config.output_config);
        
        item = cJSON_GetObjectItem(root, "logging_config");
        if (item) logging_config_from_json(item, config.logging_config);
        
        // Parse custom settings
        item = cJSON_GetObjectItem(root, "custom_settings");
        if (item && cJSON_IsObject(item)) {
            config.custom_settings.clear();
            cJSON* custom_item = nullptr;
            cJSON_ArrayForEach(custom_item, item) {
                if (cJSON_IsString(custom_item) && custom_item->string) {
                    config.custom_settings[custom_item->string] = custom_item->valuestring;
                }
            }
        }
        
        cJSON_Delete(root);
        return true;
        
    } catch (const std::exception& e) {
        cJSON_Delete(root);
        LOG_ERROR("Error parsing configuration JSON: " + std::string(e.what()));
        return false;
    }
}

ConfigValidationResult ConfigManager::validate_config(const ConditioningConfig& config) {
    ConfigValidationResult result;
    
    // Validate config version
    if (config.config_version.empty()) {
        result.errors.push_back("Configuration version is required");
    } else if (config.config_version != CURRENT_CONFIG_VERSION) {
        result.warnings.push_back("Configuration version mismatch. Current: " + 
                                 std::string(CURRENT_CONFIG_VERSION) + ", Found: " + config.config_version);
    }
    
    // Validate config name
    if (config.config_name.empty()) {
        result.errors.push_back("Configuration name is required");
    }
    
    // Validate component configurations
    if (!validate_world_config(config.world_config, result.errors)) {
        result.is_valid = false;
    }
    
    if (!validate_audio_config(config.audio_config, result.errors)) {
        result.is_valid = false;
    }
    
    if (!validate_paths(config, result.errors)) {
        result.is_valid = false;
    }
    
    // Threading validation
    if (config.batch_config.num_worker_threads < 0) {
        result.errors.push_back("Number of worker threads cannot be negative");
        result.is_valid = false;
    }
    
    if (config.batch_config.num_worker_threads > 16) {
        result.warnings.push_back("High thread count may cause performance issues on some systems");
    }
    
    if (config.batch_config.num_worker_threads > 64) {
        result.warnings.push_back("Very high thread count may cause severe performance issues");
    }
    
    // Memory validation
    if (config.batch_config.max_memory_usage_mb < 128) {
        result.warnings.push_back("Low memory limit may affect performance");
    }
    
    if (config.batch_config.max_memory_usage_mb > 16384) {
        result.warnings.push_back("Very high memory limit may cause system instability");
    }
    
    // Generate suggestions
    if (result.warnings.empty() && result.errors.empty()) {
        if (config.training_config.optimization_level == ModelTrainingConfig::FAST) {
            result.suggestions.push_back("Consider using BALANCED optimization level for better quality");
        }
        
        if (!config.batch_config.enable_progress_reporting) {
            result.suggestions.push_back("Enable progress reporting for better user experience");
        }
    }
    
    result.is_valid = result.errors.empty();
    return result;
}

ConditioningConfig ConfigManager::get_default_config() {
    ConditioningConfig config;
    config.config_name = "default";
    config.description = "Default NexusSynth conditioning configuration";
    return config;
}

ConditioningConfig ConfigManager::get_fast_config() {
    ConditioningConfig config = get_default_config();
    config.config_name = "fast";
    config.description = "Fast processing configuration for quick results";
    
    // Optimize for speed
    config.training_config.optimization_level = ModelTrainingConfig::FAST;
    config.training_config.max_training_iterations = 50;
    config.training_config.max_gaussian_components = 4;
    config.audio_config.resample_method = AudioProcessingConfig::LINEAR;
    config.scanner_config.analyze_audio_quality = false;
    config.batch_config.batch_size = 20;
    
    return config;
}

ConditioningConfig ConfigManager::get_quality_config() {
    ConditioningConfig config = get_default_config();
    config.config_name = "quality";
    config.description = "High quality configuration for best results";
    
    // Optimize for quality
    config.training_config.optimization_level = ModelTrainingConfig::MAXIMUM;
    config.training_config.max_training_iterations = 200;
    config.training_config.max_gaussian_components = 16;
    config.audio_config.resample_method = AudioProcessingConfig::SINC_BEST;
    config.scanner_config.analyze_audio_quality = true;
    config.batch_config.batch_size = 5;
    
    return config;
}

ConditioningConfig ConfigManager::get_batch_config() {
    ConditioningConfig config = get_default_config();
    config.config_name = "batch";
    config.description = "Batch processing configuration for large datasets";
    
    // Optimize for batch processing
    config.batch_config.batch_size = 50;
    config.batch_config.enable_memory_mapping = true;
    config.batch_config.cache_processed_files = true;
    config.batch_config.continue_on_error = true;
    config.logging_config.file_level = LoggingConfig::WARNING;
    
    return config;
}

bool ConfigManager::create_config_template(const std::string& file_path, const std::string& template_name) {
    ConditioningConfig config;
    
    if (template_name == "default") {
        config = get_default_config();
    } else if (template_name == "fast") {
        config = get_fast_config();
    } else if (template_name == "quality") {
        config = get_quality_config();
    } else if (template_name == "batch") {
        config = get_batch_config();
    } else {
        LOG_ERROR("Unknown template name: " + template_name);
        return false;
    }
    
    return save_config(file_path, config);
}

std::vector<std::string> ConfigManager::get_available_templates() {
    return {"default", "fast", "quality", "batch"};
}

bool ConfigManager::config_file_exists(const std::string& file_path) {
    return std::filesystem::exists(file_path);
}

std::string ConfigManager::get_config_directory() {
    std::filesystem::path config_dir;
    
#ifdef _WIN32
    // Windows: Use %APPDATA%/NexusSynth
    char* appdata = nullptr;
    size_t len = 0;
    errno_t err = _dupenv_s(&appdata, &len, "APPDATA");
    if (err == 0 && appdata) {
        config_dir = std::filesystem::path(appdata) / "NexusSynth";
        free(appdata);
    } else {
        config_dir = std::filesystem::current_path() / DEFAULT_CONFIG_DIR;
    }
#else
    // Unix-like: Use ~/.nexussynth
    const char* home = getenv("HOME");
    if (home) {
        config_dir = std::filesystem::path(home) / DEFAULT_CONFIG_DIR;
    } else {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            config_dir = std::filesystem::path(pw->pw_dir) / DEFAULT_CONFIG_DIR;
        } else {
            config_dir = std::filesystem::current_path() / DEFAULT_CONFIG_DIR;
        }
    }
#endif
    
    return config_dir.string();
}

bool ConfigManager::ensure_config_directory_exists() {
    try {
        std::string config_dir = get_config_directory();
        if (!std::filesystem::exists(config_dir)) {
            std::filesystem::create_directories(config_dir);
            LOG_INFO("Created configuration directory: " + config_dir);
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create configuration directory: " + std::string(e.what()));
        return false;
    }
}

std::string ConfigManager::get_supported_config_version() {
    return CURRENT_CONFIG_VERSION;
}

// JSON conversion helper implementations
cJSON* ConfigManager::world_config_to_json(const WorldConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "frame_period", config.frame_period);
    cJSON_AddNumberToObject(obj, "f0_floor", config.f0_floor);
    cJSON_AddNumberToObject(obj, "f0_ceil", config.f0_ceil);
    cJSON_AddNumberToObject(obj, "allowed_range", config.allowed_range);
    cJSON_AddNumberToObject(obj, "q1", config.q1);
    cJSON_AddNumberToObject(obj, "threshold", config.threshold);
    return obj;
}

bool ConfigManager::world_config_from_json(const cJSON* json, WorldConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "frame_period");
    if (item && cJSON_IsNumber(item)) config.frame_period = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "f0_floor");
    if (item && cJSON_IsNumber(item)) config.f0_floor = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "f0_ceil");
    if (item && cJSON_IsNumber(item)) config.f0_ceil = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "allowed_range");
    if (item && cJSON_IsNumber(item)) config.allowed_range = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "q1");
    if (item && cJSON_IsNumber(item)) config.q1 = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "threshold");
    if (item && cJSON_IsNumber(item)) config.threshold = item->valuedouble;
    
    return true;
}

cJSON* ConfigManager::scanner_config_to_json(const ScannerConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    
    // Boolean options
    cJSON_AddBoolToObject(obj, "recursive_search", config.recursive_search);
    cJSON_AddBoolToObject(obj, "validate_audio_files", config.validate_audio_files);
    cJSON_AddBoolToObject(obj, "validate_timing_parameters", config.validate_timing_parameters);
    cJSON_AddBoolToObject(obj, "detect_encoding_issues", config.detect_encoding_issues);
    cJSON_AddBoolToObject(obj, "analyze_audio_quality", config.analyze_audio_quality);
    cJSON_AddBoolToObject(obj, "parallel_scanning", config.parallel_scanning);
    
    // Numeric options
    cJSON_AddNumberToObject(obj, "max_scan_depth", config.max_scan_depth);
    cJSON_AddNumberToObject(obj, "max_files_per_directory", config.max_files_per_directory);
    cJSON_AddNumberToObject(obj, "max_threads", config.max_threads);
    cJSON_AddNumberToObject(obj, "min_audio_duration_ms", config.min_audio_duration_ms);
    cJSON_AddNumberToObject(obj, "max_audio_duration_ms", config.max_audio_duration_ms);
    cJSON_AddNumberToObject(obj, "preferred_sample_rate", config.preferred_sample_rate);
    cJSON_AddNumberToObject(obj, "preferred_bit_depth", config.preferred_bit_depth);
    
    // Arrays
    cJSON* formats_array = cJSON_CreateArray();
    for (const auto& format : config.supported_audio_formats) {
        cJSON_AddItemToArray(formats_array, cJSON_CreateString(format.c_str()));
    }
    cJSON_AddItemToObject(obj, "supported_audio_formats", formats_array);
    
    cJSON* excluded_dirs = cJSON_CreateArray();
    for (const auto& dir : config.excluded_directories) {
        cJSON_AddItemToArray(excluded_dirs, cJSON_CreateString(dir.c_str()));
    }
    cJSON_AddItemToObject(obj, "excluded_directories", excluded_dirs);
    
    cJSON* excluded_files = cJSON_CreateArray();
    for (const auto& file : config.excluded_files) {
        cJSON_AddItemToArray(excluded_files, cJSON_CreateString(file.c_str()));
    }
    cJSON_AddItemToObject(obj, "excluded_files", excluded_files);
    
    return obj;
}

bool ConfigManager::scanner_config_from_json(const cJSON* json, ScannerConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "recursive_search");
    if (item && cJSON_IsBool(item)) config.recursive_search = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "validate_audio_files");
    if (item && cJSON_IsBool(item)) config.validate_audio_files = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "validate_timing_parameters");
    if (item && cJSON_IsBool(item)) config.validate_timing_parameters = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "detect_encoding_issues");
    if (item && cJSON_IsBool(item)) config.detect_encoding_issues = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "analyze_audio_quality");
    if (item && cJSON_IsBool(item)) config.analyze_audio_quality = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "parallel_scanning");
    if (item && cJSON_IsBool(item)) config.parallel_scanning = cJSON_IsTrue(item);
    
    // Numeric values
    item = cJSON_GetObjectItem(json, "max_scan_depth");
    if (item && cJSON_IsNumber(item)) config.max_scan_depth = item->valueint;
    
    item = cJSON_GetObjectItem(json, "max_files_per_directory");
    if (item && cJSON_IsNumber(item)) config.max_files_per_directory = item->valueint;
    
    item = cJSON_GetObjectItem(json, "max_threads");
    if (item && cJSON_IsNumber(item)) config.max_threads = item->valueint;
    
    item = cJSON_GetObjectItem(json, "min_audio_duration_ms");
    if (item && cJSON_IsNumber(item)) config.min_audio_duration_ms = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "max_audio_duration_ms");
    if (item && cJSON_IsNumber(item)) config.max_audio_duration_ms = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "preferred_sample_rate");
    if (item && cJSON_IsNumber(item)) config.preferred_sample_rate = item->valueint;
    
    item = cJSON_GetObjectItem(json, "preferred_bit_depth");
    if (item && cJSON_IsNumber(item)) config.preferred_bit_depth = item->valueint;
    
    // Arrays
    item = cJSON_GetObjectItem(json, "supported_audio_formats");
    if (item && cJSON_IsArray(item)) {
        config.supported_audio_formats.clear();
        cJSON* format_item = nullptr;
        cJSON_ArrayForEach(format_item, item) {
            if (cJSON_IsString(format_item)) {
                config.supported_audio_formats.insert(format_item->valuestring);
            }
        }
    }
    
    item = cJSON_GetObjectItem(json, "excluded_directories");
    if (item && cJSON_IsArray(item)) {
        config.excluded_directories.clear();
        cJSON* dir_item = nullptr;
        cJSON_ArrayForEach(dir_item, item) {
            if (cJSON_IsString(dir_item)) {
                config.excluded_directories.push_back(dir_item->valuestring);
            }
        }
    }
    
    item = cJSON_GetObjectItem(json, "excluded_files");
    if (item && cJSON_IsArray(item)) {
        config.excluded_files.clear();
        cJSON* file_item = nullptr;
        cJSON_ArrayForEach(file_item, item) {
            if (cJSON_IsString(file_item)) {
                config.excluded_files.push_back(file_item->valuestring);
            }
        }
    }
    
    return true;
}

cJSON* ConfigManager::audio_config_to_json(const AudioProcessingConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(obj, "target_sample_rate", config.target_sample_rate);
    cJSON_AddNumberToObject(obj, "target_bit_depth", config.target_bit_depth);
    cJSON_AddBoolToObject(obj, "force_mono", config.force_mono);
    cJSON_AddBoolToObject(obj, "normalize_audio", config.normalize_audio);
    
    cJSON_AddNumberToObject(obj, "noise_threshold_db", config.noise_threshold_db);
    cJSON_AddNumberToObject(obj, "silence_threshold_db", config.silence_threshold_db);
    cJSON_AddNumberToObject(obj, "max_duration_seconds", config.max_duration_seconds);
    cJSON_AddNumberToObject(obj, "min_duration_seconds", config.min_duration_seconds);
    
    cJSON_AddNumberToObject(obj, "resample_method", static_cast<int>(config.resample_method));
    
    cJSON_AddBoolToObject(obj, "apply_preemphasis", config.apply_preemphasis);
    cJSON_AddNumberToObject(obj, "preemphasis_coefficient", config.preemphasis_coefficient);
    cJSON_AddBoolToObject(obj, "apply_dc_removal", config.apply_dc_removal);
    
    return obj;
}

bool ConfigManager::audio_config_from_json(const cJSON* json, AudioProcessingConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "target_sample_rate");
    if (item && cJSON_IsNumber(item)) config.target_sample_rate = item->valueint;
    
    item = cJSON_GetObjectItem(json, "target_bit_depth");
    if (item && cJSON_IsNumber(item)) config.target_bit_depth = item->valueint;
    
    item = cJSON_GetObjectItem(json, "force_mono");
    if (item && cJSON_IsBool(item)) config.force_mono = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "normalize_audio");
    if (item && cJSON_IsBool(item)) config.normalize_audio = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "noise_threshold_db");
    if (item && cJSON_IsNumber(item)) config.noise_threshold_db = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "silence_threshold_db");
    if (item && cJSON_IsNumber(item)) config.silence_threshold_db = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "max_duration_seconds");
    if (item && cJSON_IsNumber(item)) config.max_duration_seconds = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "min_duration_seconds");
    if (item && cJSON_IsNumber(item)) config.min_duration_seconds = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "resample_method");
    if (item && cJSON_IsNumber(item)) {
        config.resample_method = static_cast<AudioProcessingConfig::ResampleMethod>(item->valueint);
    }
    
    item = cJSON_GetObjectItem(json, "apply_preemphasis");
    if (item && cJSON_IsBool(item)) config.apply_preemphasis = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "preemphasis_coefficient");
    if (item && cJSON_IsNumber(item)) config.preemphasis_coefficient = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "apply_dc_removal");
    if (item && cJSON_IsBool(item)) config.apply_dc_removal = cJSON_IsTrue(item);
    
    return true;
}

cJSON* ConfigManager::training_config_to_json(const ModelTrainingConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(obj, "max_training_iterations", config.max_training_iterations);
    cJSON_AddNumberToObject(obj, "convergence_threshold", config.convergence_threshold);
    cJSON_AddNumberToObject(obj, "convergence_patience", config.convergence_patience);
    
    cJSON_AddNumberToObject(obj, "min_gaussian_components", config.min_gaussian_components);
    cJSON_AddNumberToObject(obj, "max_gaussian_components", config.max_gaussian_components);
    cJSON_AddBoolToObject(obj, "auto_component_selection", config.auto_component_selection);
    
    cJSON_AddBoolToObject(obj, "enable_pitch_augmentation", config.enable_pitch_augmentation);
    cJSON_AddNumberToObject(obj, "pitch_shift_range_cents", config.pitch_shift_range_cents);
    cJSON_AddBoolToObject(obj, "enable_tempo_augmentation", config.enable_tempo_augmentation);
    cJSON_AddNumberToObject(obj, "tempo_stretch_range", config.tempo_stretch_range);
    
    cJSON_AddNumberToObject(obj, "optimization_level", static_cast<int>(config.optimization_level));
    
    return obj;
}

bool ConfigManager::training_config_from_json(const cJSON* json, ModelTrainingConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "max_training_iterations");
    if (item && cJSON_IsNumber(item)) config.max_training_iterations = item->valueint;
    
    item = cJSON_GetObjectItem(json, "convergence_threshold");
    if (item && cJSON_IsNumber(item)) config.convergence_threshold = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "convergence_patience");
    if (item && cJSON_IsNumber(item)) config.convergence_patience = item->valueint;
    
    item = cJSON_GetObjectItem(json, "min_gaussian_components");
    if (item && cJSON_IsNumber(item)) config.min_gaussian_components = item->valueint;
    
    item = cJSON_GetObjectItem(json, "max_gaussian_components");
    if (item && cJSON_IsNumber(item)) config.max_gaussian_components = item->valueint;
    
    item = cJSON_GetObjectItem(json, "auto_component_selection");
    if (item && cJSON_IsBool(item)) config.auto_component_selection = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "enable_pitch_augmentation");
    if (item && cJSON_IsBool(item)) config.enable_pitch_augmentation = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "pitch_shift_range_cents");
    if (item && cJSON_IsNumber(item)) config.pitch_shift_range_cents = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "enable_tempo_augmentation");
    if (item && cJSON_IsBool(item)) config.enable_tempo_augmentation = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "tempo_stretch_range");
    if (item && cJSON_IsNumber(item)) config.tempo_stretch_range = item->valuedouble;
    
    item = cJSON_GetObjectItem(json, "optimization_level");
    if (item && cJSON_IsNumber(item)) {
        config.optimization_level = static_cast<ModelTrainingConfig::OptimizationLevel>(item->valueint);
    }
    
    return true;
}

cJSON* ConfigManager::batch_config_to_json(const BatchProcessingConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    
    // Threading and parallelism
    cJSON_AddNumberToObject(obj, "num_worker_threads", config.num_worker_threads);
    cJSON_AddNumberToObject(obj, "queue_size_limit", config.queue_size_limit);
    cJSON_AddNumberToObject(obj, "batch_size", config.batch_size);
    
    // Memory management
    cJSON_AddNumberToObject(obj, "max_memory_usage_mb", config.max_memory_usage_mb);
    cJSON_AddBoolToObject(obj, "enable_memory_mapping", config.enable_memory_mapping);
    cJSON_AddBoolToObject(obj, "cache_processed_files", config.cache_processed_files);
    
    // Progress reporting
    cJSON_AddBoolToObject(obj, "enable_progress_reporting", config.enable_progress_reporting);
    cJSON_AddNumberToObject(obj, "progress_update_interval_ms", config.progress_update_interval_ms);
    cJSON_AddBoolToObject(obj, "show_eta", config.show_eta);
    
    // Error handling
    cJSON_AddBoolToObject(obj, "continue_on_error", config.continue_on_error);
    cJSON_AddNumberToObject(obj, "max_consecutive_errors", config.max_consecutive_errors);
    cJSON_AddBoolToObject(obj, "save_error_files", config.save_error_files);
    
    // Output options
    cJSON_AddBoolToObject(obj, "preserve_directory_structure", config.preserve_directory_structure);
    cJSON_AddBoolToObject(obj, "compress_output", config.compress_output);
    
    return obj;
}

bool ConfigManager::batch_config_from_json(const cJSON* json, BatchProcessingConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "num_worker_threads");
    if (item && cJSON_IsNumber(item)) config.num_worker_threads = item->valueint;
    
    item = cJSON_GetObjectItem(json, "queue_size_limit");
    if (item && cJSON_IsNumber(item)) config.queue_size_limit = item->valueint;
    
    item = cJSON_GetObjectItem(json, "batch_size");
    if (item && cJSON_IsNumber(item)) config.batch_size = item->valueint;
    
    item = cJSON_GetObjectItem(json, "max_memory_usage_mb");
    if (item && cJSON_IsNumber(item)) config.max_memory_usage_mb = item->valueint;
    
    item = cJSON_GetObjectItem(json, "enable_memory_mapping");
    if (item && cJSON_IsBool(item)) config.enable_memory_mapping = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "cache_processed_files");
    if (item && cJSON_IsBool(item)) config.cache_processed_files = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "enable_progress_reporting");
    if (item && cJSON_IsBool(item)) config.enable_progress_reporting = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "progress_update_interval_ms");
    if (item && cJSON_IsNumber(item)) config.progress_update_interval_ms = item->valueint;
    
    item = cJSON_GetObjectItem(json, "show_eta");
    if (item && cJSON_IsBool(item)) config.show_eta = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "continue_on_error");
    if (item && cJSON_IsBool(item)) config.continue_on_error = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "max_consecutive_errors");
    if (item && cJSON_IsNumber(item)) config.max_consecutive_errors = item->valueint;
    
    item = cJSON_GetObjectItem(json, "save_error_files");
    if (item && cJSON_IsBool(item)) config.save_error_files = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "preserve_directory_structure");
    if (item && cJSON_IsBool(item)) config.preserve_directory_structure = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "compress_output");
    if (item && cJSON_IsBool(item)) config.compress_output = cJSON_IsTrue(item);
    
    return true;
}

cJSON* ConfigManager::output_config_to_json(const OutputConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    
    // Output paths
    cJSON_AddStringToObject(obj, "output_directory", config.output_directory.c_str());
    cJSON_AddStringToObject(obj, "model_file_extension", config.model_file_extension.c_str());
    cJSON_AddStringToObject(obj, "metadata_file_extension", config.metadata_file_extension.c_str());
    
    // File naming
    cJSON_AddNumberToObject(obj, "naming_scheme", static_cast<int>(config.naming_scheme));
    cJSON_AddStringToObject(obj, "custom_prefix", config.custom_prefix.c_str());
    
    // File organization
    cJSON_AddBoolToObject(obj, "create_subdirectories", config.create_subdirectories);
    cJSON_AddBoolToObject(obj, "generate_index_file", config.generate_index_file);
    cJSON_AddBoolToObject(obj, "backup_original_files", config.backup_original_files);
    
    // Quality assurance
    cJSON_AddBoolToObject(obj, "validate_output_files", config.validate_output_files);
    cJSON_AddBoolToObject(obj, "generate_quality_reports", config.generate_quality_reports);
    cJSON_AddStringToObject(obj, "quality_report_format", config.quality_report_format.c_str());
    
    return obj;
}

bool ConfigManager::output_config_from_json(const cJSON* json, OutputConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "output_directory");
    if (item && cJSON_IsString(item)) config.output_directory = item->valuestring;
    
    item = cJSON_GetObjectItem(json, "model_file_extension");
    if (item && cJSON_IsString(item)) config.model_file_extension = item->valuestring;
    
    item = cJSON_GetObjectItem(json, "metadata_file_extension");
    if (item && cJSON_IsString(item)) config.metadata_file_extension = item->valuestring;
    
    item = cJSON_GetObjectItem(json, "naming_scheme");
    if (item && cJSON_IsNumber(item)) {
        config.naming_scheme = static_cast<OutputConfig::NamingScheme>(item->valueint);
    }
    
    item = cJSON_GetObjectItem(json, "custom_prefix");
    if (item && cJSON_IsString(item)) config.custom_prefix = item->valuestring;
    
    item = cJSON_GetObjectItem(json, "create_subdirectories");
    if (item && cJSON_IsBool(item)) config.create_subdirectories = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "generate_index_file");
    if (item && cJSON_IsBool(item)) config.generate_index_file = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "backup_original_files");
    if (item && cJSON_IsBool(item)) config.backup_original_files = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "validate_output_files");
    if (item && cJSON_IsBool(item)) config.validate_output_files = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "generate_quality_reports");
    if (item && cJSON_IsBool(item)) config.generate_quality_reports = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "quality_report_format");
    if (item && cJSON_IsString(item)) config.quality_report_format = item->valuestring;
    
    return true;
}

cJSON* ConfigManager::logging_config_to_json(const LoggingConfig& config) {
    cJSON* obj = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(obj, "console_level", static_cast<int>(config.console_level));
    cJSON_AddNumberToObject(obj, "file_level", static_cast<int>(config.file_level));
    cJSON_AddStringToObject(obj, "log_file_path", config.log_file_path.c_str());
    cJSON_AddBoolToObject(obj, "timestamp_enabled", config.timestamp_enabled);
    cJSON_AddBoolToObject(obj, "thread_id_enabled", config.thread_id_enabled);
    cJSON_AddNumberToObject(obj, "max_log_file_size_mb", config.max_log_file_size_mb);
    cJSON_AddNumberToObject(obj, "max_log_files", config.max_log_files);
    
    return obj;
}

bool ConfigManager::logging_config_from_json(const cJSON* json, LoggingConfig& config) {
    cJSON* item = cJSON_GetObjectItem(json, "console_level");
    if (item && cJSON_IsNumber(item)) {
        config.console_level = static_cast<LoggingConfig::Level>(item->valueint);
    }
    
    item = cJSON_GetObjectItem(json, "file_level");
    if (item && cJSON_IsNumber(item)) {
        config.file_level = static_cast<LoggingConfig::Level>(item->valueint);
    }
    
    item = cJSON_GetObjectItem(json, "log_file_path");
    if (item && cJSON_IsString(item)) config.log_file_path = item->valuestring;
    
    item = cJSON_GetObjectItem(json, "timestamp_enabled");
    if (item && cJSON_IsBool(item)) config.timestamp_enabled = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "thread_id_enabled");
    if (item && cJSON_IsBool(item)) config.thread_id_enabled = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(json, "max_log_file_size_mb");
    if (item && cJSON_IsNumber(item)) config.max_log_file_size_mb = item->valueint;
    
    item = cJSON_GetObjectItem(json, "max_log_files");
    if (item && cJSON_IsNumber(item)) config.max_log_files = item->valueint;
    
    return true;
}

// Validation helper implementations
bool ConfigManager::validate_world_config(const WorldConfig& config, std::vector<std::string>& errors) {
    bool valid = true;
    
    if (config.frame_period <= 0.0 || config.frame_period > 50.0) {
        errors.push_back("Frame period must be between 0 and 50 milliseconds");
        valid = false;
    }
    
    if (config.f0_floor <= 0.0 || config.f0_floor >= config.f0_ceil) {
        errors.push_back("F0 floor must be positive and less than F0 ceiling");
        valid = false;
    }
    
    if (config.f0_ceil <= config.f0_floor || config.f0_ceil > 2000.0) {
        errors.push_back("F0 ceiling must be greater than F0 floor and less than 2000 Hz");
        valid = false;
    }
    
    if (config.allowed_range <= 0.0 || config.allowed_range > 1.0) {
        errors.push_back("Allowed range must be between 0 and 1");
        valid = false;
    }
    
    return valid;
}

bool ConfigManager::validate_audio_config(const AudioProcessingConfig& config, std::vector<std::string>& errors) {
    bool valid = true;
    
    if (config.target_sample_rate <= 0 || config.target_sample_rate > 192000) {
        errors.push_back("Target sample rate must be between 1 and 192000 Hz");
        valid = false;
    }
    
    if (config.target_bit_depth != 16 && config.target_bit_depth != 24 && config.target_bit_depth != 32) {
        errors.push_back("Target bit depth must be 16, 24, or 32 bits");
        valid = false;
    }
    
    if (config.max_duration_seconds <= config.min_duration_seconds) {
        errors.push_back("Maximum duration must be greater than minimum duration");
        valid = false;
    }
    
    if (config.min_duration_seconds <= 0.0) {
        errors.push_back("Minimum duration must be positive");
        valid = false;
    }
    
    return valid;
}

bool ConfigManager::validate_paths(const ConditioningConfig& config, std::vector<std::string>& errors) {
    bool valid = true;
    
    // Validate output directory path
    const std::string& output_dir = config.output_config.output_directory;
    if (output_dir.empty()) {
        errors.push_back("Output directory cannot be empty");
        valid = false;
    }
    
    // Check if output directory can be created (if it doesn't exist)
    try {
        std::filesystem::path output_path(output_dir);
        if (!std::filesystem::exists(output_path)) {
            std::filesystem::path parent = output_path.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                errors.push_back("Output directory parent path does not exist: " + parent.string());
                valid = false;
            }
        }
    } catch (const std::exception& e) {
        errors.push_back("Invalid output directory path: " + output_dir);
        valid = false;
    }
    
    return valid;
}

// Utility helper implementations
std::string ConfigManager::time_to_iso8601(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

std::chrono::system_clock::time_point ConfigManager::time_from_iso8601(const std::string& iso_str) {
    std::istringstream iss(iso_str);
    std::tm tm = {};
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    if (iss.fail()) {
        return std::chrono::system_clock::now(); // Return current time on parse error
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Config utility function implementations
namespace config_utils {

ConditioningConfig create_utau_compatible_config() {
    ConditioningConfig config;
    config.config_name = "utau_compatible";
    config.description = "Configuration optimized for UTAU compatibility";
    
    // Set UTAU-standard audio settings
    config.audio_config.target_sample_rate = 44100;
    config.audio_config.target_bit_depth = 16;
    config.audio_config.force_mono = true;
    
    // Enable all validation for compatibility
    config.scanner_config.validate_audio_files = true;
    config.scanner_config.validate_timing_parameters = true;
    config.scanner_config.detect_encoding_issues = true;
    
    return config;
}

ConditioningConfig create_high_quality_config() {
    ConditioningConfig config;
    config.config_name = "high_quality";
    config.description = "Configuration for highest quality output";
    
    // High quality audio settings
    config.audio_config.target_sample_rate = 48000;
    config.audio_config.target_bit_depth = 24;
    config.audio_config.resample_method = AudioProcessingConfig::SINC_BEST;
    
    // Maximum quality training
    config.training_config.optimization_level = ModelTrainingConfig::MAXIMUM;
    config.training_config.max_training_iterations = 200;
    config.training_config.max_gaussian_components = 16;
    
    // Enable quality analysis
    config.scanner_config.analyze_audio_quality = true;
    config.output_config.generate_quality_reports = true;
    
    return config;
}

ConditioningConfig create_fast_processing_config() {
    ConditioningConfig config;
    config.config_name = "fast_processing";
    config.description = "Configuration optimized for speed";
    
    // Fast processing settings
    config.training_config.optimization_level = ModelTrainingConfig::FAST;
    config.training_config.max_training_iterations = 50;
    config.training_config.max_gaussian_components = 4;
    
    // Minimal validation
    config.scanner_config.analyze_audio_quality = false;
    config.output_config.generate_quality_reports = false;
    
    // Higher batch sizes for throughput
    config.batch_config.batch_size = 50;
    
    return config;
}

ConditioningConfig create_batch_processing_config() {
    ConditioningConfig config;
    config.config_name = "batch_processing";
    config.description = "Configuration for large batch operations";
    
    // Batch optimization
    config.batch_config.batch_size = 100;
    config.batch_config.enable_memory_mapping = true;
    config.batch_config.cache_processed_files = true;
    config.batch_config.continue_on_error = true;
    
    // Reduced logging for performance
    config.logging_config.console_level = LoggingConfig::WARNING;
    config.logging_config.file_level = LoggingConfig::INFO;
    
    return config;
}

} // namespace config_utils

} // namespace conditioning
} // namespace nexussynth
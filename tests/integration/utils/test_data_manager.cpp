#include "test_data_manager.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <cmath>

namespace nexussynth {
namespace integration_test {

TestDataManager::TestDataManager() = default;
TestDataManager::~TestDataManager() {
    cleanup_test_environment();
}

bool TestDataManager::initialize(const std::string& test_data_root) {
    test_data_root_ = std::filesystem::absolute(test_data_root);
    temp_dir_ = test_data_root_ + "/temp";
    
    if (!ensure_directory_exists(test_data_root_)) {
        return false;
    }
    
    if (!ensure_directory_exists(temp_dir_)) {
        return false;
    }
    
    // Create standard directory structure
    std::vector<std::string> subdirs = {
        "voice_banks/japanese/basic_cv",
        "voice_banks/japanese/vcv", 
        "voice_banks/english",
        "reference_audio",
        "test_scenarios",
        "benchmarks",
        "results"
    };
    
    for (const auto& subdir : subdirs) {
        if (!ensure_directory_exists(test_data_root_ + "/" + subdir)) {
            return false;
        }
    }
    
    return true;
}

bool TestDataManager::setup_test_environment() {
    // Create minimal test voice bank if it doesn't exist
    if (!file_exists(get_minimal_voice_bank_path())) {
        if (!create_minimal_test_voice_bank()) {
            return false;
        }
    }
    
    // Scan for available voice banks
    return scan_voice_banks();
}

void TestDataManager::cleanup_test_environment() {
    cleanup_temp_files();
}

bool TestDataManager::scan_voice_banks() {
    voice_banks_.clear();
    
    std::string voice_banks_dir = test_data_root_ + "/voice_banks";
    if (!std::filesystem::exists(voice_banks_dir)) {
        return false;
    }
    
    // Recursively scan for voice banks (directories with oto.ini)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(voice_banks_dir)) {
        if (entry.is_directory()) {
            std::string oto_path = entry.path() / "oto.ini";
            if (std::filesystem::exists(oto_path)) {
                TestVoiceBank voice_bank;
                voice_bank.path = entry.path().string();
                voice_bank.name = entry.path().filename().string();
                
                if (validate_voice_bank(voice_bank.path, voice_bank)) {
                    voice_banks_.push_back(voice_bank);
                }
            }
        }
    }
    
    return !voice_banks_.empty();
}

std::vector<TestVoiceBank> TestDataManager::get_available_voice_banks() const {
    return voice_banks_;
}

TestVoiceBank TestDataManager::get_voice_bank(const std::string& name) const {
    auto it = std::find_if(voice_banks_.begin(), voice_banks_.end(),
        [&name](const TestVoiceBank& vb) { return vb.name == name; });
    
    if (it != voice_banks_.end()) {
        return *it;
    }
    
    return TestVoiceBank(); // Invalid voice bank
}

bool TestDataManager::is_voice_bank_valid(const std::string& name) const {
    return get_voice_bank(name).is_valid;
}

bool TestDataManager::load_test_scenarios(const std::string& config_file) {
    return TestConfigLoader::load_scenarios(config_file, test_scenarios_);
}

std::vector<TestScenario> TestDataManager::get_test_scenarios() const {
    return test_scenarios_;
}

std::vector<TestScenario> TestDataManager::get_scenarios_by_type(const std::string& type) const {
    std::vector<TestScenario> filtered;
    for (const auto& scenario : test_scenarios_) {
        if (scenario.voice_bank.find(type) != std::string::npos) {
            filtered.push_back(scenario);
        }
    }
    return filtered;
}

TestScenario TestDataManager::get_scenario(const std::string& id) const {
    auto it = std::find_if(test_scenarios_.begin(), test_scenarios_.end(),
        [&id](const TestScenario& s) { return s.id == id; });
    
    if (it != test_scenarios_.end()) {
        return *it;
    }
    
    return TestScenario(); // Invalid scenario
}

bool TestDataManager::prepare_test_data(const TestScenario& scenario) {
    // Ensure voice bank exists
    if (!is_voice_bank_valid(scenario.voice_bank)) {
        return false;
    }
    
    // Create output directory
    std::string output_dir = get_test_output_path(scenario);
    return ensure_directory_exists(std::filesystem::path(output_dir).parent_path());
}

std::string TestDataManager::get_test_input_path(const TestScenario& scenario) const {
    return test_data_root_ + "/test_scenarios/" + scenario.id + "_input.txt";
}

std::string TestDataManager::get_test_output_path(const TestScenario& scenario) const {
    return test_data_root_ + "/results/" + scenario.id + "_output.wav";
}

std::string TestDataManager::get_reference_output_path(const TestScenario& scenario) const {
    return test_data_root_ + "/reference_audio/" + scenario.expected_output_file;
}

std::string TestDataManager::create_temp_file(const std::string& suffix) {
    std::string filename = "test_" + generate_test_id() + suffix;
    std::string filepath = temp_dir_ + "/" + filename;
    
    // Create empty file
    std::ofstream file(filepath);
    file.close();
    
    temp_files_.push_back(filepath);
    return filepath;
}

std::string TestDataManager::create_temp_directory(const std::string& name) {
    std::string dir_path = temp_dir_ + "/" + name + "_" + generate_test_id();
    
    if (ensure_directory_exists(dir_path)) {
        temp_directories_.push_back(dir_path);
        return dir_path;
    }
    
    return "";
}

void TestDataManager::cleanup_temp_files() {
    // Remove temporary files
    for (const auto& file : temp_files_) {
        try {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        } catch (const std::exception&) {
            // Ignore cleanup errors
        }
    }
    temp_files_.clear();
    
    // Remove temporary directories
    for (const auto& dir : temp_directories_) {
        try {
            if (std::filesystem::exists(dir)) {
                std::filesystem::remove_all(dir);
            }
        } catch (const std::exception&) {
            // Ignore cleanup errors
        }
    }
    temp_directories_.clear();
}

bool TestDataManager::create_minimal_test_voice_bank() {
    std::string voice_bank_path = test_data_root_ + "/voice_banks/japanese/basic_cv/minimal_test";
    
    if (!ensure_directory_exists(voice_bank_path)) {
        return false;
    }
    
    return create_cv_voice_bank(voice_bank_path);
}

std::string TestDataManager::get_minimal_voice_bank_path() const {
    return test_data_root_ + "/voice_banks/japanese/basic_cv/minimal_test";
}

bool TestDataManager::file_exists(const std::string& path) const {
    return std::filesystem::exists(path);
}

size_t TestDataManager::get_file_size(const std::string& path) const {
    if (!file_exists(path)) {
        return 0;
    }
    
    return std::filesystem::file_size(path);
}

std::string TestDataManager::get_absolute_path(const std::string& relative_path) const {
    return std::filesystem::absolute(relative_path).string();
}

bool TestDataManager::validate_voice_bank(const std::string& path, TestVoiceBank& voice_bank) {
    std::string oto_path = path + "/oto.ini";
    
    if (!file_exists(oto_path)) {
        voice_bank.is_valid = false;
        return false;
    }
    
    // Parse oto.ini to get phoneme information
    if (!parse_oto_ini(oto_path, voice_bank)) {
        voice_bank.is_valid = false;
        return false;
    }
    
    // Count audio files
    voice_bank.file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
                voice_bank.file_count++;
            }
        }
    }
    
    // Determine language and type from path
    std::string path_str = path;
    if (path_str.find("japanese") != std::string::npos) {
        voice_bank.language = "japanese";
    } else if (path_str.find("english") != std::string::npos) {
        voice_bank.language = "english";
    } else {
        voice_bank.language = "unknown";
    }
    
    if (path_str.find("vcv") != std::string::npos) {
        voice_bank.type = "VCV";
    } else if (path_str.find("vccv") != std::string::npos) {
        voice_bank.type = "VCCV";  
    } else {
        voice_bank.type = "CV";
    }
    
    voice_bank.is_valid = (voice_bank.file_count > 0 && !voice_bank.phonemes.empty());
    return voice_bank.is_valid;
}

bool TestDataManager::parse_oto_ini(const std::string& oto_path, TestVoiceBank& voice_bank) {
    std::ifstream file(oto_path);
    if (!file.is_open()) {
        return false;
    }
    
    voice_bank.phonemes.clear();
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse UTAU oto.ini format: filename=phoneme,offset,consonant,cutoff,preutterance,overlap
        size_t equal_pos = line.find('=');
        if (equal_pos != std::string::npos) {
            std::string params = line.substr(equal_pos + 1);
            size_t comma_pos = params.find(',');
            
            if (comma_pos != std::string::npos) {
                std::string phoneme = params.substr(0, comma_pos);
                if (!phoneme.empty()) {
                    voice_bank.phonemes.push_back(phoneme);
                }
            }
        }
    }
    
    return !voice_bank.phonemes.empty();
}

std::string TestDataManager::generate_test_id() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto time_t = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return std::to_string(time_t) + "_" + std::to_string(dis(gen));
}

bool TestDataManager::ensure_directory_exists(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return std::filesystem::create_directories(path);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool TestDataManager::create_cv_voice_bank(const std::string& output_path) {
    // Create basic Japanese CV phonemes
    std::vector<std::string> cv_phonemes = {
        "a", "i", "u", "e", "o",
        "ka", "ki", "ku", "ke", "ko",
        "sa", "si", "su", "se", "so",
        "ta", "ti", "tu", "te", "to",
        "na", "ni", "nu", "ne", "no",
        "ma", "mi", "mu", "me", "mo",
        "ra", "ri", "ru", "re", "ro",
        "n"
    };
    
    // Create test audio files
    if (!create_test_audio_files(output_path, cv_phonemes)) {
        return false;
    }
    
    // Create oto.ini
    return create_oto_ini(output_path, cv_phonemes);
}

bool TestDataManager::create_test_audio_files(const std::string& voice_bank_path, 
                                            const std::vector<std::string>& phonemes) {
    // Create simple sine wave audio files for testing
    const int sample_rate = 44100;
    const double duration = 0.5; // 500ms
    const int num_samples = static_cast<int>(sample_rate * duration);
    
    for (const auto& phoneme : phonemes) {
        std::string filename = phoneme + ".wav";
        std::string filepath = voice_bank_path + "/" + filename;
        
        // Generate simple sine wave (different frequency for each phoneme)
        std::vector<int16_t> samples(num_samples);
        double frequency = 200.0 + (std::hash<std::string>{}(phoneme) % 400); // 200-600 Hz
        
        for (int i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double amplitude = 0.3 * std::sin(2.0 * M_PI * frequency * t);
            samples[i] = static_cast<int16_t>(amplitude * 32767);
        }
        
        // Write basic WAV file (simplified header)
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // WAV header (44 bytes)
        file.write("RIFF", 4);
        uint32_t file_size = 36 + num_samples * sizeof(int16_t);
        file.write(reinterpret_cast<const char*>(&file_size), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        uint32_t fmt_size = 16;
        file.write(reinterpret_cast<const char*>(&fmt_size), 4);
        uint16_t audio_format = 1; // PCM
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        uint16_t channels = 1; // Mono
        file.write(reinterpret_cast<const char*>(&channels), 2);
        uint32_t sample_rate_u32 = sample_rate;
        file.write(reinterpret_cast<const char*>(&sample_rate_u32), 4);
        uint32_t byte_rate = sample_rate * channels * sizeof(int16_t);
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        uint16_t block_align = channels * sizeof(int16_t);
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        uint16_t bits_per_sample = 16;
        file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
        file.write("data", 4);
        uint32_t data_size = num_samples * sizeof(int16_t);
        file.write(reinterpret_cast<const char*>(&data_size), 4);
        
        // Write audio data
        file.write(reinterpret_cast<const char*>(samples.data()), data_size);
        file.close();
    }
    
    return true;
}

bool TestDataManager::create_oto_ini(const std::string& voice_bank_path,
                                   const std::vector<std::string>& phonemes) {
    std::string oto_path = voice_bank_path + "/oto.ini";
    std::ofstream file(oto_path);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Write oto.ini entries
    for (const auto& phoneme : phonemes) {
        std::string filename = phoneme + ".wav";
        // Format: filename=phoneme,offset,consonant,cutoff,preutterance,overlap
        file << filename << "=" << phoneme << ",0,50,450,0,0\n";
    }
    
    return true;
}

// ScopedTempFile implementation
ScopedTempFile::ScopedTempFile(TestDataManager& manager, const std::string& suffix)
    : manager_(manager), should_cleanup_(true) {
    file_path_ = manager_.create_temp_file(suffix);
}

ScopedTempFile::~ScopedTempFile() {
    if (should_cleanup_ && std::filesystem::exists(file_path_)) {
        try {
            std::filesystem::remove(file_path_);
        } catch (const std::exception&) {
            // Ignore cleanup errors
        }
    }
}

bool ScopedTempFile::exists() const {
    return std::filesystem::exists(file_path_);
}

size_t ScopedTempFile::size() const {
    if (!exists()) {
        return 0;
    }
    return std::filesystem::file_size(file_path_);
}

ScopedTempFile::ScopedTempFile(ScopedTempFile&& other) noexcept
    : manager_(other.manager_), file_path_(std::move(other.file_path_)), 
      should_cleanup_(other.should_cleanup_) {
    other.should_cleanup_ = false;
}

ScopedTempFile& ScopedTempFile::operator=(ScopedTempFile&& other) noexcept {
    if (this != &other) {
        manager_ = other.manager_;
        file_path_ = std::move(other.file_path_);
        should_cleanup_ = other.should_cleanup_;
        other.should_cleanup_ = false;
    }
    return *this;
}

// TestConfigLoader implementation
bool TestConfigLoader::load_scenarios(const std::string& config_file, 
                                     std::vector<TestScenario>& scenarios) {
    // For now, create default scenarios programmatically
    // In a full implementation, this would parse JSON configuration
    
    scenarios.clear();
    
    // Basic synthesis test
    TestScenario basic;
    basic.id = "basic_synthesis";
    basic.name = "Basic Synthesis Test";
    basic.description = "Test basic CV synthesis with minimal voice bank";
    basic.voice_bank = "minimal_test";
    basic.input_text = "a i u e o";
    basic.pitch_shift = 0.0;
    basic.tempo_factor = 1.0;
    basic.expected_output_file = "basic_synthesis_expected.wav";
    scenarios.push_back(basic);
    
    // Pitch shift test
    TestScenario pitch;
    pitch.id = "pitch_shift_test";
    pitch.name = "Pitch Shift Test";
    pitch.description = "Test pitch shifting functionality";
    pitch.voice_bank = "minimal_test";
    pitch.input_text = "ka ki ku ke ko";
    pitch.pitch_shift = 12.0; // One octave up
    pitch.tempo_factor = 1.0;
    pitch.expected_output_file = "pitch_shift_expected.wav";
    scenarios.push_back(pitch);
    
    // Tempo change test
    TestScenario tempo;
    tempo.id = "tempo_change_test";
    tempo.name = "Tempo Change Test";
    tempo.description = "Test tempo modification";
    tempo.voice_bank = "minimal_test";
    tempo.input_text = "sa si su se so";
    tempo.pitch_shift = 0.0;
    tempo.tempo_factor = 1.5; // 50% faster
    tempo.expected_output_file = "tempo_change_expected.wav";
    scenarios.push_back(tempo);
    
    return true;
}

bool TestConfigLoader::load_voice_bank_specs(const std::string& config_file,
                                           std::vector<TestVoiceBank>& voice_banks) {
    // Placeholder - would load from JSON in full implementation
    return true;
}

bool TestConfigLoader::save_scenarios(const std::string& config_file,
                                     const std::vector<TestScenario>& scenarios) {
    // Placeholder - would save to JSON in full implementation
    return true;
}

} // namespace integration_test
} // namespace nexussynth
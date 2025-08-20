#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Test voice bank specification
     */
    struct TestVoiceBank {
        std::string name;
        std::string path;
        std::string language;
        std::string type; // "CV", "VCV", "VCCV", etc.
        std::vector<std::string> phonemes;
        size_t file_count;
        bool is_valid;
        
        TestVoiceBank() : file_count(0), is_valid(false) {}
    };

    /**
     * @brief Test scenario specification
     */
    struct TestScenario {
        std::string id;
        std::string name;
        std::string description;
        
        // Test parameters
        std::string voice_bank;
        std::string input_text;
        double pitch_shift = 0.0;      // semitones
        double tempo_factor = 1.0;     // speed multiplier
        double volume = 1.0;           // amplitude multiplier
        std::string expression = "";   // Additional UTAU flags
        
        // Quality expectations
        double min_snr_db = 20.0;
        double max_render_time_ms = 5000.0;
        size_t max_memory_mb = 512;
        
        // Output expectations
        std::string expected_output_file;
        double similarity_threshold = 0.85;
    };

    /**
     * @brief Manages test data, voice banks, and test scenarios
     */
    class TestDataManager {
    public:
        TestDataManager();
        ~TestDataManager();
        
        // Initialization and setup
        bool initialize(const std::string& test_data_root);
        bool setup_test_environment();
        void cleanup_test_environment();
        
        // Voice bank management
        bool scan_voice_banks();
        std::vector<TestVoiceBank> get_available_voice_banks() const;
        TestVoiceBank get_voice_bank(const std::string& name) const;
        bool is_voice_bank_valid(const std::string& name) const;
        
        // Test scenario management
        bool load_test_scenarios(const std::string& config_file);
        std::vector<TestScenario> get_test_scenarios() const;
        std::vector<TestScenario> get_scenarios_by_type(const std::string& type) const;
        TestScenario get_scenario(const std::string& id) const;
        
        // Test data preparation
        bool prepare_test_data(const TestScenario& scenario);
        std::string get_test_input_path(const TestScenario& scenario) const;
        std::string get_test_output_path(const TestScenario& scenario) const;
        std::string get_reference_output_path(const TestScenario& scenario) const;
        
        // Temporary file management
        std::string create_temp_file(const std::string& suffix = ".wav");
        std::string create_temp_directory(const std::string& name);
        void cleanup_temp_files();
        
        // Standard test voice banks
        bool download_standard_voice_banks();
        bool create_minimal_test_voice_bank();
        std::string get_minimal_voice_bank_path() const;
        
        // Utility methods
        bool file_exists(const std::string& path) const;
        size_t get_file_size(const std::string& path) const;
        std::string get_absolute_path(const std::string& relative_path) const;
        
    private:
        std::string test_data_root_;
        std::string temp_dir_;
        std::vector<TestVoiceBank> voice_banks_;
        std::vector<TestScenario> test_scenarios_;
        std::vector<std::string> temp_files_;
        std::vector<std::string> temp_directories_;
        
        // Helper methods
        bool validate_voice_bank(const std::string& path, TestVoiceBank& voice_bank);
        bool parse_oto_ini(const std::string& oto_path, TestVoiceBank& voice_bank);
        std::string generate_test_id() const;
        bool ensure_directory_exists(const std::string& path);
        
        // Standard voice bank creation
        bool create_cv_voice_bank(const std::string& output_path);
        bool create_test_audio_files(const std::string& voice_bank_path, 
                                   const std::vector<std::string>& phonemes);
        bool create_oto_ini(const std::string& voice_bank_path,
                           const std::vector<std::string>& phonemes);
    };

    /**
     * @brief RAII wrapper for temporary test files
     */
    class ScopedTempFile {
    public:
        explicit ScopedTempFile(TestDataManager& manager, const std::string& suffix = ".wav");
        ~ScopedTempFile();
        
        const std::string& path() const { return file_path_; }
        bool exists() const;
        size_t size() const;
        
        // Non-copyable
        ScopedTempFile(const ScopedTempFile&) = delete;
        ScopedTempFile& operator=(const ScopedTempFile&) = delete;
        
        // Movable
        ScopedTempFile(ScopedTempFile&& other) noexcept;
        ScopedTempFile& operator=(ScopedTempFile&& other) noexcept;
        
    private:
        TestDataManager& manager_;
        std::string file_path_;
        bool should_cleanup_;
    };

    /**
     * @brief Test data configuration loader
     */
    class TestConfigLoader {
    public:
        static bool load_scenarios(const std::string& config_file, 
                                 std::vector<TestScenario>& scenarios);
        static bool load_voice_bank_specs(const std::string& config_file,
                                        std::vector<TestVoiceBank>& voice_banks);
        static bool save_scenarios(const std::string& config_file,
                                 const std::vector<TestScenario>& scenarios);
        
    private:
        static TestScenario parse_scenario_json(const std::string& json_str);
        static std::string scenario_to_json(const TestScenario& scenario);
    };

} // namespace integration_test
} // namespace nexussynth
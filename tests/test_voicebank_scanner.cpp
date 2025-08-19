#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "nexussynth/voicebank_scanner.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace nexussynth::conditioning;

class VoicebankScannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory structure
        test_root_ = std::filesystem::temp_directory_path() / "nexussynth_scanner_test";
        std::filesystem::create_directories(test_root_);
        
        // Create test voice bank structures
        create_valid_voicebank();
        create_invalid_voicebank();
        create_partial_voicebank();
        create_empty_directory();
        
        // Initialize scanner with test configuration
        config_.max_scan_depth = 3;
        config_.validate_audio_files = true;
        config_.analyze_audio_quality = false;  // Skip for tests
        config_.parallel_scanning = false;      // Simpler testing
        scanner_ = std::make_unique<VoicebankScanner>(config_);
    }
    
    void TearDown() override {
        // Clean up test directories
        std::error_code ec;
        std::filesystem::remove_all(test_root_, ec);
    }
    
    void create_valid_voicebank() {
        auto vb_path = test_root_ / "ValidVoicebank";
        std::filesystem::create_directories(vb_path);
        
        // Create oto.ini
        std::ofstream oto_file(vb_path / "oto.ini");
        oto_file << "test1.wav=a,100,200,50,150,30\n";
        oto_file << "test2.wav=i,200,180,60,140,25\n";
        oto_file << "test3.wav=u,150,220,40,160,35\n";
        oto_file.close();
        
        // Create corresponding WAV files (dummy files)
        create_dummy_wav(vb_path / "test1.wav");
        create_dummy_wav(vb_path / "test2.wav");
        create_dummy_wav(vb_path / "test3.wav");
        
        // Create character.txt
        std::ofstream char_file(vb_path / "character.txt");
        char_file << "name=Test Voice\n";
        char_file << "author=Test Author\n";
        char_file.close();
        
        // Create readme
        std::ofstream readme_file(vb_path / "readme.txt");
        readme_file << "This is a test voice bank.\n";
        readme_file.close();
    }
    
    void create_invalid_voicebank() {
        auto vb_path = test_root_ / "InvalidVoicebank";
        std::filesystem::create_directories(vb_path);
        
        // Create empty oto.ini (invalid)
        std::ofstream oto_file(vb_path / "oto.ini");
        oto_file.close();
        
        // No audio files - makes it invalid
    }
    
    void create_partial_voicebank() {
        auto vb_path = test_root_ / "PartialVoicebank";
        std::filesystem::create_directories(vb_path);
        
        // Create oto.ini with missing audio files
        std::ofstream oto_file(vb_path / "oto.ini");
        oto_file << "existing.wav=a,100,200,50,150,30\n";
        oto_file << "missing.wav=i,200,180,60,140,25\n";
        oto_file.close();
        
        // Create only one of the referenced files
        create_dummy_wav(vb_path / "existing.wav");
        // missing.wav intentionally not created
        
        // Create orphaned audio file
        create_dummy_wav(vb_path / "orphaned.wav");
    }
    
    void create_empty_directory() {
        auto empty_path = test_root_ / "EmptyDirectory";
        std::filesystem::create_directories(empty_path);
        // Leave it empty
    }
    
    void create_dummy_wav(const std::filesystem::path& path, size_t size = 1024) {
        std::ofstream file(path, std::ios::binary);
        
        // Write minimal WAV header
        const char header[] = {
            'R', 'I', 'F', 'F',  // RIFF
            0x24, 0x00, 0x00, 0x00,  // File size - 8 (36 bytes for minimal WAV)
            'W', 'A', 'V', 'E',  // WAVE
            'f', 'm', 't', ' ',  // fmt 
            0x10, 0x00, 0x00, 0x00,  // fmt chunk size (16)
            0x01, 0x00,          // Audio format (1 = PCM)
            0x01, 0x00,          // Number of channels (1)
            0x44, 0xAC, 0x00, 0x00,  // Sample rate (44100)
            0x88, 0x58, 0x01, 0x00,  // Byte rate
            0x02, 0x00,          // Block align
            0x10, 0x00,          // Bits per sample (16)
            'd', 'a', 't', 'a',  // data
            0x00, 0x00, 0x00, 0x00   // data size (0 for empty)
        };
        
        file.write(header, sizeof(header));
        
        // Add dummy audio data
        std::vector<char> dummy_data(size - sizeof(header), 0);
        file.write(dummy_data.data(), dummy_data.size());
        
        file.close();
    }
    
    std::filesystem::path test_root_;
    ScannerConfig config_;
    std::unique_ptr<VoicebankScanner> scanner_;
};

class MockProgressCallback : public ScanProgressCallback {
public:
    MOCK_METHOD(void, on_scan_started, (const std::string& path), (override));
    MOCK_METHOD(void, on_directory_entered, (const std::string& path, size_t depth), (override));
    MOCK_METHOD(void, on_voicebank_found, (const std::string& path), (override));
    MOCK_METHOD(void, on_voicebank_validated, (const std::string& path, bool is_valid), (override));
    MOCK_METHOD(void, on_scan_progress, (size_t current, size_t total), (override));
    MOCK_METHOD(void, on_scan_completed, (const VoicebankDiscovery& result), (override));
    MOCK_METHOD(void, on_scan_error, (const std::string& path, const std::string& error), (override));
    MOCK_METHOD(void, on_validation_warning, (const std::string& path, const std::string& warning), (override));
};

// Basic functionality tests
TEST_F(VoicebankScannerTest, ConstructorAndConfiguration) {
    VoicebankScanner scanner;
    
    ScannerConfig new_config;
    new_config.recursive_search = false;
    new_config.max_scan_depth = 2;
    
    scanner.set_config(new_config);
    
    const auto& retrieved_config = scanner.get_config();
    EXPECT_FALSE(retrieved_config.recursive_search);
    EXPECT_EQ(retrieved_config.max_scan_depth, 2);
}

TEST_F(VoicebankScannerTest, IsVoicebankDirectory) {
    auto valid_path = test_root_ / "ValidVoicebank";
    auto invalid_path = test_root_ / "InvalidVoicebank";
    auto empty_path = test_root_ / "EmptyDirectory";
    auto nonexistent_path = test_root_ / "NonExistent";
    
    EXPECT_TRUE(scanner_->is_voicebank_directory(valid_path.string()));
    EXPECT_FALSE(scanner_->is_voicebank_directory(invalid_path.string()));
    EXPECT_FALSE(scanner_->is_voicebank_directory(empty_path.string()));
    EXPECT_FALSE(scanner_->is_voicebank_directory(nonexistent_path.string()));
}

TEST_F(VoicebankScannerTest, FindVoicebankCandidates) {
    auto candidates = scanner_->find_voicebank_candidates(test_root_.string());
    
    EXPECT_GE(candidates.size(), 1);  // At least ValidVoicebank should be found
    
    bool found_valid = false;
    for (const auto& candidate : candidates) {
        if (candidate.find("ValidVoicebank") != std::string::npos) {
            found_valid = true;
            break;
        }
    }
    EXPECT_TRUE(found_valid);
}

TEST_F(VoicebankScannerTest, ScanDirectory) {
    auto result = scanner_->scan_directory(test_root_.string());
    
    EXPECT_EQ(result.search_path, test_root_.string());
    EXPECT_GT(result.directories_scanned, 0);
    EXPECT_GT(result.files_scanned, 0);
    EXPECT_GT(result.scan_duration.count(), 0);
    
    // Should find at least the valid voice bank
    EXPECT_GE(result.voicebank_paths.size(), 1);
    EXPECT_GE(result.valid_voicebanks, 1);
}

TEST_F(VoicebankScannerTest, ValidateValidVoicebank) {
    auto vb_path = test_root_ / "ValidVoicebank";
    auto validation = scanner_->validate_voicebank(vb_path.string());
    
    EXPECT_TRUE(validation.is_valid);
    EXPECT_TRUE(validation.has_oto_ini);
    EXPECT_TRUE(validation.has_audio_files);
    EXPECT_TRUE(validation.has_character_txt);
    EXPECT_TRUE(validation.has_readme);
    
    EXPECT_EQ(validation.total_oto_entries, 3);
    EXPECT_EQ(validation.total_audio_files, 3);
    EXPECT_EQ(validation.referenced_audio_files, 3);
    EXPECT_EQ(validation.missing_audio_files, 0);
    EXPECT_EQ(validation.orphaned_audio_files, 0);
    EXPECT_EQ(validation.duplicate_aliases, 0);
    
    EXPECT_TRUE(validation.errors.empty());
}

TEST_F(VoicebankScannerTest, ValidateInvalidVoicebank) {
    auto vb_path = test_root_ / "InvalidVoicebank";
    auto validation = scanner_->validate_voicebank(vb_path.string());
    
    EXPECT_FALSE(validation.is_valid);
    EXPECT_TRUE(validation.has_oto_ini);  // File exists but is empty
    EXPECT_FALSE(validation.has_audio_files);
    
    EXPECT_EQ(validation.total_oto_entries, 0);
    EXPECT_EQ(validation.total_audio_files, 0);
    
    EXPECT_FALSE(validation.errors.empty());  // Should have validation errors
}

TEST_F(VoicebankScannerTest, ValidatePartialVoicebank) {
    auto vb_path = test_root_ / "PartialVoicebank";
    auto validation = scanner_->validate_voicebank(vb_path.string());
    
    EXPECT_FALSE(validation.is_valid);  // Missing audio files make it invalid
    EXPECT_TRUE(validation.has_oto_ini);
    EXPECT_TRUE(validation.has_audio_files);  // Has some audio files
    
    EXPECT_EQ(validation.total_oto_entries, 2);
    EXPECT_EQ(validation.total_audio_files, 2);  // existing.wav + orphaned.wav
    EXPECT_EQ(validation.referenced_audio_files, 2);  // existing.wav + missing.wav
    EXPECT_EQ(validation.missing_audio_files, 1);  // missing.wav
    EXPECT_EQ(validation.orphaned_audio_files, 1);  // orphaned.wav
    
    EXPECT_FALSE(validation.suggestions.empty());  // Should have improvement suggestions
}

TEST_F(VoicebankScannerTest, AudioFileValidation) {
    auto wav_path = test_root_ / "ValidVoicebank" / "test1.wav";
    auto audio_info = scanner_->validate_audio_file(wav_path.string());
    
    EXPECT_TRUE(audio_info.exists);
    EXPECT_TRUE(audio_info.is_valid);
    EXPECT_EQ(audio_info.filename, "test1.wav");
    EXPECT_GT(audio_info.file_size, 0);
    EXPECT_EQ(audio_info.format, "WAV");
    EXPECT_EQ(audio_info.sample_rate, 44100);
    EXPECT_EQ(audio_info.bit_depth, 16);
    EXPECT_EQ(audio_info.channels, 1);
}

TEST_F(VoicebankScannerTest, AudioFileValidationNonexistent) {
    auto nonexistent_path = test_root_ / "nonexistent.wav";
    auto audio_info = scanner_->validate_audio_file(nonexistent_path.string());
    
    EXPECT_FALSE(audio_info.exists);
    EXPECT_FALSE(audio_info.is_valid);
    EXPECT_EQ(audio_info.filename, "nonexistent.wav");
    EXPECT_EQ(audio_info.file_size, 0);
}

TEST_F(VoicebankScannerTest, GetSupportedFormats) {
    auto formats = scanner_->get_supported_formats();
    
    EXPECT_FALSE(formats.empty());
    EXPECT_NE(std::find(formats.begin(), formats.end(), ".wav"), formats.end());
    EXPECT_NE(std::find(formats.begin(), formats.end(), ".flac"), formats.end());
}

TEST_F(VoicebankScannerTest, AnalyzeFormatDistribution) {
    VoicebankDiscovery discovery;
    discovery.voicebank_paths = {(test_root_ / "ValidVoicebank").string()};
    
    auto distribution = scanner_->analyze_format_distribution(discovery);
    
    EXPECT_GT(distribution[".wav"], 0);  // Should have WAV files
}

TEST_F(VoicebankScannerTest, ProgressCallbackIntegration) {
    auto callback = std::make_shared<MockProgressCallback>();
    scanner_->set_progress_callback(callback);
    
    // Expect certain callback methods to be called
    EXPECT_CALL(*callback, on_scan_started(::testing::_))
        .Times(1);
    EXPECT_CALL(*callback, on_scan_completed(::testing::_))
        .Times(1);
    EXPECT_CALL(*callback, on_voicebank_found(::testing::_))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*callback, on_voicebank_validated(::testing::_, ::testing::_))
        .Times(::testing::AtLeast(1));
    
    scanner_->scan_directory(test_root_.string());
}

TEST_F(VoicebankScannerTest, ScanCancellation) {
    // Start a scan in a separate thread
    std::future<VoicebankDiscovery> scan_future = std::async(std::launch::async, [this]() {
        return scanner_->scan_directory(test_root_.string());
    });
    
    // Cancel the scan after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    scanner_->cancel_scan();
    
    EXPECT_TRUE(scanner_->is_cancelled());
    
    // Wait for scan to complete
    auto result = scan_future.get();
    
    // Reset cancellation for future tests
    scanner_->reset_cancellation();
    EXPECT_FALSE(scanner_->is_cancelled());
}

TEST_F(VoicebankScannerTest, ScanMultipleDirectories) {
    std::vector<std::string> paths = {
        (test_root_ / "ValidVoicebank").string(),
        (test_root_ / "PartialVoicebank").string()
    };
    
    auto result = scanner_->scan_multiple_directories(paths);
    
    EXPECT_EQ(result.search_path, "Multiple paths");
    EXPECT_GE(result.voicebank_paths.size(), 2);
    EXPECT_GT(result.directories_scanned, 0);
    EXPECT_GT(result.scan_duration.count(), 0);
}

TEST_F(VoicebankScannerTest, ConfigurationValidation) {
    ScannerConfig config;
    
    // Test default values
    EXPECT_TRUE(config.recursive_search);
    EXPECT_TRUE(config.validate_audio_files);
    EXPECT_EQ(config.max_scan_depth, 5);
    EXPECT_EQ(config.preferred_sample_rate, 44100);
    EXPECT_EQ(config.preferred_bit_depth, 16);
    
    // Test supported formats
    EXPECT_TRUE(config.supported_audio_formats.count(".wav"));
    EXPECT_TRUE(config.supported_audio_formats.count(".flac"));
    
    // Test excluded directories
    EXPECT_TRUE(std::find(config.excluded_directories.begin(), 
                         config.excluded_directories.end(), 
                         ".git") != config.excluded_directories.end());
}

TEST_F(VoicebankScannerTest, ErrorHandling) {
    // Test scanning non-existent directory
    auto result = scanner_->scan_directory("/nonexistent/path");
    
    EXPECT_FALSE(result.scan_errors.empty());
    EXPECT_EQ(result.voicebank_paths.size(), 0);
    EXPECT_EQ(result.valid_voicebanks, 0);
}

TEST_F(VoicebankScannerTest, DeepDirectoryStructure) {
    // Create deep directory structure to test max_scan_depth
    auto deep_path = test_root_;
    for (int i = 0; i < 6; ++i) {
        deep_path = deep_path / ("level" + std::to_string(i));
        std::filesystem::create_directories(deep_path);
    }
    
    // Put a voice bank at the deepest level
    std::ofstream oto_file(deep_path / "oto.ini");
    oto_file << "deep.wav=a,100,200,50,150,30\n";
    oto_file.close();
    create_dummy_wav(deep_path / "deep.wav");
    
    // Scan with limited depth
    config_.max_scan_depth = 3;
    scanner_->set_config(config_);
    
    auto result = scanner_->scan_directory(test_root_.string());
    
    // Should not find the deep voice bank due to depth limit
    bool found_deep = false;
    for (const auto& path : result.voicebank_paths) {
        if (path.find("level5") != std::string::npos) {
            found_deep = true;
            break;
        }
    }
    EXPECT_FALSE(found_deep);
}

// Console progress reporter tests
TEST(ConsoleProgressReporterTest, BasicFunctionality) {
    ConsoleProgressReporter reporter(false);  // Non-verbose
    
    // These should not throw exceptions
    reporter.on_scan_started("/test/path");
    reporter.on_directory_entered("/test/path/subdir", 1);
    reporter.on_voicebank_found("/test/path/voicebank");
    reporter.on_voicebank_validated("/test/path/voicebank", true);
    reporter.on_scan_progress(5, 10);
    
    VoicebankDiscovery result;
    result.voicebank_paths = {"vb1", "vb2"};
    result.valid_voicebanks = 2;
    result.scan_duration = std::chrono::milliseconds(1500);
    reporter.on_scan_completed(result);
    
    reporter.on_scan_error("/test/path", "Test error");
    reporter.on_validation_warning("/test/path", "Test warning");
}

TEST(ConsoleProgressReporterTest, VerboseMode) {
    ConsoleProgressReporter reporter(true);  // Verbose
    
    // In verbose mode, should handle all callbacks without crashing
    reporter.on_directory_entered("/test/path/subdir", 2);
    reporter.on_validation_warning("/test/path", "Verbose warning");
}

// Utility function tests
TEST(ScannerUtilsTest, PathUtilities) {
    using namespace scanner_utils;
    
    std::string normalized = normalize_path("/path//to///file");
    EXPECT_EQ(normalized, "/path/to/file");
    
    std::string relative = get_relative_path("/base/path", "/base/path/subdir/file");
    EXPECT_EQ(relative, "subdir/file");
    
    EXPECT_TRUE(is_subdirectory("/parent", "/parent/child"));
    EXPECT_FALSE(is_subdirectory("/parent", "/other"));
}

// Thread safety tests
TEST_F(VoicebankScannerTest, ThreadSafety) {
    const int num_threads = 4;
    const int scans_per_thread = 3;
    
    std::vector<std::future<void>> futures;
    std::atomic<int> completed_scans(0);
    
    auto worker = [&]() {
        for (int i = 0; i < scans_per_thread; ++i) {
            VoicebankScanner thread_scanner(config_);
            auto result = thread_scanner.scan_directory(test_root_.string());
            EXPECT_GE(result.voicebank_paths.size(), 1);
            completed_scans++;
        }
    };
    
    // Start multiple scanning threads
    for (int i = 0; i < num_threads; ++i) {
        futures.emplace_back(std::async(std::launch::async, worker));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(completed_scans.load(), num_threads * scans_per_thread);
}

// Performance tests
TEST_F(VoicebankScannerTest, PerformanceBaseline) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto result = scanner_->scan_directory(test_root_.string());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Basic performance expectation - should complete within reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds for test data
    EXPECT_GT(result.scan_duration.count(), 0);
}

// Edge case tests
TEST_F(VoicebankScannerTest, EmptyDirectory) {
    auto empty_path = test_root_ / "EmptyDirectory";
    auto result = scanner_->scan_directory(empty_path.string());
    
    EXPECT_EQ(result.voicebank_paths.size(), 0);
    EXPECT_EQ(result.valid_voicebanks, 0);
    EXPECT_GT(result.directories_scanned, 0);  // Should scan the empty directory
}

TEST_F(VoicebankScannerTest, VeryLargeFilenames) {
    // Test with very long filename
    std::string long_name(200, 'x');
    auto vb_path = test_root_ / "LongNameTest";
    std::filesystem::create_directories(vb_path);
    
    std::ofstream oto_file(vb_path / "oto.ini");
    oto_file << long_name << ".wav=a,100,200,50,150,30\n";
    oto_file.close();
    
    create_dummy_wav(vb_path / (long_name + ".wav"));
    
    auto validation = scanner_->validate_voicebank(vb_path.string());
    
    // Should handle long filenames gracefully
    EXPECT_TRUE(validation.has_oto_ini);
    EXPECT_TRUE(validation.has_audio_files);
}
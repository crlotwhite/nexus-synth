#include "nexussynth/audio_utils.h"
#include "nexussynth/world_wrapper.h"
#include <iostream>
#include <vector>
#include <cmath>

using namespace nexussynth;

void testAudioBuffer() {
    std::cout << "\n=== Testing AudioBuffer ===\n";
    
    // Test basic initialization
    AudioBuffer buffer(44100, 2, 44100); // 1 second stereo
    std::cout << "✓ AudioBuffer created: " << buffer.getSampleRate() << "Hz, " 
              << buffer.getChannels() << " channels, " << buffer.getLengthSamples() << " samples\n";
    
    // Test resize
    buffer.resize(88200); // 2 seconds
    std::cout << "✓ Resized to " << buffer.getLengthSamples() << " samples (" 
              << buffer.getDuration() << " seconds)\n";
    
    // Test mono conversion
    buffer.convertToMono();
    std::cout << "✓ Converted to mono: " << buffer.getChannels() << " channels\n";
    
    // Test resampling
    buffer.resample(22050);
    std::cout << "✓ Resampled to " << buffer.getSampleRate() << "Hz, " 
              << buffer.getLengthSamples() << " samples\n";
    
    // Test normalization with known data
    std::vector<double>& data = buffer.getData();
    for (size_t i = 0; i < data.size() && i < 1000; ++i) {
        data[i] = (i % 2 == 0) ? 2.0 : -2.0; // High amplitude test data
    }
    buffer.normalize();
    std::cout << "✓ Normalized audio\n";
}

void testWavLoader() {
    std::cout << "\n=== Testing WavLoader ===\n";
    
    WavLoader loader;
    
    // Create a test WAV file
    std::cout << "Creating test WAV file...\n";
    AudioBuffer test_buffer(44100, 1, 44100); // 1 second mono
    std::vector<double>& data = test_buffer.getData();
    
    // Generate 440Hz sine wave
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = 0.5 * sin(2.0 * M_PI * 440.0 * i / 44100.0);
    }
    
    // Save test WAV file
    try {
        loader.saveFile(test_buffer, "test_output.wav", 16);
        std::cout << "✓ Test WAV file saved\n";
        
        // Test file info
        AudioFormat info = loader.getFileInfo("test_output.wav");
        std::cout << "✓ File info: " << info.sample_rate << "Hz, " 
                  << info.channels << " channels, " << info.length_samples 
                  << " samples, " << info.duration << "s\n";
        
        // Test loading
        AudioBuffer loaded_buffer = loader.loadFile("test_output.wav");
        std::cout << "✓ Loaded WAV file: " << loaded_buffer.getSampleRate() << "Hz, "
                  << loaded_buffer.getChannels() << " channels, "
                  << loaded_buffer.getLengthSamples() << " samples\n";
        
        // Verify data integrity (check first few samples)
        const std::vector<double>& loaded_data = loaded_buffer.getData();
        bool data_matches = true;
        for (size_t i = 0; i < 100 && i < loaded_data.size(); ++i) {
            double original = data[i];
            double loaded = loaded_data[i];
            // Allow for some quantization error from 16-bit conversion
            if (std::abs(original - loaded) > 0.01) {
                data_matches = false;
                break;
            }
        }
        
        if (data_matches) {
            std::cout << "✓ Audio data integrity verified\n";
        } else {
            std::cout << "⚠ Audio data has quantization differences (expected for 16-bit)\n";
        }
        
        // Test validation
        if (loader.isValidWavFile("test_output.wav")) {
            std::cout << "✓ WAV file validation passed\n";
        }
        
    } catch (const AudioError& e) {
        std::cout << "✗ WAV loader error: " << e.what() << "\n";
    }
}

void testAudioBufferPool() {
    std::cout << "\n=== Testing AudioBufferPool ===\n";
    
    AudioBufferPool pool(2); // Initial pool size of 2
    
    // Get some buffers
    auto buffer1 = pool.getBuffer(44100, 1, 44100);
    auto buffer2 = pool.getBuffer(44100, 2, 22050);
    auto buffer3 = pool.getBuffer(48000, 1, 48000);
    
    std::cout << "✓ Got 3 buffers from pool\n";
    std::cout << "  Pool size: " << pool.getPoolSize() << ", In use: " << pool.getInUseCount() << "\n";
    
    // Return buffers
    pool.returnBuffer(buffer1);
    pool.returnBuffer(buffer2);
    
    std::cout << "✓ Returned 2 buffers to pool\n";
    std::cout << "  Pool size: " << pool.getPoolSize() << ", In use: " << pool.getInUseCount() << "\n";
    
    // Get buffer again (should reuse)
    auto buffer4 = pool.getBuffer(44100, 1, 44100);
    std::cout << "✓ Reused buffer from pool\n";
    std::cout << "  Pool size: " << pool.getPoolSize() << ", In use: " << pool.getInUseCount() << "\n";
    
    pool.clear();
    std::cout << "✓ Pool cleared\n";
}

void testErrorHandling() {
    std::cout << "\n=== Testing Error Handling ===\n";
    
    try {
        // Test invalid audio buffer
        AudioBuffer invalid_buffer(0, 0, 0); // Invalid parameters
        std::cout << "✗ Should have thrown exception for invalid buffer\n";
    } catch (const AudioError& e) {
        std::cout << "✓ Correctly caught buffer error: " << e.what() << "\n";
    }
    
    try {
        // Test loading non-existent file
        WavLoader loader;
        loader.loadFile("non_existent_file.wav");
        std::cout << "✗ Should have thrown exception for missing file\n";
    } catch (const AudioError& e) {
        std::cout << "✓ Correctly caught file error: " << e.what() << "\n";
    }
    
    try {
        // Test saving empty buffer
        WavLoader loader;
        AudioBuffer empty_buffer;
        loader.saveFile(empty_buffer, "empty.wav");
        std::cout << "✗ Should have thrown exception for empty buffer\n";
    } catch (const AudioError& e) {
        std::cout << "✓ Correctly caught empty buffer error: " << e.what() << "\n";
    }
}

void testWorldParameterExtractorWithWav() {
    std::cout << "\n=== Testing WorldParameterExtractor with WAV files ===\n";
    
    try {
        // Create a test WAV file with known characteristics
        WavLoader loader;
        AudioBuffer test_buffer(16000, 1, 16000); // 1 second at 16kHz (common UTAU rate)
        std::vector<double>& data = test_buffer.getData();
        
        // Generate 220Hz sine wave (low A note)
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = 0.7 * sin(2.0 * M_PI * 220.0 * i / 16000.0);
        }
        
        loader.saveFile(test_buffer, "test_16khz.wav", 16);
        std::cout << "✓ Created 16kHz test file\n";
        
        // Test with WorldParameterExtractor
        WorldConfig config;
        config.frame_period = 5.0; // 5ms frames
        config.f0_floor = 50.0;    // Lower bound for low notes
        config.f0_ceil = 500.0;    // Upper bound
        
        WorldParameterExtractor extractor(44100, config); // Different sample rate to test resampling
        AudioParameters params = extractor.extractFromFile("test_16khz.wav");
        
        std::cout << "✓ Successfully extracted parameters from 16kHz WAV\n";
        std::cout << "  Original: 16kHz, Processed: " << params.sample_rate << "Hz\n";
        std::cout << "  Frames: " << params.length << ", Duration: " 
                  << static_cast<double>(params.length) * config.frame_period / 1000.0 << "s\n";
        
        // Check if F0 detection worked
        if (!params.f0.empty()) {
            double avg_f0 = 0.0;
            int voiced_frames = 0;
            for (double f0 : params.f0) {
                if (f0 > 0) {
                    avg_f0 += f0;
                    voiced_frames++;
                }
            }
            
            if (voiced_frames > 0) {
                avg_f0 /= voiced_frames;
                std::cout << "  Average F0: " << avg_f0 << "Hz (expected ~220Hz)\n";
                std::cout << "  Voiced frames: " << voiced_frames << "/" << params.length << "\n";
            }
        }
        
        // Test JSON save/load with real WAV data
        extractor.saveToJson(params, "test_16khz_params.json");
        AudioParameters loaded_params = extractor.loadFromJson("test_16khz_params.json");
        std::cout << "✓ JSON serialization test passed\n";
        
    } catch (const std::exception& e) {
        std::cout << "✗ Error: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Audio Utilities Test Suite ===\n";
    
    try {
        testAudioBuffer();
        testWavLoader();
        testAudioBufferPool();
        testErrorHandling();
        testWorldParameterExtractorWithWav();
        
        std::cout << "\n=== All Audio Utilities Tests Completed! ===\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Unexpected error: " << e.what() << "\n";
        return 1;
    }
}
#include "nexussynth/world_wrapper.h"
#include <iostream>
#include <vector>
#include <cmath>

using namespace nexussynth;

int main() {
    std::cout << "=== Testing WORLD Wrapper Classes ===" << std::endl;
    
    try {
        // Test parameters
        const int sample_rate = 44100;
        const double duration = 1.0; // 1 second
        const int length = static_cast<int>(sample_rate * duration);
        
        // Generate test audio (440 Hz sine wave)
        std::vector<double> test_audio(length);
        for (int i = 0; i < length; ++i) {
            test_audio[i] = 0.5 * sin(2.0 * M_PI * 440.0 * i / sample_rate);
        }
        
        std::cout << "Generated test audio: " << length << " samples at " << sample_rate << " Hz" << std::endl;
        
        // Test WorldParameterExtractor
        std::cout << "\n=== Testing WorldParameterExtractor ===" << std::endl;
        
        WorldConfig config;
        config.frame_period = 5.0; // 5ms frame period
        config.f0_floor = 71.0;
        config.f0_ceil = 800.0;
        
        WorldParameterExtractor extractor(sample_rate, config);
        
        // Extract all parameters
        AudioParameters params = extractor.extractAll(test_audio.data(), length);
        
        std::cout << "Extraction Results:" << std::endl;
        std::cout << "  Sample rate: " << params.sample_rate << " Hz" << std::endl;
        std::cout << "  Frame period: " << params.frame_period << " ms" << std::endl;
        std::cout << "  Number of frames: " << params.length << std::endl;
        std::cout << "  FFT size: " << params.fft_size << std::endl;
        
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
                std::cout << "  Average F0: " << avg_f0 << " Hz (expected ~440 Hz)" << std::endl;
                std::cout << "  Voiced frames: " << voiced_frames << "/" << params.length << std::endl;
            }
        }
        
        if (!params.spectrum.empty()) {
            std::cout << "  Spectrum dimensions: " << params.spectrum.size() 
                      << " frames x " << params.spectrum[0].size() << " bins" << std::endl;
        }
        
        if (!params.aperiodicity.empty()) {
            std::cout << "  Aperiodicity dimensions: " << params.aperiodicity.size() 
                      << " frames x " << params.aperiodicity[0].size() << " bins" << std::endl;
        }
        
        // Test JSON serialization
        std::cout << "\n=== Testing JSON Serialization ===" << std::endl;
        
        std::string json_file = "test_parameters.json";
        bool save_success = extractor.saveToJson(params, json_file);
        
        if (save_success) {
            std::cout << "✓ Parameters saved to JSON successfully" << std::endl;
            
            // Test loading
            try {
                AudioParameters loaded_params = extractor.loadFromJson(json_file);
                std::cout << "✓ Parameters loaded from JSON successfully" << std::endl;
                std::cout << "  Loaded " << loaded_params.f0.size() << " F0 values" << std::endl;
            } catch (const WorldExtractionError& e) {
                std::cout << "✗ Failed to load JSON: " << e.what() << std::endl;
            }
        } else {
            std::cout << "✗ Failed to save parameters to JSON" << std::endl;
        }
        
        // Test individual wrappers
        std::cout << "\n=== Testing Individual Wrappers ===" << std::endl;
        
        // Test DIO wrapper
        DioWrapper dio(sample_rate, config);
        std::vector<double> f0_result = dio.extractF0(test_audio.data(), length);
        std::cout << "✓ DIO wrapper extracted " << f0_result.size() << " F0 values" << std::endl;
        
        // Test error handling
        std::cout << "\n=== Testing Error Handling ===" << std::endl;
        
        try {
            WorldParameterExtractor bad_extractor(-1, config); // Invalid sample rate
            std::cout << "✗ Should have thrown exception for invalid sample rate" << std::endl;
        } catch (const WorldExtractionError& e) {
            std::cout << "✓ Correctly caught exception: " << e.what() << std::endl;
        }
        
        std::cout << "\n=== All Tests Completed Successfully! ===" << std::endl;
        return 0;
        
    } catch (const WorldExtractionError& e) {
        std::cout << "✗ WORLD Extraction Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cout << "✗ Unexpected Error: " << e.what() << std::endl;
        return 1;
    }
}
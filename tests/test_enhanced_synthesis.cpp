#include "nexussynth/pbp_synthesis_engine.h"
#include "nexussynth/world_wrapper.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>

using namespace nexussynth::synthesis;
using namespace nexussynth;

// Generate test WORLD parameters
AudioParameters generate_test_parameters() {
    AudioParameters params;
    params.sample_rate = 44100;
    params.frame_period = 5.0;
    params.length = 100;  // 100 frames = ~0.5 seconds
    
    // Allocate parameter arrays
    params.f0.resize(params.length);
    params.spectrum.resize(params.length);
    params.aperiodicity.resize(params.length);
    
    const int spectrum_size = 513;  // For 44.1kHz, typically 513 bins
    
    // Generate F0 contour (sliding pitch)
    for (int i = 0; i < params.length; ++i) {
        double t = static_cast<double>(i) / params.length;
        params.f0[i] = 220.0 + 100.0 * std::sin(2.0 * M_PI * t);  // F0 varies 220-320 Hz
        
        // Generate spectral envelope (formant-like structure)
        params.spectrum[i].resize(spectrum_size);
        params.aperiodicity[i].resize(spectrum_size);
        
        for (int bin = 0; bin < spectrum_size; ++bin) {
            double freq = bin * params.sample_rate / 2.0 / spectrum_size;
            
            // Simple formant structure
            double formant1 = 800.0;   // First formant
            double formant2 = 1200.0;  // Second formant
            
            // Spectral envelope with formants
            double amp1 = std::exp(-0.5 * std::pow((freq - formant1) / 150.0, 2));
            double amp2 = 0.7 * std::exp(-0.5 * std::pow((freq - formant2) / 200.0, 2));
            double rolloff = 1.0 / (1.0 + freq / 4000.0);  // High frequency rolloff
            
            params.spectrum[i][bin] = (amp1 + amp2) * rolloff;
            
            // Aperiodicity increases with frequency
            params.aperiodicity[i][bin] = std::min(0.9, freq / 8000.0);
        }
    }
    
    return params;
}

int main() {
    std::cout << "Testing Enhanced Pulse-by-Pulse Synthesis Engine\n";
    std::cout << "==============================================\n\n";
    
    // Configure synthesis engine
    PbpConfig config;
    config.sample_rate = 44100;
    config.fft_size = 1024;
    config.hop_size = 220;        // 75% overlap for smooth synthesis
    config.frame_period = 5.0;
    config.window_type = PbpConfig::WindowType::HANN;
    config.enable_anti_aliasing = true;
    config.use_fast_fft = true;
    
    std::cout << "Configuration:\n";
    std::cout << "  Sample Rate: " << config.sample_rate << " Hz\n";
    std::cout << "  FFT Size: " << config.fft_size << "\n";
    std::cout << "  Hop Size: " << config.hop_size << " (overlap: " 
              << 100.0 * (1.0 - static_cast<double>(config.hop_size) / config.fft_size) << "%)\n";
    std::cout << "  Window: Hann\n\n";
    
    // Create synthesis engine
    PbpSynthesisEngine engine(config);
    
    // Generate test parameters
    std::cout << "Generating test WORLD parameters...\n";
    AudioParameters test_params = generate_test_parameters();
    
    std::cout << "  Duration: " << test_params.length << " frames (" 
              << test_params.length * test_params.frame_period / 1000.0 << " seconds)\n";
    std::cout << "  F0 range: 220-320 Hz (sliding)\n";
    std::cout << "  Formants: 800 Hz, 1200 Hz\n\n";
    
    // Perform synthesis
    std::cout << "Performing pulse-by-pulse synthesis...\n";
    SynthesisStats stats;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<double> synthesized_audio = engine.synthesize(test_params, &stats);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    double total_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Display results
    std::cout << "Synthesis completed!\n\n";
    std::cout << "Results:\n";
    std::cout << "  Output length: " << synthesized_audio.size() << " samples\n";
    std::cout << "  Duration: " << synthesized_audio.size() / static_cast<double>(config.sample_rate) << " seconds\n";
    std::cout << "  Peak amplitude: " << *std::max_element(synthesized_audio.begin(), synthesized_audio.end()) << "\n";
    std::cout << "  RMS level: ";
    
    // Calculate RMS
    double rms = 0.0;
    for (double sample : synthesized_audio) {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / synthesized_audio.size());
    std::cout << rms << "\n\n";
    
    std::cout << "Performance Statistics:\n";
    std::cout << "  Total synthesis time: " << total_time << " ms\n";
    std::cout << "  Real-time factor: " << (synthesized_audio.size() / static_cast<double>(config.sample_rate) * 1000.0) / total_time << "x\n";
    std::cout << "  Average frame time: " << stats.average_frame_time_ms << " ms\n";
    std::cout << "  Peak frame time: " << stats.peak_frame_time_ms << " ms\n";
    std::cout << "  Harmonics generated: " << stats.harmonics_generated << "\n";
    std::cout << "  Harmonic energy ratio: " << stats.harmonic_energy_ratio << "\n";
    std::cout << "  Temporal smoothness: " << stats.temporal_smoothness << "\n\n";
    
    // Quality checks
    std::cout << "Quality Analysis:\n";
    
    // Check for clipping
    bool has_clipping = false;
    int clipped_samples = 0;
    for (double sample : synthesized_audio) {
        if (std::abs(sample) > 0.95) {
            has_clipping = true;
            clipped_samples++;
        }
    }
    
    std::cout << "  Clipping: " << (has_clipping ? "DETECTED" : "None") << "\n";
    if (has_clipping) {
        std::cout << "    Clipped samples: " << clipped_samples << " (" 
                  << 100.0 * clipped_samples / synthesized_audio.size() << "%)\n";
    }
    
    // Check for discontinuities
    int discontinuities = 0;
    double max_discontinuity = 0.0;
    for (size_t i = 1; i < synthesized_audio.size(); ++i) {
        double diff = std::abs(synthesized_audio[i] - synthesized_audio[i-1]);
        if (diff > 0.1) {
            discontinuities++;
            max_discontinuity = std::max(max_discontinuity, diff);
        }
    }
    
    std::cout << "  Discontinuities: " << discontinuities << "\n";
    std::cout << "  Max discontinuity: " << max_discontinuity << "\n\n";
    
    // Save output for analysis
    std::ofstream outfile("enhanced_synthesis_output.raw", std::ios::binary);
    if (outfile) {
        for (double sample : synthesized_audio) {
            float sample_f = static_cast<float>(sample);
            outfile.write(reinterpret_cast<const char*>(&sample_f), sizeof(float));
        }
        outfile.close();
        std::cout << "Audio saved to: enhanced_synthesis_output.raw (32-bit float, " 
                  << config.sample_rate << " Hz)\n";
    }
    
    // Overall assessment
    std::cout << "\nOverall Assessment:\n";
    if (!has_clipping && discontinuities < 10 && stats.harmonic_energy_ratio > 0.7) {
        std::cout << "  Status: ✓ EXCELLENT - High quality synthesis achieved\n";
    } else if (!has_clipping && discontinuities < 50) {
        std::cout << "  Status: ✓ GOOD - Acceptable synthesis quality\n";
    } else {
        std::cout << "  Status: ⚠ NEEDS IMPROVEMENT - Quality issues detected\n";
    }
    
    std::cout << "\nEnhanced overlap-add synthesis test completed.\n";
    
    return 0;
}
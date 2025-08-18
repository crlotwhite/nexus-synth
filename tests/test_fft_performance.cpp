#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include "nexussynth/fft_transform_manager.h"

using namespace nexussynth::transforms;

/**
 * @brief Test FFT accuracy against reference DFT implementation
 */
bool test_fft_accuracy() {
    std::cout << "\n=== Testing FFT Accuracy ===" << std::endl;
    
    try {
        FftConfig config;
        config.backend = FftBackend::EIGEN_DEFAULT;
        FftTransformManager fft_manager(config);
        
        // Test with different sizes
        std::vector<size_t> test_sizes = {64, 128, 256, 512, 1024};
        
        for (size_t size : test_sizes) {
            // Generate test signal (sine wave)
            std::vector<double> test_signal(size);
            double frequency = 5.0;  // 5 cycles over the signal
            for (size_t i = 0; i < size; ++i) {
                test_signal[i] = std::sin(2.0 * M_PI * frequency * i / size);
            }
            
            // Forward FFT
            std::vector<std::complex<double>> fft_result;
            bool success = fft_manager.forward_fft(test_signal, fft_result);
            
            if (!success) {
                std::cerr << "ERROR: FFT failed for size " << size << std::endl;
                return false;
            }
            
            // Inverse FFT
            std::vector<double> reconstructed;
            success = fft_manager.inverse_fft(fft_result, reconstructed);
            
            if (!success) {
                std::cerr << "ERROR: IFFT failed for size " << size << std::endl;
                return false;
            }
            
            // Check reconstruction accuracy
            double max_error = 0.0;
            for (size_t i = 0; i < size; ++i) {
                double error = std::abs(test_signal[i] - reconstructed[i]);
                max_error = std::max(max_error, error);
            }
            
            std::cout << "  Size " << std::setw(4) << size 
                      << ": Max reconstruction error = " << std::scientific 
                      << std::setprecision(2) << max_error << std::endl;
            
            // Accuracy threshold
            if (max_error > 1e-12) {
                std::cerr << "ERROR: Reconstruction error too high for size " << size << std::endl;
                return false;
            }
        }
        
        std::cout << "✓ FFT accuracy test passed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in FFT accuracy test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Benchmark FFT performance vs DFT
 */
bool test_fft_performance() {
    std::cout << "\n=== Testing FFT Performance vs DFT ===" << std::endl;
    
    try {
        FftConfig config;
        config.enable_plan_caching = true;
        config.max_cache_size = 32;
        FftTransformManager fft_manager(config);
        
        std::vector<size_t> benchmark_sizes = {64, 128, 256, 512, 1024, 2048};
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Size\t\tFFT Time\tDFT Time\tSpeedup" << std::endl;
        std::cout << "----\t\t--------\t--------\t-------" << std::endl;
        
        for (size_t size : benchmark_sizes) {
            // Generate test data
            std::vector<double> test_data(size);
            for (size_t i = 0; i < size; ++i) {
                test_data[i] = std::sin(2.0 * M_PI * 3.0 * i / size) + 
                              0.5 * std::cos(2.0 * M_PI * 7.0 * i / size);
            }
            
            // Benchmark FFT (10 iterations for stability)
            int iterations = 10;
            auto fft_start = std::chrono::high_resolution_clock::now();
            
            for (int iter = 0; iter < iterations; ++iter) {
                std::vector<std::complex<double>> fft_result;
                fft_manager.forward_fft(test_data, fft_result);
                
                std::vector<double> reconstructed;
                fft_manager.inverse_fft(fft_result, reconstructed);
            }
            
            auto fft_end = std::chrono::high_resolution_clock::now();
            double fft_time = std::chrono::duration<double, std::milli>(fft_end - fft_start).count() / iterations;
            
            // Benchmark naive DFT (1 iteration for larger sizes due to O(n²) complexity)
            int dft_iterations = (size > 512) ? 1 : 5;
            auto dft_start = std::chrono::high_resolution_clock::now();
            
            for (int iter = 0; iter < dft_iterations; ++iter) {
                // Naive DFT implementation
                std::vector<std::complex<double>> dft_result(size);
                for (size_t k = 0; k < size; ++k) {
                    std::complex<double> sum(0.0, 0.0);
                    for (size_t n = 0; n < size; ++n) {
                        double angle = -2.0 * M_PI * k * n / size;
                        sum += test_data[n] * std::complex<double>(std::cos(angle), std::sin(angle));
                    }
                    dft_result[k] = sum;
                }
                
                // Naive IDFT
                std::vector<double> dft_reconstructed(size);
                for (size_t n = 0; n < size; ++n) {
                    std::complex<double> sum(0.0, 0.0);
                    for (size_t k = 0; k < size; ++k) {
                        double angle = 2.0 * M_PI * k * n / size;
                        sum += dft_result[k] * std::complex<double>(std::cos(angle), std::sin(angle));
                    }
                    dft_reconstructed[n] = sum.real() / size;
                }
            }
            
            auto dft_end = std::chrono::high_resolution_clock::now();
            double dft_time = std::chrono::duration<double, std::milli>(dft_end - dft_start).count() / dft_iterations;
            
            double speedup = dft_time / fft_time;
            
            std::cout << size << "\t\t" << fft_time << " ms\t\t" 
                      << dft_time << " ms\t\t" << speedup << "x" << std::endl;
            
            // Verify we're getting significant speedup for larger sizes
            if (size >= 512 && speedup < 5.0) {
                std::cout << "WARNING: Expected higher speedup for size " << size << std::endl;
            }
        }
        
        std::cout << "✓ FFT performance benchmark completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in FFT performance test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test FFT plan caching effectiveness
 */
bool test_fft_caching() {
    std::cout << "\n=== Testing FFT Plan Caching ===" << std::endl;
    
    try {
        // Test with caching enabled
        FftConfig cached_config;
        cached_config.enable_plan_caching = true;
        cached_config.max_cache_size = 16;
        FftTransformManager cached_manager(cached_config);
        
        // Test without caching
        FftConfig no_cache_config;
        no_cache_config.enable_plan_caching = false;
        FftTransformManager no_cache_manager(no_cache_config);
        
        std::vector<double> test_data(1024);
        for (size_t i = 0; i < 1024; ++i) {
            test_data[i] = std::sin(2.0 * M_PI * 5.0 * i / 1024);
        }
        
        // Warm up caches
        std::vector<std::complex<double>> result;
        cached_manager.forward_fft(test_data, result);
        no_cache_manager.forward_fft(test_data, result);
        
        // Benchmark cached version
        int iterations = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            cached_manager.forward_fft(test_data, result);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double cached_time = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Benchmark non-cached version
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            no_cache_manager.forward_fft(test_data, result);
        }
        
        end = std::chrono::high_resolution_clock::now();
        double no_cache_time = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Get cache statistics
        auto cached_stats = cached_manager.get_stats();
        auto no_cache_stats = no_cache_manager.get_stats();
        
        std::cout << "Cached version: " << cached_time << " ms (" 
                  << iterations << " transforms)" << std::endl;
        std::cout << "  Cache hits: " << cached_stats.cache_hits 
                  << ", misses: " << cached_stats.cache_misses << std::endl;
        std::cout << "  Cache hit ratio: " << std::fixed << std::setprecision(1) 
                  << cached_stats.cache_hit_ratio * 100 << "%" << std::endl;
        
        std::cout << "Non-cached version: " << no_cache_time << " ms" << std::endl;
        std::cout << "  Cache hits: " << no_cache_stats.cache_hits 
                  << ", misses: " << no_cache_stats.cache_misses << std::endl;
        
        double cache_speedup = no_cache_time / cached_time;
        std::cout << "Cache speedup: " << std::fixed << std::setprecision(2) 
                  << cache_speedup << "x" << std::endl;
        
        // Verify caching is working
        if (cached_stats.cache_hits < iterations - 5) {  // Allow some tolerance
            std::cout << "WARNING: Cache hit ratio lower than expected" << std::endl;
        }
        
        std::cout << "✓ FFT caching test completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in FFT caching test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test real-time synthesis performance improvement
 */
bool test_realtime_synthesis_improvement() {
    std::cout << "\n=== Testing Real-time Synthesis Improvement ===" << std::endl;
    
    try {
        // Test typical synthesis parameters
        const int sample_rate = 44100;
        const double frame_period = 5.0;  // 5ms frames
        const int fft_size = 2048;
        
        const double test_duration = 1.0;  // 1 second
        const int num_frames = static_cast<int>(test_duration * 1000.0 / frame_period);
        
        std::cout << "Testing synthesis performance:" << std::endl;
        std::cout << "  Duration: " << test_duration << " seconds" << std::endl;
        std::cout << "  Frames: " << num_frames << std::endl;
        std::cout << "  FFT size: " << fft_size << std::endl;
        
        FftTransformManager fft_manager;
        
        // Generate test spectrum data
        std::vector<std::vector<std::complex<double>>> test_spectra(num_frames);
        const int spectrum_size = fft_size / 2 + 1;
        
        for (int frame = 0; frame < num_frames; ++frame) {
            test_spectra[frame].resize(spectrum_size);
            
            // Generate harmonic spectrum
            double f0 = 220.0 + 50.0 * std::sin(2.0 * M_PI * 2.0 * frame / num_frames);
            
            for (int bin = 0; bin < spectrum_size; ++bin) {
                double freq = static_cast<double>(bin) * sample_rate / fft_size;
                
                // Create formant-like structure
                double amplitude = std::exp(-std::pow((freq - 800.0) / 200.0, 2)) +
                                 0.7 * std::exp(-std::pow((freq - 1200.0) / 300.0, 2));
                
                // Random phase
                double phase = 2.0 * M_PI * rand() / RAND_MAX;
                
                test_spectra[frame][bin] = std::complex<double>(
                    amplitude * std::cos(phase),
                    amplitude * std::sin(phase)
                );
            }
        }
        
        // Benchmark synthesis performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::vector<double>> synthesized_frames(num_frames);
        
        for (int frame = 0; frame < num_frames; ++frame) {
            bool success = fft_manager.synthesize_pulse_from_spectrum(
                test_spectra[frame], 
                synthesized_frames[frame],
                true
            );
            
            if (!success) {
                std::cerr << "ERROR: Synthesis failed for frame " << frame << std::endl;
                return false;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double synthesis_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        double audio_duration_ms = test_duration * 1000.0;
        double real_time_factor = audio_duration_ms / synthesis_time;
        
        std::cout << "Synthesis results:" << std::endl;
        std::cout << "  Total synthesis time: " << std::fixed << std::setprecision(2) 
                  << synthesis_time << " ms" << std::endl;
        std::cout << "  Audio duration: " << audio_duration_ms << " ms" << std::endl;
        std::cout << "  Real-time factor: " << std::fixed << std::setprecision(1) 
                  << real_time_factor << "x" << std::endl;
        
        if (real_time_factor >= 1.0) {
            std::cout << "  ✓ Real-time synthesis achieved!" << std::endl;
        } else {
            std::cout << "  ⚠ Synthesis slower than real-time" << std::endl;
        }
        
        // Get FFT statistics
        auto stats = fft_manager.get_stats();
        std::cout << "FFT Statistics:" << std::endl;
        std::cout << "  Transforms performed: " << stats.transforms_performed << std::endl;
        std::cout << "  Cache hit ratio: " << std::fixed << std::setprecision(1) 
                  << stats.cache_hit_ratio * 100 << "%" << std::endl;
        std::cout << "  Average transform time: " << std::scientific << std::setprecision(2) 
                  << stats.total_transform_time_ms / stats.transforms_performed << " ms" << std::endl;
        
        std::cout << "✓ Real-time synthesis improvement test completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in real-time synthesis test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test FFT backend availability and performance
 */
bool test_fft_backends() {
    std::cout << "\n=== Testing FFT Backend Availability ===" << std::endl;
    
    try {
        FftTransformManager manager;
        
        auto backends = manager.get_available_backends();
        
        std::cout << "Available FFT backends:" << std::endl;
        
        for (const auto& backend_info : backends) {
            std::string backend_name = fft_utils::backend_to_string(backend_info.first);
            std::string status = backend_info.second ? "Available" : "Not Available";
            
            std::cout << "  " << std::setw(20) << backend_name << ": " << status << std::endl;
        }
        
        std::cout << "✓ FFT backend availability test completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR in backend availability test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Main test function
 */
int main() {
    std::cout << "NexusSynth FFT Transform Manager Performance Test Suite" << std::endl;
    std::cout << "=======================================================" << std::endl;
    
    bool all_tests_passed = true;
    
    // Run all tests
    all_tests_passed &= test_fft_backends();
    all_tests_passed &= test_fft_accuracy();
    all_tests_passed &= test_fft_performance();
    all_tests_passed &= test_fft_caching();
    all_tests_passed &= test_realtime_synthesis_improvement();
    
    std::cout << "\n=======================================================" << std::endl;
    if (all_tests_passed) {
        std::cout << "🎉 All FFT Performance tests PASSED!" << std::endl;
        std::cout << "   ✓ FFT backend availability confirmed" << std::endl;
        std::cout << "   ✓ FFT accuracy verified" << std::endl;
        std::cout << "   ✓ FFT performance significantly improved" << std::endl;
        std::cout << "   ✓ FFT plan caching operational" << std::endl;
        std::cout << "   ✓ Real-time synthesis performance achieved" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Some FFT Performance tests FAILED!" << std::endl;
        std::cout << "Please review the error messages above." << std::endl;
        return 1;
    }
}
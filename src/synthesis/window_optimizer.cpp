#include "nexussynth/window_optimizer.h"
#include <algorithm>
#include <numeric>
#include <complex>
#include <cstring>
#include <iostream>
#include <memory>

namespace nexussynth {

constexpr double PI = 3.141592653589793;
constexpr double TWO_PI = 2.0 * PI;

class WindowOptimizer::Impl {
public:
    // Bessel function for Kaiser window
    static double modified_bessel_i0(double x) {
        double sum = 1.0;
        double term = 1.0;
        double x2 = x * x / 4.0;
        
        for (int i = 1; i < 100; ++i) {
            term *= x2 / (i * i);
            sum += term;
            if (term < 1e-15) break;
        }
        return sum;
    }
    
    // FFT for spectral analysis (simple DFT implementation for analysis)
    static std::vector<std::complex<double>> compute_dft(const std::vector<double>& input) {
        int N = input.size();
        std::vector<std::complex<double>> output(N);
        
        for (int k = 0; k < N; ++k) {
            std::complex<double> sum(0.0, 0.0);
            for (int n = 0; n < N; ++n) {
                double angle = -TWO_PI * k * n / N;
                sum += input[n] * std::complex<double>(std::cos(angle), std::sin(angle));
            }
            output[k] = sum;
        }
        return output;
    }
};

WindowOptimizer::WindowOptimizer() : pimpl_(std::make_unique<Impl>()) {}

WindowOptimizer::~WindowOptimizer() = default;

std::vector<double> WindowOptimizer::generate_optimal_window(
    int length,
    const ContentAnalysis& content_analysis,
    const WindowOptimizationParams& params) {
    
    // Select optimal window type based on content
    OptimalWindowType optimal_type = select_optimal_window_type(content_analysis, params);
    
    // Generate base window
    std::vector<double> window = generate_window(optimal_type, length, params);
    
    // Apply optimizations
    if (params.minimize_pre_echo) {
        apply_pre_echo_suppression(window, 0.8);
    }
    
    if (params.optimize_for_overlap_add) {
        int hop_size = static_cast<int>(length * params.hop_factor);
        optimize_for_overlap_add(window, params.overlap_factor, hop_size);
    }
    
    // Apply spectral leakage minimization
    minimize_spectral_leakage(window, params.side_lobe_suppression_db);
    
    return window;
}

std::vector<double> WindowOptimizer::generate_window(
    OptimalWindowType type,
    int length,
    const WindowOptimizationParams& /* params */) {
    
    switch (type) {
        case OptimalWindowType::HANN:
            return generate_hann_window(length);
        case OptimalWindowType::HAMMING:
            return generate_hamming_window(length);
        case OptimalWindowType::BLACKMAN:
            return generate_blackman_window(length);
        case OptimalWindowType::BLACKMAN_HARRIS:
            return generate_blackman_harris_window(length);
        case OptimalWindowType::GAUSSIAN:
            return generate_gaussian_window(length, 2.5);
        case OptimalWindowType::TUKEY:
            return generate_tukey_window(length, 0.5);
        case OptimalWindowType::KAISER:
            return generate_kaiser_window(length, 8.6);
        case OptimalWindowType::NUTTALL:
            return generate_nuttall_window(length);
        default:
            return generate_hann_window(length);
    }
}

OptimalWindowType WindowOptimizer::select_optimal_window_type(
    const ContentAnalysis& content_analysis,
    const WindowOptimizationParams& /* params */) {
    
    // Decision logic based on content characteristics
    
    // For high harmonic content, use windows with good spectral resolution
    if (content_analysis.harmonic_ratio > 0.8) {
        if (content_analysis.pitch_frequency < 200.0) {
            // Low pitch: need better frequency resolution
            return OptimalWindowType::BLACKMAN_HARRIS;
        } else {
            // Higher pitch: Blackman provides good balance
            return OptimalWindowType::BLACKMAN;
        }
    }
    
    // For transient content, use windows with good temporal resolution
    if (content_analysis.transient_factor > 0.6) {
        return OptimalWindowType::TUKEY;  // Good for transients
    }
    
    // For broadband content with formants
    if (!content_analysis.formant_frequencies.empty() && 
        content_analysis.formant_frequencies.size() >= 2) {
        return OptimalWindowType::KAISER;  // Adjustable characteristics
    }
    
    // High dynamic range content
    if (content_analysis.dynamic_range_db > 40.0) {
        return OptimalWindowType::BLACKMAN_HARRIS;  // Excellent side lobe suppression
    }
    
    // Default for general synthesis applications
    return OptimalWindowType::HANN;
}

void WindowOptimizer::apply_pre_echo_suppression(
    std::vector<double>& window,
    double suppression_factor) {
    
    int length = window.size();
    int fade_length = static_cast<int>(length * 0.1);  // 10% fade-in
    
    // Apply asymmetric fade-in to suppress pre-echo
    for (int i = 0; i < fade_length; ++i) {
        double fade_factor = static_cast<double>(i) / (fade_length - 1);
        // Use steeper rise to minimize pre-echo
        fade_factor = std::pow(fade_factor, 1.0 + suppression_factor);
        window[i] *= fade_factor;
    }
    
    // Compensate by slightly extending the main lobe
    int center = length / 2;
    int extend_length = fade_length / 2;
    
    for (int i = center; i < center + extend_length && i < length; ++i) {
        window[i] *= (1.0 + 0.05 * suppression_factor);
    }
}

void WindowOptimizer::minimize_spectral_leakage(
    std::vector<double>& window,
    double target_side_lobe_db) {
    
    // Apply frequency domain shaping to reduce side lobes
    int length = window.size();
    std::vector<double> shaped_window = window;
    
    // Compute current spectral characteristics
    auto spectrum = pimpl_->compute_dft(window);
    
    // Find peak side lobe level
    double peak_magnitude = 0.0;
    int peak_index = 0;
    for (size_t i = 0; i < spectrum.size() / 2; ++i) {
        double mag = std::abs(spectrum[i]);
        if (i > 0 && mag > peak_magnitude) {  // Skip DC component
            peak_magnitude = mag;
            peak_index = i;
        }
    }
    
    if (peak_index > 0) {
        double current_side_lobe_db = 20.0 * std::log10(
            std::abs(spectrum[peak_index]) / std::abs(spectrum[0]));
        
        if (current_side_lobe_db > target_side_lobe_db) {
            // Apply tapering to reduce side lobes
            double taper_factor = std::min(0.3, 
                (current_side_lobe_db - target_side_lobe_db) / 60.0);
            
            // Taper the window edges more aggressively
            int taper_length = static_cast<int>(length * taper_factor);
            
            for (int i = 0; i < taper_length; ++i) {
                double taper = std::sin(PI * i / (2 * taper_length));
                window[i] *= taper;
                window[length - 1 - i] *= taper;
            }
        }
    }
}

void WindowOptimizer::optimize_for_overlap_add(
    std::vector<double>& window,
    double overlap_factor,
    int hop_size) {
    
    int length = window.size();
    int overlap_length = static_cast<int>(length * overlap_factor);
    
    // Ensure perfect reconstruction property for OLA
    std::vector<double> reconstructed(length, 0.0);
    
    // Simulate overlap-add reconstruction
    for (int offset = 0; offset < length; offset += hop_size) {
        for (int i = 0; i < length && offset + i < length; ++i) {
            reconstructed[offset + i] += window[i] * window[i];
        }
    }
    
    // Find the normalization factor to achieve constant overlap-add
    double target_gain = 1.0;
    double avg_reconstruction = 0.0;
    int valid_samples = 0;
    
    // Skip edge effects and calculate average reconstruction gain
    int skip = overlap_length / 2;
    for (int i = skip; i < length - skip; ++i) {
        if (reconstructed[i] > 0.0) {
            avg_reconstruction += reconstructed[i];
            valid_samples++;
        }
    }
    
    if (valid_samples > 0) {
        avg_reconstruction /= valid_samples;
        
        // Adjust window to achieve better reconstruction
        if (avg_reconstruction > 0.0) {
            double correction_factor = target_gain / avg_reconstruction;
            for (int i = 0; i < length; ++i) {
                window[i] *= std::sqrt(correction_factor);
            }
        }
    }
}

// Window generation implementations
std::vector<double> WindowOptimizer::generate_hann_window(int length) {
    std::vector<double> window(length);
    for (int i = 0; i < length; ++i) {
        window[i] = 0.5 * (1.0 - std::cos(TWO_PI * i / (length - 1)));
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_hamming_window(int length) {
    std::vector<double> window(length);
    for (int i = 0; i < length; ++i) {
        window[i] = 0.54 - 0.46 * std::cos(TWO_PI * i / (length - 1));
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_blackman_window(int length) {
    std::vector<double> window(length);
    for (int i = 0; i < length; ++i) {
        double factor = TWO_PI * i / (length - 1);
        window[i] = 0.42 - 0.5 * std::cos(factor) + 0.08 * std::cos(2 * factor);
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_blackman_harris_window(int length) {
    std::vector<double> window(length);
    const double a0 = 0.35875;
    const double a1 = 0.48829;
    const double a2 = 0.14128;
    const double a3 = 0.01168;
    
    for (int i = 0; i < length; ++i) {
        double factor = TWO_PI * i / (length - 1);
        window[i] = a0 - a1 * std::cos(factor) + 
                   a2 * std::cos(2 * factor) - 
                   a3 * std::cos(3 * factor);
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_gaussian_window(int length, double alpha) {
    std::vector<double> window(length);
    double sigma = (length - 1) / (2.0 * alpha);
    double center = (length - 1) / 2.0;
    
    for (int i = 0; i < length; ++i) {
        double x = (i - center) / sigma;
        window[i] = std::exp(-0.5 * x * x);
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_tukey_window(int length, double alpha) {
    std::vector<double> window(length);
    int taper_length = static_cast<int>(alpha * (length - 1) / 2.0);
    
    for (int i = 0; i < length; ++i) {
        if (i <= taper_length) {
            // Rising taper
            window[i] = 0.5 * (1.0 - std::cos(PI * i / taper_length));
        } else if (i >= length - taper_length - 1) {
            // Falling taper
            int idx = length - 1 - i;
            window[i] = 0.5 * (1.0 - std::cos(PI * idx / taper_length));
        } else {
            // Flat top
            window[i] = 1.0;
        }
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_kaiser_window(int length, double beta) {
    std::vector<double> window(length);
    double bessel_beta = Impl::modified_bessel_i0(beta);
    double center = (length - 1) / 2.0;
    
    for (int i = 0; i < length; ++i) {
        double x = 2.0 * (i - center) / (length - 1);
        double arg = beta * std::sqrt(1.0 - x * x);
        window[i] = Impl::modified_bessel_i0(arg) / bessel_beta;
    }
    return window;
}

std::vector<double> WindowOptimizer::generate_nuttall_window(int length) {
    std::vector<double> window(length);
    const double a0 = 0.3635819;
    const double a1 = 0.4891775;
    const double a2 = 0.1365995;
    const double a3 = 0.0106411;
    
    for (int i = 0; i < length; ++i) {
        double factor = TWO_PI * i / (length - 1);
        window[i] = a0 - a1 * std::cos(factor) + 
                   a2 * std::cos(2 * factor) - 
                   a3 * std::cos(3 * factor);
    }
    return window;
}

WindowCharacteristics WindowOptimizer::analyze_window_characteristics(
    const std::vector<double>& window,
    double sample_rate) {
    
    WindowCharacteristics characteristics;
    
    // Compute window spectrum
    auto spectrum = pimpl_->compute_dft(window);
    int N = spectrum.size();
    
    // Find main lobe width (first nulls on either side of peak)
    double peak_magnitude = std::abs(spectrum[0]);
    int main_lobe_end = 0;
    
    // Find first null after peak
    for (int i = 1; i < N / 2; ++i) {
        double mag = std::abs(spectrum[i]);
        if (mag < peak_magnitude * 0.01) {  // -40dB threshold
            main_lobe_end = i;
            break;
        }
    }
    
    characteristics.main_lobe_width = 2.0 * main_lobe_end * sample_rate / N;
    
    // Find peak side lobe level
    double max_side_lobe = 0.0;
    for (int i = main_lobe_end; i < N / 2; ++i) {
        double mag = std::abs(spectrum[i]);
        max_side_lobe = std::max(max_side_lobe, mag);
    }
    
    characteristics.peak_side_lobe_db = 20.0 * std::log10(max_side_lobe / peak_magnitude);
    
    // Calculate coherent gain
    characteristics.coherent_gain = std::accumulate(window.begin(), window.end(), 0.0) / window.size();
    
    // Scalloping loss (approximation)
    characteristics.scalloping_loss_db = -3.92;  // Typical for well-designed windows
    
    characteristics.description = "Analyzed window characteristics";
    
    return characteristics;
}

double WindowOptimizer::evaluate_window_quality(
    const std::vector<double>& window,
    const ContentAnalysis& content_analysis) {
    
    auto characteristics = analyze_window_characteristics(window);
    
    double quality_score = 0.0;
    double weight_sum = 0.0;
    
    // Side lobe suppression quality (higher suppression = better)
    double side_lobe_quality = std::max(0.0, 
        std::min(1.0, (-characteristics.peak_side_lobe_db - 40.0) / 40.0));
    quality_score += side_lobe_quality * 0.3;
    weight_sum += 0.3;
    
    // Main lobe width quality (narrower main lobe = better frequency resolution)
    double main_lobe_quality = std::max(0.0, 
        std::min(1.0, 1.0 - (characteristics.main_lobe_width - 1000.0) / 2000.0));
    quality_score += main_lobe_quality * 0.2;
    weight_sum += 0.2;
    
    // Coherent gain quality
    double gain_quality = std::max(0.0, characteristics.coherent_gain);
    quality_score += gain_quality * 0.2;
    weight_sum += 0.2;
    
    // Content-specific quality adjustments
    if (content_analysis.harmonic_ratio > 0.7) {
        // For harmonic content, prioritize side lobe suppression
        quality_score += side_lobe_quality * 0.3;
        weight_sum += 0.3;
    }
    
    return weight_sum > 0.0 ? quality_score / weight_sum : 0.0;
}

// Utility functions
namespace window_utils {

double compare_window_quality(
    const std::vector<double>& window1,
    const std::vector<double>& window2,
    const ContentAnalysis& content_analysis) {
    
    WindowOptimizer optimizer;
    double quality1 = optimizer.evaluate_window_quality(window1, content_analysis);
    double quality2 = optimizer.evaluate_window_quality(window2, content_analysis);
    
    return quality1 - quality2;
}

void calculate_spectral_leakage(
    const std::vector<double>& window,
    std::vector<double>& frequency_bins) {
    
    // Simple DFT implementation for spectral analysis
    int N = window.size();
    std::vector<std::complex<double>> spectrum(N);
    
    for (int k = 0; k < N; ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (int n = 0; n < N; ++n) {
            double angle = -TWO_PI * k * n / N;
            sum += window[n] * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        spectrum[k] = sum;
    }
    
    frequency_bins.resize(spectrum.size() / 2);
    
    double peak_magnitude = std::abs(spectrum[0]);
    for (size_t i = 0; i < frequency_bins.size(); ++i) {
        frequency_bins[i] = std::abs(spectrum[i]) / peak_magnitude;
    }
}

double calculate_ola_reconstruction_error(
    const std::vector<double>& window,
    int hop_size) {
    
    int length = window.size();
    std::vector<double> reconstructed(length * 2, 0.0);
    
    // Simulate overlap-add with multiple overlapping windows
    for (int offset = 0; offset < length; offset += hop_size) {
        for (int i = 0; i < length; ++i) {
            if (offset + i < static_cast<int>(reconstructed.size())) {
                reconstructed[offset + i] += window[i] * window[i];
            }
        }
    }
    
    // Calculate RMS error from perfect reconstruction (constant value)
    double target_value = 1.0;  // Perfect reconstruction target
    double error_sum = 0.0;
    int valid_samples = 0;
    
    // Skip edge effects
    int skip = length / 4;
    for (int i = skip; i < length - skip; ++i) {
        if (reconstructed[i] > 0.0) {
            double error = reconstructed[i] - target_value;
            error_sum += error * error;
            valid_samples++;
        }
    }
    
    return valid_samples > 0 ? std::sqrt(error_sum / valid_samples) : 0.0;
}

} // namespace window_utils

} // namespace nexussynth
#include "nexussynth/pbp_synthesis_engine.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <chrono>
#include <random>
#include <iostream>

namespace nexussynth {
namespace synthesis {

    // Mathematical constants
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;

    PbpSynthesisEngine::PbpSynthesisEngine(const PbpConfig& config)
        : config_(config) {
        
        // Initialize high-performance FFT manager
        transforms::FftConfig fft_config;
        fft_config.backend = transforms::FftBackend::EIGEN_DEFAULT;
        fft_config.enable_plan_caching = true;
        fft_config.max_cache_size = 64;  // Cache plans for common FFT sizes
        fft_config.enable_multithreading = (config_.synthesis_threads > 1);
        
        fft_manager_ = std::make_unique<transforms::FftTransformManager>(fft_config);
        
        initialize_engine();
    }

    void PbpSynthesisEngine::initialize_engine() {
        if (engine_initialized_) {
            deallocate_synthesis_buffers();
        }

        // Validate configuration
        if (config_.sample_rate <= 0 || config_.fft_size <= 0 || config_.hop_size <= 0) {
            throw std::invalid_argument("Invalid synthesis configuration parameters");
        }

        // Allocate buffers
        allocate_synthesis_buffers();
        
        // Generate window function
        int window_length = static_cast<int>(config_.sample_rate * config_.frame_period / 1000.0 * config_.window_length_factor);
        window_function_ = generate_window(window_length, config_.window_type);
        
        // Initialize FFT buffers
        fft_buffer_.resize(config_.fft_size);
        spectrum_buffer_.resize(config_.fft_size);
        
        // Precompute synthesis tables for optimization
        if (config_.use_fast_fft) {
            precompute_synthesis_tables();
            
            // Precompute FFT plans for common synthesis sizes
            std::vector<size_t> common_fft_sizes = {
                256, 512, 1024, 2048, 4096, 8192
            };
            fft_manager_->precompute_plans(common_fft_sizes);
        }
        
        engine_initialized_ = true;
        
        if (config_.synthesis_threads > 1) {
            // TODO: Initialize thread pool for multi-threaded synthesis
        }
    }

    std::vector<double> PbpSynthesisEngine::synthesize(
        const AudioParameters& parameters,
        SynthesisStats* stats) {
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Validate input parameters
        validate_synthesis_parameters(parameters);
        
        // Calculate synthesis buffer size
        int duration_samples = static_cast<int>(parameters.length * config_.sample_rate * config_.frame_period / 1000.0);
        synthesis_buffer_.resize(duration_samples + config_.fft_size, 0.0); // Extra padding for overlap
        
        // Initialize statistics
        SynthesisStats local_stats;
        local_stats.frames_processed = parameters.length;
        local_stats.synthesis_method = "Pulse-by-Pulse WORLD";
        
        // Calculate pulse positions based on F0 contour
        std::vector<double> pulse_positions = calculate_pulse_positions(parameters.f0, parameters.frame_period);
        
        int harmonics_total = 0;
        double max_frame_time = 0.0;
        double total_frame_time = 0.0;
        
        // Process each frame with pulse-by-pulse synthesis
        for (int frame = 0; frame < parameters.length; ++frame) {
            auto frame_start = std::chrono::high_resolution_clock::now();
            
            // Skip unvoiced frames (F0 == 0)
            if (parameters.f0[frame] <= 0.0) {
                // Generate noise-only frame for unvoiced regions
                double frame_energy = 0.0;
                for (double ap : parameters.aperiodicity[frame]) {
                    frame_energy += ap;
                }
                
                if (frame_energy > config_.noise_floor) {
                    // Add aperiodic noise component
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::normal_distribution<double> noise_dist(0.0, 0.1);
                    
                    int frame_start_sample = static_cast<int>(frame * config_.hop_size);
                    int frame_length = config_.hop_size;
                    
                    for (int i = 0; i < frame_length && frame_start_sample + i < static_cast<int>(synthesis_buffer_.size()); ++i) {
                        double noise_sample = noise_dist(gen) * frame_energy * 0.01; // Scale noise
                        synthesis_buffer_[frame_start_sample + i] += noise_sample;
                    }
                }
                continue;
            }
            
            // Create pulse parameters for current frame
            PulseParams pulse_params;
            pulse_params.f0 = parameters.f0[frame];
            pulse_params.spectrum = parameters.spectrum[frame];
            pulse_params.aperiodicity = parameters.aperiodicity[frame];
            pulse_params.pulse_position = pulse_positions[frame];
            
            // Generate harmonic structure
            std::vector<std::complex<double>> harmonics(config_.fft_size / 2 + 1, std::complex<double>(0.0, 0.0));
            generate_harmonics(pulse_params.f0, pulse_params.spectrum, pulse_params.aperiodicity, harmonics);
            
            // Apply spectral envelope filtering
            std::vector<std::complex<double>> filtered_harmonics(config_.fft_size / 2 + 1);
            apply_spectral_envelope(harmonics, pulse_params.spectrum, filtered_harmonics);
            
            // Mix aperiodic component
            std::vector<std::complex<double>> mixed_spectrum(config_.fft_size / 2 + 1);
            mix_aperiodic_component(filtered_harmonics, pulse_params.aperiodicity, mixed_spectrum);
            
            // Perform inverse FFT to generate time-domain pulse
            std::vector<double> pulse_waveform(config_.fft_size);
            inverse_fft_synthesis(mixed_spectrum, pulse_waveform);
            
            // Apply window function
            std::vector<double> windowed_pulse;
            apply_window_function(pulse_waveform, windowed_pulse);
            
            // Overlap-add into synthesis buffer
            int pulse_position_samples = static_cast<int>(pulse_params.pulse_position);
            overlap_add_pulse(windowed_pulse, pulse_position_samples, synthesis_buffer_);
            
            // Update statistics
            int frame_harmonics = 0;
            for (const auto& h : harmonics) {
                if (std::abs(h) > config_.harmonic_amplitude_threshold) {
                    frame_harmonics++;
                }
            }
            harmonics_total += frame_harmonics;
            
            auto frame_end = std::chrono::high_resolution_clock::now();
            double frame_time = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
            max_frame_time = std::max(max_frame_time, frame_time);
            total_frame_time += frame_time;
        }
        
        // Trim synthesis buffer to actual duration
        synthesis_buffer_.resize(duration_samples);
        
        // Calculate final statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        local_stats.synthesis_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        local_stats.average_frame_time_ms = total_frame_time / parameters.length;
        local_stats.peak_frame_time_ms = max_frame_time;
        local_stats.harmonics_generated = harmonics_total;
        
        // Calculate quality metrics
        local_stats.harmonic_energy_ratio = calculate_harmonic_energy_ratio();
        local_stats.temporal_smoothness = calculate_trajectory_smoothness(synthesis_buffer_);
        
        // Copy statistics if requested
        if (stats) {
            *stats = local_stats;
        }
        
        return synthesis_buffer_;
    }

    std::vector<double> PbpSynthesisEngine::synthesize_from_trajectory(
        const std::vector<Eigen::VectorXd>& trajectory,
        const AudioParameters& world_params,
        SynthesisStats* stats) {
        
        // Merge MLPG trajectory with WORLD parameters
        AudioParameters modified_params = pbp_utils::merge_trajectory_with_world(trajectory, world_params);
        
        // Perform synthesis with modified parameters
        return synthesize(modified_params, stats);
    }

    std::vector<double> PbpSynthesisEngine::synthesize_pulse(
        const PulseParams& pulse_params,
        double /* synthesis_time */) {
        
        if (!engine_initialized_) {
            throw std::runtime_error("PbP synthesis engine not initialized");
        }
        
        // Generate harmonic structure for single pulse
        std::vector<std::complex<double>> harmonics(config_.fft_size / 2 + 1, std::complex<double>(0.0, 0.0));
        generate_harmonics(pulse_params.f0, pulse_params.spectrum, pulse_params.aperiodicity, harmonics);
        
        // Apply spectral envelope and aperiodic mixing
        std::vector<std::complex<double>> filtered_harmonics(config_.fft_size / 2 + 1);
        apply_spectral_envelope(harmonics, pulse_params.spectrum, filtered_harmonics);
        
        std::vector<std::complex<double>> mixed_spectrum(config_.fft_size / 2 + 1);
        mix_aperiodic_component(filtered_harmonics, pulse_params.aperiodicity, mixed_spectrum);
        
        // Generate time-domain pulse
        std::vector<double> pulse_waveform(config_.fft_size);
        inverse_fft_synthesis(mixed_spectrum, pulse_waveform);
        
        // Apply windowing
        std::vector<double> windowed_pulse;
        apply_window_function(pulse_waveform, windowed_pulse);
        
        return windowed_pulse;
    }

    void PbpSynthesisEngine::generate_harmonics(
        double f0,
        const std::vector<double>& spectrum,
        const std::vector<double>& aperiodicity,
        std::vector<std::complex<double>>& harmonics) {
        
        if (f0 <= 0.0) return;
        
        double nyquist = config_.sample_rate / 2.0;
        int max_harmonic = std::min(config_.max_harmonics, static_cast<int>(nyquist / f0));
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> phase_dist(0.0, TWO_PI);
        
        // Generate harmonic components
        for (int h = 1; h <= max_harmonic; ++h) {
            double harmonic_freq = f0 * h;
            if (harmonic_freq >= nyquist) break;
            
            // Map harmonic frequency to spectrum bin
            int bin = static_cast<int>(harmonic_freq * config_.fft_size / config_.sample_rate);
            if (bin >= static_cast<int>(spectrum.size())) continue;
            
            // Get amplitude from spectral envelope
            double amplitude = spectrum[bin];
            if (amplitude < config_.harmonic_amplitude_threshold) continue;
            
            // Apply aperiodicity (reduces harmonic amplitude)
            if (bin < static_cast<int>(aperiodicity.size())) {
                amplitude *= (1.0 - aperiodicity[bin]);
            }
            
            // Generate phase (random or from parameters)
            double phase = config_.enable_phase_randomization ? 
                          phase_dist(gen) : 0.0;
            
            // Store harmonic complex amplitude
            if (bin < static_cast<int>(harmonics.size())) {
                harmonics[bin] = std::complex<double>(
                    amplitude * std::cos(phase),
                    amplitude * std::sin(phase)
                );
            }
        }
    }

    void PbpSynthesisEngine::apply_spectral_envelope(
        const std::vector<std::complex<double>>& harmonics,
        const std::vector<double>& spectrum,
        std::vector<std::complex<double>>& filtered_harmonics) {
        
        filtered_harmonics.resize(harmonics.size());
        
        for (size_t i = 0; i < harmonics.size(); ++i) {
            double envelope_value = (i < spectrum.size()) ? spectrum[i] : 0.0;
            filtered_harmonics[i] = harmonics[i] * envelope_value;
        }
    }

    void PbpSynthesisEngine::mix_aperiodic_component(
        const std::vector<std::complex<double>>& harmonics,
        const std::vector<double>& aperiodicity,
        std::vector<std::complex<double>>& mixed_spectrum) {
        
        mixed_spectrum.resize(harmonics.size());
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> noise_dist(0.0, 1.0);
        
        for (size_t i = 0; i < harmonics.size(); ++i) {
            // Start with harmonic component
            mixed_spectrum[i] = harmonics[i];
            
            // Add aperiodic noise component
            if (i < aperiodicity.size() && aperiodicity[i] > 0.0) {
                double noise_amplitude = aperiodicity[i] * 0.1; // Scale noise
                double noise_phase = noise_dist(gen);
                
                std::complex<double> noise_component(
                    noise_amplitude * std::cos(noise_phase),
                    noise_amplitude * std::sin(noise_phase)
                );
                
                mixed_spectrum[i] += noise_component;
            }
        }
    }

    void PbpSynthesisEngine::inverse_fft_synthesis(
        const std::vector<std::complex<double>>& spectrum,
        std::vector<double>& pulse_waveform) {
        
        if (!fft_manager_) {
            throw std::runtime_error("FFT Transform Manager not initialized");
        }
        
        // Use high-performance FFT for pulse synthesis (replaces O(n²) DFT)
        bool success = fft_manager_->synthesize_pulse_from_spectrum(
            spectrum, 
            pulse_waveform, 
            true  // Apply normalization
        );
        
        if (!success) {
            std::cerr << "Warning: FFT synthesis failed, falling back to DFT" << std::endl;
            
            // Fallback to original DFT implementation if FFT fails
            pulse_waveform.resize(config_.fft_size, 0.0);
            
            for (int n = 0; n < config_.fft_size; ++n) {
                std::complex<double> sum(0.0, 0.0);
                
                for (size_t k = 0; k < spectrum.size() && k < static_cast<size_t>(config_.fft_size / 2 + 1); ++k) {
                    double angle = TWO_PI * k * n / config_.fft_size;
                    std::complex<double> exponential(std::cos(angle), std::sin(angle));
                    sum += spectrum[k] * exponential;
                    
                    // Handle negative frequencies (conjugate symmetry)
                    if (k > 0 && k < static_cast<size_t>(config_.fft_size / 2)) {
                        std::complex<double> conjugate_exp(std::cos(-angle), std::sin(-angle));
                        sum += std::conj(spectrum[k]) * conjugate_exp;
                    }
                }
                
                pulse_waveform[n] = sum.real() / config_.fft_size;
            }
        }
        
        // Ensure output size matches expected FFT size
        if (pulse_waveform.size() != config_.fft_size) {
            pulse_waveform.resize(config_.fft_size, 0.0);
        }
    }

    void PbpSynthesisEngine::apply_window_function(
        const std::vector<double>& pulse,
        std::vector<double>& windowed_pulse) {
        
        int window_size = std::min(static_cast<int>(window_function_.size()), static_cast<int>(pulse.size()));
        windowed_pulse.resize(window_size);
        
        for (int i = 0; i < window_size; ++i) {
            windowed_pulse[i] = pulse[i] * window_function_[i];
        }
    }

    void PbpSynthesisEngine::overlap_add_pulse(
        const std::vector<double>& pulse,
        int pulse_position,
        std::vector<double>& synthesis_buffer) {
        
        for (size_t i = 0; i < pulse.size(); ++i) {
            int buffer_index = pulse_position + i;
            if (buffer_index >= 0 && buffer_index < static_cast<int>(synthesis_buffer.size())) {
                synthesis_buffer[buffer_index] += pulse[i];
            }
        }
    }

    std::vector<double> PbpSynthesisEngine::generate_window(int length, PbpConfig::WindowType type) {
        std::vector<double> window(length);
        
        switch (type) {
            case PbpConfig::WindowType::HANN:
                for (int i = 0; i < length; ++i) {
                    window[i] = 0.5 * (1.0 - std::cos(TWO_PI * i / (length - 1)));
                }
                break;
                
            case PbpConfig::WindowType::HAMMING:
                for (int i = 0; i < length; ++i) {
                    window[i] = 0.54 - 0.46 * std::cos(TWO_PI * i / (length - 1));
                }
                break;
                
            case PbpConfig::WindowType::BLACKMAN:
                for (int i = 0; i < length; ++i) {
                    double factor = TWO_PI * i / (length - 1);
                    window[i] = 0.42 - 0.5 * std::cos(factor) + 0.08 * std::cos(2 * factor);
                }
                break;
                
            case PbpConfig::WindowType::GAUSSIAN:
                {
                    double sigma = length / 6.0; // Standard deviation
                    double center = (length - 1) / 2.0;
                    for (int i = 0; i < length; ++i) {
                        double x = (i - center) / sigma;
                        window[i] = std::exp(-0.5 * x * x);
                    }
                }
                break;
                
            default:
                std::fill(window.begin(), window.end(), 1.0);
        }
        
        return window;
    }

    std::vector<double> PbpSynthesisEngine::calculate_pulse_positions(
        const std::vector<double>& f0_contour,
        double /* frame_period */) {
        
        std::vector<double> positions(f0_contour.size());
        double time_accumulator = 0.0;
        
        for (size_t i = 0; i < f0_contour.size(); ++i) {
            if (f0_contour[i] > 0.0) {
                double period_samples = config_.sample_rate / f0_contour[i];
                positions[i] = time_accumulator;
                time_accumulator += period_samples;
            } else {
                positions[i] = i * config_.hop_size; // Regular frame spacing for unvoiced
            }
        }
        
        return positions;
    }

    void PbpSynthesisEngine::validate_synthesis_parameters(const AudioParameters& parameters) {
        if (parameters.f0.empty() || parameters.spectrum.empty() || parameters.aperiodicity.empty()) {
            throw std::invalid_argument("Empty parameter vectors in AudioParameters");
        }
        
        if (parameters.f0.size() != parameters.spectrum.size() || 
            parameters.f0.size() != parameters.aperiodicity.size()) {
            throw std::invalid_argument("Mismatched parameter vector sizes");
        }
        
        if (parameters.sample_rate != config_.sample_rate) {
            std::cerr << "Warning: Sample rate mismatch between parameters (" 
                      << parameters.sample_rate << ") and config (" 
                      << config_.sample_rate << ")" << std::endl;
        }
    }

    void PbpSynthesisEngine::allocate_synthesis_buffers() {
        // Pre-allocate synthesis buffers
        synthesis_buffer_.reserve(config_.sample_rate * 10); // Reserve for 10 seconds max
        
        // Streaming buffers
        if (config_.buffer_size > 0) {
            overlap_buffer_.resize(config_.buffer_size, 0.0);
        }
    }

    void PbpSynthesisEngine::deallocate_synthesis_buffers() {
        synthesis_buffer_.clear();
        synthesis_buffer_.shrink_to_fit();
        
        overlap_buffer_.clear();
        overlap_buffer_.shrink_to_fit();
        
        fft_buffer_.clear();
        spectrum_buffer_.clear();
    }

    void PbpSynthesisEngine::precompute_synthesis_tables() {
        // Precompute common trigonometric values for FFT optimization
        // This would be implementation-specific for the chosen FFT library
        
        // For now, just placeholder for future optimization
    }

    double PbpSynthesisEngine::calculate_harmonic_energy_ratio() {
        // Calculate the ratio of harmonic to total energy in synthesized signal
        // This is a simplified implementation
        double total_energy = 0.0;
        for (double sample : synthesis_buffer_) {
            total_energy += sample * sample;
        }
        
        // For now, return a placeholder value
        return 0.85; // Typical value for good synthesis
    }

    double PbpSynthesisEngine::calculate_trajectory_smoothness(const std::vector<double>& trajectory) {
        if (trajectory.size() < 3) return 0.0;
        
        double smoothness = 0.0;
        for (size_t i = 1; i < trajectory.size() - 1; ++i) {
            double second_diff = trajectory[i-1] - 2*trajectory[i] + trajectory[i+1];
            smoothness += second_diff * second_diff;
        }
        
        return smoothness / (trajectory.size() - 2);
    }

    // Utility functions implementation
    namespace pbp_utils {
        
        std::vector<PulseParams> world_to_pulse_params(const AudioParameters& world_params) {
            std::vector<PulseParams> pulse_sequence;
            pulse_sequence.reserve(world_params.length);
            
            for (int frame = 0; frame < world_params.length; ++frame) {
                PulseParams params;
                params.f0 = world_params.f0[frame];
                params.spectrum = world_params.spectrum[frame];
                params.aperiodicity = world_params.aperiodicity[frame];
                params.pulse_position = frame * (world_params.sample_rate * world_params.frame_period / 1000.0);
                
                pulse_sequence.push_back(params);
            }
            
            return pulse_sequence;
        }

        AudioParameters merge_trajectory_with_world(
            const std::vector<Eigen::VectorXd>& trajectory,
            const AudioParameters& world_params) {
            
            AudioParameters modified_params = world_params;
            
            // Apply trajectory modifications to spectral parameters
            for (size_t frame = 0; frame < trajectory.size() && frame < world_params.spectrum.size(); ++frame) {
                const auto& traj_frame = trajectory[frame];
                
                // Apply trajectory to spectrum (simplified mapping)
                for (size_t bin = 0; bin < static_cast<size_t>(traj_frame.size()) && bin < modified_params.spectrum[frame].size(); ++bin) {
                    modified_params.spectrum[frame][bin] *= std::exp(traj_frame[bin] * 0.1); // Scale factor
                }
            }
            
            return modified_params;
        }

        int calculate_synthesis_buffer_size(
            double duration_seconds,
            int sample_rate,
            double overlap_factor) {
            
            return static_cast<int>(duration_seconds * sample_rate * overlap_factor);
        }

        SynthesisStats benchmark_synthesis_performance(
            std::function<std::vector<double>()> synthesis_function,
            int iterations) {
            
            SynthesisStats benchmark_stats;
            double total_time = 0.0;
            
            for (int i = 0; i < iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                auto result = synthesis_function();
                auto end = std::chrono::high_resolution_clock::now();
                
                double iteration_time = std::chrono::duration<double, std::milli>(end - start).count();
                total_time += iteration_time;
                
                benchmark_stats.peak_frame_time_ms = std::max(benchmark_stats.peak_frame_time_ms, iteration_time);
            }
            
            benchmark_stats.synthesis_time_ms = total_time;
            benchmark_stats.average_frame_time_ms = total_time / iterations;
            benchmark_stats.synthesis_method = "Benchmark";
            
            return benchmark_stats;
        }
    }

} // namespace synthesis
} // namespace nexussynth
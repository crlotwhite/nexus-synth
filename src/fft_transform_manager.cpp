#include "nexussynth/fft_transform_manager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>

namespace nexussynth {
namespace transforms {

    // Mathematical constants
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 2.0 * PI;

    FftTransformManager::FftTransformManager(const FftConfig& config)
        : config_(config) {
        
        // Initialize Eigen FFT instance
        eigen_fft_ = std::make_unique<Eigen::FFT<double>>();
        
        // Initialize backend
        if (!initialize_backend()) {
            std::cerr << "Warning: Failed to initialize FFT backend, falling back to default" << std::endl;
            config_.backend = FftBackend::EIGEN_DEFAULT;
            initialize_backend();
        }
        
        // Initialize statistics
        reset_stats();
        
        std::cout << "FFT Transform Manager initialized with backend: " 
                  << fft_utils::backend_to_string(config_.backend) << std::endl;
    }

    FftTransformManager::~FftTransformManager() {
        cleanup_backend();
    }

    bool FftTransformManager::forward_fft(
        const std::vector<double>& input,
        std::vector<std::complex<double>>& output) {
        
        if (input.empty()) return false;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t fft_size = input.size();
        std::string cache_key = generate_cache_key(fft_size, true, true);
        
        std::shared_ptr<FftPlan> plan;
        bool cache_hit = false;
        
        if (config_.enable_plan_caching) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = plan_cache_.find(cache_key);
            if (it != plan_cache_.end()) {
                plan = it->second;
                plan->usage_count++;
                plan->last_used_time = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();
                cache_hit = true;
            }
        }
        
        if (!plan) {
            plan = get_or_create_plan(fft_size, true, true);
        }
        
        // Perform forward real FFT using Eigen
        Eigen::VectorXd eigen_input = Eigen::Map<const Eigen::VectorXd>(input.data(), input.size());
        Eigen::VectorXcd eigen_output;
        
        try {
            eigen_fft_->fwd(eigen_output, eigen_input);
            
            // Convert to std::vector format
            output.resize(eigen_output.size());
            for (int i = 0; i < eigen_output.size(); ++i) {
                output[i] = std::complex<double>(eigen_output[i].real(), eigen_output[i].imag());
            }
        } catch (const std::exception& e) {
            std::cerr << "FFT Error: " << e.what() << std::endl;
            return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double transform_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        update_stats(transform_time, cache_hit);
        
        return true;
    }

    bool FftTransformManager::forward_fft(
        const std::vector<std::complex<double>>& input,
        std::vector<std::complex<double>>& output) {
        
        if (input.empty()) return false;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t fft_size = input.size();
        std::string cache_key = generate_cache_key(fft_size, true, false);
        
        std::shared_ptr<FftPlan> plan;
        bool cache_hit = false;
        
        if (config_.enable_plan_caching) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = plan_cache_.find(cache_key);
            if (it != plan_cache_.end()) {
                plan = it->second;
                plan->usage_count++;
                plan->last_used_time = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();
                cache_hit = true;
            }
        }
        
        if (!plan) {
            plan = get_or_create_plan(fft_size, true, false);
        }
        
        // Perform forward complex FFT using Eigen
        Eigen::VectorXcd eigen_input(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            eigen_input[i] = std::complex<double>(input[i].real(), input[i].imag());
        }
        
        Eigen::VectorXcd eigen_output;
        
        try {
            eigen_fft_->fwd(eigen_output, eigen_input);
            
            // Convert to std::vector format
            output.resize(eigen_output.size());
            for (int i = 0; i < eigen_output.size(); ++i) {
                output[i] = std::complex<double>(eigen_output[i].real(), eigen_output[i].imag());
            }
        } catch (const std::exception& e) {
            std::cerr << "FFT Error: " << e.what() << std::endl;
            return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double transform_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        update_stats(transform_time, cache_hit);
        
        return true;
    }

    bool FftTransformManager::inverse_fft(
        const std::vector<std::complex<double>>& input,
        std::vector<double>& output) {
        
        if (input.empty()) return false;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t fft_size = (input.size() - 1) * 2;  // Reconstruct original size from half-spectrum
        std::string cache_key = generate_cache_key(fft_size, false, true);
        
        std::shared_ptr<FftPlan> plan;
        bool cache_hit = false;
        
        if (config_.enable_plan_caching) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = plan_cache_.find(cache_key);
            if (it != plan_cache_.end()) {
                plan = it->second;
                plan->usage_count++;
                plan->last_used_time = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();
                cache_hit = true;
            }
        }
        
        if (!plan) {
            plan = get_or_create_plan(fft_size, false, true);
        }
        
        // Prepare full spectrum with conjugate symmetry for real IFFT
        Eigen::VectorXcd eigen_input(fft_size);
        
        // Copy positive frequencies
        for (size_t i = 0; i < input.size(); ++i) {
            eigen_input[i] = std::complex<double>(input[i].real(), input[i].imag());
        }
        
        // Apply conjugate symmetry for negative frequencies
        for (size_t i = input.size(); i < fft_size; ++i) {
            size_t conjugate_index = fft_size - i;
            if (conjugate_index < input.size()) {
                eigen_input[i] = std::conj(eigen_input[conjugate_index]);
            } else {
                eigen_input[i] = std::complex<double>(0.0, 0.0);
            }
        }
        
        // Ensure DC and Nyquist components are real
        eigen_input[0] = std::complex<double>(eigen_input[0].real(), 0.0);
        if (input.size() > 1 && fft_size % 2 == 0) {
            eigen_input[fft_size / 2] = std::complex<double>(eigen_input[fft_size / 2].real(), 0.0);
        }
        
        Eigen::VectorXd eigen_output;
        
        try {
            eigen_fft_->inv(eigen_output, eigen_input);
            
            // Convert to std::vector format
            output.resize(eigen_output.size());
            for (int i = 0; i < eigen_output.size(); ++i) {
                output[i] = eigen_output[i];
            }
            
            // Normalize if needed
            normalize_fft_output(output, fft_size);
            
        } catch (const std::exception& e) {
            std::cerr << "IFFT Error: " << e.what() << std::endl;
            return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double transform_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        update_stats(transform_time, cache_hit);
        
        return true;
    }

    bool FftTransformManager::inverse_fft(
        const std::vector<std::complex<double>>& input,
        std::vector<std::complex<double>>& output) {
        
        if (input.empty()) return false;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t fft_size = input.size();
        std::string cache_key = generate_cache_key(fft_size, false, false);
        
        std::shared_ptr<FftPlan> plan;
        bool cache_hit = false;
        
        if (config_.enable_plan_caching) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = plan_cache_.find(cache_key);
            if (it != plan_cache_.end()) {
                plan = it->second;
                plan->usage_count++;
                plan->last_used_time = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();
                cache_hit = true;
            }
        }
        
        if (!plan) {
            plan = get_or_create_plan(fft_size, false, false);
        }
        
        // Perform inverse complex FFT using Eigen
        Eigen::VectorXcd eigen_input(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            eigen_input[i] = std::complex<double>(input[i].real(), input[i].imag());
        }
        
        Eigen::VectorXcd eigen_output;
        
        try {
            eigen_fft_->inv(eigen_output, eigen_input);
            
            // Convert to std::vector format
            output.resize(eigen_output.size());
            for (int i = 0; i < eigen_output.size(); ++i) {
                output[i] = std::complex<double>(eigen_output[i].real(), eigen_output[i].imag());
            }
            
            // Normalize if needed
            normalize_fft_output(output, fft_size);
            
        } catch (const std::exception& e) {
            std::cerr << "IFFT Error: " << e.what() << std::endl;
            return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double transform_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        update_stats(transform_time, cache_hit);
        
        return true;
    }

    bool FftTransformManager::synthesize_pulse_from_spectrum(
        const std::vector<std::complex<double>>& spectrum,
        std::vector<double>& pulse_waveform,
        bool normalize) {
        
        if (spectrum.empty()) return false;
        
        // This is the key function that replaces the O(n²) DFT in PbP synthesis
        bool success = inverse_fft(spectrum, pulse_waveform);
        
        if (success && normalize && !pulse_waveform.empty()) {
            // Apply additional normalization for pulse synthesis
            double max_amplitude = *std::max_element(
                pulse_waveform.begin(), pulse_waveform.end(),
                [](double a, double b) { return std::abs(a) < std::abs(b); }
            );
            
            if (max_amplitude > 1e-10) {
                double scale_factor = 1.0 / max_amplitude;
                for (double& sample : pulse_waveform) {
                    sample *= scale_factor;
                }
            }
        }
        
        return success;
    }

    bool FftTransformManager::analyze_pulse_to_spectrum(
        const std::vector<double>& pulse_waveform,
        std::vector<std::complex<double>>& spectrum) {
        
        if (pulse_waveform.empty()) return false;
        
        return forward_fft(pulse_waveform, spectrum);
    }

    bool FftTransformManager::set_config(const FftConfig& config) {
        // Validate new configuration
        if (!is_backend_available(config.backend)) {
            std::cerr << "Error: Backend " << fft_utils::backend_to_string(config.backend) 
                      << " is not available" << std::endl;
            return false;
        }
        
        // Clean up current backend if changing
        if (config.backend != config_.backend) {
            cleanup_backend();
            config_ = config;
            return initialize_backend();
        } else {
            config_ = config;
            return true;
        }
    }

    FftStats FftTransformManager::get_stats() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        FftStats current_stats = stats_;
        
        // Update cache statistics
        if (stats_.transforms_performed > 0) {
            current_stats.cache_hit_ratio = 
                static_cast<double>(stats_.cache_hits) / stats_.transforms_performed;
        }
        
        // Update backend information
        current_stats.backend_name = fft_utils::backend_to_string(config_.backend);
        current_stats.multithreading_active = config_.enable_multithreading;
        
        return current_stats;
    }

    void FftTransformManager::reset_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = FftStats{};
    }

    std::vector<std::pair<FftBackend, bool>> FftTransformManager::get_available_backends() const {
        return {
            {FftBackend::EIGEN_DEFAULT, true},
            {FftBackend::KISS_FFT, true},
            {FftBackend::FFTW, is_backend_available(FftBackend::FFTW)},
            {FftBackend::MKL, is_backend_available(FftBackend::MKL)},
            {FftBackend::POCKET_FFT, is_backend_available(FftBackend::POCKET_FFT)}
        };
    }

    bool FftTransformManager::is_backend_available(FftBackend backend) const {
        switch (backend) {
            case FftBackend::EIGEN_DEFAULT:
            case FftBackend::KISS_FFT:
                return true;  // Always available with Eigen
                
            case FftBackend::FFTW:
                // Check if FFTW is available at compile time
                #ifdef EIGEN_FFTW_DEFAULT
                return true;
                #else
                return false;
                #endif
                
            case FftBackend::MKL:
                // Check if MKL is available at compile time
                #ifdef EIGEN_USE_MKL_ALL
                return true;
                #else
                return false;
                #endif
                
            case FftBackend::POCKET_FFT:
                // PocketFFT availability check
                return false; // Not yet implemented
                
            default:
                return false;
        }
    }

    size_t FftTransformManager::precompute_plans(const std::vector<size_t>& sizes) {
        size_t cached_count = 0;
        
        for (size_t size : sizes) {
            if (is_valid_fft_size(size)) {
                // Pre-create plans for common operations
                auto forward_real = get_or_create_plan(size, true, true);
                auto inverse_real = get_or_create_plan(size, false, true);
                
                if (forward_real && inverse_real) {
                    cached_count += 2;
                }
            }
        }
        
        std::cout << "Pre-computed " << cached_count << " FFT plans for common sizes" << std::endl;
        return cached_count;
    }

    void FftTransformManager::clear_plan_cache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        plan_cache_.clear();
        std::cout << "FFT plan cache cleared" << std::endl;
    }

    std::pair<size_t, size_t> FftTransformManager::get_cache_usage() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return {plan_cache_.size(), static_cast<size_t>(config_.max_cache_size)};
    }

    // Private helper function implementations

    std::string FftTransformManager::generate_cache_key(size_t size, bool forward, bool real_input) const {
        std::ostringstream oss;
        oss << size << "_" << (forward ? "fwd" : "inv") << "_" << (real_input ? "real" : "cplx");
        return oss.str();
    }

    std::shared_ptr<FftPlan> FftTransformManager::get_or_create_plan(
        size_t size, bool forward, bool real_input) {
        
        std::string cache_key = generate_cache_key(size, forward, real_input);
        
        if (config_.enable_plan_caching) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            
            auto it = plan_cache_.find(cache_key);
            if (it != plan_cache_.end()) {
                return it->second;
            }
            
            // Check if cache is full
            if (plan_cache_.size() >= static_cast<size_t>(config_.max_cache_size)) {
                cleanup_cache();
            }
            
            // Create new plan
            auto plan = std::make_shared<FftPlan>(size, forward, real_input, config_.backend);
            plan_cache_[cache_key] = plan;
            
            return plan;
        } else {
            // Create temporary plan without caching
            return std::make_shared<FftPlan>(size, forward, real_input, config_.backend);
        }
    }

    bool FftTransformManager::initialize_backend() {
        // Eigen FFT is initialized in constructor, no additional setup needed
        // for basic functionality. Backend-specific optimizations could be added here.
        
        switch (config_.backend) {
            case FftBackend::FFTW:
                #ifdef EIGEN_FFTW_DEFAULT
                // Configure FFTW-specific options
                if (config_.enable_multithreading) {
                    // FFTW multithreading setup would go here
                }
                #endif
                break;
                
            case FftBackend::MKL:
                #ifdef EIGEN_USE_MKL_ALL
                // MKL-specific configuration would go here
                #endif
                break;
                
            default:
                // Default Eigen/KissFFT backend needs no special setup
                break;
        }
        
        return true;
    }

    void FftTransformManager::cleanup_backend() {
        // Cleanup any backend-specific resources
        switch (config_.backend) {
            case FftBackend::FFTW:
                // FFTW cleanup would go here
                break;
                
            default:
                // No special cleanup needed for default backends
                break;
        }
    }

    void FftTransformManager::cleanup_cache() {
        if (plan_cache_.empty()) return;
        
        // Remove least recently used plans
        size_t target_size = static_cast<size_t>(config_.max_cache_size * config_.cache_cleanup_threshold);
        
        if (plan_cache_.size() <= target_size) return;
        
        // Find plans to remove (LRU strategy)
        std::vector<std::pair<double, std::string>> usage_times;
        for (const auto& pair : plan_cache_) {
            usage_times.emplace_back(pair.second->last_used_time, pair.first);
        }
        
        std::sort(usage_times.begin(), usage_times.end());
        
        size_t plans_to_remove = plan_cache_.size() - target_size;
        for (size_t i = 0; i < plans_to_remove && i < usage_times.size(); ++i) {
            plan_cache_.erase(usage_times[i].second);
        }
    }

    void FftTransformManager::update_stats(double transform_time, bool cache_hit) const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        stats_.transforms_performed++;
        stats_.total_transform_time_ms += transform_time;
        
        if (cache_hit) {
            stats_.cache_hits++;
        } else {
            stats_.cache_misses++;
        }
        
        // Update peak memory usage (simplified calculation)
        size_t current_memory = plan_cache_.size() * sizeof(FftPlan) + 
                               (stats_.transforms_performed * 16);  // Rough estimate
        stats_.memory_usage_bytes = current_memory;
        stats_.peak_memory_mb = std::max(stats_.peak_memory_mb, 
                                        static_cast<double>(current_memory) / (1024.0 * 1024.0));
    }

    bool FftTransformManager::is_valid_fft_size(size_t size) const {
        return size > 0 && size <= (1024 * 1024);  // Reasonable size limits
    }

    size_t FftTransformManager::get_optimal_fft_size(size_t desired_size) const {
        return fft_utils::next_power_of_2(desired_size);
    }

    void FftTransformManager::normalize_fft_output(std::vector<double>& output, size_t fft_size) const {
        // Eigen's IFFT includes the 1/N normalization, but we might want additional scaling
        // For now, no additional normalization beyond what Eigen provides
    }

    void FftTransformManager::normalize_fft_output(std::vector<std::complex<double>>& output, size_t fft_size) const {
        // Eigen's IFFT includes the 1/N normalization, but we might want additional scaling
        // For now, no additional normalization beyond what Eigen provides
    }

    // FFT utility functions implementation
    namespace fft_utils {
        
        bool is_power_of_2(size_t n) {
            return n > 0 && (n & (n - 1)) == 0;
        }
        
        size_t next_power_of_2(size_t n) {
            if (n <= 1) return 1;
            size_t power = 1;
            while (power < n) {
                power <<= 1;
            }
            return power;
        }
        
        size_t next_composite_size(size_t n) {
            // Find next highly composite number (good for FFT performance)
            // This is a simplified version - could be enhanced with more factors
            static const std::vector<size_t> good_sizes = {
                1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30, 32,
                36, 40, 45, 48, 50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120,
                125, 128, 135, 144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243,
                250, 256, 270, 288, 300, 320, 324, 360, 375, 384, 400, 405, 432, 450,
                480, 486, 500, 512, 540, 576, 600, 625, 640, 648, 675, 720, 729, 750,
                768, 800, 810, 864, 900, 960, 972, 1000, 1024
            };
            
            auto it = std::lower_bound(good_sizes.begin(), good_sizes.end(), n);
            if (it != good_sizes.end()) {
                return *it;
            } else {
                return next_power_of_2(n);  // Fallback to power of 2
            }
        }
        
        std::string backend_to_string(FftBackend backend) {
            switch (backend) {
                case FftBackend::EIGEN_DEFAULT: return "Eigen Default";
                case FftBackend::KISS_FFT: return "KissFFT";
                case FftBackend::FFTW: return "FFTW";
                case FftBackend::MKL: return "Intel MKL";
                case FftBackend::POCKET_FFT: return "PocketFFT";
                default: return "Unknown";
            }
        }
        
        FftBackend backend_from_string(const std::string& name) {
            if (name == "Eigen Default") return FftBackend::EIGEN_DEFAULT;
            if (name == "KissFFT") return FftBackend::KISS_FFT;
            if (name == "FFTW") return FftBackend::FFTW;
            if (name == "Intel MKL") return FftBackend::MKL;
            if (name == "PocketFFT") return FftBackend::POCKET_FFT;
            return FftBackend::EIGEN_DEFAULT;
        }
        
        size_t calculate_fft_memory_requirement(size_t fft_size, bool complex_input) {
            // Rough estimate of memory requirements
            size_t input_size = complex_input ? fft_size * 16 : fft_size * 8;  // Complex vs real
            size_t output_size = fft_size * 16;  // Always complex output
            size_t workspace_size = fft_size * 16;  // FFT workspace
            
            return input_size + output_size + workspace_size;
        }
        
        std::vector<double> zero_pad_to_fft_size(
            const std::vector<double>& input, 
            size_t target_size) {
            
            std::vector<double> padded(target_size, 0.0);
            size_t copy_size = std::min(input.size(), target_size);
            std::copy(input.begin(), input.begin() + copy_size, padded.begin());
            return padded;
        }
        
        std::vector<std::complex<double>> zero_pad_to_fft_size(
            const std::vector<std::complex<double>>& input, 
            size_t target_size) {
            
            std::vector<std::complex<double>> padded(target_size, std::complex<double>(0.0, 0.0));
            size_t copy_size = std::min(input.size(), target_size);
            std::copy(input.begin(), input.begin() + copy_size, padded.begin());
            return padded;
        }
    }

} // namespace transforms
} // namespace nexussynth
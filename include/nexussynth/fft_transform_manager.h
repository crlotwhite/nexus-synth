#pragma once

#include <vector>
#include <complex>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <Eigen/Core>
#include <unsupported/Eigen/FFT>

namespace nexussynth {
namespace transforms {

    /**
     * @brief FFT backend types supported by the transform manager
     */
    enum class FftBackend {
        EIGEN_DEFAULT,     // Eigen's default FFT (usually KissFFT)
        KISS_FFT,         // KissFFT backend (header-only, no external deps)
        FFTW,             // FFTW backend (high performance, external dependency)
        MKL,              // Intel MKL backend (Intel optimized)
        POCKET_FFT        // PocketFFT backend (modern, header-only)
    };

    /**
     * @brief FFT configuration parameters for optimization
     */
    struct FftConfig {
        FftBackend backend = FftBackend::EIGEN_DEFAULT;
        bool enable_plan_caching = true;       // Cache FFT plans for repeated sizes
        bool enable_multithreading = false;    // Use multi-threaded FFT (backend dependent)
        int max_cache_size = 32;              // Maximum number of cached plans
        bool prefer_real_fft = true;          // Use real FFT for real-only inputs
        double cache_cleanup_threshold = 0.8; // Cleanup cache when usage exceeds this
        
        // Performance tuning
        bool enable_simd_optimization = true;  // Enable SIMD instructions
        int thread_count = 0;                  // 0 = auto-detect, >0 = explicit count
        bool use_wisdom_file = false;          // Use FFTW wisdom file (FFTW only)
        std::string wisdom_file_path;          // Path to wisdom file
    };

    /**
     * @brief FFT performance statistics and metrics
     */
    struct FftStats {
        double forward_fft_time_ms = 0.0;      // Time for forward FFT
        double inverse_fft_time_ms = 0.0;      // Time for inverse FFT
        double total_transform_time_ms = 0.0;  // Total transformation time
        
        size_t transforms_performed = 0;       // Number of transforms
        size_t cache_hits = 0;                // Number of cache hits
        size_t cache_misses = 0;              // Number of cache misses
        double cache_hit_ratio = 0.0;         // Cache efficiency ratio
        
        size_t memory_usage_bytes = 0;        // Memory used by plans/workspace
        double peak_memory_mb = 0.0;          // Peak memory usage
        
        std::string backend_name;              // Active backend name
        bool multithreading_active = false;   // Whether MT is active
    };

    /**
     * @brief Cached FFT plan for reuse optimization
     */
    struct FftPlan {
        size_t fft_size;
        bool is_forward;
        bool is_real_input;
        FftBackend backend;
        std::shared_ptr<void> plan_data;       // Backend-specific plan data
        size_t usage_count = 0;
        double last_used_time = 0.0;
        
        FftPlan(size_t size, bool forward, bool real, FftBackend b) 
            : fft_size(size), is_forward(forward), is_real_input(real), backend(b) {}
    };

    /**
     * @brief High-performance FFT Transform Manager
     * 
     * Provides optimized FFT/IFFT operations with plan caching, multi-backend support,
     * and performance monitoring. Designed for real-time audio synthesis applications.
     */
    class FftTransformManager {
    public:
        explicit FftTransformManager(const FftConfig& config = FftConfig{});
        ~FftTransformManager();

        // Core FFT operations
        /**
         * @brief Forward FFT: time domain -> frequency domain
         * 
         * @param input Real-valued time domain input
         * @param output Complex frequency domain output (half spectrum + DC/Nyquist)
         * @return true if transform successful
         */
        bool forward_fft(
            const std::vector<double>& input,
            std::vector<std::complex<double>>& output
        );

        /**
         * @brief Forward FFT with complex input
         * 
         * @param input Complex time domain input
         * @param output Complex frequency domain output (full spectrum)
         * @return true if transform successful
         */
        bool forward_fft(
            const std::vector<std::complex<double>>& input,
            std::vector<std::complex<double>>& output
        );

        /**
         * @brief Inverse FFT: frequency domain -> time domain
         * 
         * @param input Complex frequency domain input (half spectrum for real output)
         * @param output Real-valued time domain output
         * @return true if transform successful
         */
        bool inverse_fft(
            const std::vector<std::complex<double>>& input,
            std::vector<double>& output
        );

        /**
         * @brief Inverse FFT with complex output
         * 
         * @param input Complex frequency domain input
         * @param output Complex time domain output
         * @return true if transform successful
         */
        bool inverse_fft(
            const std::vector<std::complex<double>>& input,
            std::vector<std::complex<double>>& output
        );

        // Convenience functions for pulse-by-pulse synthesis
        /**
         * @brief Synthesize time-domain pulse from frequency spectrum
         * 
         * Optimized for PbP synthesis: takes half-spectrum complex input,
         * applies conjugate symmetry, and outputs real time-domain pulse
         * 
         * @param spectrum Half-spectrum frequency domain data
         * @param pulse_waveform Output time domain pulse
         * @param normalize Apply normalization (default: true)
         * @return true if synthesis successful
         */
        bool synthesize_pulse_from_spectrum(
            const std::vector<std::complex<double>>& spectrum,
            std::vector<double>& pulse_waveform,
            bool normalize = true
        );

        /**
         * @brief Analyze pulse to extract frequency spectrum
         * 
         * @param pulse_waveform Input time domain pulse
         * @param spectrum Output half-spectrum frequency domain data
         * @return true if analysis successful
         */
        bool analyze_pulse_to_spectrum(
            const std::vector<double>& pulse_waveform,
            std::vector<std::complex<double>>& spectrum
        );

        // Performance and configuration
        /**
         * @brief Set FFT configuration
         * 
         * @param config New configuration parameters
         * @return true if configuration applied successfully
         */
        bool set_config(const FftConfig& config);

        /**
         * @brief Get current configuration
         */
        const FftConfig& get_config() const { return config_; }

        /**
         * @brief Get performance statistics
         */
        FftStats get_stats() const;

        /**
         * @brief Reset performance statistics
         */
        void reset_stats();

        /**
         * @brief Get information about available backends
         * 
         * @return Vector of backend names and availability
         */
        std::vector<std::pair<FftBackend, bool>> get_available_backends() const;

        /**
         * @brief Validate backend is available and functional
         * 
         * @param backend Backend to test
         * @return true if backend is available
         */
        bool is_backend_available(FftBackend backend) const;

        // Cache management
        /**
         * @brief Precompute and cache FFT plans for common sizes
         * 
         * @param sizes Vector of FFT sizes to pre-cache
         * @return Number of plans successfully cached
         */
        size_t precompute_plans(const std::vector<size_t>& sizes);

        /**
         * @brief Clear all cached FFT plans
         */
        void clear_plan_cache();

        /**
         * @brief Get cache usage information
         * 
         * @return Pair of (used_slots, total_slots)
         */
        std::pair<size_t, size_t> get_cache_usage() const;

        // Advanced features
        /**
         * @brief Benchmark different backends for given FFT sizes
         * 
         * @param test_sizes FFT sizes to benchmark
         * @param iterations Number of iterations per test
         * @return Map of backend -> performance metrics
         */
        std::map<FftBackend, double> benchmark_backends(
            const std::vector<size_t>& test_sizes,
            int iterations = 100
        );

        /**
         * @brief Auto-select optimal backend based on performance
         * 
         * @param test_sizes Representative FFT sizes for optimization
         * @return Best performing backend
         */
        FftBackend auto_select_backend(const std::vector<size_t>& test_sizes);

        /**
         * @brief Enable/disable multi-threading (if supported by backend)
         * 
         * @param enable Enable multithreading
         * @param thread_count Number of threads (0 = auto)
         * @return true if multithreading configured successfully
         */
        bool configure_multithreading(bool enable, int thread_count = 0);

    private:
        FftConfig config_;
        mutable std::mutex cache_mutex_;
        mutable std::mutex stats_mutex_;
        
        // FFT plan cache
        std::unordered_map<std::string, std::shared_ptr<FftPlan>> plan_cache_;
        
        // Eigen FFT instance (thread-safe)
        std::unique_ptr<Eigen::FFT<double>> eigen_fft_;
        
        // Performance tracking
        mutable FftStats stats_;
        
        // Backend-specific state
        void* backend_state_ = nullptr;  // Opaque pointer to backend-specific data
        
        // Internal helper functions
        /**
         * @brief Generate cache key for FFT plan
         */
        std::string generate_cache_key(size_t size, bool forward, bool real_input) const;
        
        /**
         * @brief Get or create cached FFT plan
         */
        std::shared_ptr<FftPlan> get_or_create_plan(
            size_t size, bool forward, bool real_input
        );
        
        /**
         * @brief Initialize selected backend
         */
        bool initialize_backend();
        
        /**
         * @brief Cleanup backend resources
         */
        void cleanup_backend();
        
        /**
         * @brief Perform cache cleanup when full
         */
        void cleanup_cache();
        
        /**
         * @brief Update performance statistics
         */
        void update_stats(double transform_time, bool cache_hit) const;
        
        /**
         * @brief Backend-specific FFT implementation
         */
        bool perform_fft_backend(
            const void* input, 
            void* output, 
            size_t size, 
            bool forward, 
            bool real_input,
            std::shared_ptr<FftPlan> plan
        );
        
        /**
         * @brief Validate FFT size is supported
         */
        bool is_valid_fft_size(size_t size) const;
        
        /**
         * @brief Get optimal FFT size (next power of 2 or composite)
         */
        size_t get_optimal_fft_size(size_t desired_size) const;
        
        /**
         * @brief Apply conjugate symmetry for real IFFT
         */
        void apply_conjugate_symmetry(std::vector<std::complex<double>>& spectrum) const;
        
        /**
         * @brief Normalize FFT output
         */
        void normalize_fft_output(std::vector<double>& output, size_t fft_size) const;
        void normalize_fft_output(std::vector<std::complex<double>>& output, size_t fft_size) const;
    };

    /**
     * @brief FFT utility functions for audio processing
     */
    namespace fft_utils {
        /**
         * @brief Check if size is a power of 2 (optimal for many FFT algorithms)
         */
        bool is_power_of_2(size_t n);
        
        /**
         * @brief Get next power of 2 >= n
         */
        size_t next_power_of_2(size_t n);
        
        /**
         * @brief Get next highly composite number >= n (good for FFT)
         */
        size_t next_composite_size(size_t n);
        
        /**
         * @brief Estimate FFT performance for given size and backend
         */
        double estimate_fft_performance(size_t size, FftBackend backend);
        
        /**
         * @brief Convert backend enum to string
         */
        std::string backend_to_string(FftBackend backend);
        
        /**
         * @brief Parse backend from string
         */
        FftBackend backend_from_string(const std::string& name);
        
        /**
         * @brief Calculate memory requirements for FFT of given size
         */
        size_t calculate_fft_memory_requirement(size_t fft_size, bool complex_input = false);
        
        /**
         * @brief Zero-pad signal to optimal FFT size
         */
        std::vector<double> zero_pad_to_fft_size(
            const std::vector<double>& input, 
            size_t target_size
        );
        
        /**
         * @brief Zero-pad complex signal to optimal FFT size
         */
        std::vector<std::complex<double>> zero_pad_to_fft_size(
            const std::vector<std::complex<double>>& input, 
            size_t target_size
        );
    }

} // namespace transforms
} // namespace nexussynth
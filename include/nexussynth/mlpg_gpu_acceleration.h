#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <Eigen/Core>
#include <Eigen/Sparse>

namespace nexussynth {
namespace mlpg {
namespace gpu {

    /**
     * @brief GPU acceleration capabilities detection and management
     */
    struct GpuCapabilities {
        bool cuda_available = false;           // CUDA support detected
        bool opencl_available = false;         // OpenCL support detected
        bool metal_available = false;          // Metal support (macOS)
        
        // Device information
        int cuda_devices = 0;                  // Number of CUDA devices
        int opencl_devices = 0;                // Number of OpenCL devices
        
        std::vector<std::string> device_names; // GPU device names
        std::vector<size_t> device_memory;     // Memory per device (bytes)
        
        // Compute capabilities
        bool supports_double_precision = false;
        bool supports_sparse_matrices = false;
        int max_threads_per_block = 0;
        int max_blocks_per_grid = 0;
        
        // Performance thresholds
        size_t min_matrix_size_for_gpu = 10000;     // Minimum matrix size for GPU benefit
        size_t max_matrix_size_for_gpu = 1000000;   // Maximum practical matrix size
    };

    /**
     * @brief GPU acceleration benchmark results
     */
    struct BenchmarkResults {
        double cpu_time_ms = 0.0;              // CPU execution time
        double gpu_time_ms = 0.0;              // GPU execution time (including transfers)
        double gpu_compute_time_ms = 0.0;      // Pure GPU compute time
        double memory_transfer_time_ms = 0.0;  // Memory transfer overhead
        
        double speedup_factor = 0.0;           // GPU speedup vs CPU
        bool gpu_faster = false;               // Whether GPU was actually faster
        
        size_t matrix_size = 0;                // Problem size tested
        std::string gpu_backend;               // GPU backend used
        std::string error_message;             // Error description if benchmark failed
    };

    /**
     * @brief MLPG GPU acceleration interface
     * 
     * Provides GPU-accelerated implementations of MLPG operations
     * with automatic fallback to CPU implementations
     */
    class MlpgGpuAccelerator {
    public:
        /**
         * @brief GPU acceleration configuration
         */
        struct Config {
            bool auto_select_backend = true;       // Automatically choose best backend
            bool prefer_cuda = true;                // Prefer CUDA over OpenCL if both available
            bool enable_mixed_precision = false;   // Use FP16 where possible
            
            size_t memory_limit_mb = 4096;         // GPU memory limit (MB)
            double cpu_gpu_threshold = 5000;       // Matrix size threshold for GPU usage
            
            bool enable_benchmarking = true;       // Run benchmarks to optimize threshold
            int benchmark_iterations = 10;         // Number of benchmark runs
        };

        explicit MlpgGpuAccelerator(const Config& config = Config{});
        ~MlpgGpuAccelerator();

        // Capability detection
        /**
         * @brief Detect available GPU capabilities
         * @return Detected GPU capabilities
         */
        GpuCapabilities detect_gpu_capabilities();

        /**
         * @brief Check if GPU acceleration is available and beneficial
         * @param matrix_size Size of the problem
         * @return True if GPU should be used
         */
        bool should_use_gpu(size_t matrix_size) const;

        // Configuration management
        void set_config(const Config& config) { config_ = config; }
        const Config& get_config() const { return config_; }
        const GpuCapabilities& get_capabilities() const { return capabilities_; }

    private:
        Config config_;
        GpuCapabilities capabilities_;
        
        // Performance tracking
        mutable std::vector<BenchmarkResults> performance_history_;
    };

    /**
     * @brief GPU memory management utilities
     */
    namespace gpu_memory {
        
        /**
         * @brief Estimate GPU memory requirement for MLPG operation
         */
        size_t estimate_gpu_memory_requirement(int total_frames, int feature_dim,
                                              bool use_delta, bool use_delta_delta);
        
    } // namespace gpu_memory

} // namespace gpu
} // namespace mlpg
} // namespace nexussynth
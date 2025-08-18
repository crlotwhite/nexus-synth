#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cmath>

namespace nexussynth {

    /**
     * @brief Advanced window function types for different synthesis scenarios
     */
    enum class OptimalWindowType {
        HANN,               // Standard Hann (good general purpose)
        HAMMING,            // Hamming (better spectral resolution)
        BLACKMAN,           // Blackman (low side lobes, wide main lobe)
        BLACKMAN_HARRIS,    // Blackman-Harris (very low side lobes)
        GAUSSIAN,           // Gaussian (smooth, good for tonal content)
        TUKEY,              // Tukey/tapered cosine (adjustable tapering)
        KAISER,             // Kaiser (adjustable trade-off between main lobe and side lobes)
        NUTTALL,            // Nuttall (excellent spectral characteristics)
        ADAPTIVE_HARMONIC,  // Harmonic-content adaptive window
        ADAPTIVE_TRANSIENT, // Transient-content adaptive window
        OPTIMAL_SYNTHESIS   // Synthesis-optimized window
    };

    /**
     * @brief Window function characteristics and quality metrics
     */
    struct WindowCharacteristics {
        double main_lobe_width;         // Main lobe width in bins
        double peak_side_lobe_db;       // Peak side lobe level in dB
        double side_lobe_roll_off_db;   // Side lobe roll-off rate
        double coherent_gain;           // Coherent processing gain
        double equivalent_noise_bw;     // Equivalent noise bandwidth
        double scalloping_loss_db;      // Scalloping loss in dB
        std::string description;        // Human-readable description
    };

    /**
     * @brief Content analysis parameters for adaptive windowing
     */
    struct ContentAnalysis {
        double pitch_frequency;         // Fundamental frequency in Hz
        double spectral_centroid;       // Spectral centroid in Hz
        double harmonic_ratio;          // Ratio of harmonic to noise content (0-1)
        double transient_factor;        // Transient content factor (0-1)
        double spectral_flux;           // Spectral change rate
        std::vector<double> formant_frequencies;  // Formant locations
        double dynamic_range_db;        // Dynamic range of content
    };

    /**
     * @brief Window optimization parameters
     */
    struct WindowOptimizationParams {
        double sample_rate = 44100.0;
        int fft_size = 1024;
        double hop_factor = 0.25;       // Hop size as factor of window length
        double transition_bandwidth = 0.1;  // Transition band as factor of sample rate
        double side_lobe_suppression_db = -60.0;  // Target side lobe suppression
        bool minimize_pre_echo = true;
        bool optimize_for_overlap_add = true;
        double overlap_factor = 0.75;   // Overlap factor for OLA
    };

    /**
     * @brief Advanced Window Function Optimizer
     * 
     * Provides adaptive window selection and generation optimized for 
     * pulse-by-pulse synthesis scenarios with pre-echo suppression
     * and spectral leakage minimization.
     */
    class WindowOptimizer {
    public:
        WindowOptimizer();
        ~WindowOptimizer();

        /**
         * @brief Generate optimal window for given content characteristics
         * @param length Window length in samples
         * @param content_analysis Analysis of audio content
         * @param params Optimization parameters
         * @return Optimal window coefficients
         */
        std::vector<double> generate_optimal_window(
            int length,
            const ContentAnalysis& content_analysis,
            const WindowOptimizationParams& params = WindowOptimizationParams{}
        );

        /**
         * @brief Generate specific window type with optimized parameters
         * @param type Window type
         * @param length Window length in samples
         * @param params Optimization parameters
         * @return Window coefficients
         */
        std::vector<double> generate_window(
            OptimalWindowType type,
            int length,
            const WindowOptimizationParams& params = WindowOptimizationParams{}
        );

        /**
         * @brief Analyze window characteristics for quality assessment
         * @param window Window coefficients
         * @param sample_rate Sample rate in Hz
         * @return Window characteristics
         */
        WindowCharacteristics analyze_window_characteristics(
            const std::vector<double>& window,
            double sample_rate = 44100.0
        );

        /**
         * @brief Apply pre-echo suppression to window
         * @param window Input/output window coefficients
         * @param suppression_factor Suppression strength (0.0-1.0)
         */
        void apply_pre_echo_suppression(
            std::vector<double>& window,
            double suppression_factor = 0.8
        );

        /**
         * @brief Apply spectral leakage minimization
         * @param window Input/output window coefficients
         * @param target_side_lobe_db Target side lobe level
         */
        void minimize_spectral_leakage(
            std::vector<double>& window,
            double target_side_lobe_db = -60.0
        );

        /**
         * @brief Optimize window for overlap-add processing
         * @param window Input/output window coefficients
         * @param overlap_factor Overlap factor (0.0-1.0)
         * @param hop_size Hop size in samples
         */
        void optimize_for_overlap_add(
            std::vector<double>& window,
            double overlap_factor,
            int hop_size
        );

        /**
         * @brief Select optimal window type based on content analysis
         * @param content_analysis Audio content characteristics
         * @param params Optimization parameters
         * @return Recommended window type
         */
        OptimalWindowType select_optimal_window_type(
            const ContentAnalysis& content_analysis,
            const WindowOptimizationParams& params = WindowOptimizationParams{}
        );

        /**
         * @brief Evaluate window quality for synthesis
         * @param window Window coefficients
         * @param content_analysis Content characteristics
         * @return Quality score (0.0-1.0, higher is better)
         */
        double evaluate_window_quality(
            const std::vector<double>& window,
            const ContentAnalysis& content_analysis
        );

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;

        // Window generation functions
        std::vector<double> generate_hann_window(int length);
        std::vector<double> generate_hamming_window(int length);
        std::vector<double> generate_blackman_window(int length);
        std::vector<double> generate_blackman_harris_window(int length);
        std::vector<double> generate_gaussian_window(int length, double alpha = 2.5);
        std::vector<double> generate_tukey_window(int length, double alpha = 0.5);
        std::vector<double> generate_kaiser_window(int length, double beta = 8.6);
        std::vector<double> generate_nuttall_window(int length);
        
        // Adaptive window generation
        std::vector<double> generate_adaptive_harmonic_window(
            int length, const ContentAnalysis& content_analysis);
        std::vector<double> generate_adaptive_transient_window(
            int length, const ContentAnalysis& content_analysis);
        std::vector<double> generate_synthesis_optimized_window(
            int length, const ContentAnalysis& content_analysis,
            const WindowOptimizationParams& params);

        // Analysis functions
        double calculate_main_lobe_width(const std::vector<double>& window);
        double calculate_peak_side_lobe_db(const std::vector<double>& window);
        double calculate_coherent_gain(const std::vector<double>& window);
        double calculate_scalloping_loss(const std::vector<double>& window);

        // Optimization algorithms
        void apply_frequency_domain_shaping(
            std::vector<double>& window,
            const std::vector<double>& target_response);
        void apply_time_domain_constraints(
            std::vector<double>& window,
            const WindowOptimizationParams& params);
    };

    /**
     * @brief Utility functions for window analysis and comparison
     */
    namespace window_utils {
        
        /**
         * @brief Compare windows by quality metrics
         * @param window1 First window
         * @param window2 Second window
         * @param content_analysis Content characteristics
         * @return Quality difference (positive means window1 is better)
         */
        double compare_window_quality(
            const std::vector<double>& window1,
            const std::vector<double>& window2,
            const ContentAnalysis& content_analysis
        );

        /**
         * @brief Calculate spectral leakage for a window
         * @param window Window coefficients
         * @param frequency_bins Output spectral leakage per frequency bin
         */
        void calculate_spectral_leakage(
            const std::vector<double>& window,
            std::vector<double>& frequency_bins
        );

        /**
         * @brief Calculate overlap-add reconstruction error
         * @param window Window coefficients
         * @param hop_size Hop size in samples
         * @return RMS reconstruction error
         */
        double calculate_ola_reconstruction_error(
            const std::vector<double>& window,
            int hop_size
        );

    } // namespace window_utils

} // namespace nexussynth
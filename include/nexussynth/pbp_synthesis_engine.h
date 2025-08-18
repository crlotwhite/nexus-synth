#pragma once

#include <vector>
#include <memory>
#include <complex>
#include <functional>
#include <Eigen/Core>
#include <Eigen/Dense>
#include "world_wrapper.h"
#include "mlpg_engine.h"

namespace nexussynth {
namespace synthesis {

    /**
     * @brief PbP synthesis configuration
     * 
     * Controls the pulse-by-pulse synthesis process parameters
     */
    struct PbpConfig {
        // Core synthesis parameters
        int sample_rate = 44100;           // Target sample rate
        double frame_period = 5.0;         // Frame period in milliseconds
        int fft_size = 2048;              // FFT size for frequency domain processing
        int hop_size = 220;               // Hop size for overlap-add (samples)
        
        // Harmonic synthesis parameters
        int max_harmonics = 100;           // Maximum number of harmonics to generate
        double harmonic_amplitude_threshold = 0.001; // Minimum harmonic amplitude
        
        // Windowing parameters
        enum class WindowType {
            HANN,
            HAMMING,
            BLACKMAN,
            GAUSSIAN
        };
        WindowType window_type = WindowType::HANN;
        double window_length_factor = 2.0; // Window length as factor of pitch period
        
        // Quality control
        bool enable_anti_aliasing = true;  // Enable anti-aliasing for high frequencies
        double noise_floor = -60.0;        // Noise floor in dB
        bool enable_phase_randomization = false; // Randomize harmonic phases
        
        // Performance options
        bool use_fast_fft = true;         // Use optimized FFT implementation
        int synthesis_threads = 1;        // Number of synthesis threads
        
        // Real-time parameters
        int buffer_size = 512;            // Internal buffer size for streaming
        double latency_target_ms = 10.0;  // Target latency in milliseconds
    };

    /**
     * @brief Synthesis quality metrics
     * 
     * Performance and quality statistics for synthesis process
     */
    struct SynthesisStats {
        // Performance metrics
        double synthesis_time_ms = 0.0;
        double average_frame_time_ms = 0.0;
        double peak_frame_time_ms = 0.0;
        
        // Quality metrics
        double harmonic_energy_ratio = 0.0;     // Ratio of harmonic to total energy
        double spectral_distortion_db = 0.0;    // Spectral distortion in dB
        double temporal_smoothness = 0.0;       // Temporal continuity metric
        
        // Processing statistics
        int frames_processed = 0;
        int harmonics_generated = 0;
        double cpu_usage_percent = 0.0;
        
        // Memory usage
        size_t peak_memory_mb = 0;
        size_t average_memory_mb = 0;
        
        std::string synthesis_method;
    };

    /**
     * @brief Single pulse synthesis parameters
     * 
     * Contains all parameters needed to synthesize one pulse
     */
    struct PulseParams {
        double f0;                              // Fundamental frequency (Hz)
        std::vector<double> spectrum;           // Spectral envelope
        std::vector<double> aperiodicity;       // Aperiodicity coefficients
        double pulse_position;                  // Position within frame (0.0-1.0)
        double amplitude_scale = 1.0;           // Overall amplitude scaling
        
        // Advanced parameters
        std::vector<double> harmonic_phases;    // Explicit harmonic phases
        double formant_shift = 1.0;            // Formant frequency shift factor
        double pitch_shift = 1.0;              // Pitch shift factor
    };

    /**
     * @brief Pulse-by-Pulse Synthesis Engine
     * 
     * Core engine for high-quality audio waveform synthesis from WORLD parameters
     * using pulse-by-pulse approach with frequency domain harmonic generation
     */
    class PbpSynthesisEngine {
    public:
        explicit PbpSynthesisEngine(const PbpConfig& config = PbpConfig{});
        ~PbpSynthesisEngine() = default;

        // Core synthesis functions
        /**
         * @brief Synthesize audio waveform from WORLD parameters
         * 
         * @param parameters Complete WORLD parameters (F0, SP, AP)
         * @param stats Optional synthesis statistics output
         * @return Synthesized audio waveform
         */
        std::vector<double> synthesize(
            const AudioParameters& parameters,
            SynthesisStats* stats = nullptr
        );

        /**
         * @brief Synthesize from MLPG trajectory and WORLD parameters
         * 
         * @param trajectory MLPG-generated smooth parameter trajectory
         * @param world_params Base WORLD parameters for synthesis
         * @param stats Optional synthesis statistics output
         * @return Synthesized audio waveform
         */
        std::vector<double> synthesize_from_trajectory(
            const std::vector<Eigen::VectorXd>& trajectory,
            const AudioParameters& world_params,
            SynthesisStats* stats = nullptr
        );

        /**
         * @brief Synthesize single pulse at specified time
         * 
         * @param pulse_params Parameters for pulse synthesis
         * @param synthesis_time Target time in samples
         * @return Synthesized pulse waveform
         */
        std::vector<double> synthesize_pulse(
            const PulseParams& pulse_params,
            double synthesis_time
        );

        // Configuration management
        void set_config(const PbpConfig& config) { config_ = config; initialize_engine(); }
        const PbpConfig& get_config() const { return config_; }

        // Quality control
        /**
         * @brief Validate synthesis quality against reference
         * 
         * @param synthesized Synthesized waveform
         * @param reference Reference waveform
         * @return Quality metrics comparing synthesized vs reference
         */
        double validate_synthesis_quality(
            const std::vector<double>& synthesized,
            const std::vector<double>& reference
        );

        // Real-time synthesis support
        /**
         * @brief Initialize streaming synthesis
         * 
         * @param buffer_size Size of internal buffers
         * @return true if initialization successful
         */
        bool initialize_streaming(int buffer_size = 512);

        /**
         * @brief Process next frame for streaming synthesis
         * 
         * @param frame_params Parameters for current frame
         * @param output_buffer Output buffer for synthesized samples
         * @param buffer_size Size of output buffer
         * @return Number of samples written to buffer
         */
        int process_streaming_frame(
            const PulseParams& frame_params,
            double* output_buffer,
            int buffer_size
        );

        /**
         * @brief Finalize streaming synthesis
         * 
         * @param output_buffer Final output buffer
         * @param buffer_size Size of output buffer
         * @return Number of final samples
         */
        int finalize_streaming(double* output_buffer, int buffer_size);

    private:
        PbpConfig config_;
        
        // Engine state
        bool engine_initialized_ = false;
        std::vector<double> window_function_;
        std::vector<double> synthesis_buffer_;
        
        // FFT workspace
        std::vector<std::complex<double>> fft_buffer_;
        std::vector<std::complex<double>> spectrum_buffer_;
        
        // Streaming state
        bool streaming_active_ = false;
        std::vector<double> overlap_buffer_;
        int current_frame_ = 0;
        double synthesis_time_ = 0.0;

        // Core synthesis algorithms
        /**
         * @brief Initialize synthesis engine and precompute tables
         */
        void initialize_engine();

        /**
         * @brief Generate harmonic structure for given F0
         * 
         * @param f0 Fundamental frequency
         * @param spectrum Spectral envelope
         * @param aperiodicity Aperiodicity coefficients
         * @param harmonics Output harmonic amplitudes and phases
         */
        void generate_harmonics(
            double f0,
            const std::vector<double>& spectrum,
            const std::vector<double>& aperiodicity,
            std::vector<std::complex<double>>& harmonics
        );

        /**
         * @brief Apply spectral envelope filtering
         * 
         * @param harmonics Input harmonic components
         * @param spectrum Spectral envelope to apply
         * @param filtered_harmonics Output filtered harmonics
         */
        void apply_spectral_envelope(
            const std::vector<std::complex<double>>& harmonics,
            const std::vector<double>& spectrum,
            std::vector<std::complex<double>>& filtered_harmonics
        );

        /**
         * @brief Mix aperiodic noise component
         * 
         * @param harmonics Harmonic components
         * @param aperiodicity Aperiodicity coefficients
         * @param mixed_spectrum Output mixed spectrum
         */
        void mix_aperiodic_component(
            const std::vector<std::complex<double>>& harmonics,
            const std::vector<double>& aperiodicity,
            std::vector<std::complex<double>>& mixed_spectrum
        );

        /**
         * @brief Perform inverse FFT to generate time-domain pulse
         * 
         * @param spectrum Frequency domain spectrum
         * @param pulse_waveform Output time-domain pulse
         */
        void inverse_fft_synthesis(
            const std::vector<std::complex<double>>& spectrum,
            std::vector<double>& pulse_waveform
        );

        /**
         * @brief Apply window function to pulse
         * 
         * @param pulse Input pulse waveform
         * @param windowed_pulse Output windowed pulse
         */
        void apply_window_function(
            const std::vector<double>& pulse,
            std::vector<double>& windowed_pulse
        );

        /**
         * @brief Perform overlap-add to combine pulses
         * 
         * @param pulse Current pulse to add
         * @param pulse_position Position in synthesis buffer
         * @param synthesis_buffer Target synthesis buffer
         */
        void overlap_add_pulse(
            const std::vector<double>& pulse,
            int pulse_position,
            std::vector<double>& synthesis_buffer
        );

        // Utility functions
        /**
         * @brief Generate window function coefficients
         * 
         * @param length Window length
         * @param type Window type
         * @return Window coefficients
         */
        std::vector<double> generate_window(int length, PbpConfig::WindowType type);

        /**
         * @brief Calculate optimal pulse positions for given F0 contour
         * 
         * @param f0_contour F0 values per frame
         * @param frame_period Frame period in ms
         * @return Pulse positions in samples
         */
        std::vector<double> calculate_pulse_positions(
            const std::vector<double>& f0_contour,
            double frame_period
        );

        /**
         * @brief Interpolate parameters between frames
         * 
         * @param params1 Parameters at frame 1
         * @param params2 Parameters at frame 2
         * @param interpolation_factor Factor between 0.0-1.0
         * @return Interpolated parameters
         */
        PulseParams interpolate_parameters(
            const PulseParams& params1,
            const PulseParams& params2,
            double interpolation_factor
        );

        /**
         * @brief Validate synthesis parameters
         * 
         * @param parameters WORLD parameters to validate
         * @throws std::invalid_argument on validation failure
         */
        void validate_synthesis_parameters(const AudioParameters& parameters);

        // Performance optimization
        void optimize_for_realtime();
        void precompute_synthesis_tables();
        
        // Quality metrics calculation
        double calculate_harmonic_energy_ratio();
        double calculate_trajectory_smoothness(const std::vector<double>& trajectory);
        
        // Memory management
        void allocate_synthesis_buffers();
        void deallocate_synthesis_buffers();
    };

    /**
     * @brief Utility functions for pulse-by-pulse synthesis
     */
    namespace pbp_utils {
        /**
         * @brief Convert WORLD parameters to PulseParams sequence
         * 
         * @param world_params WORLD AudioParameters
         * @return Sequence of PulseParams for synthesis
         */
        std::vector<PulseParams> world_to_pulse_params(const AudioParameters& world_params);

        /**
         * @brief Merge MLPG trajectory with WORLD parameters
         * 
         * @param trajectory MLPG-generated parameter trajectory
         * @param world_params Base WORLD parameters
         * @return Modified WORLD parameters with MLPG trajectory applied
         */
        AudioParameters merge_trajectory_with_world(
            const std::vector<Eigen::VectorXd>& trajectory,
            const AudioParameters& world_params
        );

        /**
         * @brief Calculate synthesis buffer size for given parameters
         * 
         * @param duration_seconds Target synthesis duration
         * @param sample_rate Sample rate
         * @param overlap_factor Overlap-add factor
         * @return Required buffer size in samples
         */
        int calculate_synthesis_buffer_size(
            double duration_seconds,
            int sample_rate,
            double overlap_factor = 2.0
        );

        /**
         * @brief Measure synthesis performance
         * 
         * @param synthesis_function Function to benchmark
         * @param iterations Number of test iterations
         * @return Performance statistics
         */
        SynthesisStats benchmark_synthesis_performance(
            std::function<std::vector<double>()> synthesis_function,
            int iterations = 10
        );
    }

} // namespace synthesis
} // namespace nexussynth
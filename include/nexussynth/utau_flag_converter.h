#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include "utau_argument_parser.h"
#include "pbp_synthesis_engine.h"

namespace nexussynth {
namespace utau {

/**
 * @brief NexusSynth internal synthesis parameters
 * 
 * Represents the complete set of parameters used internally by NexusSynth
 * for voice synthesis, derived from UTAU flags and other inputs
 */
struct NexusSynthParams {
    // Spectral envelope modifications
    double formant_shift_factor = 1.0;        ///< Formant frequency shift (g flag influence)
    double spectral_tilt = 0.0;               ///< Overall spectral tilt in dB/octave
    double brightness_gain = 1.0;             ///< Overall brightness scaling (bri flag)
    
    // Harmonic structure modifications  
    double harmonic_emphasis = 0.0;           ///< Harmonic emphasis factor (t flag)
    double fundamental_boost = 1.0;           ///< Fundamental frequency boost
    double high_frequency_rolloff = 0.0;     ///< High frequency rolloff factor
    
    // Noise and breathiness
    double breathiness_level = 0.0;           ///< Breathiness/noise ratio (bre flag)
    double aperiodicity_scaling = 1.0;        ///< Aperiodicity parameter scaling
    double noise_floor_db = -60.0;           ///< Noise floor level
    
    // Voice character modulation
    double vocal_effort = 0.0;                ///< Overall vocal effort (-1.0 to 1.0)
    double tension_factor = 0.0;              ///< Vocal tension (t flag primary influence)
    double roughness = 0.0;                   ///< Voice roughness/irregularity
    
    // Advanced synthesis control
    double phase_randomization = 0.0;         ///< Harmonic phase randomization amount
    double temporal_jitter = 0.0;             ///< Pulse timing jitter
    double amplitude_modulation = 0.0;        ///< Subtle amplitude modulation depth
    
    // Quality and processing flags
    bool enable_formant_correction = true;    ///< Apply formant frequency correction
    bool enable_spectral_smoothing = true;    ///< Apply spectral envelope smoothing
    bool preserve_original_character = true;  ///< Maintain original voice characteristics
    
    /**
     * @brief Validate parameter ranges
     * @return true if all parameters are within valid ranges
     */
    bool is_valid() const {
        return (formant_shift_factor > 0.1 && formant_shift_factor < 3.0) &&
               (brightness_gain > 0.1 && brightness_gain < 5.0) &&
               (breathiness_level >= 0.0 && breathiness_level <= 1.0) &&
               (vocal_effort >= -1.0 && vocal_effort <= 1.0) &&
               (tension_factor >= -1.0 && tension_factor <= 1.0);
    }
    
    /**
     * @brief Apply parameters to synthesis engine configuration
     * @param config Target PbP synthesis configuration
     */
    void apply_to_pbp_config(synthesis::PbpConfig& config) const;
    
    /**
     * @brief Apply parameters to pulse synthesis parameters
     * @param pulse_params Target pulse parameters
     */
    void apply_to_pulse_params(synthesis::PulseParams& pulse_params) const;
};

/**
 * @brief Flag conversion profile for different voice types
 */
enum class VoiceType {
    UNKNOWN,
    MALE_ADULT,      ///< Adult male voice
    FEMALE_ADULT,    ///< Adult female voice
    CHILD,           ///< Child voice
    ROBOTIC,         ///< Synthetic/robotic voice
    WHISPER,         ///< Whispered voice type
    GROWL            ///< Growl/death metal style
};

/**
 * @brief Conversion configuration for different scenarios
 */
struct ConversionConfig {
    VoiceType voice_type = VoiceType::UNKNOWN;
    
    // Flag sensitivity settings
    double g_sensitivity = 1.0;       ///< Sensitivity to g flag changes
    double t_sensitivity = 1.0;       ///< Sensitivity to t flag changes
    double bre_sensitivity = 1.0;     ///< Sensitivity to bre flag changes
    double bri_sensitivity = 1.0;     ///< Sensitivity to bri flag changes
    
    // Processing preferences
    bool preserve_naturalness = true; ///< Prioritize natural sound over extreme effects
    bool enable_cross_flag_interaction = true; ///< Allow flags to interact with each other
    bool apply_voice_type_compensation = true; ///< Apply voice-type-specific adjustments
    
    // Quality control
    double max_formant_shift = 2.0;   ///< Maximum allowed formant shift
    double max_brightness_change = 3.0; ///< Maximum brightness change
    bool enable_safety_limiting = true; ///< Prevent extreme parameter values
};

/**
 * @brief UTAU Flag to NexusSynth Parameter Converter
 * 
 * Converts UTAU standard flags (g, t, bre, bri) into NexusSynth internal
 * synthesis parameters with proper scaling, interaction modeling, and
 * voice-type-specific adjustments.
 */
class UtauFlagConverter {
public:
    explicit UtauFlagConverter(const ConversionConfig& config = ConversionConfig{});
    ~UtauFlagConverter();
    
    // Main conversion interface
    /**
     * @brief Convert UTAU flags to NexusSynth parameters
     * @param flag_values Parsed UTAU flag values
     * @return Complete NexusSynth synthesis parameters
     */
    NexusSynthParams convert(const FlagValues& flag_values);
    
    /**
     * @brief Convert with additional context information
     * @param flag_values Parsed UTAU flag values
     * @param voice_type Detected or specified voice type
     * @param base_f0 Base fundamental frequency for context
     * @return Complete NexusSynth synthesis parameters
     */
    NexusSynthParams convert_with_context(
        const FlagValues& flag_values, 
        VoiceType voice_type,
        double base_f0 = 220.0
    );
    
    // Configuration management
    void set_config(const ConversionConfig& config) { config_ = config; }
    const ConversionConfig& get_config() const { return config_; }
    
    // Voice type detection
    /**
     * @brief Detect voice type from audio characteristics
     * @param f0_mean Mean fundamental frequency
     * @param spectral_centroid Mean spectral centroid
     * @param harmonic_richness Harmonic-to-noise ratio
     * @return Detected voice type
     */
    static VoiceType detect_voice_type(
        double f0_mean, 
        double spectral_centroid, 
        double harmonic_richness
    );
    
    // Conversion quality analysis
    /**
     * @brief Analyze conversion quality and potential issues
     * @param original_flags Original UTAU flags
     * @param converted_params Converted NexusSynth parameters
     * @return Quality analysis results
     */
    struct ConversionAnalysis {
        double conversion_fidelity = 1.0;     ///< How well flags map to parameters (0-1)
        double parameter_stability = 1.0;     ///< Parameter value stability (0-1)
        std::vector<std::string> warnings;    ///< Potential issues or warnings
        std::map<std::string, double> flag_contributions; ///< Individual flag contributions
    };
    
    ConversionAnalysis analyze_conversion(
        const FlagValues& original_flags,
        const NexusSynthParams& converted_params
    );
    
    // Advanced conversion features
    /**
     * @brief Apply gradual transition between flag states
     * @param from_flags Starting flag values
     * @param to_flags Target flag values
     * @param transition_progress Progress from 0.0 to 1.0
     * @return Interpolated NexusSynth parameters
     */
    NexusSynthParams interpolate_conversion(
        const FlagValues& from_flags,
        const FlagValues& to_flags,
        double transition_progress
    );
    
    // Debugging and analysis tools
    /**
     * @brief Generate detailed conversion report
     * @param flag_values Input flag values
     * @param params Output parameters
     * @return Human-readable conversion report
     */
    std::string generate_conversion_report(
        const FlagValues& flag_values,
        const NexusSynthParams& params
    );
    
    /**
     * @brief Test conversion with various flag combinations
     * @return Conversion test results for validation
     */
    std::vector<ConversionAnalysis> run_conversion_tests();

private:
    ConversionConfig config_;
    
    // Core conversion algorithms
    double convert_g_flag(int g_value, VoiceType voice_type, double base_f0);
    double convert_t_flag(int t_value, VoiceType voice_type, double base_f0);
    double convert_bre_flag(int bre_value, VoiceType voice_type);
    double convert_bri_flag(int bri_value, VoiceType voice_type);
    
    // Cross-flag interaction modeling
    void apply_flag_interactions(NexusSynthParams& params, const FlagValues& flags);
    
    // Voice-type-specific adjustments
    void apply_voice_type_adjustments(NexusSynthParams& params, VoiceType voice_type);
    
    // Safety and quality control
    void apply_safety_limits(NexusSynthParams& params);
    void validate_parameter_consistency(NexusSynthParams& params);
    
    // Internal utilities
    double scale_with_sensitivity(double base_value, double flag_value, double sensitivity);
    double apply_voice_type_scaling(double value, VoiceType voice_type, const std::string& param_name);
};

/**
 * @brief Utility functions for flag conversion
 */
namespace FlagConversionUtils {
    /**
     * @brief Create conversion config optimized for specific voice type
     * @param voice_type Target voice type
     * @return Optimized conversion configuration
     */
    ConversionConfig create_voice_type_config(VoiceType voice_type);
    
    /**
     * @brief Benchmark flag conversion performance
     * @param test_cases Number of test cases to run
     * @return Performance metrics
     */
    struct ConversionBenchmark {
        double average_conversion_time_us;
        double peak_conversion_time_us;
        size_t memory_usage_bytes;
        double conversions_per_second;
    };
    
    ConversionBenchmark benchmark_conversion_performance(size_t test_cases = 1000);
    
    /**
     * @brief Validate flag conversion against reference implementations
     * @param reference_converter Reference converter for comparison
     * @param test_flags Set of test flag combinations
     * @return Validation results
     */
    bool validate_conversion_compatibility(
        const UtauFlagConverter& reference_converter,
        const std::vector<FlagValues>& test_flags
    );
}

} // namespace utau
} // namespace nexussynth
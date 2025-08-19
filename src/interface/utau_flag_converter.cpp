#include "nexussynth/utau_flag_converter.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace nexussynth {
namespace utau {

// NexusSynthParams implementation
void NexusSynthParams::apply_to_pbp_config(synthesis::PbpConfig& config) const {
    // Apply spectral processing settings
    config.enable_anti_aliasing = enable_formant_correction;
    config.noise_floor = noise_floor_db;
    config.enable_phase_randomization = (phase_randomization > 0.1);
    
    // Adjust harmonic processing based on parameters
    if (harmonic_emphasis > 0.1) {
        config.max_harmonics = static_cast<int>(config.max_harmonics * (1.0 + harmonic_emphasis));
        config.harmonic_amplitude_threshold *= (1.0 - harmonic_emphasis * 0.5);
    }
    
    // Apply windowing optimizations for different voice characteristics
    if (breathiness_level > 0.3) {
        config.window_type = synthesis::PbpConfig::WindowType::GAUSSIAN;
        config.enable_adaptive_windowing = true;
    }
    
    if (tension_factor > 0.5) {
        config.minimize_pre_echo = true;
        config.side_lobe_suppression_db = -70.0;
    }
}

void NexusSynthParams::apply_to_pulse_params(synthesis::PulseParams& pulse_params) const {
    // Apply formant shifting
    pulse_params.formant_shift = formant_shift_factor;
    pulse_params.pitch_shift = 1.0; // Pitch handled separately
    
    // Apply brightness through amplitude scaling
    pulse_params.amplitude_scale = brightness_gain;
    
    // Modify aperiodicity based on breathiness
    if (!pulse_params.aperiodicity.empty()) {
        for (auto& ap : pulse_params.aperiodicity) {
            ap = std::min(1.0, ap + breathiness_level * aperiodicity_scaling);
        }
    }
    
    // Apply spectral modifications to spectrum envelope
    if (!pulse_params.spectrum.empty() && spectral_tilt != 0.0) {
        double freq_step = 1.0 / pulse_params.spectrum.size();
        for (size_t i = 0; i < pulse_params.spectrum.size(); ++i) {
            double frequency_ratio = i * freq_step;
            double tilt_factor = std::pow(10.0, spectral_tilt * frequency_ratio / 20.0);
            pulse_params.spectrum[i] *= tilt_factor;
        }
    }
}

// UtauFlagConverter implementation
UtauFlagConverter::UtauFlagConverter(const ConversionConfig& config)
    : config_(config) {
}

UtauFlagConverter::~UtauFlagConverter() = default;

NexusSynthParams UtauFlagConverter::convert(const FlagValues& flag_values) {
    return convert_with_context(flag_values, config_.voice_type, 220.0);
}

NexusSynthParams UtauFlagConverter::convert_with_context(
    const FlagValues& flag_values, 
    VoiceType voice_type,
    double base_f0) {
    
    NexusSynthParams params;
    
    // Convert individual flags
    double g_contribution = convert_g_flag(flag_values.g, voice_type, base_f0);
    double t_contribution = convert_t_flag(flag_values.t, voice_type, base_f0);
    double bre_contribution = convert_bre_flag(flag_values.bre, voice_type);
    double bri_contribution = convert_bri_flag(flag_values.bri, voice_type);
    
    // Apply primary flag effects
    params.formant_shift_factor = 1.0 + g_contribution * 0.8; // g flag primarily affects formants
    params.tension_factor = t_contribution; // t flag directly maps to tension
    params.breathiness_level = bre_contribution; // bre flag directly maps to breathiness
    params.brightness_gain = 1.0 + bri_contribution; // bri flag affects overall brightness
    
    // Calculate secondary effects from flag interactions
    if (config_.enable_cross_flag_interaction) {
        apply_flag_interactions(params, flag_values);
    }
    
    // Apply voice-type-specific adjustments
    if (config_.apply_voice_type_compensation) {
        apply_voice_type_adjustments(params, voice_type);
    }
    
    // Apply safety limits and validation
    if (config_.enable_safety_limiting) {
        apply_safety_limits(params);
    }
    
    validate_parameter_consistency(params);
    
    return params;
}

double UtauFlagConverter::convert_g_flag(int g_value, VoiceType voice_type, double base_f0) {
    if (g_value == 0) return 0.0;
    
    // Base conversion: g±100 maps to ±0.5 formant shift
    double base_shift = (g_value / 100.0) * 0.5 * config_.g_sensitivity;
    
    // Voice-type-specific scaling
    double voice_scaling = apply_voice_type_scaling(base_shift, voice_type, "g_flag");
    
    // Frequency-dependent scaling (higher base frequencies are more sensitive)
    double freq_factor = std::log(base_f0 / 110.0) / std::log(2.0); // Octaves above A2
    double freq_scaling = 1.0 + freq_factor * 0.2;
    
    return voice_scaling * freq_scaling;
}

double UtauFlagConverter::convert_t_flag(int t_value, VoiceType voice_type, double /* base_f0 */) {
    if (t_value == 0) return 0.0;
    
    // Base conversion: t±100 maps to ±1.0 tension factor
    double base_tension = (t_value / 100.0) * config_.t_sensitivity;
    
    // Voice-type-specific scaling
    double voice_scaling = apply_voice_type_scaling(base_tension, voice_type, "t_flag");
    
    // Non-linear response for more natural tension effects
    double sign = (voice_scaling >= 0) ? 1.0 : -1.0;
    double magnitude = std::abs(voice_scaling);
    double non_linear = std::tanh(magnitude * 1.5) * sign;
    
    return non_linear;
}

double UtauFlagConverter::convert_bre_flag(int bre_value, VoiceType voice_type) {
    if (bre_value == 0) return 0.0;
    
    // Base conversion: bre0-100 maps to 0.0-0.8 breathiness
    double base_breathiness = (bre_value / 100.0) * 0.8 * config_.bre_sensitivity;
    
    // Voice-type-specific scaling
    double voice_scaling = apply_voice_type_scaling(base_breathiness, voice_type, "bre_flag");
    
    // Ensure breathiness stays in valid range
    return std::clamp(voice_scaling, 0.0, 1.0);
}

double UtauFlagConverter::convert_bri_flag(int bri_value, VoiceType voice_type) {
    if (bri_value == 0) return 0.0;
    
    // Base conversion: bri±100 maps to ±0.6 brightness change
    double base_brightness = (bri_value / 100.0) * 0.6 * config_.bri_sensitivity;
    
    // Voice-type-specific scaling
    double voice_scaling = apply_voice_type_scaling(base_brightness, voice_type, "bri_flag");
    
    return voice_scaling;
}

void UtauFlagConverter::apply_flag_interactions(NexusSynthParams& params, const FlagValues& flags) {
    // g and t interaction: high g with high t creates more spectral complexity
    if (flags.g > 30 && flags.t > 30) {
        params.harmonic_emphasis += 0.2;
        params.spectral_tilt += 1.0; // Slightly brighter
    }
    
    // bre and t interaction: high breathiness with high tension is unnatural
    if (flags.bre > 50 && flags.t > 40) {
        params.breathiness_level *= 0.7; // Reduce breathiness
        params.tension_factor *= 0.8;    // Reduce tension
    }
    
    // bri and g interaction: brightness affects formant perception
    if (std::abs(flags.bri) > 30 && std::abs(flags.g) > 20) {
        double interaction = (flags.bri / 100.0) * (flags.g / 100.0) * 0.15;
        params.formant_shift_factor += interaction;
    }
    
    // bre and bri interaction: breathiness affects brightness perception
    if (flags.bre > 30 && flags.bri != 0) {
        double bre_factor = flags.bre / 100.0;
        params.brightness_gain *= (1.0 - bre_factor * 0.2); // Breathiness dulls brightness
    }
}

void UtauFlagConverter::apply_voice_type_adjustments(NexusSynthParams& params, VoiceType voice_type) {
    switch (voice_type) {
        case VoiceType::MALE_ADULT:
            // Male voices: more conservative formant shifts, stronger tension effects
            params.formant_shift_factor = 1.0 + (params.formant_shift_factor - 1.0) * 0.7;
            params.tension_factor *= 1.2;
            break;
            
        case VoiceType::FEMALE_ADULT:
            // Female voices: more sensitive to formant changes, less to tension
            params.formant_shift_factor = 1.0 + (params.formant_shift_factor - 1.0) * 1.3;
            params.tension_factor *= 0.8;
            break;
            
        case VoiceType::CHILD:
            // Child voices: very sensitive to all changes, limit extreme values
            params.formant_shift_factor = 1.0 + (params.formant_shift_factor - 1.0) * 0.5;
            params.brightness_gain = 1.0 + (params.brightness_gain - 1.0) * 0.8;
            params.breathiness_level *= 0.6; // Children typically less breathy
            break;
            
        case VoiceType::ROBOTIC:
            // Robotic voices: allow extreme values, emphasize artificial characteristics
            params.phase_randomization = std::min(0.8, params.phase_randomization + 0.3);
            params.harmonic_emphasis *= 1.5;
            break;
            
        case VoiceType::WHISPER:
            // Whisper voices: maximize breathiness, minimize harmonic structure
            params.breathiness_level = std::max(0.6, params.breathiness_level);
            params.harmonic_emphasis *= 0.3;
            params.aperiodicity_scaling = 1.5;
            break;
            
        case VoiceType::GROWL:
            // Growl voices: emphasize low frequencies, add roughness
            params.spectral_tilt -= 2.0; // Emphasize low frequencies
            params.roughness += 0.4;
            params.harmonic_emphasis *= 1.8;
            break;
            
        case VoiceType::UNKNOWN:
        default:
            // No specific adjustments for unknown voice types
            break;
    }
}

void UtauFlagConverter::apply_safety_limits(NexusSynthParams& params) {
    // Clamp formant shift to reasonable range
    params.formant_shift_factor = std::clamp(params.formant_shift_factor, 
                                           1.0 / config_.max_formant_shift, 
                                           config_.max_formant_shift);
    
    // Clamp brightness changes
    params.brightness_gain = std::clamp(params.brightness_gain,
                                      1.0 / config_.max_brightness_change,
                                      config_.max_brightness_change);
    
    // Ensure other parameters stay in valid ranges
    params.breathiness_level = std::clamp(params.breathiness_level, 0.0, 1.0);
    params.tension_factor = std::clamp(params.tension_factor, -1.0, 1.0);
    params.vocal_effort = std::clamp(params.vocal_effort, -1.0, 1.0);
    params.harmonic_emphasis = std::clamp(params.harmonic_emphasis, -1.0, 2.0);
    params.spectral_tilt = std::clamp(params.spectral_tilt, -10.0, 10.0);
}

void UtauFlagConverter::validate_parameter_consistency(NexusSynthParams& params) {
    // Ensure high breathiness doesn't coexist with high harmonic emphasis
    if (params.breathiness_level > 0.7 && params.harmonic_emphasis > 0.5) {
        double reduction_factor = 0.5;
        params.harmonic_emphasis *= reduction_factor;
    }
    
    // Ensure extreme formant shifts don't create unrealistic brightness
    if (std::abs(params.formant_shift_factor - 1.0) > 0.5) {
        double brightness_compensation = 1.0 - std::abs(params.formant_shift_factor - 1.0) * 0.3;
        params.brightness_gain *= brightness_compensation;
    }
}

VoiceType UtauFlagConverter::detect_voice_type(
    double f0_mean, 
    double spectral_centroid, 
    double harmonic_richness) {
    
    // Simple heuristic-based voice type detection
    if (f0_mean < 120) {
        return VoiceType::MALE_ADULT;
    } else if (f0_mean > 250) {
        if (f0_mean > 350 && spectral_centroid > 3000) {
            return VoiceType::CHILD;
        }
        return VoiceType::FEMALE_ADULT;
    } else {
        // Mid-range frequency, check other characteristics
        if (harmonic_richness < 0.3) {
            return VoiceType::WHISPER;
        } else if (harmonic_richness > 0.9 && spectral_centroid < 1500) {
            return VoiceType::GROWL;
        }
        return VoiceType::UNKNOWN;
    }
}

double UtauFlagConverter::scale_with_sensitivity(double base_value, double flag_value, double sensitivity) {
    return base_value * (flag_value / 100.0) * sensitivity;
}

double UtauFlagConverter::apply_voice_type_scaling(double value, VoiceType voice_type, const std::string& param_name) {
    // Voice-type-specific scaling factors
    static const std::map<VoiceType, std::map<std::string, double>> scaling_factors = {
        {VoiceType::MALE_ADULT, {{"g_flag", 0.8}, {"t_flag", 1.2}, {"bre_flag", 0.9}, {"bri_flag", 0.9}}},
        {VoiceType::FEMALE_ADULT, {{"g_flag", 1.3}, {"t_flag", 0.8}, {"bre_flag", 1.1}, {"bri_flag", 1.1}}},
        {VoiceType::CHILD, {{"g_flag", 0.6}, {"t_flag", 0.7}, {"bre_flag", 0.6}, {"bri_flag", 1.2}}},
        {VoiceType::ROBOTIC, {{"g_flag", 1.5}, {"t_flag", 1.5}, {"bre_flag", 0.3}, {"bri_flag", 1.8}}},
        {VoiceType::WHISPER, {{"g_flag", 0.7}, {"t_flag", 0.3}, {"bre_flag", 2.0}, {"bri_flag", 0.5}}},
        {VoiceType::GROWL, {{"g_flag", 1.2}, {"t_flag", 1.8}, {"bre_flag", 1.3}, {"bri_flag", 0.7}}}
    };
    
    auto voice_it = scaling_factors.find(voice_type);
    if (voice_it != scaling_factors.end()) {
        auto param_it = voice_it->second.find(param_name);
        if (param_it != voice_it->second.end()) {
            return value * param_it->second;
        }
    }
    
    return value; // No scaling for unknown voice types or parameters
}

UtauFlagConverter::ConversionAnalysis UtauFlagConverter::analyze_conversion(
    const FlagValues& original_flags,
    const NexusSynthParams& converted_params) {
    
    ConversionAnalysis analysis;
    
    // Calculate conversion fidelity (how well flags map to expected parameters)
    double g_fidelity = (original_flags.g == 0) ? 1.0 : 
        std::max(0.0, 1.0 - std::abs(converted_params.formant_shift_factor - 1.0) / 0.5);
    double t_fidelity = (original_flags.t == 0) ? 1.0 :
        std::max(0.0, 1.0 - std::abs(converted_params.tension_factor) / 1.0);
    double bre_fidelity = (original_flags.bre == 0) ? 1.0 :
        std::max(0.0, 1.0 - std::abs(converted_params.breathiness_level) / 0.8);
    double bri_fidelity = (original_flags.bri == 0) ? 1.0 :
        std::max(0.0, 1.0 - std::abs(converted_params.brightness_gain - 1.0) / 0.6);
    
    analysis.conversion_fidelity = (g_fidelity + t_fidelity + bre_fidelity + bri_fidelity) / 4.0;
    
    // Calculate parameter stability (absence of extreme values)
    bool stable = converted_params.is_valid() &&
                 std::abs(converted_params.formant_shift_factor - 1.0) < 1.0 &&
                 std::abs(converted_params.brightness_gain - 1.0) < 2.0;
    analysis.parameter_stability = stable ? 1.0 : 0.5;
    
    // Record individual flag contributions
    analysis.flag_contributions["g"] = std::abs(converted_params.formant_shift_factor - 1.0);
    analysis.flag_contributions["t"] = std::abs(converted_params.tension_factor);
    analysis.flag_contributions["bre"] = converted_params.breathiness_level;
    analysis.flag_contributions["bri"] = std::abs(converted_params.brightness_gain - 1.0);
    
    // Generate warnings for potential issues
    if (converted_params.formant_shift_factor > 2.0 || converted_params.formant_shift_factor < 0.5) {
        analysis.warnings.push_back("Extreme formant shift detected - may sound unnatural");
    }
    if (converted_params.breathiness_level > 0.8) {
        analysis.warnings.push_back("Very high breathiness - may reduce intelligibility");
    }
    if (converted_params.brightness_gain > 3.0 || converted_params.brightness_gain < 0.3) {
        analysis.warnings.push_back("Extreme brightness change - may cause harsh or muffled sound");
    }
    
    return analysis;
}

std::string UtauFlagConverter::generate_conversion_report(
    const FlagValues& flag_values,
    const NexusSynthParams& params) {
    
    std::ostringstream report;
    report << std::fixed << std::setprecision(3);
    
    report << "=== UTAU Flag Conversion Report ===\n\n";
    
    // Input flags
    report << "Input Flags:\n";
    report << "  g: " << flag_values.g << "\n";
    report << "  t: " << flag_values.t << "\n";
    report << "  bre: " << flag_values.bre << "\n";
    report << "  bri: " << flag_values.bri << "\n";
    
    if (!flag_values.custom_flags.empty()) {
        report << "  custom: ";
        for (const auto& [name, value] : flag_values.custom_flags) {
            report << name << value << " ";
        }
        report << "\n";
    }
    report << "\n";
    
    // Converted parameters
    report << "Converted Parameters:\n";
    report << "  Formant Shift: " << params.formant_shift_factor << " (ratio)\n";
    report << "  Brightness Gain: " << params.brightness_gain << " (ratio)\n";
    report << "  Breathiness: " << params.breathiness_level << " (0-1)\n";
    report << "  Tension: " << params.tension_factor << " (-1 to 1)\n";
    report << "  Harmonic Emphasis: " << params.harmonic_emphasis << "\n";
    report << "  Spectral Tilt: " << params.spectral_tilt << " dB/octave\n";
    report << "\n";
    
    // Analysis
    auto analysis = const_cast<UtauFlagConverter*>(this)->analyze_conversion(flag_values, params);
    report << "Conversion Analysis:\n";
    report << "  Fidelity: " << (analysis.conversion_fidelity * 100) << "%\n";
    report << "  Stability: " << (analysis.parameter_stability * 100) << "%\n";
    
    if (!analysis.warnings.empty()) {
        report << "  Warnings:\n";
        for (const auto& warning : analysis.warnings) {
            report << "    - " << warning << "\n";
        }
    }
    
    report << "\n=== End Report ===";
    
    return report.str();
}

NexusSynthParams UtauFlagConverter::interpolate_conversion(
    const FlagValues& from_flags,
    const FlagValues& to_flags,
    double transition_progress) {
    
    // Clamp progress to valid range
    transition_progress = std::clamp(transition_progress, 0.0, 1.0);
    
    // Interpolate flag values
    FlagValues interpolated_flags;
    interpolated_flags.g = static_cast<int>(from_flags.g + (to_flags.g - from_flags.g) * transition_progress);
    interpolated_flags.t = static_cast<int>(from_flags.t + (to_flags.t - from_flags.t) * transition_progress);
    interpolated_flags.bre = static_cast<int>(from_flags.bre + (to_flags.bre - from_flags.bre) * transition_progress);
    interpolated_flags.bri = static_cast<int>(from_flags.bri + (to_flags.bri - from_flags.bri) * transition_progress);
    
    // Interpolate custom flags
    for (const auto& [name, from_value] : from_flags.custom_flags) {
        auto to_it = to_flags.custom_flags.find(name);
        if (to_it != to_flags.custom_flags.end()) {
            int interpolated_value = static_cast<int>(from_value + (to_it->second - from_value) * transition_progress);
            interpolated_flags.custom_flags[name] = interpolated_value;
        } else {
            interpolated_flags.custom_flags[name] = static_cast<int>(from_value * (1.0 - transition_progress));
        }
    }
    
    // Add any flags that exist only in to_flags
    for (const auto& [name, to_value] : to_flags.custom_flags) {
        if (from_flags.custom_flags.find(name) == from_flags.custom_flags.end()) {
            interpolated_flags.custom_flags[name] = static_cast<int>(to_value * transition_progress);
        }
    }
    
    return convert(interpolated_flags);
}

std::vector<UtauFlagConverter::ConversionAnalysis> UtauFlagConverter::run_conversion_tests() {
    std::vector<ConversionAnalysis> results;
    
    // Define test cases covering various flag combinations
    std::vector<FlagValues> test_cases;
    
    // Single flag tests
    test_cases.push_back({50, 0, 0, 0});    // g+50
    test_cases.push_back({-50, 0, 0, 0});   // g-50
    test_cases.push_back({0, 50, 0, 0});    // t+50
    test_cases.push_back({0, -50, 0, 0});   // t-50
    test_cases.push_back({0, 0, 50, 0});    // bre50
    test_cases.push_back({0, 0, 0, 50});    // bri+50
    test_cases.push_back({0, 0, 0, -50});   // bri-50
    
    // Combination tests
    test_cases.push_back({30, 30, 0, 0});   // g+30 t+30
    test_cases.push_back({-30, -30, 0, 0}); // g-30 t-30
    test_cases.push_back({20, 0, 40, 0});   // g+20 bre40
    test_cases.push_back({0, 30, 0, 30});   // t+30 bri+30
    test_cases.push_back({0, 0, 50, -30});  // bre50 bri-30
    
    // Extreme value tests
    test_cases.push_back({100, 0, 0, 0});   // g+100 (maximum)
    test_cases.push_back({-100, 0, 0, 0});  // g-100 (minimum)
    test_cases.push_back({0, 100, 0, 0});   // t+100 (maximum)
    test_cases.push_back({0, 0, 100, 0});   // bre100 (maximum)
    test_cases.push_back({0, 0, 0, 100});   // bri+100 (maximum)
    test_cases.push_back({0, 0, 0, -100});  // bri-100 (minimum)
    
    // Complex combinations
    test_cases.push_back({50, -30, 20, 10}); // Mixed positive/negative
    test_cases.push_back({-20, 60, 80, -40}); // High breathiness with other flags
    test_cases.push_back({30, 30, 30, 30});   // All positive moderate
    test_cases.push_back({-30, -30, 0, -30}); // All negative (except bre)
    
    // Run tests for each case
    for (const auto& test_flags : test_cases) {
        auto converted_params = convert(test_flags);
        auto analysis = analyze_conversion(test_flags, converted_params);
        results.push_back(analysis);
    }
    
    return results;
}

// Utility functions
namespace FlagConversionUtils {

ConversionConfig create_voice_type_config(VoiceType voice_type) {
    ConversionConfig config;
    config.voice_type = voice_type;
    
    switch (voice_type) {
        case VoiceType::MALE_ADULT:
            config.g_sensitivity = 0.8;
            config.t_sensitivity = 1.2;
            config.max_formant_shift = 1.8;
            break;
            
        case VoiceType::FEMALE_ADULT:
            config.g_sensitivity = 1.3;
            config.t_sensitivity = 0.8;
            config.max_formant_shift = 2.2;
            break;
            
        case VoiceType::CHILD:
            config.g_sensitivity = 0.6;
            config.t_sensitivity = 0.7;
            config.preserve_naturalness = true;
            config.enable_safety_limiting = true;
            config.max_formant_shift = 1.5;
            break;
            
        case VoiceType::ROBOTIC:
            config.preserve_naturalness = false;
            config.enable_safety_limiting = false;
            config.max_formant_shift = 3.0;
            config.max_brightness_change = 5.0;
            break;
            
        case VoiceType::WHISPER:
            config.bre_sensitivity = 2.0;
            config.t_sensitivity = 0.3;
            config.preserve_naturalness = true;
            break;
            
        case VoiceType::GROWL:
            config.t_sensitivity = 1.8;
            config.bri_sensitivity = 0.7;
            config.preserve_naturalness = false;
            break;
            
        default:
            // Default configuration
            break;
    }
    
    return config;
}

FlagConversionUtils::ConversionBenchmark benchmark_conversion_performance(size_t test_cases) {
    FlagConversionUtils::ConversionBenchmark benchmark;
    
    UtauFlagConverter converter;
    std::vector<double> conversion_times;
    conversion_times.reserve(test_cases);
    
    // Generate random test flag combinations
    std::vector<FlagValues> test_flags;
    test_flags.reserve(test_cases);
    
    for (size_t i = 0; i < test_cases; ++i) {
        FlagValues flags;
        flags.g = (rand() % 201) - 100;  // -100 to 100
        flags.t = (rand() % 201) - 100;  // -100 to 100
        flags.bre = rand() % 101;        // 0 to 100
        flags.bri = (rand() % 201) - 100; // -100 to 100
        test_flags.push_back(flags);
    }
    
    // Measure conversion performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& flags : test_flags) {
        auto conversion_start = std::chrono::high_resolution_clock::now();
        
        converter.convert(flags); // Conversion test
        
        auto conversion_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(conversion_end - conversion_start);
        conversion_times.push_back(duration.count());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    // Calculate total duration for potential future use
    (void)std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Calculate statistics
    benchmark.average_conversion_time_us = std::accumulate(conversion_times.begin(), conversion_times.end(), 0.0) / conversion_times.size();
    benchmark.peak_conversion_time_us = *std::max_element(conversion_times.begin(), conversion_times.end());
    benchmark.conversions_per_second = 1000000.0 / benchmark.average_conversion_time_us;
    
    // Estimate memory usage (rough approximation)
    benchmark.memory_usage_bytes = sizeof(UtauFlagConverter) + sizeof(ConversionConfig) + sizeof(NexusSynthParams);
    
    return benchmark;
}

bool validate_conversion_compatibility(
    const UtauFlagConverter& reference_converter,
    const std::vector<FlagValues>& test_flags) {
    
    UtauFlagConverter test_converter;
    const double tolerance = 0.1; // 10% tolerance for parameter differences
    
    for (const auto& flags : test_flags) {
        auto reference_params = const_cast<UtauFlagConverter&>(reference_converter).convert(flags);
        auto test_params = test_converter.convert(flags);
        
        // Compare key parameters
        double formant_diff = std::abs(reference_params.formant_shift_factor - test_params.formant_shift_factor);
        double brightness_diff = std::abs(reference_params.brightness_gain - test_params.brightness_gain);
        double breathiness_diff = std::abs(reference_params.breathiness_level - test_params.breathiness_level);
        double tension_diff = std::abs(reference_params.tension_factor - test_params.tension_factor);
        
        // Check if differences exceed tolerance
        if (formant_diff > tolerance || brightness_diff > tolerance ||
            breathiness_diff > tolerance || tension_diff > tolerance) {
            return false;
        }
    }
    
    return true;
}

} // namespace FlagConversionUtils

} // namespace utau
} // namespace nexussynth
#include "nexussynth/data_augmentation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <numeric>
#include <iomanip>
#include <limits>

namespace nexussynth {
namespace augmentation {

    // DataAugmentor Implementation
    DataAugmentor::DataAugmentor(const AugmentationConfig& config)
        : config_(config), rng_(config.random_seed) {
    }

    void DataAugmentor::set_random_seed(int seed) {
        config_.random_seed = seed;
        rng_.seed(seed);
    }

    std::vector<AugmentedData> DataAugmentor::augment_sample(
        const AudioParameters& original_params, const std::string& label) {
        
        std::vector<AugmentedData> augmented_samples;
        
        // Add original if configured
        if (config_.preserve_original) {
            AugmentedData original;
            original.parameters = original_params;
            original.original_label = label;
            original.augmented_label = label;
            original.augmentation_type = "original";
            augmented_samples.push_back(original);
        }
        
        // Apply pitch shifting
        if (config_.enable_pitch_shift) {
            double pitch_shift = generate_random_pitch_shift();
            auto pitched_params = apply_pitch_shift(original_params, pitch_shift);
            
            // Validate quality
            auto quality = validate_quality(original_params, pitched_params);
            if (quality.passes_quality_check) {
                AugmentedData augmented;
                augmented.parameters = pitched_params;
                augmented.original_label = label;
                augmented.augmented_label = label;  // Phoneme labels typically don't change
                augmented.pitch_shift_semitones = pitch_shift;
                augmented.augmentation_type = "pitch_shift";
                augmented_samples.push_back(augmented);
            } else {
                stats_.quality_failures++;
            }
        }
        
        // Apply time stretching
        if (config_.enable_time_stretch) {
            double stretch_factor = generate_random_time_stretch();
            auto stretched_params = apply_time_stretch(original_params, stretch_factor);
            
            auto quality = validate_quality(original_params, stretched_params);
            if (quality.passes_quality_check) {
                AugmentedData augmented;
                augmented.parameters = stretched_params;
                augmented.original_label = label;
                augmented.augmented_label = label;
                augmented.time_stretch_factor = stretch_factor;
                augmented.augmentation_type = "time_stretch";
                augmented_samples.push_back(augmented);
            } else {
                stats_.quality_failures++;
            }
        }
        
        // Apply noise injection
        if (config_.enable_noise_injection) {
            std::uniform_real_distribution<double> noise_prob_dist(0.0, 1.0);
            if (noise_prob_dist(rng_) < config_.noise_probability) {
                double noise_level = generate_random_noise_level();
                auto noisy_params = apply_noise_injection(original_params, noise_level);
                
                auto quality = validate_quality(original_params, noisy_params);
                if (quality.passes_quality_check) {
                    AugmentedData augmented;
                    augmented.parameters = noisy_params;
                    augmented.original_label = label;
                    augmented.augmented_label = label;
                    augmented.noise_level_db = noise_level;
                    augmented.augmentation_type = "noise_injection";
                    augmented_samples.push_back(augmented);
                } else {
                    stats_.quality_failures++;
                }
            }
        }
        
        // Apply spectral filtering
        if (config_.enable_spectral_filtering) {
            double spectral_tilt = generate_random_spectral_tilt();
            auto filtered_params = apply_spectral_filtering(original_params, spectral_tilt);
            
            auto quality = validate_quality(original_params, filtered_params);
            if (quality.passes_quality_check) {
                AugmentedData augmented;
                augmented.parameters = filtered_params;
                augmented.original_label = label;
                augmented.augmented_label = label;
                augmented.spectral_tilt_db = spectral_tilt;
                augmented.augmentation_type = "spectral_filtering";
                augmented_samples.push_back(augmented);
            } else {
                stats_.quality_failures++;
            }
        }
        
        // Update statistics
        stats_.total_samples_processed++;
        stats_.total_augmentations_generated += static_cast<int>(augmented_samples.size());
        
        return augmented_samples;
    }

    std::vector<AugmentedData> DataAugmentor::augment_batch(
        const std::vector<std::pair<AudioParameters, std::string>>& samples) {
        
        std::vector<AugmentedData> all_augmented;
        
        for (const auto& sample : samples) {
            auto augmented = augment_sample(sample.first, sample.second);
            all_augmented.insert(all_augmented.end(), augmented.begin(), augmented.end());
        }
        
        return all_augmented;
    }

    AudioParameters DataAugmentor::apply_pitch_shift(const AudioParameters& params, double semitones) {
        AudioParameters shifted_params = params;
        
        // Convert semitones to frequency ratio
        double pitch_ratio = std::pow(2.0, semitones / 12.0);
        
        // Apply pitch shift to F0
        for (size_t i = 0; i < shifted_params.f0.size(); ++i) {
            if (shifted_params.f0[i] > 0.0) {  // Only shift voiced frames
                shifted_params.f0[i] *= pitch_ratio;
                
                // Clamp to reasonable F0 range
                shifted_params.f0[i] = std::clamp(shifted_params.f0[i], 50.0, 1000.0);
            }
        }
        
        // Note: In a complete implementation, spectral envelope should also be
        // shifted to maintain formant relationships, but this requires more
        // sophisticated spectral processing
        
        return shifted_params;
    }

    AudioParameters DataAugmentor::apply_time_stretch(const AudioParameters& params, double stretch_factor) {
        AudioParameters stretched_params = params;
        
        // Interpolate F0 contour
        stretched_params.f0 = interpolate_time_series(params.f0, stretch_factor);
        
        // Interpolate spectral envelope
        stretched_params.spectrum = interpolate_spectral_series(params.spectrum, stretch_factor);
        
        // Interpolate aperiodicity
        stretched_params.aperiodicity = interpolate_spectral_series(params.aperiodicity, stretch_factor);
        
        // Update timing information
        stretched_params.length = static_cast<int>(stretched_params.f0.size());
        stretched_params.time_axis.clear();
        for (int i = 0; i < stretched_params.length; ++i) {
            stretched_params.time_axis.push_back(i * stretched_params.frame_period / 1000.0);
        }
        
        return stretched_params;
    }

    AudioParameters DataAugmentor::apply_noise_injection(const AudioParameters& params, double noise_level_db) {
        AudioParameters noisy_params = params;
        
        // Convert dB to linear scale
        double noise_variance = std::pow(10.0, noise_level_db / 20.0);
        
        // Add noise to spectral envelope
        for (size_t frame = 0; frame < noisy_params.spectrum.size(); ++frame) {
            auto noise = generate_gaussian_noise(static_cast<int>(noisy_params.spectrum[frame].size()), 
                                               noise_variance);
            for (size_t bin = 0; bin < noisy_params.spectrum[frame].size(); ++bin) {
                // Add noise in log domain (multiplicative in linear domain)
                noisy_params.spectrum[frame][bin] = std::log(
                    std::max(1e-10, std::exp(noisy_params.spectrum[frame][bin]) + noise[bin]));
            }
        }
        
        // Add smaller amount of noise to aperiodicity
        for (size_t frame = 0; frame < noisy_params.aperiodicity.size(); ++frame) {
            auto noise = generate_gaussian_noise(static_cast<int>(noisy_params.aperiodicity[frame].size()),
                                               noise_variance * 0.1);  // 10% of spectral noise
            for (size_t bin = 0; bin < noisy_params.aperiodicity[frame].size(); ++bin) {
                noisy_params.aperiodicity[frame][bin] = std::clamp(
                    noisy_params.aperiodicity[frame][bin] + noise[bin], 0.0, 1.0);
            }
        }
        
        return noisy_params;
    }

    AudioParameters DataAugmentor::apply_spectral_filtering(const AudioParameters& params, double tilt_db) {
        AudioParameters filtered_params = params;
        
        // Apply spectral tilt to each frame
        for (size_t frame = 0; frame < filtered_params.spectrum.size(); ++frame) {
            apply_spectral_tilt(filtered_params.spectrum[frame], tilt_db, params.sample_rate);
        }
        
        return filtered_params;
    }

    QualityMetrics DataAugmentor::validate_quality(const AudioParameters& original, 
                                                  const AudioParameters& augmented) {
        QualityMetrics metrics;
        
        // Calculate spectral distortion
        metrics.spectral_distortion = calculate_spectral_distortion(original.spectrum, augmented.spectrum);
        
        // Calculate F0 continuity
        metrics.f0_continuity_score = calculate_f0_continuity(augmented.f0);
        
        // Calculate dynamic range preservation
        double orig_range = calculate_dynamic_range(original.spectrum);
        double aug_range = calculate_dynamic_range(augmented.spectrum);
        metrics.dynamic_range_ratio = (orig_range > 0.0) ? aug_range / orig_range : 1.0;
        
        // Overall quality assessment
        metrics.passes_quality_check = (
            metrics.spectral_distortion < 2.0 &&  // Max 2 dB spectral distortion
            metrics.f0_continuity_score > 0.7 &&  // Min 70% F0 continuity
            metrics.dynamic_range_ratio > 0.5 &&  // Preserve at least 50% dynamic range
            metrics.dynamic_range_ratio < 2.0     // Don't exceed 200% dynamic range
        );
        
        // Record issues
        if (!metrics.passes_quality_check) {
            std::ostringstream issues;
            if (metrics.spectral_distortion >= 2.0) issues << "High spectral distortion; ";
            if (metrics.f0_continuity_score <= 0.7) issues << "Poor F0 continuity; ";
            if (metrics.dynamic_range_ratio <= 0.5 || metrics.dynamic_range_ratio >= 2.0) 
                issues << "Dynamic range issues; ";
            metrics.quality_issues = issues.str();
        }
        
        return metrics;
    }

    // Helper method implementations
    std::vector<double> DataAugmentor::interpolate_time_series(const std::vector<double>& original,
                                                             double stretch_factor) {
        if (original.empty()) return {};
        
        int new_length = static_cast<int>(original.size() / stretch_factor);
        std::vector<double> interpolated(new_length);
        
        for (int i = 0; i < new_length; ++i) {
            double orig_index = i * stretch_factor;
            int base_index = static_cast<int>(orig_index);
            double fraction = orig_index - base_index;
            
            if (base_index >= static_cast<int>(original.size()) - 1) {
                interpolated[i] = original.back();
            } else {
                // Linear interpolation
                interpolated[i] = original[base_index] * (1.0 - fraction) + 
                                original[base_index + 1] * fraction;
            }
        }
        
        return interpolated;
    }

    std::vector<std::vector<double>> DataAugmentor::interpolate_spectral_series(
        const std::vector<std::vector<double>>& original, double stretch_factor) {
        
        if (original.empty()) return {};
        
        int new_length = static_cast<int>(original.size() / stretch_factor);
        std::vector<std::vector<double>> interpolated(new_length);
        
        for (int i = 0; i < new_length; ++i) {
            double orig_index = i * stretch_factor;
            int base_index = static_cast<int>(orig_index);
            double fraction = orig_index - base_index;
            
            if (base_index >= static_cast<int>(original.size()) - 1) {
                interpolated[i] = original.back();
            } else {
                // Linear interpolation of spectral vectors
                interpolated[i].resize(original[base_index].size());
                for (size_t bin = 0; bin < original[base_index].size(); ++bin) {
                    interpolated[i][bin] = original[base_index][bin] * (1.0 - fraction) +
                                         original[base_index + 1][bin] * fraction;
                }
            }
        }
        
        return interpolated;
    }

    void DataAugmentor::apply_spectral_tilt(std::vector<double>& spectrum, double tilt_db, double sample_rate) {
        int num_bins = static_cast<int>(spectrum.size());
        double nyquist = sample_rate / 2.0;
        
        for (int bin = 0; bin < num_bins; ++bin) {
            double frequency = (bin * nyquist) / num_bins;
            double normalized_freq = frequency / nyquist;  // 0 to 1
            
            // Apply linear tilt in dB
            double tilt_factor = tilt_db * normalized_freq;
            double linear_factor = std::pow(10.0, tilt_factor / 20.0);
            
            // Apply tilt (spectrum is in log domain)
            spectrum[bin] += std::log(linear_factor);
        }
    }

    double DataAugmentor::calculate_spectral_distortion(
        const std::vector<std::vector<double>>& orig_spectrum,
        const std::vector<std::vector<double>>& aug_spectrum) {
        
        if (orig_spectrum.empty() || aug_spectrum.empty()) return 0.0;
        
        double total_distortion = 0.0;
        int frame_count = 0;
        
        int min_frames = std::min(static_cast<int>(orig_spectrum.size()), 
                                static_cast<int>(aug_spectrum.size()));
        
        for (int frame = 0; frame < min_frames; ++frame) {
            if (orig_spectrum[frame].empty() || aug_spectrum[frame].empty()) continue;
            
            int min_bins = std::min(static_cast<int>(orig_spectrum[frame].size()),
                                  static_cast<int>(aug_spectrum[frame].size()));
            
            double frame_distortion = 0.0;
            for (int bin = 0; bin < min_bins; ++bin) {
                double diff = orig_spectrum[frame][bin] - aug_spectrum[frame][bin];
                frame_distortion += diff * diff;
            }
            
            total_distortion += std::sqrt(frame_distortion / min_bins);
            frame_count++;
        }
        
        return (frame_count > 0) ? total_distortion / frame_count : 0.0;
    }

    double DataAugmentor::calculate_f0_continuity(const std::vector<double>& f0_contour) {
        if (f0_contour.size() < 2) return 1.0;
        
        int voiced_frames = 0;
        int continuous_transitions = 0;
        
        for (size_t i = 1; i < f0_contour.size(); ++i) {
            if (f0_contour[i] > 0.0 && f0_contour[i-1] > 0.0) {
                voiced_frames++;
                
                // Check for reasonable F0 transition (< 20% change)
                double ratio = f0_contour[i] / f0_contour[i-1];
                if (ratio > 0.8 && ratio < 1.25) {
                    continuous_transitions++;
                }
            }
        }
        
        return (voiced_frames > 0) ? static_cast<double>(continuous_transitions) / voiced_frames : 1.0;
    }

    double DataAugmentor::calculate_dynamic_range(const std::vector<std::vector<double>>& spectrum) {
        if (spectrum.empty()) return 0.0;
        
        double min_energy = std::numeric_limits<double>::infinity();
        double max_energy = -std::numeric_limits<double>::infinity();
        
        for (const auto& frame : spectrum) {
            for (double value : frame) {
                min_energy = std::min(min_energy, value);
                max_energy = std::max(max_energy, value);
            }
        }
        
        return max_energy - min_energy;
    }

    std::vector<double> DataAugmentor::generate_gaussian_noise(int length, double variance) {
        std::vector<double> noise(length);
        std::normal_distribution<double> dist(0.0, std::sqrt(variance));
        
        for (int i = 0; i < length; ++i) {
            noise[i] = dist(rng_);
        }
        
        return noise;
    }

    std::vector<std::vector<double>> DataAugmentor::generate_spectral_noise(
        const std::vector<std::vector<double>>& template_spectrum, double variance) {
        
        std::vector<std::vector<double>> noise;
        noise.reserve(template_spectrum.size());
        
        for (const auto& frame : template_spectrum) {
            noise.push_back(generate_gaussian_noise(static_cast<int>(frame.size()), variance));
        }
        
        return noise;
    }

    bool DataAugmentor::validate_f0_contour(const std::vector<double>& f0) {
        for (double f0_value : f0) {
            if (f0_value < 0.0 || (f0_value > 0.0 && (f0_value < 50.0 || f0_value > 1000.0))) {
                return false;
            }
        }
        return true;
    }

    bool DataAugmentor::validate_spectral_envelope(const std::vector<std::vector<double>>& spectrum) {
        for (const auto& frame : spectrum) {
            for (double value : frame) {
                if (!std::isfinite(value)) return false;
            }
        }
        return true;
    }

    bool DataAugmentor::validate_aperiodicity(const std::vector<std::vector<double>>& aperiodicity) {
        for (const auto& frame : aperiodicity) {
            for (double value : frame) {
                if (!std::isfinite(value) || value < 0.0 || value > 1.0) return false;
            }
        }
        return true;
    }

    double DataAugmentor::generate_random_pitch_shift() {
        std::uniform_real_distribution<double> dist(config_.min_pitch_shift_semitones,
                                                   config_.max_pitch_shift_semitones);
        return dist(rng_);
    }

    double DataAugmentor::generate_random_time_stretch() {
        std::uniform_real_distribution<double> dist(config_.min_time_stretch_factor,
                                                   config_.max_time_stretch_factor);
        return dist(rng_);
    }

    double DataAugmentor::generate_random_noise_level() {
        // Generate noise level around the configured variance with some randomness
        std::normal_distribution<double> dist(config_.noise_variance_db, 5.0);  // ±5dB variation
        return std::clamp(dist(rng_), config_.noise_variance_db - 10.0, config_.noise_variance_db + 10.0);
    }

    double DataAugmentor::generate_random_spectral_tilt() {
        std::uniform_real_distribution<double> dist(-config_.spectral_tilt_range,
                                                   config_.spectral_tilt_range);
        return dist(rng_);
    }

    // LabelManager Implementation
    std::string LabelManager::generate_augmented_label(const std::string& original_label,
                                                      const AugmentedData& augmentation_metadata) {
        // For phoneme-level labels, typically no change is needed
        // Future extensions could handle prosodic or acoustic modifications
        return original_label;
    }

    bool LabelManager::validate_label_consistency(const std::string& original_label,
                                                 const std::string& augmented_label) {
        // For now, simple equality check
        // Future versions could implement more sophisticated consistency rules
        return original_label == augmented_label;
    }

    bool LabelManager::save_training_manifest(const std::vector<AugmentedData>& augmented_data,
                                             const std::string& output_path) {
        std::ofstream manifest_file(output_path);
        if (!manifest_file.is_open()) return false;
        
        // Write header
        manifest_file << "# NexusSynth Data Augmentation Training Manifest\n";
        manifest_file << "# Format: augmentation_type,original_label,augmented_label,pitch_shift,time_stretch,noise_level,spectral_tilt\n";
        
        for (const auto& data : augmented_data) {
            manifest_file << data.augmentation_type << ","
                         << data.original_label << ","
                         << data.augmented_label << ","
                         << data.pitch_shift_semitones << ","
                         << data.time_stretch_factor << ","
                         << data.noise_level_db << ","
                         << data.spectral_tilt_db << "\n";
        }
        
        return true;
    }

    std::vector<AugmentedData> LabelManager::load_training_manifest(const std::string& manifest_path) {
        std::vector<AugmentedData> augmented_data;
        std::ifstream manifest_file(manifest_path);
        
        if (!manifest_file.is_open()) return augmented_data;
        
        std::string line;
        while (std::getline(manifest_file, line)) {
            if (line.empty() || line[0] == '#') continue;  // Skip comments and empty lines
            
            std::istringstream iss(line);
            std::string token;
            AugmentedData data;
            
            // Parse CSV format
            if (std::getline(iss, token, ',')) data.augmentation_type = token;
            if (std::getline(iss, token, ',')) data.original_label = token;
            if (std::getline(iss, token, ',')) data.augmented_label = token;
            if (std::getline(iss, token, ',')) data.pitch_shift_semitones = std::stod(token);
            if (std::getline(iss, token, ',')) data.time_stretch_factor = std::stod(token);
            if (std::getline(iss, token, ',')) data.noise_level_db = std::stod(token);
            if (std::getline(iss, token, ',')) data.spectral_tilt_db = std::stod(token);
            
            augmented_data.push_back(data);
        }
        
        return augmented_data;
    }

    // AugmentationPipeline Implementation
    AugmentationPipeline::AugmentationPipeline(const AugmentationConfig& config)
        : augmentor_(config) {
    }

    int AugmentationPipeline::process_dataset(
        const std::vector<std::pair<AudioParameters, std::string>>& input_dataset,
        const std::string& output_path) {
        
        errors_.clear();
        int successful_count = 0;
        
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_path);
        
        auto augmented_data = augmentor_.augment_batch(input_dataset);
        
        // Save augmented data
        for (size_t i = 0; i < augmented_data.size(); ++i) {
            std::string output_filename = output_path + "/augmented_" + 
                                        std::to_string(i) + "_" + 
                                        augmented_data[i].augmentation_type + ".json";
            
            // Here you would save the AudioParameters to JSON
            // This requires implementing JSON serialization for AudioParameters
            // For now, we'll just count as successful
            successful_count++;
            
            if (progress_callback_) {
                progress_callback_(static_cast<int>(i + 1), static_cast<int>(augmented_data.size()), 
                                 output_filename);
            }
        }
        
        // Save training manifest
        label_manager_.save_training_manifest(augmented_data, output_path + "/training_manifest.csv");
        
        return successful_count;
    }

    int AugmentationPipeline::process_directory(const std::string& input_directory,
                                              const std::string& output_directory) {
        errors_.clear();
        int processed_count = 0;
        
        // Create output directory
        std::filesystem::create_directories(output_directory);
        
        // Process all JSON files in input directory
        for (const auto& entry : std::filesystem::directory_iterator(input_directory)) {
            if (entry.path().extension() == ".json") {
                if (process_single_file(entry.path().string(), output_directory)) {
                    processed_count++;
                }
                
                if (progress_callback_) {
                    progress_callback_(processed_count, -1, entry.path().filename().string());
                }
            }
        }
        
        return processed_count;
    }

    bool AugmentationPipeline::process_single_file(const std::string& input_file,
                                                  const std::string& output_directory) {
        try {
            // This would require implementing AudioParameters JSON loading
            // For now, return true as placeholder
            return true;
        } catch (const std::exception& e) {
            ProcessingError error;
            error.filename = input_file;
            error.error_message = e.what();
            error.augmentation_type = "file_processing";
            errors_.push_back(error);
            return false;
        }
    }

    std::string AugmentationPipeline::generate_output_filename(const std::string& original_filename,
                                                              const AugmentedData& augmented_data) {
        std::filesystem::path original_path(original_filename);
        std::string base_name = original_path.stem().string();
        std::string extension = original_path.extension().string();
        
        std::ostringstream output_name;
        output_name << base_name << "_" << augmented_data.augmentation_type;
        
        if (augmented_data.pitch_shift_semitones != 0.0) {
            output_name << "_pitch" << std::fixed << std::setprecision(1) 
                       << augmented_data.pitch_shift_semitones;
        }
        
        if (augmented_data.time_stretch_factor != 1.0) {
            output_name << "_stretch" << std::fixed << std::setprecision(2) 
                       << augmented_data.time_stretch_factor;
        }
        
        output_name << extension;
        return output_name.str();
    }

} // namespace augmentation
} // namespace nexussynth
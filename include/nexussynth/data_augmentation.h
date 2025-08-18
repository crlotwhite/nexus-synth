#pragma once

#include "world_wrapper.h"
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <functional>

namespace nexussynth {
namespace augmentation {

    /**
     * @brief Configuration for data augmentation parameters
     */
    struct AugmentationConfig {
        // Pitch shifting parameters
        double min_pitch_shift_semitones;     // Minimum pitch shift (default: -2.0)
        double max_pitch_shift_semitones;     // Maximum pitch shift (default: +2.0)
        bool enable_pitch_shift;              // Enable pitch shifting (default: true)
        
        // Time stretching parameters
        double min_time_stretch_factor;       // Minimum time stretch (default: 0.8)
        double max_time_stretch_factor;       // Maximum time stretch (default: 1.2)
        bool enable_time_stretch;             // Enable time stretching (default: true)
        
        // Noise injection parameters
        double noise_variance_db;             // Noise variance in dB (default: -40.0)
        double noise_probability;             // Probability of adding noise (default: 0.5)
        bool enable_noise_injection;          // Enable noise injection (default: true)
        
        // Spectral filtering parameters
        double spectral_tilt_range;           // Spectral tilt range in dB (default: ±3.0)
        bool enable_spectral_filtering;       // Enable spectral filtering (default: true)
        
        // General parameters
        int random_seed;                      // Random seed for reproducibility
        bool preserve_original;               // Keep original in augmented set (default: true)
        
        AugmentationConfig() 
            : min_pitch_shift_semitones(-2.0), max_pitch_shift_semitones(2.0), enable_pitch_shift(true)
            , min_time_stretch_factor(0.8), max_time_stretch_factor(1.2), enable_time_stretch(true)
            , noise_variance_db(-40.0), noise_probability(0.5), enable_noise_injection(true)
            , spectral_tilt_range(3.0), enable_spectral_filtering(true)
            , random_seed(42), preserve_original(true) {}
    };

    /**
     * @brief Augmented audio data with metadata
     */
    struct AugmentedData {
        AudioParameters parameters;           // Augmented WORLD parameters
        std::string original_label;          // Original phoneme/label
        std::string augmented_label;         // Augmented label (usually same as original)
        
        // Augmentation metadata
        double pitch_shift_semitones;        // Applied pitch shift
        double time_stretch_factor;          // Applied time stretch factor
        double noise_level_db;               // Applied noise level
        double spectral_tilt_db;             // Applied spectral tilt
        std::string augmentation_type;       // Type of augmentation applied
        
        AugmentedData() 
            : pitch_shift_semitones(0.0), time_stretch_factor(1.0)
            , noise_level_db(-std::numeric_limits<double>::infinity())
            , spectral_tilt_db(0.0), augmentation_type("original") {}
    };

    /**
     * @brief Quality metrics for augmented data validation
     */
    struct QualityMetrics {
        double spectral_distortion;          // Spectral distortion measure
        double f0_continuity_score;          // F0 continuity score [0-1]
        double dynamic_range_ratio;          // Dynamic range preservation ratio
        double signal_to_noise_ratio;        // SNR after augmentation
        bool passes_quality_check;           // Overall quality assessment
        std::string quality_issues;          // Description of any quality issues
        
        QualityMetrics() 
            : spectral_distortion(0.0), f0_continuity_score(1.0)
            , dynamic_range_ratio(1.0), signal_to_noise_ratio(std::numeric_limits<double>::infinity())
            , passes_quality_check(true) {}
    };

    /**
     * @brief Core data augmentation engine
     * 
     * Implements various audio augmentation techniques on WORLD parameters
     * to increase training data diversity for robust HMM model training
     */
    class DataAugmentor {
    public:
        explicit DataAugmentor(const AugmentationConfig& config = AugmentationConfig());
        ~DataAugmentor() = default;

        // Main augmentation interface
        /**
         * @brief Augment a single audio sample
         * @param original_params Original WORLD parameters
         * @param label Associated phoneme/label
         * @return Vector of augmented samples (includes original if configured)
         */
        std::vector<AugmentedData> augment_sample(const AudioParameters& original_params,
                                                 const std::string& label);

        /**
         * @brief Augment multiple audio samples in batch
         * @param samples Vector of (parameters, label) pairs
         * @return Vector of augmented data
         */
        std::vector<AugmentedData> augment_batch(
            const std::vector<std::pair<AudioParameters, std::string>>& samples);

        // Individual augmentation methods
        /**
         * @brief Apply pitch shifting to WORLD parameters
         * @param params Original parameters
         * @param semitones Pitch shift in semitones
         * @return Pitch-shifted parameters
         */
        AudioParameters apply_pitch_shift(const AudioParameters& params, double semitones);

        /**
         * @brief Apply time stretching to WORLD parameters
         * @param params Original parameters
         * @param stretch_factor Time stretch factor (1.0 = no change)
         * @return Time-stretched parameters
         */
        AudioParameters apply_time_stretch(const AudioParameters& params, double stretch_factor);

        /**
         * @brief Apply noise injection to WORLD parameters
         * @param params Original parameters
         * @param noise_level_db Noise level in dB
         * @return Noise-injected parameters
         */
        AudioParameters apply_noise_injection(const AudioParameters& params, double noise_level_db);

        /**
         * @brief Apply spectral filtering/tilt to WORLD parameters
         * @param params Original parameters
         * @param tilt_db Spectral tilt in dB
         * @return Spectrally filtered parameters
         */
        AudioParameters apply_spectral_filtering(const AudioParameters& params, double tilt_db);

        // Quality validation
        /**
         * @brief Validate quality of augmented data
         * @param original Original parameters
         * @param augmented Augmented parameters
         * @return Quality metrics
         */
        QualityMetrics validate_quality(const AudioParameters& original, 
                                      const AudioParameters& augmented);

        // Configuration management
        void set_config(const AugmentationConfig& config) { config_ = config; }
        const AugmentationConfig& get_config() const { return config_; }

        // Random seed management
        void set_random_seed(int seed);
        
        // Statistics and reporting
        struct AugmentationStats {
            int total_samples_processed;
            int total_augmentations_generated;
            int quality_failures;
            double average_spectral_distortion;
            double average_f0_continuity;
            
            AugmentationStats() 
                : total_samples_processed(0), total_augmentations_generated(0)
                , quality_failures(0), average_spectral_distortion(0.0)
                , average_f0_continuity(1.0) {}
        };
        
        const AugmentationStats& get_stats() const { return stats_; }
        void reset_stats() { stats_ = AugmentationStats(); }

    private:
        AugmentationConfig config_;
        mutable std::mt19937 rng_;
        AugmentationStats stats_;

        // Helper methods for parameter manipulation
        std::vector<double> interpolate_time_series(const std::vector<double>& original,
                                                   double stretch_factor);
        
        std::vector<std::vector<double>> interpolate_spectral_series(
            const std::vector<std::vector<double>>& original, double stretch_factor);

        void apply_spectral_tilt(std::vector<double>& spectrum, double tilt_db, double sample_rate);
        
        double calculate_spectral_distortion(const std::vector<std::vector<double>>& orig_spectrum,
                                           const std::vector<std::vector<double>>& aug_spectrum);
        
        double calculate_f0_continuity(const std::vector<double>& f0_contour);
        
        double calculate_dynamic_range(const std::vector<std::vector<double>>& spectrum);
        
        // Noise generation utilities
        std::vector<double> generate_gaussian_noise(int length, double variance);
        std::vector<std::vector<double>> generate_spectral_noise(
            const std::vector<std::vector<double>>& template_spectrum, double variance);

        // Validation helpers
        bool validate_f0_contour(const std::vector<double>& f0);
        bool validate_spectral_envelope(const std::vector<std::vector<double>>& spectrum);
        bool validate_aperiodicity(const std::vector<std::vector<double>>& aperiodicity);
        
        // Random parameter generation
        double generate_random_pitch_shift();
        double generate_random_time_stretch();
        double generate_random_noise_level();
        double generate_random_spectral_tilt();
    };

    /**
     * @brief Label management for augmented training data
     * 
     * Handles automatic generation and validation of labels for augmented samples
     */
    class LabelManager {
    public:
        LabelManager() = default;

        /**
         * @brief Generate augmented label from original label and augmentation metadata
         * @param original_label Original phoneme/label
         * @param augmentation_metadata Augmentation parameters applied
         * @return Augmented label (usually same as original for phoneme-level labels)
         */
        std::string generate_augmented_label(const std::string& original_label,
                                           const AugmentedData& augmentation_metadata);

        /**
         * @brief Validate label consistency across augmentation
         * @param original_label Original label
         * @param augmented_label Generated augmented label
         * @return true if labels are consistent
         */
        bool validate_label_consistency(const std::string& original_label,
                                      const std::string& augmented_label);

        /**
         * @brief Generate training manifest with augmented data
         * @param augmented_data Vector of augmented samples
         * @param output_path Path to save training manifest
         * @return true if successful
         */
        bool save_training_manifest(const std::vector<AugmentedData>& augmented_data,
                                   const std::string& output_path);

        /**
         * @brief Load training manifest
         * @param manifest_path Path to training manifest
         * @return Vector of augmented data
         */
        std::vector<AugmentedData> load_training_manifest(const std::string& manifest_path);
    };

    /**
     * @brief High-level augmentation pipeline
     * 
     * Orchestrates the complete data augmentation workflow
     */
    class AugmentationPipeline {
    public:
        AugmentationPipeline(const AugmentationConfig& config = AugmentationConfig());
        
        /**
         * @brief Process a dataset through the complete augmentation pipeline
         * @param input_dataset Vector of (parameters, label) pairs
         * @param output_path Directory to save augmented dataset
         * @return Number of successfully augmented samples
         */
        int process_dataset(const std::vector<std::pair<AudioParameters, std::string>>& input_dataset,
                          const std::string& output_path);

        /**
         * @brief Process dataset from directory of WORLD parameter files
         * @param input_directory Directory containing WORLD parameter JSON files
         * @param output_directory Directory to save augmented dataset
         * @return Number of successfully processed files
         */
        int process_directory(const std::string& input_directory,
                            const std::string& output_directory);

        // Progress callback support
        using ProgressCallback = std::function<void(int processed, int total, const std::string& current_file)>;
        void set_progress_callback(ProgressCallback callback) { progress_callback_ = callback; }

        // Error handling
        struct ProcessingError {
            std::string filename;
            std::string error_message;
            std::string augmentation_type;
        };
        
        const std::vector<ProcessingError>& get_errors() const { return errors_; }
        void clear_errors() { errors_.clear(); }

    private:
        DataAugmentor augmentor_;
        LabelManager label_manager_;
        ProgressCallback progress_callback_;
        std::vector<ProcessingError> errors_;
        
        // Internal processing helpers
        bool process_single_file(const std::string& input_file,
                               const std::string& output_directory);
        
        std::string generate_output_filename(const std::string& original_filename,
                                           const AugmentedData& augmented_data);
    };

} // namespace augmentation
} // namespace nexussynth
#pragma once

#include "hmm_trainer.h"
#include "context_features.h"
#include "context_feature_extractor.h"
#include "label_file_generator.h"
#include <string>
#include <vector>
#include <map>

namespace nexussynth {
namespace bridge {

    /**
     * @brief Configuration for context-dependent HMM training
     */
    struct ContextHmmConfig {
        int feature_dimension;              // Dimension of acoustic features (e.g., 39 for MFCC)
        int num_mixtures_per_state;         // Number of Gaussian mixtures per HMM state
        int num_states_per_phoneme;         // Number of states per phoneme (default: 5)
        bool use_delta_features;            // Include delta and delta-delta features
        bool use_context_clustering;        // Use context-dependent clustering
        hmm::PhonemeEncoding encoding_type; // How to encode phoneme contexts
        
        ContextHmmConfig()
            : feature_dimension(39)
            , num_mixtures_per_state(1)
            , num_states_per_phoneme(5)
            , use_delta_features(true)
            , use_context_clustering(true)
            , encoding_type(hmm::PhonemeEncoding::CATEGORICAL) {}
    };

    /**
     * @brief Training data bundle containing all necessary information
     */
    struct TrainingDataBundle {
        std::vector<context::ContextFeatures> context_features;
        std::vector<std::vector<Eigen::VectorXd>> acoustic_features;  // Acoustic feature sequences
        std::vector<context::PhonemeTimingInfo> timing_info;
        std::string utterance_id;
        
        bool is_valid() const {
            return !context_features.empty() && 
                   !acoustic_features.empty() && 
                   context_features.size() == timing_info.size();
        }
    };

    /**
     * @brief Bridge between context features and HMM training system
     * 
     * Handles the conversion from UTAU/linguistic context to trainable HMM models
     * and manages the training pipeline from context features to trained HMMs
     */
    class ContextHmmBridge {
    public:
        explicit ContextHmmBridge(const ContextHmmConfig& config = ContextHmmConfig());
        
        // Training data preparation
        std::map<std::string, hmm::PhonemeHmm> initialize_hmm_models(
            const std::vector<TrainingDataBundle>& training_data);
        
        std::map<std::string, std::vector<std::vector<Eigen::VectorXd>>> prepare_training_sequences(
            const std::vector<TrainingDataBundle>& training_data,
            const std::map<std::string, hmm::PhonemeHmm>& models) const;
        
        // Context feature to HMM conversion
        hmm::PhonemeHmm create_hmm_from_context(const context::ContextFeatures& context_features);
        
        std::string generate_model_name(const context::ContextFeatures& context_features) const;
        std::string generate_model_name(const hmm::ContextFeatureVector& context_vector) const;
        
        // Feature vector conversion
        std::vector<Eigen::VectorXd> convert_context_to_features(
            const std::vector<context::ContextFeatures>& context_sequence);
        
        Eigen::VectorXd extract_acoustic_features(
            const context::ContextFeatures& context_features) const;
        
        // Training pipeline
        std::map<std::string, hmm::TrainingStats> train_context_dependent_models(
            const std::vector<TrainingDataBundle>& training_data,
            const hmm::TrainingConfig& training_config = hmm::TrainingConfig());
        
        // Model management
        void save_trained_models(const std::map<std::string, hmm::PhonemeHmm>& models,
                               const std::string& output_directory) const;
        
        std::map<std::string, hmm::PhonemeHmm> load_trained_models(
            const std::string& model_directory) const;
        
        // Evaluation and validation
        std::map<std::string, double> evaluate_models(
            const std::map<std::string, hmm::PhonemeHmm>& models,
            const std::vector<TrainingDataBundle>& test_data) const;
        
        // Label file integration
        bool generate_training_labels(
            const std::vector<TrainingDataBundle>& training_data,
            const std::string& output_directory) const;
        
        // Configuration
        void set_config(const ContextHmmConfig& config) { config_ = config; }
        const ContextHmmConfig& get_config() const { return config_; }
        
    private:
        ContextHmmConfig config_;
        hmm::PhonemeInventory phoneme_inventory_;
        hmm::FeatureNormalizer feature_normalizer_;
        
        // Context analysis
        std::map<std::string, int> analyze_context_distribution(
            const std::vector<TrainingDataBundle>& training_data) const;
        
        // Feature processing
        Eigen::VectorXd compute_delta_features(
            const std::vector<Eigen::VectorXd>& feature_sequence, int frame_index) const;
        
        std::vector<Eigen::VectorXd> add_delta_features(
            const std::vector<Eigen::VectorXd>& static_features) const;
        
        // Context clustering (for future context-dependent modeling)
        std::map<std::string, std::vector<std::string>> cluster_contexts(
            const std::vector<std::string>& context_names) const;
        
        // Normalization
        void compute_global_normalization_stats(
            const std::vector<TrainingDataBundle>& training_data);
        
        std::vector<Eigen::VectorXd> normalize_feature_sequence(
            const std::vector<Eigen::VectorXd>& features) const;
        
        // Utility methods
        bool is_silence_context(const context::ContextFeatures& context) const;
        std::string simplify_context_name(const std::string& full_context_name) const;
        
        // File I/O helpers
        void save_model_to_file(const hmm::PhonemeHmm& model, const std::string& filename) const;
        hmm::PhonemeHmm load_model_from_file(const std::string& filename) const;
    };

    /**
     * @brief Factory for creating training data from various sources
     */
    namespace training_data_factory {
        
        // From UTAU oto.ini and WAV files
        std::vector<TrainingDataBundle> create_from_utau_voicebank(
            const std::string& voicebank_directory);
        
        // From context features and acoustic analysis
        TrainingDataBundle create_from_context_sequence(
            const std::vector<context::ContextFeatures>& context_sequence,
            const std::vector<double>& audio_samples,
            double sample_rate,
            const std::string& utterance_id = "");
        
        // From label files (for compatibility with existing HTS data)
        std::vector<TrainingDataBundle> create_from_label_files(
            const std::vector<std::string>& label_files,
            const std::vector<std::string>& feature_files);
        
        // Synthetic data for testing
        std::vector<TrainingDataBundle> create_synthetic_data(
            int num_utterances, int avg_length_frames,
            const std::vector<std::string>& phoneme_set);
        
    } // namespace training_data_factory

    /**
     * @brief High-level training pipeline coordinator
     */
    class HmmTrainingPipeline {
    public:
        struct PipelineConfig {
            ContextHmmConfig context_config;
            hmm::TrainingConfig training_config;
            std::string output_directory;
            bool generate_labels;
            bool run_validation;
            double validation_split;
            bool verbose;
            
            PipelineConfig() 
                : output_directory("./hmm_models")
                , generate_labels(true)
                , run_validation(true)
                , validation_split(0.1)
                , verbose(true) {}
        };
        
        explicit HmmTrainingPipeline(const PipelineConfig& config = PipelineConfig());
        
        // Full training pipeline
        bool run_training_pipeline(const std::vector<TrainingDataBundle>& training_data);
        
        // Step-by-step training
        bool prepare_training_data(const std::vector<TrainingDataBundle>& training_data);
        bool train_models();
        bool validate_models();
        bool save_results();
        
        // Results access
        const std::map<std::string, hmm::PhonemeHmm>& get_trained_models() const { return trained_models_; }
        const std::map<std::string, hmm::TrainingStats>& get_training_stats() const { return training_stats_; }
        
        // Configuration
        void set_config(const PipelineConfig& config) { config_ = config; }
        const PipelineConfig& get_config() const { return config_; }
        
    private:
        PipelineConfig config_;
        ContextHmmBridge bridge_;
        
        std::vector<TrainingDataBundle> training_data_;
        std::vector<TrainingDataBundle> validation_data_;
        std::map<std::string, hmm::PhonemeHmm> trained_models_;
        std::map<std::string, hmm::TrainingStats> training_stats_;
        
        // Pipeline steps
        void split_training_validation();
        void log_pipeline_progress(const std::string& step, const std::string& message) const;
    };

} // namespace bridge
} // namespace nexussynth
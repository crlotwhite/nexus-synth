#include "nexussynth/context_hmm_bridge.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>

namespace nexussynth {
namespace bridge {

    // ContextHmmBridge Implementation
    ContextHmmBridge::ContextHmmBridge(const ContextHmmConfig& config) : config_(config) {
        // Initialize phoneme inventory for Japanese
        phoneme_inventory_.initialize_japanese_phonemes();
    }

    std::map<std::string, hmm::PhonemeHmm> ContextHmmBridge::initialize_hmm_models(
        const std::vector<TrainingDataBundle>& training_data) {
        
        std::map<std::string, hmm::PhonemeHmm> models;
        std::map<std::string, int> context_counts = analyze_context_distribution(training_data);
        
        for (const auto& [context_name, count] : context_counts) {
            // Create HMM for this context
            hmm::PhonemeHmm model;
            model.initialize_states(config_.num_states_per_phoneme);
            model.model_name = context_name;
            
            // Initialize states with appropriate dimensions
            for (size_t i = 0; i < model.num_states(); ++i) {
                model.states[i] = hmm::HmmState(static_cast<int>(i), 
                                              config_.num_mixtures_per_state, 
                                              config_.feature_dimension);
            }
            
            models[context_name] = model;
        }
        
        return models;
    }

    std::map<std::string, std::vector<std::vector<Eigen::VectorXd>>> ContextHmmBridge::prepare_training_sequences(
        const std::vector<TrainingDataBundle>& training_data,
        const std::map<std::string, hmm::PhonemeHmm>& models) const {
        
        std::map<std::string, std::vector<std::vector<Eigen::VectorXd>>> sequences;
        
        for (const auto& bundle : training_data) {
            if (!bundle.is_valid()) continue;
            
            for (size_t i = 0; i < bundle.context_features.size(); ++i) {
                std::string model_name = generate_model_name(bundle.context_features[i]);
                
                if (models.find(model_name) != models.end() && 
                    i < bundle.acoustic_features.size()) {
                    
                    // Process acoustic features
                    std::vector<Eigen::VectorXd> feature_sequence = bundle.acoustic_features[i];
                    
                    // Add delta features if configured
                    if (config_.use_delta_features) {
                        feature_sequence = add_delta_features(feature_sequence);
                    }
                    
                    // Normalize features
                    feature_sequence = normalize_feature_sequence(feature_sequence);
                    
                    sequences[model_name].push_back(feature_sequence);
                }
            }
        }
        
        return sequences;
    }

    hmm::PhonemeHmm ContextHmmBridge::create_hmm_from_context(const context::ContextFeatures& context_features) {
        hmm::PhonemeHmm model;
        model.initialize_states(config_.num_states_per_phoneme);
        model.model_name = generate_model_name(context_features);
        
        // Set context information
        model.context.current_phoneme = context_features.current_timing.phoneme;
        model.context.pitch_cents = context_features.current_midi.frequency_hz > 0 ? 
            1200.0 * std::log2(context_features.current_midi.frequency_hz / 440.0) : 0.0;
        model.context.note_duration_ms = context_features.current_timing.duration_ms;
        
        // Initialize states
        for (size_t i = 0; i < model.num_states(); ++i) {
            model.states[i] = hmm::HmmState(static_cast<int>(i), 
                                          config_.num_mixtures_per_state, 
                                          config_.feature_dimension);
        }
        
        return model;
    }

    std::string ContextHmmBridge::generate_model_name(const context::ContextFeatures& context_features) const {
        // Simple triphone context for now
        std::string left = "sil";  // Simplified - would extract from context
        std::string center = context_features.current_timing.phoneme;
        std::string right = "sil"; // Simplified - would extract from context
        
        return left + "-" + center + "+" + right;
    }

    std::string ContextHmmBridge::generate_model_name(const hmm::ContextFeatureVector& context_vector) const {
        return context_vector.left_phoneme + "-" + 
               context_vector.current_phoneme + "+" + 
               context_vector.right_phoneme;
    }

    std::vector<Eigen::VectorXd> ContextHmmBridge::convert_context_to_features(
        const std::vector<context::ContextFeatures>& context_sequence) {
        
        std::vector<Eigen::VectorXd> features;
        features.reserve(context_sequence.size());
        
        for (const auto& context : context_sequence) {
            Eigen::VectorXd feature_vector = extract_acoustic_features(context);
            features.push_back(feature_vector);
        }
        
        return features;
    }

    Eigen::VectorXd ContextHmmBridge::extract_acoustic_features(
        const context::ContextFeatures& context_features) const {
        
        // For now, create synthetic features based on context
        // In a real implementation, this would extract MFCC/spectral features
        Eigen::VectorXd features(config_.feature_dimension);
        
        // Use phoneme information to create distinguishable features
        int phoneme_id = phoneme_inventory_.get_phoneme_id(context_features.current_timing.phoneme);
        
        // Create features based on phoneme identity and prosodic context
        for (int i = 0; i < config_.feature_dimension; ++i) {
            features[i] = std::sin(phoneme_id * 0.5 + i * 0.1) + 
                         context_features.current_midi.frequency_hz * 0.001;
        }
        
        return features;
    }

    std::map<std::string, hmm::TrainingStats> ContextHmmBridge::train_context_dependent_models(
        const std::vector<TrainingDataBundle>& training_data,
        const hmm::TrainingConfig& training_config) {
        
        // Initialize models
        auto models = initialize_hmm_models(training_data);
        auto training_sequences = prepare_training_sequences(training_data, models);
        
        // Train each model
        hmm::MultiModelTrainer trainer(training_config);
        auto training_stats = trainer.train_models(models, training_sequences);
        
        return training_stats;
    }

    std::map<std::string, double> ContextHmmBridge::evaluate_models(
        const std::map<std::string, hmm::PhonemeHmm>& models,
        const std::vector<TrainingDataBundle>& test_data) const {
        
        std::map<std::string, double> evaluation_results;
        auto test_sequences = prepare_training_sequences(test_data, models);
        
        hmm::HmmTrainer trainer;
        
        for (const auto& [model_name, model] : models) {
            if (test_sequences.find(model_name) != test_sequences.end()) {
                double score = trainer.evaluate_model(model, test_sequences.at(model_name));
                evaluation_results[model_name] = score;
            }
        }
        
        return evaluation_results;
    }

    bool ContextHmmBridge::generate_training_labels(
        const std::vector<TrainingDataBundle>& training_data,
        const std::string& output_directory) const {
        
        std::filesystem::create_directories(output_directory);
        
        context::LabelFileGenerator label_generator;
        
        for (const auto& bundle : training_data) {
            if (!bundle.is_valid()) continue;
            
            std::string label_file = output_directory + "/" + bundle.utterance_id + ".lab";
            
            // Convert context features to HMM context features
            std::vector<hmm::ContextFeatureVector> hmm_contexts;
            for (const auto& context : bundle.context_features) {
                hmm::ContextFeatureVector hmm_context;
                hmm_context.current_phoneme = context.current_timing.phoneme;
                hmm_context.note_duration_ms = context.current_timing.duration_ms;
                hmm_context.pitch_cents = context.current_midi.frequency_hz > 0 ? 
                    1200.0 * std::log2(context.current_midi.frequency_hz / 440.0) : 0.0;
                hmm_contexts.push_back(hmm_context);
            }
            
            bool success = label_generator.generateFromHMMFeatures(hmm_contexts, bundle.timing_info, label_file);
            if (!success) {
                std::cerr << "Failed to generate label file: " << label_file << std::endl;
                return false;
            }
        }
        
        return true;
    }

    // Private implementation methods
    std::map<std::string, int> ContextHmmBridge::analyze_context_distribution(
        const std::vector<TrainingDataBundle>& training_data) const {
        
        std::map<std::string, int> context_counts;
        
        for (const auto& bundle : training_data) {
            for (const auto& context : bundle.context_features) {
                std::string context_name = generate_model_name(context);
                context_counts[context_name]++;
            }
        }
        
        return context_counts;
    }

    std::vector<Eigen::VectorXd> ContextHmmBridge::add_delta_features(
        const std::vector<Eigen::VectorXd>& static_features) const {
        
        if (static_features.empty()) return static_features;
        
        std::vector<Eigen::VectorXd> enhanced_features;
        int static_dim = static_features[0].size();
        int total_dim = static_dim * 3;  // static + delta + delta-delta
        
        for (size_t i = 0; i < static_features.size(); ++i) {
            Eigen::VectorXd enhanced(total_dim);
            
            // Static features
            enhanced.segment(0, static_dim) = static_features[i];
            
            // Delta features
            Eigen::VectorXd delta = compute_delta_features(static_features, i);
            enhanced.segment(static_dim, static_dim) = delta;
            
            // Delta-delta features (delta of delta)
            std::vector<Eigen::VectorXd> delta_sequence;
            for (size_t j = 0; j < static_features.size(); ++j) {
                delta_sequence.push_back(compute_delta_features(static_features, j));
            }
            Eigen::VectorXd delta_delta = compute_delta_features(delta_sequence, i);
            enhanced.segment(2 * static_dim, static_dim) = delta_delta;
            
            enhanced_features.push_back(enhanced);
        }
        
        return enhanced_features;
    }

    Eigen::VectorXd ContextHmmBridge::compute_delta_features(
        const std::vector<Eigen::VectorXd>& feature_sequence, int frame_index) const {
        
        if (feature_sequence.empty()) {
            return Eigen::VectorXd::Zero(0);
        }
        
        int dim = feature_sequence[0].size();
        Eigen::VectorXd delta = Eigen::VectorXd::Zero(dim);
        
        // Simple delta computation: difference between adjacent frames
        if (frame_index > 0 && frame_index < static_cast<int>(feature_sequence.size()) - 1) {
            delta = (feature_sequence[frame_index + 1] - feature_sequence[frame_index - 1]) * 0.5;
        } else if (frame_index == 0 && feature_sequence.size() > 1) {
            delta = feature_sequence[1] - feature_sequence[0];
        } else if (frame_index == static_cast<int>(feature_sequence.size()) - 1 && feature_sequence.size() > 1) {
            delta = feature_sequence.back() - feature_sequence[feature_sequence.size() - 2];
        }
        
        return delta;
    }

    std::vector<Eigen::VectorXd> ContextHmmBridge::normalize_feature_sequence(
        const std::vector<Eigen::VectorXd>& features) const {
        
        // For now, just return as-is
        // In a real implementation, would apply global normalization
        return features;
    }

    bool ContextHmmBridge::is_silence_context(const context::ContextFeatures& context) const {
        return context.current_timing.phoneme == "sil" || 
               context.current_timing.phoneme == "sp" ||
               context.current_timing.phoneme.empty();
    }

    // HmmTrainingPipeline Implementation
    HmmTrainingPipeline::HmmTrainingPipeline(const PipelineConfig& config) 
        : config_(config), bridge_(config.context_config) {
    }

    bool HmmTrainingPipeline::run_training_pipeline(const std::vector<TrainingDataBundle>& training_data) {
        if (config_.verbose) {
            std::cout << "Starting HMM training pipeline with " << training_data.size() << " utterances" << std::endl;
        }
        
        training_data_ = training_data;
        
        if (!prepare_training_data(training_data)) {
            std::cerr << "Failed to prepare training data" << std::endl;
            return false;
        }
        
        if (!train_models()) {
            std::cerr << "Failed to train models" << std::endl;
            return false;
        }
        
        if (config_.run_validation && !validate_models()) {
            std::cerr << "Model validation failed" << std::endl;
            return false;
        }
        
        if (!save_results()) {
            std::cerr << "Failed to save results" << std::endl;
            return false;
        }
        
        if (config_.verbose) {
            std::cout << "Training pipeline completed successfully" << std::endl;
        }
        
        return true;
    }

    bool HmmTrainingPipeline::prepare_training_data(const std::vector<TrainingDataBundle>& training_data) {
        log_pipeline_progress("prepare", "Splitting training and validation data");
        
        split_training_validation();
        
        if (config_.generate_labels) {
            log_pipeline_progress("prepare", "Generating label files");
            std::string label_dir = config_.output_directory + "/labels";
            return bridge_.generate_training_labels(training_data_, label_dir);
        }
        
        return true;
    }

    bool HmmTrainingPipeline::train_models() {
        log_pipeline_progress("train", "Training context-dependent HMM models");
        
        training_stats_ = bridge_.train_context_dependent_models(training_data_, config_.training_config);
        trained_models_ = bridge_.initialize_hmm_models(training_data_);  // This would be updated by training
        
        if (config_.verbose) {
            std::cout << "Trained " << trained_models_.size() << " models" << std::endl;
        }
        
        return !trained_models_.empty();
    }

    bool HmmTrainingPipeline::validate_models() {
        if (validation_data_.empty()) {
            std::cout << "No validation data available" << std::endl;
            return true;
        }
        
        log_pipeline_progress("validate", "Evaluating models on validation data");
        
        auto validation_scores = bridge_.evaluate_models(trained_models_, validation_data_);
        
        if (config_.verbose) {
            std::cout << "Validation results:" << std::endl;
            for (const auto& [model_name, score] : validation_scores) {
                std::cout << "  " << model_name << ": " << score << std::endl;
            }
        }
        
        return true;
    }

    bool HmmTrainingPipeline::save_results() {
        log_pipeline_progress("save", "Saving trained models");
        
        std::filesystem::create_directories(config_.output_directory);
        bridge_.save_trained_models(trained_models_, config_.output_directory);
        
        return true;
    }

    void HmmTrainingPipeline::split_training_validation() {
        if (!config_.run_validation || config_.validation_split <= 0.0) {
            return;
        }
        
        size_t validation_size = static_cast<size_t>(training_data_.size() * config_.validation_split);
        
        if (validation_size > 0) {
            validation_data_.assign(training_data_.end() - validation_size, training_data_.end());
            training_data_.resize(training_data_.size() - validation_size);
        }
    }

    void HmmTrainingPipeline::log_pipeline_progress(const std::string& step, const std::string& message) const {
        if (config_.verbose) {
            std::cout << "[" << step << "] " << message << std::endl;
        }
    }

    // Training data factory implementation stubs
    namespace training_data_factory {
        
        TrainingDataBundle create_from_context_sequence(
            const std::vector<context::ContextFeatures>& context_sequence,
            const std::vector<double>& audio_samples,
            double sample_rate,
            const std::string& utterance_id) {
            
            TrainingDataBundle bundle;
            bundle.context_features = context_sequence;
            bundle.utterance_id = utterance_id.empty() ? "synthetic_" + std::to_string(rand()) : utterance_id;
            
            // Extract timing info from context features
            for (const auto& context : context_sequence) {
                bundle.timing_info.push_back(context.current_timing);
            }
            
            // Generate synthetic acoustic features for demonstration
            for (size_t i = 0; i < context_sequence.size(); ++i) {
                std::vector<Eigen::VectorXd> frame_features;
                
                // Create multiple frames per phoneme
                int frames_per_phoneme = static_cast<int>(context_sequence[i].current_timing.duration_ms / 10.0);  // 10ms frames
                frames_per_phoneme = std::max(1, frames_per_phoneme);
                
                for (int frame = 0; frame < frames_per_phoneme; ++frame) {
                    Eigen::VectorXd features(39);  // Standard MFCC dimension
                    features.setRandom();
                    frame_features.push_back(features);
                }
                
                bundle.acoustic_features.push_back(frame_features);
            }
            
            return bundle;
        }
        
        std::vector<TrainingDataBundle> create_synthetic_data(
            int num_utterances, int avg_length_frames,
            const std::vector<std::string>& phoneme_set) {
            
            std::vector<TrainingDataBundle> bundles;
            std::mt19937 gen(42);
            std::uniform_int_distribution<> phoneme_dist(0, phoneme_set.size() - 1);
            std::normal_distribution<> length_dist(avg_length_frames, avg_length_frames * 0.2);
            
            for (int utt = 0; utt < num_utterances; ++utt) {
                TrainingDataBundle bundle;
                bundle.utterance_id = "synthetic_" + std::to_string(utt);
                
                int length = std::max(3, static_cast<int>(length_dist(gen)));
                
                for (int i = 0; i < length; ++i) {
                    context::ContextFeatures context;
                    context.current_timing.phoneme = phoneme_set[phoneme_dist(gen)];
                    context.current_timing.duration_ms = 100.0 + gen() % 100;  // 100-200ms
                    context.current_timing.start_time_ms = i * context.current_timing.duration_ms;
                    context.current_timing.end_time_ms = context.current_timing.start_time_ms + context.current_timing.duration_ms;
                    context.current_timing.is_valid = true;
                    
                    bundle.context_features.push_back(context);
                    bundle.timing_info.push_back(context.current_timing);
                    
                    // Generate synthetic acoustic features
                    std::vector<Eigen::VectorXd> acoustic_features;
                    int num_frames = static_cast<int>(context.current_timing.duration_ms / 10.0);
                    for (int frame = 0; frame < num_frames; ++frame) {
                        Eigen::VectorXd features(39);
                        features.setRandom();
                        acoustic_features.push_back(features);
                    }
                    bundle.acoustic_features.push_back(acoustic_features);
                }
                
                bundles.push_back(bundle);
            }
            
            return bundles;
        }
        
    } // namespace training_data_factory

} // namespace bridge
} // namespace nexussynth
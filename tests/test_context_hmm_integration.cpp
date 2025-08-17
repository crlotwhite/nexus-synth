#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>

// Include the integration bridge
#include "../src/context_hmm_bridge.cpp"

using namespace nexussynth::bridge;
using namespace nexussynth;

/**
 * @brief Integration test for context features + HMM training system
 */
class ContextHmmIntegrationTest {
public:
    void run_all_tests() {
        std::cout << "=== Context-HMM Integration Test Suite ===" << std::endl;
        
        test_training_data_creation();
        test_context_to_hmm_conversion();
        test_feature_extraction();
        test_model_initialization();
        test_end_to_end_training();
        
        std::cout << "\n✓ All integration tests passed!" << std::endl;
    }
    
private:
    void test_training_data_creation() {
        std::cout << "\n--- Testing Training Data Creation ---" << std::endl;
        
        // Create synthetic training data
        std::vector<std::string> phoneme_set = {"a", "ka", "sa", "ta", "na"};
        auto training_bundles = training_data_factory::create_synthetic_data(3, 5, phoneme_set);
        
        assert(training_bundles.size() == 3);
        
        for (const auto& bundle : training_bundles) {
            assert(bundle.is_valid());
            assert(!bundle.context_features.empty());
            assert(!bundle.acoustic_features.empty());
            assert(bundle.context_features.size() == bundle.timing_info.size());
            assert(bundle.context_features.size() == bundle.acoustic_features.size());
            
            std::cout << "Bundle " << bundle.utterance_id << ": " 
                      << bundle.context_features.size() << " phonemes" << std::endl;
        }
        
        std::cout << "✓ Training data creation working correctly" << std::endl;
    }
    
    void test_context_to_hmm_conversion() {
        std::cout << "\n--- Testing Context to HMM Conversion ---" << std::endl;
        
        ContextHmmBridge bridge;
        
        // Create sample context features
        context::ContextFeatures context;
        context.current_timing.phoneme = "ka";
        context.current_timing.duration_ms = 150.0;
        context.current_timing.start_time_ms = 0.0;
        context.current_timing.end_time_ms = 150.0;
        context.current_timing.is_valid = true;
        
        context.current_midi.note_number = 60;  // C4
        context.current_midi.frequency_hz = 261.63;
        
        // Convert to HMM
        hmm::PhonemeHmm hmm_model = bridge.create_hmm_from_context(context);
        
        assert(hmm_model.num_states() == 5);  // Default 5-state HMM
        assert(!hmm_model.model_name.empty());
        assert(hmm_model.context.current_phoneme == "ka");
        
        // Check that all states are initialized
        for (size_t i = 0; i < hmm_model.num_states(); ++i) {
            assert(hmm_model.states[i].state_id == static_cast<int>(i));
            assert(hmm_model.states[i].feature_dimension() > 0);
        }
        
        std::cout << "✓ Context to HMM conversion working correctly" << std::endl;
        std::cout << "  Model name: " << hmm_model.model_name << std::endl;
        std::cout << "  States: " << hmm_model.num_states() << std::endl;
    }
    
    void test_feature_extraction() {
        std::cout << "\n--- Testing Feature Extraction ---" << std::endl;
        
        ContextHmmBridge bridge;
        
        // Create context sequence
        std::vector<context::ContextFeatures> context_sequence;
        for (int i = 0; i < 3; ++i) {
            context::ContextFeatures context;
            context.current_timing.phoneme = (i == 0) ? "ka" : (i == 1) ? "sa" : "ki";
            context.current_timing.duration_ms = 150.0;
            context.current_midi.frequency_hz = 261.63 + i * 50;  // Different pitches
            context_sequence.push_back(context);
        }
        
        // Extract features
        auto feature_vectors = bridge.convert_context_to_features(context_sequence);
        
        assert(feature_vectors.size() == 3);
        
        for (size_t i = 0; i < feature_vectors.size(); ++i) {
            assert(feature_vectors[i].size() > 0);
            assert(std::isfinite(feature_vectors[i].norm()));
            
            // Features should be different for different phonemes
            if (i > 0) {
                double distance = (feature_vectors[i] - feature_vectors[i-1]).norm();
                assert(distance > 0.01);  // Should be distinguishable
            }
        }
        
        std::cout << "✓ Feature extraction working correctly" << std::endl;
        std::cout << "  Feature dimension: " << feature_vectors[0].size() << std::endl;
    }
    
    void test_model_initialization() {
        std::cout << "\n--- Testing Model Initialization ---" << std::endl;
        
        ContextHmmBridge bridge;
        
        // Create training data
        auto training_data = training_data_factory::create_synthetic_data(5, 4, {"a", "ka", "sa"});
        
        // Initialize models
        auto models = bridge.initialize_hmm_models(training_data);
        
        assert(!models.empty());
        std::cout << "  Initialized " << models.size() << " models" << std::endl;
        
        for (const auto& [model_name, model] : models) {
            assert(!model_name.empty());
            assert(model.num_states() == 5);
            assert(!model.model_name.empty());
            
            std::cout << "  Model: " << model_name << std::endl;
        }
        
        std::cout << "✓ Model initialization working correctly" << std::endl;
    }
    
    void test_end_to_end_training() {
        std::cout << "\n--- Testing End-to-End Training Pipeline ---" << std::endl;
        
        // Configure pipeline
        HmmTrainingPipeline::PipelineConfig config;
        config.context_config.feature_dimension = 20;  // Smaller for faster testing
        config.context_config.num_mixtures_per_state = 1;
        config.training_config.max_iterations = 5;  // Quick training
        config.training_config.verbose = false;
        config.output_directory = "./test_hmm_output";
        config.run_validation = true;
        config.validation_split = 0.2;
        config.verbose = true;
        
        HmmTrainingPipeline pipeline(config);
        
        // Create synthetic training data
        std::vector<std::string> phoneme_set = {"a", "i", "u", "ka", "sa"};
        auto training_data = training_data_factory::create_synthetic_data(10, 6, phoneme_set);
        
        std::cout << "Training with " << training_data.size() << " utterances..." << std::endl;
        
        // Run training pipeline
        bool success = pipeline.run_training_pipeline(training_data);
        assert(success);
        
        // Check results
        const auto& trained_models = pipeline.get_trained_models();
        const auto& training_stats = pipeline.get_training_stats();
        
        assert(!trained_models.empty());
        assert(!training_stats.empty());
        
        std::cout << "✓ End-to-end training completed successfully" << std::endl;
        std::cout << "  Trained models: " << trained_models.size() << std::endl;
        std::cout << "  Training stats: " << training_stats.size() << std::endl;
        
        // Verify output directory was created
        assert(std::filesystem::exists(config.output_directory));
        
        // Cleanup
        std::filesystem::remove_all(config.output_directory);
    }
};

int main() {
    try {
        ContextHmmIntegrationTest test;
        test.run_all_tests();
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Integration test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
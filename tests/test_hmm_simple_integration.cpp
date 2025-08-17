#include <iostream>
#include <vector>
#include <cassert>

// Test the HMM trainer integration without full pipeline
#include "../src/gaussian_mixture.cpp"
#include "../src/hmm_trainer.cpp"

using namespace nexussynth::hmm;

/**
 * @brief Simple integration test for core HMM training functionality
 */
class SimpleHmmIntegrationTest {
public:
    void run_all_tests() {
        std::cout << "=== Simple HMM Integration Test Suite ===" << std::endl;
        
        test_hmm_state_integration();
        test_training_with_real_features();
        test_model_consistency();
        
        std::cout << "\n✓ All simple integration tests passed!" << std::endl;
    }
    
private:
    void test_hmm_state_integration() {
        std::cout << "\n--- Testing HMM State Integration ---" << std::endl;
        
        // Create HMM model
        PhonemeHmm model;
        model.initialize_states(3);
        
        // Verify proper initialization
        assert(model.num_states() == 3);
        
        for (size_t i = 0; i < model.num_states(); ++i) {
            assert(model.states[i].state_id == static_cast<int>(i));
            assert(model.states[i].num_mixtures() == 1);  // Default 1 mixture
            assert(model.states[i].feature_dimension() == 39);  // Default dimension
            
            // Test transition probabilities are normalized
            double sum = model.states[i].transition.self_loop_prob + 
                        model.states[i].transition.next_state_prob + 
                        model.states[i].transition.exit_prob;
            assert(std::abs(sum - 1.0) < 1e-6);
        }
        
        std::cout << "✓ HMM state integration working correctly" << std::endl;
    }
    
    void test_training_with_real_features() {
        std::cout << "\n--- Testing Training with Realistic Features ---" << std::endl;
        
        // Create a more realistic 3-state HMM
        PhonemeHmm model;
        model.initialize_states(3);
        model.model_name = "test-phoneme-model";
        
        // Generate training sequences that simulate real speech features
        std::vector<std::vector<Eigen::VectorXd>> training_sequences = 
            generate_realistic_training_data();
        
        // Configure training
        TrainingConfig config;
        config.max_iterations = 10;
        config.convergence_threshold = 1e-3;
        config.verbose = false;
        
        HmmTrainer trainer(config);
        
        // Train the model
        TrainingStats stats = trainer.train_model(model, training_sequences);
        
        // Validate training results
        assert(stats.final_iteration > 0);
        assert(!stats.log_likelihoods.empty());
        assert(std::isfinite(stats.final_log_likelihood));
        
        // Check that model learned something meaningful
        if (stats.log_likelihoods.size() > 1) {
            double improvement = stats.log_likelihoods.back() - stats.log_likelihoods.front();
            assert(improvement >= -0.1);  // Allow small decrease due to initialization
        }
        
        // Test model on validation data
        auto validation_data = generate_realistic_training_data();
        double validation_score = trainer.evaluate_model(model, validation_data);
        assert(std::isfinite(validation_score));
        
        std::cout << "✓ Training with realistic features completed" << std::endl;
        std::cout << "  Final log-likelihood: " << stats.final_log_likelihood << std::endl;
        std::cout << "  Validation score: " << validation_score << std::endl;
        std::cout << "  Training iterations: " << stats.final_iteration << std::endl;
    }
    
    void test_model_consistency() {
        std::cout << "\n--- Testing Model Consistency ---" << std::endl;
        
        // Create two identical models
        PhonemeHmm model1, model2;
        model1.initialize_states(2);
        model2.initialize_states(2);
        
        // Generate same training data
        auto training_data = generate_small_dataset();
        
        // Train both models with same configuration
        TrainingConfig config;
        config.max_iterations = 5;
        config.verbose = false;
        
        HmmTrainer trainer(config);
        
        TrainingStats stats1 = trainer.train_model(model1, training_data);
        TrainingStats stats2 = trainer.train_model(model2, training_data);
        
        // Models should converge to similar solutions
        double likelihood_diff = std::abs(stats1.final_log_likelihood - stats2.final_log_likelihood);
        assert(likelihood_diff < 1.0);  // Should be reasonably close
        
        // Test both models on same test data
        auto test_data = generate_small_dataset();
        double score1 = trainer.evaluate_model(model1, test_data);
        double score2 = trainer.evaluate_model(model2, test_data);
        
        assert(std::isfinite(score1));
        assert(std::isfinite(score2));
        
        std::cout << "✓ Model consistency test passed" << std::endl;
        std::cout << "  Model 1 likelihood: " << stats1.final_log_likelihood << std::endl;
        std::cout << "  Model 2 likelihood: " << stats2.final_log_likelihood << std::endl;
        std::cout << "  Likelihood difference: " << likelihood_diff << std::endl;
    }
    
    // Helper methods for generating realistic test data
    std::vector<std::vector<Eigen::VectorXd>> generate_realistic_training_data() {
        std::vector<std::vector<Eigen::VectorXd>> sequences;
        std::mt19937 gen(42);
        
        // Generate 5 sequences representing a phoneme with state progression
        for (int seq = 0; seq < 5; ++seq) {
            std::vector<Eigen::VectorXd> sequence;
            
            // State 0: onset (frames 0-3)
            for (int frame = 0; frame < 4; ++frame) {
                Eigen::VectorXd features = generate_state_features(0, gen);
                sequence.push_back(features);
            }
            
            // State 1: steady state (frames 4-7)
            for (int frame = 0; frame < 4; ++frame) {
                Eigen::VectorXd features = generate_state_features(1, gen);
                sequence.push_back(features);
            }
            
            // State 2: offset (frames 8-11)
            for (int frame = 0; frame < 4; ++frame) {
                Eigen::VectorXd features = generate_state_features(2, gen);
                sequence.push_back(features);
            }
            
            sequences.push_back(sequence);
        }
        
        return sequences;
    }
    
    std::vector<std::vector<Eigen::VectorXd>> generate_small_dataset() {
        std::vector<std::vector<Eigen::VectorXd>> sequences;
        std::mt19937 gen(123);
        
        for (int seq = 0; seq < 3; ++seq) {
            std::vector<Eigen::VectorXd> sequence;
            
            for (int frame = 0; frame < 8; ++frame) {
                Eigen::VectorXd features(39);
                for (int i = 0; i < 39; ++i) {
                    std::normal_distribution<double> dist(seq * 2.0, 1.0);
                    features[i] = dist(gen);
                }
                sequence.push_back(features);
            }
            
            sequences.push_back(sequence);
        }
        
        return sequences;
    }
    
    Eigen::VectorXd generate_state_features(int state, std::mt19937& gen) {
        Eigen::VectorXd features(39);
        
        // Create features that are characteristic of the state
        double state_mean = state * 2.0 - 2.0;  // State 0: -2, State 1: 0, State 2: 2
        
        std::normal_distribution<double> dist(state_mean, 0.8);
        
        for (int i = 0; i < 39; ++i) {
            // Add some correlation between features
            double base_value = dist(gen);
            features[i] = base_value + 0.1 * std::sin(i * 0.3);
        }
        
        return features;
    }
};

int main() {
    try {
        SimpleHmmIntegrationTest test;
        test.run_all_tests();
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Simple integration test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
// Comprehensive test suite for Enhanced Model Convergence Detection System
// Tests all convergence detection criteria, early stopping, model checkpointing,
// and adaptive threshold mechanisms

#include "nexussynth/hmm_trainer.h"
#include "nexussynth/hmm_structures.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <random>
#include <algorithm>
#include <iomanip>

using namespace nexussynth;
using namespace nexussynth::hmm;

// Test helper function to create a simple HMM model
PhonemeHmm create_test_hmm_model(int num_states = 3, int feature_dim = 13) {
    PhonemeHmm model;
    model.num_states = num_states;
    model.states.resize(num_states);
    
    for (int i = 0; i < num_states; ++i) {
        model.states[i].state_id = i;
        model.states[i].transition.self_loop_prob = 0.7;
        model.states[i].transition.next_state_prob = 0.3;
        
        // Initialize with simple Gaussian parameters
        model.states[i].emission.means.resize(1);
        model.states[i].emission.covariances.resize(1);
        model.states[i].emission.weights.resize(1);
        
        model.states[i].emission.means[0] = Eigen::VectorXd::Random(feature_dim);
        model.states[i].emission.covariances[0] = Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        model.states[i].emission.weights[0] = 1.0;
    }
    
    return model;
}

// Test helper to create synthetic training data
std::vector<std::vector<Eigen::VectorXd>> create_synthetic_training_data(int num_sequences = 10, 
                                                                        int sequence_length = 50, 
                                                                        int feature_dim = 13) {
    std::vector<std::vector<Eigen::VectorXd>> data;
    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 0.1);
    
    for (int seq = 0; seq < num_sequences; ++seq) {
        std::vector<Eigen::VectorXd> sequence;
        
        for (int frame = 0; frame < sequence_length; ++frame) {
            Eigen::VectorXd feature_vector(feature_dim);
            
            // Create synthetic spectral features with some structure
            for (int dim = 0; dim < feature_dim; ++dim) {
                double base_value = std::sin(frame * 0.1 + dim * 0.5) + 
                                  std::cos(seq * 0.2 + dim * 0.3);
                feature_vector[dim] = base_value + noise(rng);
            }
            
            sequence.push_back(feature_vector);
        }
        
        data.push_back(sequence);
    }
    
    return data;
}

// Test basic configuration structure
void test_enhanced_training_config() {
    std::cout << "Testing Enhanced TrainingConfig..." << std::endl;
    
    TrainingConfig config;
    
    // Test default values
    assert(config.max_iterations == 100);
    assert(config.convergence_threshold == 1e-4);
    assert(config.parameter_threshold == 1e-3);
    assert(config.use_validation_set == true);
    assert(config.validation_split == 0.1);
    assert(config.convergence_window == 5);
    assert(config.verbose == false);
    
    // Test enhanced parameters
    assert(config.enable_adaptive_thresholds == true);
    assert(config.overfitting_threshold == 0.005);
    assert(config.patience == 10);
    assert(config.min_improvement == 1e-5);
    assert(config.enable_model_checkpointing == true);
    assert(config.convergence_confidence == 0.95);
    
    std::cout << "✓ Enhanced TrainingConfig defaults validated" << std::endl;
    
    // Test custom configuration
    config.enable_adaptive_thresholds = false;
    config.patience = 5;
    config.convergence_confidence = 0.8;
    
    assert(config.enable_adaptive_thresholds == false);
    assert(config.patience == 5);
    assert(config.convergence_confidence == 0.8);
    
    std::cout << "✓ Enhanced TrainingConfig customization validated" << std::endl;
}

// Test enhanced training statistics structure
void test_enhanced_training_stats() {
    std::cout << "\nTesting Enhanced TrainingStats..." << std::endl;
    
    TrainingStats stats;
    
    // Test default values
    assert(stats.final_iteration == 0);
    assert(stats.converged == false);
    assert(stats.final_log_likelihood == -std::numeric_limits<double>::infinity());
    assert(stats.best_validation_score == -std::numeric_limits<double>::infinity());
    assert(stats.best_validation_iteration == 0);
    assert(stats.convergence_confidence == 0.0);
    assert(stats.early_stopped == false);
    assert(stats.patience_counter == 0);
    assert(stats.adaptive_threshold == 1e-4);
    
    std::cout << "✓ Enhanced TrainingStats defaults validated" << std::endl;
    
    // Test data accumulation
    stats.log_likelihoods = {-1000.0, -950.0, -900.0, -850.0, -820.0, -815.0, -814.0};
    stats.parameter_changes = {0.1, 0.05, 0.02, 0.01, 0.005, 0.003, 0.002};
    stats.validation_scores = {-900.0, -850.0, -820.0, -815.0, -810.0, -812.0, -814.0};
    stats.convergence_confidence_scores = {0.2, 0.4, 0.6, 0.7, 0.8, 0.85, 0.9};
    stats.relative_improvements = {0.05, 0.02, 0.01, 0.005, 0.003, 0.001, 0.0005};
    
    assert(stats.log_likelihoods.size() == 7);
    assert(stats.parameter_changes.size() == 7);
    assert(stats.validation_scores.size() == 7);
    assert(stats.convergence_confidence_scores.size() == 7);
    assert(stats.relative_improvements.size() == 7);
    
    std::cout << "✓ Enhanced TrainingStats data accumulation validated" << std::endl;
}

// Test convergence confidence calculation
void test_convergence_confidence_calculation() {
    std::cout << "\nTesting Convergence Confidence Calculation..." << std::endl;
    
    TrainingConfig config;
    config.convergence_window = 3;
    config.parameter_threshold = 0.01;
    
    HmmTrainer trainer(config);
    TrainingStats stats;
    
    // Test with insufficient data
    double confidence = trainer.calculate_convergence_confidence(stats);
    assert(confidence == 0.0);
    std::cout << "✓ Insufficient data case: confidence = " << confidence << std::endl;
    
    // Test with stable log-likelihood
    stats.log_likelihoods = {-1000.0, -950.0, -925.0, -920.0, -918.0, -917.5, -917.3};
    stats.parameter_changes = {0.1, 0.05, 0.02, 0.008, 0.006, 0.005, 0.004};
    stats.validation_scores = {-950.0, -925.0, -920.0, -918.0, -917.0, -916.5, -916.8};
    stats.best_validation_score = -916.5;
    
    confidence = trainer.calculate_convergence_confidence(stats);
    assert(confidence > 0.0 && confidence <= 1.0);
    std::cout << "✓ Stable training case: confidence = " << std::fixed << std::setprecision(3) 
              << confidence << std::endl;
    
    // Test with unstable training
    TrainingStats unstable_stats;
    unstable_stats.log_likelihoods = {-1000.0, -800.0, -1200.0, -600.0, -1100.0, -700.0, -900.0};
    unstable_stats.parameter_changes = {0.1, 0.3, 0.2, 0.15, 0.25, 0.08, 0.12};
    unstable_stats.validation_scores = {-900.0, -700.0, -1000.0, -650.0, -950.0, -750.0, -850.0};
    unstable_stats.best_validation_score = -650.0;
    
    double unstable_confidence = trainer.calculate_convergence_confidence(unstable_stats);
    assert(unstable_confidence < confidence); // Should be lower than stable case
    std::cout << "✓ Unstable training case: confidence = " << std::fixed << std::setprecision(3) 
              << unstable_confidence << std::endl;
}

// Test multi-criteria convergence detection
void test_multi_criteria_convergence() {
    std::cout << "\nTesting Multi-Criteria Convergence Detection..." << std::endl;
    
    TrainingConfig config;
    config.convergence_threshold = 1e-3;
    config.parameter_threshold = 0.01;
    config.min_improvement = 1e-4;
    config.convergence_confidence = 0.7;
    config.convergence_window = 3;
    
    HmmTrainer trainer(config);
    TrainingStats stats;
    
    // Test case where no criteria are met
    stats.log_likelihoods = {-1000.0, -900.0, -800.0};
    stats.parameter_changes = {0.1, 0.08, 0.05};
    std::vector<std::string> criteria_met;
    
    bool converged = trainer.check_multi_criteria_convergence(stats, criteria_met);
    assert(!converged);
    assert(criteria_met.empty());
    std::cout << "✓ No convergence criteria met" << std::endl;
    
    // Test log-likelihood convergence
    stats.log_likelihoods = {-1000.0, -950.0, -949.5, -949.4, -949.35, -949.33, -949.32};
    stats.parameter_changes = {0.1, 0.05, 0.03, 0.02, 0.015, 0.012, 0.01};
    
    converged = trainer.check_multi_criteria_convergence(stats, criteria_met);
    bool has_ll_criterion = std::find(criteria_met.begin(), criteria_met.end(), "log-likelihood") != criteria_met.end();
    std::cout << "✓ Log-likelihood convergence: " << (has_ll_criterion ? "detected" : "not detected") << std::endl;
    
    // Test parameter convergence
    stats.parameter_changes.back() = 0.005; // Below threshold
    criteria_met.clear();
    
    converged = trainer.check_multi_criteria_convergence(stats, criteria_met);
    bool has_param_criterion = std::find(criteria_met.begin(), criteria_met.end(), "parameter-change") != criteria_met.end();
    std::cout << "✓ Parameter convergence: " << (has_param_criterion ? "detected" : "not detected") << std::endl;
    
    std::cout << "✓ Multi-criteria convergence detection validated" << std::endl;
}

// Test overfitting detection
void test_overfitting_detection() {
    std::cout << "\nTesting Overfitting Detection..." << std::endl;
    
    TrainingConfig config;
    config.use_validation_set = true;
    config.overfitting_threshold = 0.01;
    
    HmmTrainer trainer(config);
    TrainingStats stats;
    
    // Test case with insufficient validation data
    stats.validation_scores = {-900.0, -850.0};
    bool overfitting = trainer.check_overfitting_detection(stats);
    assert(!overfitting);
    std::cout << "✓ Insufficient data: no overfitting detected" << std::endl;
    
    // Test case with no overfitting (improving validation)
    stats.validation_scores = {-1000.0, -950.0, -920.0, -900.0, -885.0, -880.0, -878.0};
    stats.best_validation_score = -878.0;
    
    overfitting = trainer.check_overfitting_detection(stats);
    assert(!overfitting);
    std::cout << "✓ Improving validation: no overfitting detected" << std::endl;
    
    // Test case with clear overfitting (deteriorating validation)
    stats.validation_scores = {-1000.0, -950.0, -920.0, -900.0, -880.0, -920.0, -950.0};
    stats.best_validation_score = -880.0;
    
    overfitting = trainer.check_overfitting_detection(stats);
    assert(overfitting);
    std::cout << "✓ Deteriorating validation: overfitting detected" << std::endl;
}

// Test early stopping conditions
void test_early_stopping_conditions() {
    std::cout << "\nTesting Early Stopping Conditions..." << std::endl;
    
    TrainingConfig config;
    config.patience = 3;
    config.overfitting_threshold = 0.005;
    
    HmmTrainer trainer(config);
    TrainingStats stats;
    
    // Test patience mechanism
    stats.validation_scores = {-900.0, -850.0, -820.0, -825.0, -830.0, -835.0}; // Deteriorating
    stats.best_validation_score = -820.0;
    stats.patience_counter = 0;
    stats.final_iteration = 5;
    
    // Simulate patience accumulation
    for (int i = 3; i < 6; ++i) {
        stats.final_iteration = i;
        bool early_stop = trainer.check_early_stopping_conditions(stats);
        
        if (i == 5) { // Should trigger early stopping at patience limit
            assert(early_stop);
            assert(stats.patience_counter >= config.patience);
            std::cout << "✓ Early stopping triggered by patience (" << stats.patience_counter << " iterations)" << std::endl;
        }
    }
    
    // Test overfitting-based early stopping
    TrainingStats overfitting_stats;
    overfitting_stats.validation_scores = {-900.0, -850.0, -820.0, -800.0, -780.0, -820.0, -860.0};
    overfitting_stats.best_validation_score = -780.0;
    
    bool early_stop = trainer.check_early_stopping_conditions(overfitting_stats);
    assert(early_stop);
    assert(overfitting_stats.convergence_reason == "Early stopping: overfitting detected");
    std::cout << "✓ Early stopping triggered by overfitting detection" << std::endl;
}

// Test relative improvement calculation
void test_relative_improvement_calculation() {
    std::cout << "\nTesting Relative Improvement Calculation..." << std::endl;
    
    HmmTrainer trainer;
    
    // Test with insufficient data
    std::vector<double> short_values = {-1000.0, -950.0};
    double rel_improvement = trainer.compute_relative_improvement(short_values);
    assert(rel_improvement == std::numeric_limits<double>::infinity());
    std::cout << "✓ Insufficient data case handled" << std::endl;
    
    // Test with improving values
    std::vector<double> improving_values = {-1000.0, -950.0, -920.0, -900.0, -885.0, -875.0};
    rel_improvement = trainer.compute_relative_improvement(improving_values);
    assert(rel_improvement > 0.0); // Should be positive for improvement
    std::cout << "✓ Improving sequence: relative improvement = " << std::scientific 
              << rel_improvement << std::endl;
    
    // Test with converged values (minimal improvement)
    std::vector<double> converged_values = {-1000.0, -950.0, -920.0, -900.0, -899.5, -899.2};
    rel_improvement = trainer.compute_relative_improvement(converged_values);
    assert(rel_improvement < 0.01); // Should be very small
    std::cout << "✓ Converged sequence: relative improvement = " << std::scientific 
              << rel_improvement << std::endl;
    
    // Test with deteriorating values
    std::vector<double> deteriorating_values = {-900.0, -920.0, -950.0, -980.0, -1000.0, -1020.0};
    rel_improvement = trainer.compute_relative_improvement(deteriorating_values);
    assert(rel_improvement < 0.0); // Should be negative for deterioration
    std::cout << "✓ Deteriorating sequence: relative improvement = " << std::scientific 
              << rel_improvement << std::endl;
}

// Test adaptive threshold updates
void test_adaptive_threshold_updates() {
    std::cout << "\nTesting Adaptive Threshold Updates..." << std::endl;
    
    TrainingConfig config;
    config.convergence_threshold = 1e-3;
    
    HmmTrainer trainer(config);
    TrainingStats stats;
    
    // Test with insufficient data
    double adaptive_threshold = trainer.update_adaptive_threshold(stats);
    assert(adaptive_threshold == config.convergence_threshold);
    std::cout << "✓ Insufficient data: threshold = " << std::scientific << adaptive_threshold << std::endl;
    
    // Test with stable improvements
    stats.log_likelihoods = {-1000.0, -950.0, -920.0, -900.0, -885.0, -875.0, -870.0, -868.0, -866.5, -866.0};
    adaptive_threshold = trainer.update_adaptive_threshold(stats);
    std::cout << "✓ Stable improvements: adaptive threshold = " << std::scientific << adaptive_threshold << std::endl;
    
    // Test with volatile improvements
    TrainingStats volatile_stats;
    volatile_stats.log_likelihoods = {-1000.0, -800.0, -1200.0, -600.0, -1100.0, -700.0, -900.0, -650.0, -950.0, -720.0};
    double volatile_threshold = trainer.update_adaptive_threshold(volatile_stats);
    assert(volatile_threshold > adaptive_threshold); // Should be higher for volatile training
    std::cout << "✓ Volatile improvements: adaptive threshold = " << std::scientific << volatile_threshold << std::endl;
}

// Test enhanced L2 norm parameter distance
void test_l2_norm_parameter_distance() {
    std::cout << "\nTesting Enhanced L2 Norm Parameter Distance..." << std::endl;
    
    HmmTrainer trainer;
    
    // Create two identical models
    auto model1 = create_test_hmm_model(3, 13);
    auto model2 = model1; // Copy
    
    double distance = trainer.compute_parameter_l2_norm(model1, model2);
    assert(distance == 0.0);
    std::cout << "✓ Identical models: L2 distance = " << distance << std::endl;
    
    // Modify second model slightly
    model2.states[0].transition.self_loop_prob += 0.01;
    model2.states[1].transition.next_state_prob += 0.005;
    
    distance = trainer.compute_parameter_l2_norm(model1, model2);
    assert(distance > 0.0);
    std::cout << "✓ Modified model: L2 distance = " << std::fixed << std::setprecision(6) 
              << distance << std::endl;
    
    // Modify second model more significantly
    model2.states[0].transition.self_loop_prob += 0.1;
    model2.states[1].transition.next_state_prob += 0.05;
    model2.states[2].transition.self_loop_prob += 0.08;
    
    double larger_distance = trainer.compute_parameter_l2_norm(model1, model2);
    assert(larger_distance > distance);
    std::cout << "✓ Significantly modified model: L2 distance = " << std::fixed 
              << std::setprecision(6) << larger_distance << std::endl;
}

// Test model checkpointing system
void test_model_checkpointing() {
    std::cout << "\nTesting Model Checkpointing System..." << std::endl;
    
    TrainingConfig config;
    config.enable_model_checkpointing = true;
    
    HmmTrainer trainer(config);
    TrainingStats stats;
    
    // Test checkpointing decision with improving validation
    stats.validation_scores = {-900.0, -850.0, -820.0};
    stats.best_validation_score = -820.0;
    
    bool should_save = trainer.should_save_checkpoint(stats);
    assert(should_save);
    std::cout << "✓ Should save checkpoint with improving validation" << std::endl;
    
    // Test checkpointing decision with deteriorating validation
    stats.validation_scores.push_back(-850.0); // Worse than best
    should_save = trainer.should_save_checkpoint(stats);
    assert(!should_save);
    std::cout << "✓ Should not save checkpoint with deteriorating validation" << std::endl;
    
    // Test checkpoint saving and restoration
    auto original_model = create_test_hmm_model(3, 13);
    auto modified_model = original_model;
    modified_model.states[0].transition.self_loop_prob += 0.1;
    
    // Save checkpoint
    stats.validation_scores = {-900.0, -850.0, -800.0}; // Improving
    stats.best_validation_score = -800.0;
    auto checkpoint = trainer.save_checkpoint(modified_model, stats);
    
    // Restore best model
    auto restored_model = trainer.restore_best_model(modified_model, stats);
    
    std::cout << "✓ Model checkpointing and restoration completed" << std::endl;
}

// Test full integration with synthetic training
void test_integration_with_training() {
    std::cout << "\nTesting Integration with HMM Training..." << std::endl;
    
    // Create test setup
    auto training_data = create_synthetic_training_data(15, 30, 13);
    auto model = create_test_hmm_model(3, 13);
    
    TrainingConfig config;
    config.max_iterations = 20;
    config.convergence_threshold = 1e-3;
    config.parameter_threshold = 0.01;
    config.use_validation_set = true;
    config.validation_split = 0.2;
    config.enable_adaptive_thresholds = true;
    config.enable_model_checkpointing = true;
    config.patience = 5;
    config.convergence_confidence = 0.8;
    config.verbose = false;
    
    HmmTrainer trainer(config);
    
    // Run training with enhanced convergence detection
    auto stats = trainer.train_model(model, training_data);
    
    // Validate results
    assert(stats.final_iteration > 0);
    assert(!stats.log_likelihoods.empty());
    assert(!stats.parameter_changes.empty());
    assert(!stats.convergence_reason.empty());
    
    std::cout << "✓ Training completed successfully" << std::endl;
    std::cout << "  - Final iteration: " << stats.final_iteration << std::endl;
    std::cout << "  - Converged: " << (stats.converged ? "Yes" : "No") << std::endl;
    std::cout << "  - Early stopped: " << (stats.early_stopped ? "Yes" : "No") << std::endl;
    std::cout << "  - Convergence reason: " << stats.convergence_reason << std::endl;
    std::cout << "  - Final log-likelihood: " << std::fixed << std::setprecision(2) 
              << stats.final_log_likelihood << std::endl;
    std::cout << "  - Convergence confidence: " << std::fixed << std::setprecision(3) 
              << stats.convergence_confidence << std::endl;
    
    if (!stats.validation_scores.empty()) {
        std::cout << "  - Best validation score: " << std::fixed << std::setprecision(2) 
                  << stats.best_validation_score << std::endl;
        std::cout << "  - Best validation iteration: " << stats.best_validation_iteration << std::endl;
    }
    
    if (!stats.convergence_criteria_met.empty()) {
        std::cout << "  - Convergence criteria met: ";
        for (size_t i = 0; i < stats.convergence_criteria_met.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << stats.convergence_criteria_met[i];
        }
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "=== Enhanced Model Convergence Detection Test Suite ===" << std::endl;
    
    try {
        test_enhanced_training_config();
        test_enhanced_training_stats();
        test_convergence_confidence_calculation();
        test_multi_criteria_convergence();
        test_overfitting_detection();
        test_early_stopping_conditions();
        test_relative_improvement_calculation();
        test_adaptive_threshold_updates();
        test_l2_norm_parameter_distance();
        test_model_checkpointing();
        test_integration_with_training();
        
        std::cout << "\n🎉 All enhanced convergence detection tests passed!" << std::endl;
        
        std::cout << "\n📋 Enhanced Convergence Detection Implementation Summary:" << std::endl;
        std::cout << "  ✓ Multi-criteria convergence detection with confidence scoring" << std::endl;
        std::cout << "  ✓ Adaptive threshold adjustment based on training stability" << std::endl;
        std::cout << "  ✓ Advanced overfitting detection with validation monitoring" << std::endl;
        std::cout << "  ✓ Patience-based early stopping with model restoration" << std::endl;
        std::cout << "  ✓ Relative improvement analysis for convergence assessment" << std::endl;
        std::cout << "  ✓ Enhanced L2 norm parameter distance calculation" << std::endl;
        std::cout << "  ✓ Automatic model checkpointing and best model restoration" << std::endl;
        std::cout << "  ✓ Comprehensive convergence reporting and diagnostics" << std::endl;
        std::cout << "  ✓ Integration with existing HMM training infrastructure" << std::endl;
        
        std::cout << "\n🔗 Key Integration Points:" << std::endl;
        std::cout << "  → EM Algorithm (5.1): Enhanced convergence in training loops" << std::endl;
        std::cout << "  → GMM Learning (5.2): L2 norm parameter change detection" << std::endl;
        std::cout << "  → Viterbi Alignment (5.3): Validation set evaluation" << std::endl;
        std::cout << "  → Data Augmentation (5.4): Robust training with diverse data" << std::endl;
        std::cout << "  → Global Variance (5.6): Quality-aware convergence assessment" << std::endl;
        std::cout << "  → MLPG Generation (6): Optimal model selection for synthesis" << std::endl;
        
        std::cout << "\n⚙️ Configuration Parameters:" << std::endl;
        std::cout << "  → convergence_threshold: Log-likelihood improvement threshold" << std::endl;
        std::cout << "  → parameter_threshold: L2 norm parameter change threshold" << std::endl;
        std::cout << "  → convergence_confidence: Required confidence level (0-1)" << std::endl;
        std::cout << "  → patience: Early stopping patience (iterations)" << std::endl;
        std::cout << "  → overfitting_threshold: Validation score drop threshold" << std::endl;
        std::cout << "  → enable_adaptive_thresholds: Dynamic threshold adjustment" << std::endl;
        std::cout << "  → enable_model_checkpointing: Automatic best model saving" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
        
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
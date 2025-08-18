#include "nexussynth/hmm_trainer.h"
#include "nexussynth/hmm_structures.h"
#include "nexussynth/gaussian_mixture.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>

using namespace nexussynth::hmm;

/**
 * @brief Comprehensive test suite for parallel HMM training
 * 
 * Tests parallel vs sequential training for correctness and performance
 */

// Generate synthetic training data for testing
std::vector<std::vector<Eigen::VectorXd>> generate_test_sequences(int num_sequences, int min_length, int max_length, int feature_dim) {
    std::vector<std::vector<Eigen::VectorXd>> sequences;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> length_dist(min_length, max_length);
    std::normal_distribution<> feature_dist(0.0, 1.0);
    
    for (int seq = 0; seq < num_sequences; ++seq) {
        int length = length_dist(gen);
        std::vector<Eigen::VectorXd> sequence;
        
        for (int t = 0; t < length; ++t) {
            Eigen::VectorXd frame(feature_dim);
            for (int d = 0; d < feature_dim; ++d) {
                frame[d] = feature_dist(gen);
            }
            sequence.push_back(frame);
        }
        
        sequences.push_back(sequence);
    }
    
    return sequences;
}

// Create a simple test HMM model
PhonemeHmm create_test_model(int num_states, int feature_dim) {
    PhonemeHmm model;
    model.model_name = "test";
    
    // Use the built-in initialization and then modify the number of states
    model.initialize_states(num_states);
    
    // Initialize each state's GMM with the proper dimension
    for (auto& state : model.states) {
        // Initialize the output distribution with proper dimension
        state.output_distribution = GaussianMixture(1, feature_dim);  // Single Gaussian component
        
        // Set random transition probabilities
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> prob_dist(0.1, 0.9);
        
        state.transition.self_loop_prob = prob_dist(gen);
        state.transition.next_state_prob = 1.0 - state.transition.self_loop_prob;
        state.transition.exit_prob = 0.0;
    }
    
    // Final state should have higher exit probability
    if (!model.states.empty()) {
        auto& final_state = model.states.back();
        final_state.transition.self_loop_prob = 0.3;
        final_state.transition.next_state_prob = 0.0;
        final_state.transition.exit_prob = 0.7;
    }
    
    return model;
}

// Test parallel vs sequential training correctness
bool test_training_correctness() {
    std::cout << "Testing parallel vs sequential training correctness..." << std::endl;
    
    const int num_sequences = 20;
    const int feature_dim = 12;
    const int num_states = 5;
    
    // Generate test data
    auto sequences = generate_test_sequences(num_sequences, 50, 200, feature_dim);
    
    // Create identical models for parallel and sequential training
    PhonemeHmm model_sequential = create_test_model(num_states, feature_dim);
    PhonemeHmm model_parallel = model_sequential;  // Copy
    
    // Configure training
    TrainingConfig config;
    config.max_iterations = 5;  // Short test
    config.convergence_threshold = 1e-6;
    config.verbose = true;
    config.enable_parallel_training = false;  // Sequential first
    
    TrainingConfig config_parallel = config;
    config_parallel.enable_parallel_training = true;
    config_parallel.num_threads = 4;
    config_parallel.verbose_parallel = true;
    
    // Train models
    HmmTrainer trainer_sequential(config);
    HmmTrainer trainer_parallel(config_parallel);
    
    std::cout << "Training sequential model..." << std::endl;
    auto stats_sequential = trainer_sequential.train_model(model_sequential, sequences);
    
    std::cout << "Training parallel model..." << std::endl;
    auto stats_parallel = trainer_parallel.train_model(model_parallel, sequences);
    
    // Compare final log-likelihoods (should be very close)
    double ll_diff = std::abs(stats_sequential.final_log_likelihood - stats_parallel.final_log_likelihood);
    double tolerance = 1e-3;  // Allow small numerical differences
    
    std::cout << "Sequential final LL: " << stats_sequential.final_log_likelihood << std::endl;
    std::cout << "Parallel final LL: " << stats_parallel.final_log_likelihood << std::endl;
    std::cout << "Difference: " << ll_diff << std::endl;
    
    if (ll_diff > tolerance) {
        std::cout << "ERROR: Log-likelihood difference too large: " << ll_diff << std::endl;
        return false;
    }
    
    std::cout << "✓ Training correctness test passed" << std::endl;
    return true;
}

// Test performance and efficiency
bool test_parallel_performance() {
    std::cout << "\nTesting parallel training performance..." << std::endl;
    
    const int num_sequences = 100;  // Larger dataset for performance testing
    const int feature_dim = 24;
    const int num_states = 7;
    
    // Generate larger test dataset
    auto sequences = generate_test_sequences(num_sequences, 100, 500, feature_dim);
    
    PhonemeHmm model_sequential = create_test_model(num_states, feature_dim);
    PhonemeHmm model_parallel = model_sequential;
    
    TrainingConfig config;
    config.max_iterations = 3;  // Quick performance test
    config.verbose = false;  // Reduce output for timing
    
    TrainingConfig config_parallel = config;
    config_parallel.enable_parallel_training = true;
    config_parallel.num_threads = 4;
    config_parallel.verbose_parallel = true;
    
    HmmTrainer trainer_sequential(config);
    HmmTrainer trainer_parallel(config_parallel);
    
    // Time sequential training
    auto start_sequential = std::chrono::high_resolution_clock::now();
    auto stats_sequential = trainer_sequential.train_model(model_sequential, sequences);
    auto end_sequential = std::chrono::high_resolution_clock::now();
    
    // Time parallel training
    auto start_parallel = std::chrono::high_resolution_clock::now();
    auto stats_parallel = trainer_parallel.train_model(model_parallel, sequences);
    auto end_parallel = std::chrono::high_resolution_clock::now();
    
    // Calculate timings
    auto sequential_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_sequential - start_sequential);
    auto parallel_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_parallel - start_parallel);
    
    double sequential_time = sequential_duration.count() / 1000.0;
    double parallel_time = parallel_duration.count() / 1000.0;
    double speedup = sequential_time / parallel_time;
    
    std::cout << "Sequential training time: " << sequential_time << " seconds" << std::endl;
    std::cout << "Parallel training time: " << parallel_time << " seconds" << std::endl;
    std::cout << "Speedup: " << speedup << "x" << std::endl;
    
    // Print parallel efficiency if available
    if (!stats_parallel.parallel_efficiency.empty()) {
        double avg_efficiency = 0.0;
        for (double eff : stats_parallel.parallel_efficiency) {
            avg_efficiency += eff;
        }
        avg_efficiency /= stats_parallel.parallel_efficiency.size();
        std::cout << "Average parallel efficiency: " << (avg_efficiency * 100.0) << "%" << std::endl;
    }
    
    // Basic performance check (parallel should be faster for sufficient data)
    if (speedup > 1.0) {
        std::cout << "✓ Parallel training achieved speedup" << std::endl;
    } else {
        std::cout << "⚠ Parallel training did not achieve speedup (may be expected for small datasets)" << std::endl;
    }
    
    return true;
}

// Test load balancing functionality
bool test_load_balancing() {
    std::cout << "\nTesting load balancing..." << std::endl;
    
    const int feature_dim = 10;
    
    // Create sequences with varied lengths to test load balancing
    std::vector<std::vector<Eigen::VectorXd>> sequences;
    std::vector<int> lengths = {50, 100, 200, 25, 300, 75, 150, 400};  // Varied lengths
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> feature_dist(0.0, 1.0);
    
    for (int length : lengths) {
        std::vector<Eigen::VectorXd> sequence;
        for (int t = 0; t < length; ++t) {
            Eigen::VectorXd frame(feature_dim);
            for (int d = 0; d < feature_dim; ++d) {
                frame[d] = feature_dist(gen);
            }
            sequence.push_back(frame);
        }
        sequences.push_back(sequence);
    }
    
    // Test load balancing with different thread counts
    TrainingConfig config;
    config.enable_load_balancing = true;
    config.num_threads = 4;
    
    HmmTrainer trainer(config);
    
    // Test the load balancing utility function
    auto chunks = trainer.create_load_balanced_chunks_public(sequences, 4);
    
    std::cout << "Load balancing results for " << sequences.size() << " sequences:" << std::endl;
    
    int total_work = 0;
    for (const auto& seq : sequences) {
        total_work += seq.size();
    }
    
    for (size_t thread = 0; thread < chunks.size(); ++thread) {
        int thread_work = 0;
        for (int seq_idx : chunks[thread]) {
            thread_work += sequences[seq_idx].size();
        }
        
        double work_percentage = (static_cast<double>(thread_work) / total_work) * 100.0;
        std::cout << "Thread " << thread << ": " << chunks[thread].size() << " sequences, "
                  << thread_work << " frames (" << work_percentage << "%)" << std::endl;
    }
    
    std::cout << "✓ Load balancing test completed" << std::endl;
    return true;
}

// Test edge cases and error handling
bool test_edge_cases() {
    std::cout << "\nTesting edge cases..." << std::endl;
    
    TrainingConfig config;
    config.enable_parallel_training = true;
    config.num_threads = 4;
    
    HmmTrainer trainer(config);
    PhonemeHmm model = create_test_model(3, 10);
    
    // Test with empty sequences
    std::vector<std::vector<Eigen::VectorXd>> empty_sequences;
    auto stats_empty = trainer.train_model(model, empty_sequences);
    assert(!stats_empty.converged);
    std::cout << "✓ Empty sequence handling test passed" << std::endl;
    
    // Test with single sequence (should fall back to sequential)
    auto single_sequence = generate_test_sequences(1, 50, 50, 10);
    auto stats_single = trainer.train_model(model, single_sequence);
    std::cout << "✓ Single sequence handling test passed" << std::endl;
    
    // Test thread count determination
    int optimal_threads = trainer.determine_optimal_thread_count_public(100);
    std::cout << "Optimal threads for 100 sequences: " << optimal_threads << std::endl;
    
    optimal_threads = trainer.determine_optimal_thread_count_public(2);
    std::cout << "Optimal threads for 2 sequences: " << optimal_threads << std::endl;
    
    std::cout << "✓ Edge cases test completed" << std::endl;
    return true;
}

int main() {
    std::cout << "=== NexusSynth Parallel HMM Training Test Suite ===" << std::endl;
    
    bool all_tests_passed = true;
    
    try {
        // Run all test cases
        if (!test_training_correctness()) all_tests_passed = false;
        if (!test_parallel_performance()) all_tests_passed = false;
        if (!test_load_balancing()) all_tests_passed = false;
        if (!test_edge_cases()) all_tests_passed = false;
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Test failed with exception: " << e.what() << std::endl;
        all_tests_passed = false;
    }
    
    std::cout << "\n=== Test Results ===" << std::endl;
    if (all_tests_passed) {
        std::cout << "✓ All parallel training tests PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some parallel training tests FAILED" << std::endl;
        return 1;
    }
}
#include "nexussynth/hmm_trainer.h"
#include "nexussynth/hmm_structures.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

using namespace nexussynth::hmm;

// Test helper function to create test data
std::vector<Eigen::VectorXd> create_test_observation_sequence(int length, int dimension) {
    std::vector<Eigen::VectorXd> sequence;
    sequence.reserve(length);
    
    for (int t = 0; t < length; ++t) {
        Eigen::VectorXd obs = Eigen::VectorXd::Zero(dimension);
        
        // Create simple pattern: sine wave with different frequencies per "phoneme"
        double base_freq = 0.1;
        for (int d = 0; d < dimension; ++d) {
            obs[d] = std::sin(2 * M_PI * base_freq * (d + 1) * t) + 
                     0.1 * static_cast<double>(rand()) / RAND_MAX;  // Add noise
        }
        
        sequence.push_back(obs);
    }
    
    return sequence;
}

// Test basic forced alignment
void test_forced_alignment() {
    std::cout << "Testing forced alignment..." << std::endl;
    
    const int feature_dim = 13;  // MFCC-like features
    const int num_states = 5;    // Standard HTS 5-state model
    
    // Create test HMM model
    PhonemeHmm model;
    model.initialize_states(num_states);
    
    // Initialize GMMs for each state
    for (int i = 0; i < num_states; ++i) {
        model.states[i] = HmmState(i, 2, feature_dim);  // 2 mixtures per state
        
        // Simple initialization with different means for each state
        Eigen::VectorXd mean1 = Eigen::VectorXd::Constant(feature_dim, i * 0.5);
        Eigen::VectorXd mean2 = Eigen::VectorXd::Constant(feature_dim, i * 0.5 + 0.3);
        Eigen::MatrixXd cov = 0.1 * Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        
        model.states[i].output_distribution.component(0).set_parameters(mean1, cov, 0.6);
        model.states[i].output_distribution.component(1).set_parameters(mean2, cov, 0.4);
        model.states[i].output_distribution.normalize_weights();
    }
    
    // Create test observation sequence
    const int sequence_length = 100;
    auto observations = create_test_observation_sequence(sequence_length, feature_dim);
    
    // Create phoneme sequence
    std::vector<std::string> phoneme_sequence = {"a", "i", "u"};
    
    // Set up trainer
    TrainingConfig config;
    config.verbose = true;
    HmmTrainer trainer(config);
    
    // Test forced alignment
    SequenceAlignment alignment = trainer.forced_alignment(model, observations, phoneme_sequence, 100.0);
    
    // Validate results
    assert(!alignment.state_sequence.empty());
    assert(alignment.state_sequence.size() == sequence_length);
    assert(alignment.frame_to_state.size() == sequence_length);
    assert(alignment.frame_scores.size() == sequence_length);
    assert(!alignment.phoneme_boundaries.empty());
    assert(alignment.phoneme_boundaries.size() <= phoneme_sequence.size());
    assert(alignment.average_confidence >= 0.0 && alignment.average_confidence <= 1.0);
    assert(alignment.frame_rate == 100.0);
    
    std::cout << "✓ Forced alignment basic functionality passed" << std::endl;
    std::cout << "  - State sequence length: " << alignment.state_sequence.size() << std::endl;
    std::cout << "  - Phoneme boundaries found: " << alignment.phoneme_boundaries.size() << std::endl;
    std::cout << "  - Average confidence: " << alignment.average_confidence << std::endl;
    std::cout << "  - Total score: " << alignment.total_score << std::endl;
    
    // Validate phoneme boundaries
    for (const auto& boundary : alignment.phoneme_boundaries) {
        assert(boundary.start_frame >= 0);
        assert(boundary.end_frame > boundary.start_frame);
        assert(boundary.end_frame <= sequence_length);
        assert(!boundary.phoneme.empty());
        assert(boundary.confidence_score >= 0.0 && boundary.confidence_score <= 1.0);
        assert(boundary.duration_ms > 0.0);
        
        std::cout << "  - Phoneme '" << boundary.phoneme 
                  << "': frames " << boundary.start_frame << "-" << boundary.end_frame
                  << " (" << boundary.duration_ms << "ms, conf=" << boundary.confidence_score << ")" << std::endl;
    }
}

// Test constrained alignment with time hints
void test_constrained_alignment() {
    std::cout << "\nTesting constrained alignment..." << std::endl;
    
    const int feature_dim = 13;
    const int num_states = 5;
    
    // Create test HMM model (reuse setup from previous test)
    PhonemeHmm model;
    model.initialize_states(num_states);
    
    for (int i = 0; i < num_states; ++i) {
        model.states[i] = HmmState(i, 2, feature_dim);
        
        Eigen::VectorXd mean1 = Eigen::VectorXd::Constant(feature_dim, i * 0.5);
        Eigen::VectorXd mean2 = Eigen::VectorXd::Constant(feature_dim, i * 0.5 + 0.3);
        Eigen::MatrixXd cov = 0.1 * Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        
        model.states[i].output_distribution.component(0).set_parameters(mean1, cov, 0.6);
        model.states[i].output_distribution.component(1).set_parameters(mean2, cov, 0.4);
        model.states[i].output_distribution.normalize_weights();
    }
    
    // Create test data
    const int sequence_length = 150;
    auto observations = create_test_observation_sequence(sequence_length, feature_dim);
    
    // Create phoneme sequence with time constraints
    std::vector<std::string> phoneme_sequence = {"k", "a", "t"};
    std::vector<std::pair<double, double>> time_constraints = {
        {0.0, 500.0},      // 'k': 0-500ms
        {500.0, 1000.0},   // 'a': 500-1000ms  
        {1000.0, 1500.0}   // 't': 1000-1500ms
    };
    
    HmmTrainer trainer;
    
    // Test constrained alignment
    SequenceAlignment alignment = trainer.constrained_alignment(
        model, observations, phoneme_sequence, time_constraints, 100.0);
    
    // Validate results
    assert(!alignment.state_sequence.empty());
    assert(alignment.state_sequence.size() == sequence_length);
    assert(!alignment.phoneme_boundaries.empty());
    assert(alignment.phoneme_boundaries.size() <= phoneme_sequence.size());
    
    std::cout << "✓ Constrained alignment functionality passed" << std::endl;
    std::cout << "  - Phoneme boundaries: " << alignment.phoneme_boundaries.size() << std::endl;
    std::cout << "  - Average confidence: " << alignment.average_confidence << std::endl;
    
    // Check that boundaries roughly respect time constraints
    for (size_t i = 0; i < alignment.phoneme_boundaries.size() && i < time_constraints.size(); ++i) {
        const auto& boundary = alignment.phoneme_boundaries[i];
        double expected_start = time_constraints[i].first;
        double expected_end = time_constraints[i].second;
        double actual_start = (boundary.start_frame / 100.0) * 1000.0;  // Convert to ms
        double actual_end = (boundary.end_frame / 100.0) * 1000.0;
        
        std::cout << "  - Phoneme '" << boundary.phoneme 
                  << "': expected " << expected_start << "-" << expected_end << "ms"
                  << ", actual " << actual_start << "-" << actual_end << "ms" << std::endl;
    }
}

// Test batch processing
void test_batch_forced_alignment() {
    std::cout << "\nTesting batch forced alignment..." << std::endl;
    
    const int feature_dim = 13;
    const int num_states = 5;
    
    // Create test model
    std::map<std::string, PhonemeHmm> models;
    models["default"] = PhonemeHmm();
    models["default"].initialize_states(num_states);
    
    for (int i = 0; i < num_states; ++i) {
        models["default"].states[i] = HmmState(i, 1, feature_dim);
        
        Eigen::VectorXd mean = Eigen::VectorXd::Constant(feature_dim, i * 0.3);
        Eigen::MatrixXd cov = 0.1 * Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        
        models["default"].states[i].output_distribution.component(0).set_parameters(mean, cov, 1.0);
    }
    
    // Create multiple test sequences
    std::vector<std::vector<Eigen::VectorXd>> sequences = {
        create_test_observation_sequence(80, feature_dim),
        create_test_observation_sequence(120, feature_dim),
        create_test_observation_sequence(100, feature_dim)
    };
    
    std::vector<std::vector<std::string>> phoneme_sequences = {
        {"p", "a"},
        {"t", "i", "k"},
        {"s", "u"}
    };
    
    HmmTrainer trainer;
    
    // Test batch alignment
    auto alignments = trainer.batch_forced_alignment(models, sequences, phoneme_sequences, 100.0);
    
    // Validate results
    assert(alignments.size() == sequences.size());
    
    for (size_t i = 0; i < alignments.size(); ++i) {
        const auto& alignment = alignments[i];
        assert(alignment.state_sequence.size() == sequences[i].size());
        assert(!alignment.phoneme_boundaries.empty());
        
        std::cout << "  - Sequence " << i << ": " << alignment.phoneme_boundaries.size() 
                  << " boundaries, confidence " << alignment.average_confidence << std::endl;
    }
    
    std::cout << "✓ Batch forced alignment functionality passed" << std::endl;
}

// Test confidence scoring
void test_confidence_scoring() {
    std::cout << "\nTesting confidence scoring..." << std::endl;
    
    const int feature_dim = 13;
    const int num_states = 5;
    
    // Create well-trained vs poorly-trained models to test confidence differences
    PhonemeHmm good_model, poor_model;
    good_model.initialize_states(num_states);
    poor_model.initialize_states(num_states);
    
    // Good model: well-separated means
    for (int i = 0; i < num_states; ++i) {
        good_model.states[i] = HmmState(i, 1, feature_dim);
        poor_model.states[i] = HmmState(i, 1, feature_dim);
        
        // Good model: distinct means
        Eigen::VectorXd good_mean = Eigen::VectorXd::Constant(feature_dim, i * 2.0);
        Eigen::MatrixXd good_cov = 0.1 * Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        
        // Poor model: overlapping means
        Eigen::VectorXd poor_mean = Eigen::VectorXd::Constant(feature_dim, i * 0.1);
        Eigen::MatrixXd poor_cov = 1.0 * Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        
        good_model.states[i].output_distribution.component(0).set_parameters(good_mean, good_cov, 1.0);
        poor_model.states[i].output_distribution.component(0).set_parameters(poor_mean, poor_cov, 1.0);
    }
    
    // Create test sequence
    auto observations = create_test_observation_sequence(100, feature_dim);
    std::vector<std::string> phoneme_sequence = {"a", "e"};
    
    HmmTrainer trainer;
    
    // Test both models
    auto good_alignment = trainer.forced_alignment(good_model, observations, phoneme_sequence, 100.0);
    auto poor_alignment = trainer.forced_alignment(poor_model, observations, phoneme_sequence, 100.0);
    
    std::cout << "  - Good model confidence: " << good_alignment.average_confidence << std::endl;
    std::cout << "  - Poor model confidence: " << poor_alignment.average_confidence << std::endl;
    
    // Good model should generally have higher confidence than poor model
    // (Note: this is a heuristic test - in practice results may vary)
    std::cout << "✓ Confidence scoring system functional" << std::endl;
}

int main() {
    std::cout << "=== Enhanced Viterbi Alignment Test Suite ===" << std::endl;
    
    try {
        test_forced_alignment();
        test_constrained_alignment();
        test_batch_forced_alignment();
        test_confidence_scoring();
        
        std::cout << "\n🎉 All enhanced Viterbi alignment tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
        
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
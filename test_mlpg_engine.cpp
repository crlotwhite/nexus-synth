#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include "nexussynth/mlpg_engine.h"
#include "nexussynth/gaussian_mixture.h"

using namespace nexussynth;
using namespace nexussynth::hmm;

void test_basic_trajectory_generation() {
    std::cout << "\n=== Test: Basic Trajectory Generation ===\n";
    
    mlpg::MlpgConfig config;
    config.verbose = true;
    config.use_delta_features = true;
    config.use_delta_delta_features = true;
    config.use_global_variance = false; // Start simple
    
    mlpg::MlpgEngine engine(config);
    
    // Create simple test data: 3 states, 39-dimensional features
    int feature_dim = 39;
    std::vector<Eigen::VectorXd> means = {
        Eigen::VectorXd::Random(feature_dim),
        Eigen::VectorXd::Random(feature_dim),
        Eigen::VectorXd::Random(feature_dim)
    };
    
    std::vector<Eigen::VectorXd> variances = {
        Eigen::VectorXd::Constant(feature_dim, 0.1),
        Eigen::VectorXd::Constant(feature_dim, 0.2),
        Eigen::VectorXd::Constant(feature_dim, 0.15)
    };
    
    std::vector<int> durations = {10, 15, 12};
    
    mlpg::TrajectoryStats stats;
    
    try {
        auto trajectory = engine.generate_trajectory(means, variances, durations, &stats);
        
        int expected_frames = 10 + 15 + 12;
        if (trajectory.size() == expected_frames) {
            std::cout << "✓ Trajectory generation successful" << std::endl;
            std::cout << "  Generated " << trajectory.size() << " frames" << std::endl;
            std::cout << "  Feature dimension: " << trajectory[0].size() << std::endl;
            std::cout << "  Optimization time: " << stats.optimization_time_ms << " ms" << std::endl;
            std::cout << "  Smoothness score: " << stats.delta_smoothness_score << std::endl;
            std::cout << "  Log-likelihood: " << stats.final_likelihood << std::endl;
        } else {
            std::cout << "✗ Wrong trajectory size. Expected " << expected_frames 
                     << ", got " << trajectory.size() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
    }
}

void test_hmm_integration() {
    std::cout << "\n=== Test: HMM Integration ===\n";
    
    mlpg::MlpgConfig config;
    config.verbose = true;
    config.use_global_variance = true;
    
    mlpg::MlpgEngine engine(config);
    
    // Create HMM states with realistic Gaussian mixtures
    int feature_dim = 39;
    std::vector<hmm::HmmState> hmm_states;
    
    for (int i = 0; i < 4; ++i) {
        hmm::HmmState state(i, 1, feature_dim); // Single Gaussian per state
        
        // Initialize with random but reasonable parameters
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> dist(0.0, 1.0);
        
        Eigen::VectorXd mean = Eigen::VectorXd::Zero(feature_dim);
        Eigen::MatrixXd covariance = Eigen::MatrixXd::Identity(feature_dim, feature_dim);
        
        // Set some structure to the means (simulate realistic features)
        for (int d = 0; d < feature_dim; ++d) {
            mean(d) = dist(gen) * 0.5;
            covariance(d, d) = 0.1 + std::abs(dist(gen)) * 0.1;
        }
        
        // Initialize the mixture with a single component
        std::vector<GaussianComponent> components;
        components.emplace_back(mean, covariance, 1.0);
        state.output_distribution = GaussianMixture(components);
        hmm_states.push_back(state);
    }
    
    std::vector<int> durations = {8, 12, 10, 6};
    
    try {
        mlpg::TrajectoryStats stats;
        auto trajectory = engine.generate_trajectory_from_hmm(hmm_states, durations, &stats);
        
        std::cout << "✓ HMM integration successful" << std::endl;
        std::cout << "  Generated " << trajectory.size() << " frames from " << hmm_states.size() << " HMM states" << std::endl;
        std::cout << "  GV constraint satisfaction: " << stats.gv_constraint_satisfaction << std::endl;
        
        // Verify trajectory continuity
        double max_jump = 0.0;
        for (size_t t = 1; t < trajectory.size(); ++t) {
            double jump = (trajectory[t] - trajectory[t-1]).norm();
            max_jump = std::max(max_jump, jump);
        }
        std::cout << "  Maximum frame-to-frame jump: " << max_jump << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "✗ HMM integration failed: " << e.what() << std::endl;
    }
}

void test_smoothness_constraints() {
    std::cout << "\n=== Test: Smoothness Constraints ===\n";
    
    // Compare trajectories with and without delta constraints
    std::vector<mlpg::MlpgConfig> configs(3);
    
    configs[0].use_delta_features = false;
    configs[0].use_delta_delta_features = false;
    configs[0].use_global_variance = false;
    
    configs[1].use_delta_features = true;
    configs[1].use_delta_delta_features = false;
    configs[1].use_global_variance = false;
    
    configs[2].use_delta_features = true;
    configs[2].use_delta_delta_features = true;
    configs[2].use_global_variance = false;
    
    // Test data with abrupt changes
    int feature_dim = 12;
    std::vector<Eigen::VectorXd> means = {
        Eigen::VectorXd::Constant(feature_dim, -1.0),
        Eigen::VectorXd::Constant(feature_dim, 1.0),
        Eigen::VectorXd::Constant(feature_dim, -0.5)
    };
    
    std::vector<Eigen::VectorXd> variances = {
        Eigen::VectorXd::Constant(feature_dim, 0.01),
        Eigen::VectorXd::Constant(feature_dim, 0.01),
        Eigen::VectorXd::Constant(feature_dim, 0.01)
    };
    
    std::vector<int> durations = {10, 10, 10};
    
    std::vector<std::string> config_names = {"No Delta", "Delta Only", "Delta + Delta-Delta"};
    
    for (size_t i = 0; i < configs.size(); ++i) {
        mlpg::MlpgEngine engine(configs[i]);
        mlpg::TrajectoryStats stats;
        
        try {
            auto trajectory = engine.generate_trajectory(means, variances, durations, &stats);
            
            std::cout << "Configuration: " << config_names[i] << std::endl;
            std::cout << "  Smoothness score: " << stats.delta_smoothness_score << std::endl;
            std::cout << "  Optimization time: " << stats.optimization_time_ms << " ms" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Configuration " << config_names[i] << " failed: " << e.what() << std::endl;
        }
    }
}

void test_performance_scaling() {
    std::cout << "\n=== Test: Performance Scaling ===\n";
    
    mlpg::MlpgConfig config;
    config.verbose = false; // Reduce output for performance testing
    config.use_delta_features = true;
    config.use_delta_delta_features = true;
    
    mlpg::MlpgEngine engine(config);
    
    std::vector<int> trajectory_lengths = {50, 100, 200, 400};
    std::vector<int> feature_dimensions = {13, 25, 39, 75};
    
    for (int length : trajectory_lengths) {
        for (int feature_dim : feature_dimensions) {
            // Generate random test data
            int num_states = length / 10; // Assume ~10 frames per state
            if (num_states < 1) num_states = 1;
            
            std::vector<Eigen::VectorXd> means, variances;
            std::vector<int> durations;
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::normal_distribution<> dist(0.0, 1.0);
            std::uniform_int_distribution<> duration_dist(5, 15);
            
            int total_duration = 0;
            for (int s = 0; s < num_states; ++s) {
                means.emplace_back(Eigen::VectorXd::Random(feature_dim));
                variances.emplace_back(Eigen::VectorXd::Constant(feature_dim, 0.1));
                
                int duration = duration_dist(gen);
                durations.push_back(duration);
                total_duration += duration;
                
                if (total_duration >= length) {
                    // Adjust last duration to match target length
                    durations.back() -= (total_duration - length);
                    if (durations.back() <= 0) durations.back() = 1;
                    break;
                }
            }
            
            try {
                mlpg::TrajectoryStats stats;
                auto start = std::chrono::high_resolution_clock::now();
                
                auto trajectory = engine.generate_trajectory(means, variances, durations, &stats);
                
                auto end = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
                
                std::cout << "Length: " << total_duration << ", Dim: " << feature_dim 
                         << " -> " << elapsed << " ms (" << stats.matrix_size << " matrix elements)" << std::endl;
                         
            } catch (const std::exception& e) {
                std::cout << "✗ Performance test failed for length " << length 
                         << ", dim " << feature_dim << ": " << e.what() << std::endl;
            }
        }
    }
}

void test_edge_cases() {
    std::cout << "\n=== Test: Edge Cases ===\n";
    
    mlpg::MlpgEngine engine;
    
    // Test 1: Single frame
    {
        std::cout << "Test 1: Single frame trajectory" << std::endl;
        std::vector<Eigen::VectorXd> means = {Eigen::VectorXd::Random(5)};
        std::vector<Eigen::VectorXd> variances = {Eigen::VectorXd::Constant(5, 0.1)};
        std::vector<int> durations = {1};
        
        try {
            auto trajectory = engine.generate_trajectory(means, variances, durations);
            std::cout << "  ✓ Single frame test passed" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  ✗ Single frame test failed: " << e.what() << std::endl;
        }
    }
    
    // Test 2: High-dimensional features
    {
        std::cout << "Test 2: High-dimensional features" << std::endl;
        int high_dim = 128;
        std::vector<Eigen::VectorXd> means = {
            Eigen::VectorXd::Random(high_dim),
            Eigen::VectorXd::Random(high_dim)
        };
        std::vector<Eigen::VectorXd> variances = {
            Eigen::VectorXd::Constant(high_dim, 0.1),
            Eigen::VectorXd::Constant(high_dim, 0.1)
        };
        std::vector<int> durations = {20, 25};
        
        try {
            auto trajectory = engine.generate_trajectory(means, variances, durations);
            std::cout << "  ✓ High-dimensional test passed (" << high_dim << " dimensions)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  ✗ High-dimensional test failed: " << e.what() << std::endl;
        }
    }
    
    // Test 3: Input validation
    {
        std::cout << "Test 3: Input validation" << std::endl;
        
        // Empty inputs
        std::vector<Eigen::VectorXd> empty_means;
        std::vector<Eigen::VectorXd> empty_variances;
        std::vector<int> empty_durations;
        
        try {
            engine.generate_trajectory(empty_means, empty_variances, empty_durations);
            std::cout << "  ✗ Empty input validation failed - should have thrown exception" << std::endl;
        } catch (const std::invalid_argument&) {
            std::cout << "  ✓ Empty input validation passed" << std::endl;
        }
        
        // Mismatched sizes
        std::vector<Eigen::VectorXd> means = {Eigen::VectorXd::Random(5)};
        std::vector<Eigen::VectorXd> variances = {
            Eigen::VectorXd::Constant(5, 0.1),
            Eigen::VectorXd::Constant(5, 0.1)
        };
        std::vector<int> durations = {10};
        
        try {
            engine.generate_trajectory(means, variances, durations);
            std::cout << "  ✗ Size mismatch validation failed - should have thrown exception" << std::endl;
        } catch (const std::invalid_argument&) {
            std::cout << "  ✓ Size mismatch validation passed" << std::endl;
        }
    }
}

int main() {
    std::cout << "MLPG Engine Test Suite\n";
    std::cout << "======================\n";
    
    try {
        test_basic_trajectory_generation();
        test_hmm_integration();
        test_smoothness_constraints();
        test_performance_scaling();
        test_edge_cases();
        
        std::cout << "\n=== All Tests Completed ===\n";
        
    } catch (const std::exception& e) {
        std::cout << "Fatal error in test suite: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
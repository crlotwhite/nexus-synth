#include "nexussynth/mlpg_engine.h"
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <cmath>
#include <Eigen/SparseLU>
#include <Eigen/SparseCholesky>

namespace nexussynth {
namespace mlpg {

    MlpgEngine::MlpgEngine(const MlpgConfig& config) : config_(config), use_gpu_(false) {
        if (config_.verbose) {
            std::cout << "MLPG Engine initialized with configuration:" << std::endl;
            std::cout << "  Delta features: " << (config_.use_delta_features ? "enabled" : "disabled") << std::endl;
            std::cout << "  Delta-Delta features: " << (config_.use_delta_delta_features ? "enabled" : "disabled") << std::endl;
            std::cout << "  Global variance: " << (config_.use_global_variance ? "enabled" : "disabled") << std::endl;
            std::cout << "  Regularization: " << config_.regularization_factor << std::endl;
        }
    }

    std::vector<Eigen::VectorXd> MlpgEngine::generate_trajectory(
        const std::vector<Eigen::VectorXd>& means,
        const std::vector<Eigen::VectorXd>& variances,
        const std::vector<int>& durations,
        TrajectoryStats* stats
    ) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Validate inputs
        validate_inputs(means, variances, durations);

        // Initialize statistics
        TrajectoryStats local_stats;
        if (!stats) {
            stats = &local_stats;
        }

        // Calculate total frames and feature dimensions
        int total_frames = 0;
        for (int duration : durations) {
            total_frames += duration;
        }
        
        if (means.empty()) {
            throw std::invalid_argument("Empty means vector provided to MLPG");
        }
        
        int feature_dim = static_cast<int>(means[0].size());
        stats->matrix_size = total_frames * feature_dim;

        if (config_.verbose) {
            std::cout << "MLPG: Processing " << total_frames << " frames, " 
                     << feature_dim << " features per frame" << std::endl;
        }

        // Build system components
        auto W = build_w_matrix(total_frames, feature_dim);
        auto precision_matrix = build_precision_matrix(variances, durations);
        auto observation_vector = build_observation_vector(means, durations);

        // Solve the MLPG optimization problem
        auto solution = solve_mlpg_system(W, precision_matrix, observation_vector, stats);

        // Convert solution back to trajectory format
        auto trajectory = reshape_trajectory(solution, total_frames, feature_dim);

        // Apply global variance constraints if enabled
        if (config_.use_global_variance) {
            // Calculate target global variance from input statistics
            Eigen::VectorXd target_gv = Eigen::VectorXd::Zero(feature_dim);
            double total_weight = 0.0;
            
            for (size_t i = 0; i < variances.size(); ++i) {
                double weight = static_cast<double>(durations[i]);
                target_gv += weight * variances[i];
                total_weight += weight;
            }
            target_gv /= total_weight;
            
            trajectory = apply_global_variance_constraints(trajectory, target_gv, stats);
        }

        // Calculate final metrics
        stats->delta_smoothness_score = calculate_trajectory_smoothness(trajectory);

        auto end_time = std::chrono::high_resolution_clock::now();
        stats->optimization_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        if (config_.verbose) {
            std::cout << "MLPG completed in " << stats->optimization_time_ms << " ms" << std::endl;
            std::cout << "Final smoothness score: " << stats->delta_smoothness_score << std::endl;
        }

        return trajectory;
    }

    std::vector<Eigen::VectorXd> MlpgEngine::generate_trajectory_from_hmm(
        const std::vector<hmm::HmmState>& hmm_sequence,
        const std::vector<int>& durations,
        TrajectoryStats* stats
    ) {
        if (hmm_sequence.size() != durations.size()) {
            throw std::invalid_argument("HMM sequence and durations must have same size");
        }

        // Extract means and variances from HMM states
        std::vector<Eigen::VectorXd> means, variances;
        means.reserve(hmm_sequence.size());
        variances.reserve(hmm_sequence.size());

        for (const auto& state : hmm_sequence) {
            if (state.num_mixtures() == 0) {
                throw std::invalid_argument("HMM state has no Gaussian components");
            }
            
            // Use the most weighted mixture component 
            // In practice, this could be more sophisticated (mixture collapse or weighted mean)
            if (state.output_distribution.num_components() > 0) {
                const auto& dominant_component = state.output_distribution.component(0); // Use first component
                means.push_back(dominant_component.mean());
                variances.push_back(dominant_component.covariance().diagonal());
            } else {
                throw std::invalid_argument("HMM state has no Gaussian components");
            }
        }

        return generate_trajectory(means, variances, durations, stats);
    }

    Eigen::SparseMatrix<double> MlpgEngine::build_w_matrix(int total_frames, int feature_dim) {
        int expanded_dim = feature_dim;
        
        // Calculate expanded dimension based on enabled features
        if (config_.use_delta_features) {
            expanded_dim += feature_dim;
        }
        if (config_.use_delta_delta_features) {
            expanded_dim += feature_dim;
        }

        // Create W matrix: maps from static parameters to static+delta+delta-delta
        Eigen::SparseMatrix<double> W(total_frames * expanded_dim, total_frames * feature_dim);
        
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(total_frames * expanded_dim * 3); // Conservative estimate

        for (int t = 0; t < total_frames; ++t) {
            for (int d = 0; d < feature_dim; ++d) {
                int static_idx = t * expanded_dim + d;
                int param_idx = t * feature_dim + d;
                
                // Static features (identity mapping)
                triplets.emplace_back(static_idx, param_idx, 1.0);
                
                // Delta features (first-order difference)
                if (config_.use_delta_features) {
                    int delta_idx = t * expanded_dim + feature_dim + d;
                    
                    if (t > 0) {
                        triplets.emplace_back(delta_idx, param_idx, 0.5);
                        triplets.emplace_back(delta_idx, (t-1) * feature_dim + d, -0.5);
                    }
                    if (t < total_frames - 1) {
                        triplets.emplace_back(delta_idx, (t+1) * feature_dim + d, 0.5);
                        triplets.emplace_back(delta_idx, param_idx, -0.5);
                    }
                }
                
                // Delta-delta features (second-order difference)
                if (config_.use_delta_delta_features) {
                    int delta_delta_idx = t * expanded_dim + (config_.use_delta_features ? 2 : 1) * feature_dim + d;
                    
                    if (t > 1) {
                        triplets.emplace_back(delta_delta_idx, (t-2) * feature_dim + d, 0.25);
                    }
                    if (t > 0) {
                        triplets.emplace_back(delta_delta_idx, (t-1) * feature_dim + d, -0.5);
                    }
                    triplets.emplace_back(delta_delta_idx, param_idx, 1.0);
                    if (t < total_frames - 1) {
                        triplets.emplace_back(delta_delta_idx, (t+1) * feature_dim + d, -0.5);
                    }
                    if (t < total_frames - 2) {
                        triplets.emplace_back(delta_delta_idx, (t+2) * feature_dim + d, 0.25);
                    }
                }
            }
        }

        W.setFromTriplets(triplets.begin(), triplets.end());
        W.makeCompressed();

        if (config_.verbose) {
            std::cout << "Built W matrix: " << W.rows() << "x" << W.cols() 
                     << " (" << W.nonZeros() << " non-zeros)" << std::endl;
        }

        return W;
    }

    Eigen::SparseMatrix<double> MlpgEngine::build_precision_matrix(
        const std::vector<Eigen::VectorXd>& variances,
        const std::vector<int>& durations
    ) {
        int total_frames = 0;
        for (int duration : durations) {
            total_frames += duration;
        }
        
        int feature_dim = static_cast<int>(variances[0].size());
        int expanded_dim = feature_dim;
        
        if (config_.use_delta_features) {
            expanded_dim += feature_dim;
        }
        if (config_.use_delta_delta_features) {
            expanded_dim += feature_dim;
        }

        // Build block diagonal precision matrix
        Eigen::SparseMatrix<double> precision(total_frames * expanded_dim, total_frames * expanded_dim);
        
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(total_frames * expanded_dim);

        int frame_idx = 0;
        for (size_t state_idx = 0; state_idx < variances.size(); ++state_idx) {
            const auto& var = variances[state_idx];
            
            for (int d = 0; d < durations[state_idx]; ++d) {
                for (int feat = 0; feat < expanded_dim; ++feat) {
                    int global_idx = frame_idx * expanded_dim + feat;
                    
                    // Use variance from corresponding static feature dimension
                    int var_idx = feat % feature_dim;
                    double variance = var(var_idx);
                    
                    // Add regularization for numerical stability
                    variance += config_.regularization_factor;
                    
                    // Precision is inverse variance
                    double precision_value = 1.0 / variance;
                    triplets.emplace_back(global_idx, global_idx, precision_value);
                }
                frame_idx++;
            }
        }

        precision.setFromTriplets(triplets.begin(), triplets.end());
        precision.makeCompressed();

        if (config_.verbose) {
            std::cout << "Built precision matrix: " << precision.rows() << "x" << precision.cols()
                     << " (" << precision.nonZeros() << " non-zeros)" << std::endl;
        }

        return precision;
    }

    Eigen::VectorXd MlpgEngine::build_observation_vector(
        const std::vector<Eigen::VectorXd>& means,
        const std::vector<int>& durations
    ) {
        int total_frames = 0;
        for (int duration : durations) {
            total_frames += duration;
        }
        
        int feature_dim = static_cast<int>(means[0].size());
        int expanded_dim = feature_dim;
        
        if (config_.use_delta_features) {
            expanded_dim += feature_dim;
        }
        if (config_.use_delta_delta_features) {
            expanded_dim += feature_dim;
        }

        Eigen::VectorXd observations = Eigen::VectorXd::Zero(total_frames * expanded_dim);

        int frame_idx = 0;
        for (size_t state_idx = 0; state_idx < means.size(); ++state_idx) {
            const auto& mean = means[state_idx];
            
            for (int d = 0; d < durations[state_idx]; ++d) {
                // Static features
                for (int feat = 0; feat < feature_dim; ++feat) {
                    observations(frame_idx * expanded_dim + feat) = mean(feat);
                }
                
                // Delta and delta-delta features are typically zero-mean
                // (we're learning the trajectory that best fits the static targets)
                
                frame_idx++;
            }
        }

        if (config_.verbose) {
            std::cout << "Built observation vector with " << observations.size() << " elements" << std::endl;
        }

        return observations;
    }

    Eigen::VectorXd MlpgEngine::solve_mlpg_system(
        const Eigen::SparseMatrix<double>& W,
        const Eigen::SparseMatrix<double>& precision_matrix,
        const Eigen::VectorXd& observation_vector,
        TrajectoryStats* stats
    ) {
        if (config_.verbose) {
            std::cout << "Solving MLPG system..." << std::endl;
        }

        // Build system: (W^T * P * W) * c = W^T * P * o
        // Where c is the parameter trajectory, P is precision matrix, o is observations
        
        auto WTP = W.transpose() * precision_matrix;
        auto system_matrix = WTP * W;
        auto rhs_vector = WTP * observation_vector;

        // Convert system_matrix from product expression to actual sparse matrix, then add regularization
        Eigen::SparseMatrix<double> system_matrix_actual = system_matrix;
        for (int i = 0; i < system_matrix_actual.rows(); ++i) {
            system_matrix_actual.coeffRef(i, i) += config_.regularization_factor;
        }

        // Choose solver based on matrix properties
        Eigen::VectorXd solution;
        
        try {
            // Try Cholesky decomposition first (faster for positive definite matrices)
            Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> cholesky_solver;
            cholesky_solver.compute(system_matrix_actual);
            
            if (cholesky_solver.info() == Eigen::Success) {
                solution = cholesky_solver.solve(rhs_vector);
                stats->convergence_reason = "Cholesky decomposition";
                
                if (config_.verbose) {
                    std::cout << "Used Cholesky decomposition" << std::endl;
                }
            } else {
                throw std::runtime_error("Cholesky decomposition failed");
            }
        } catch (...) {
            // Fall back to LU decomposition
            if (config_.verbose) {
                std::cout << "Cholesky failed, using SparseLU..." << std::endl;
            }
            
            Eigen::SparseLU<Eigen::SparseMatrix<double>> lu_solver;
            lu_solver.compute(system_matrix_actual);
            
            if (lu_solver.info() != Eigen::Success) {
                throw std::runtime_error("MLPG system solve failed - matrix is singular");
            }
            
            solution = lu_solver.solve(rhs_vector);
            stats->convergence_reason = "SparseLU decomposition";
        }

        // Calculate final likelihood
        Eigen::VectorXd residual = W * solution - observation_vector;
        stats->final_likelihood = -0.5 * residual.transpose() * precision_matrix * residual;
        stats->iterations_used = 1; // Direct solve, no iterations needed

        if (config_.verbose) {
            std::cout << "System solved successfully" << std::endl;
            std::cout << "Final log-likelihood: " << stats->final_likelihood << std::endl;
        }

        return solution;
    }

    std::vector<Eigen::VectorXd> MlpgEngine::apply_global_variance_constraints(
        const std::vector<Eigen::VectorXd>& trajectory,
        const Eigen::VectorXd& target_gv,
        TrajectoryStats* stats
    ) {
        // This is a simplified GV constraint implementation
        // In practice, this would involve iterative optimization
        
        if (trajectory.empty()) {
            return trajectory;
        }

        auto constrained_trajectory = trajectory; // Copy for modification
        int feature_dim = static_cast<int>(trajectory[0].size());
        
        // Calculate current global variance
        Eigen::VectorXd current_mean = Eigen::VectorXd::Zero(feature_dim);
        for (const auto& frame : trajectory) {
            current_mean += frame;
        }
        current_mean /= static_cast<double>(trajectory.size());
        
        Eigen::VectorXd current_gv = Eigen::VectorXd::Zero(feature_dim);
        for (const auto& frame : trajectory) {
            Eigen::VectorXd diff = frame - current_mean;
            current_gv += diff.cwiseProduct(diff);
        }
        current_gv /= static_cast<double>(trajectory.size() - 1);
        
        // Apply simple variance scaling (more sophisticated methods exist)
        for (int d = 0; d < feature_dim; ++d) {
            if (current_gv(d) > 0 && target_gv(d) > 0) {
                double scale = std::sqrt(target_gv(d) / current_gv(d));
                scale = config_.gv_weight * scale + (1.0 - config_.gv_weight);
                
                for (auto& frame : constrained_trajectory) {
                    frame(d) = current_mean(d) + scale * (frame(d) - current_mean(d));
                }
            }
        }
        
        // Calculate GV constraint satisfaction
        stats->gv_constraint_satisfaction = 1.0 - (current_gv - target_gv).norm() / target_gv.norm();
        
        if (config_.verbose) {
            std::cout << "Applied GV constraints, satisfaction: " << stats->gv_constraint_satisfaction << std::endl;
        }
        
        return constrained_trajectory;
    }

    std::vector<Eigen::VectorXd> MlpgEngine::reshape_trajectory(
        const Eigen::VectorXd& flattened_trajectory,
        int frames,
        int feature_dim
    ) {
        if (flattened_trajectory.size() != frames * feature_dim) {
            throw std::invalid_argument("Trajectory size mismatch in reshape");
        }

        std::vector<Eigen::VectorXd> trajectory;
        trajectory.reserve(frames);

        for (int t = 0; t < frames; ++t) {
            Eigen::VectorXd frame(feature_dim);
            for (int d = 0; d < feature_dim; ++d) {
                frame(d) = flattened_trajectory(t * feature_dim + d);
            }
            trajectory.push_back(std::move(frame));
        }

        return trajectory;
    }

    double MlpgEngine::calculate_trajectory_smoothness(const std::vector<Eigen::VectorXd>& trajectory) {
        if (trajectory.size() < 3) {
            return 0.0;
        }

        double total_roughness = 0.0;
        for (size_t t = 1; t < trajectory.size() - 1; ++t) {
            // Calculate second-order difference (acceleration)
            auto accel = trajectory[t+1] - 2.0 * trajectory[t] + trajectory[t-1];
            total_roughness += accel.squaredNorm();
        }

        return total_roughness / static_cast<double>(trajectory.size() - 2);
    }

    void MlpgEngine::validate_inputs(
        const std::vector<Eigen::VectorXd>& means,
        const std::vector<Eigen::VectorXd>& variances,
        const std::vector<int>& durations
    ) {
        if (means.empty() || variances.empty() || durations.empty()) {
            throw std::invalid_argument("MLPG inputs cannot be empty");
        }

        if (means.size() != variances.size() || means.size() != durations.size()) {
            throw std::invalid_argument("MLPG input vectors must have same size");
        }

        if (means[0].size() == 0) {
            throw std::invalid_argument("Feature dimension cannot be zero");
        }

        int feature_dim = static_cast<int>(means[0].size());
        
        for (size_t i = 0; i < means.size(); ++i) {
            if (means[i].size() != feature_dim || variances[i].size() != feature_dim) {
                throw std::invalid_argument("Inconsistent feature dimensions in MLPG inputs");
            }
            
            if (durations[i] <= 0) {
                throw std::invalid_argument("State durations must be positive");
            }
            
            // Check for non-positive variances
            for (int d = 0; d < feature_dim; ++d) {
                if (variances[i](d) <= 0) {
                    throw std::invalid_argument("Variances must be positive");
                }
            }
        }
    }

    void MlpgEngine::enable_gpu_acceleration(bool enable) {
        use_gpu_ = enable;
        if (config_.verbose) {
            std::cout << "GPU acceleration " << (enable ? "enabled" : "disabled") << std::endl;
        }
        
        // GPU implementation would be added here
        // For now, this is a placeholder for future GPU optimization
    }

} // namespace mlpg
} // namespace nexussynth
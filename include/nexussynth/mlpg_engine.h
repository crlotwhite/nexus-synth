#pragma once

#include <vector>
#include <memory>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include "hmm_structures.h"

namespace nexussynth {
namespace mlpg {

    /**
     * @brief MLPG parameter configuration
     * 
     * Controls the Maximum Likelihood Parameter Generation process
     * for smooth trajectory generation from HMM statistics
     */
    struct MlpgConfig {
        bool use_delta_features = true;         // Enable Δ features
        bool use_delta_delta_features = true;   // Enable ΔΔ features
        bool use_global_variance = true;        // Enable GV constraints
        
        double regularization_factor = 1e-6;    // Numerical stability regularization
        double gv_weight = 1.0;                 // Global variance constraint weight
        
        int max_iterations = 100;               // Maximum optimization iterations
        double convergence_tolerance = 1e-5;    // Convergence threshold
        
        bool verbose = false;                   // Enable debug output
    };

    /**
     * @brief MLPG trajectory generation statistics
     * 
     * Performance and convergence metrics for trajectory optimization
     */
    struct TrajectoryStats {
        int iterations_used = 0;
        double final_likelihood = 0.0;
        double convergence_change = 0.0;
        
        std::vector<double> likelihood_history;
        std::string convergence_reason;
        
        double optimization_time_ms = 0.0;
        size_t matrix_size = 0;
        
        // Constraint satisfaction metrics
        double delta_smoothness_score = 0.0;
        double gv_constraint_satisfaction = 0.0;
    };

    /**
     * @brief Maximum Likelihood Parameter Generation Engine
     * 
     * Implements MLPG algorithm for generating smooth parameter trajectories
     * from HMM mean and variance statistics with delta/delta-delta constraints
     */
    class MlpgEngine {
    public:
        explicit MlpgEngine(const MlpgConfig& config = MlpgConfig{});
        ~MlpgEngine() = default;

        // Core MLPG functionality
        /**
         * @brief Generate parameter trajectory from HMM statistics
         * 
         * @param means Sequence of mean vectors from HMM states
         * @param variances Sequence of variance vectors from HMM states  
         * @param durations State durations (frames per state)
         * @param stats Optional statistics output
         * @return Generated smooth parameter trajectory
         */
        std::vector<Eigen::VectorXd> generate_trajectory(
            const std::vector<Eigen::VectorXd>& means,
            const std::vector<Eigen::VectorXd>& variances,
            const std::vector<int>& durations,
            TrajectoryStats* stats = nullptr
        );

        /**
         * @brief Generate trajectory from HMM state sequence
         * 
         * @param hmm_sequence Sequence of HMM states with emission statistics
         * @param durations State durations (frames per state)
         * @param stats Optional statistics output
         * @return Generated smooth parameter trajectory
         */
        std::vector<Eigen::VectorXd> generate_trajectory_from_hmm(
            const std::vector<hmm::HmmState>& hmm_sequence,
            const std::vector<int>& durations,
            TrajectoryStats* stats = nullptr
        );

        // Configuration management
        void set_config(const MlpgConfig& config) { config_ = config; }
        const MlpgConfig& get_config() const { return config_; }

        // Advanced features
        void enable_gpu_acceleration(bool enable = true);
        bool is_gpu_acceleration_enabled() const { return use_gpu_; }

    private:
        MlpgConfig config_;
        bool use_gpu_ = false;

        // Core algorithm components
        /**
         * @brief Construct W matrix for delta/delta-delta feature generation
         * 
         * @param total_frames Total number of frames in trajectory
         * @param feature_dim Feature vector dimension
         * @return Sparse W matrix for feature expansion
         */
        Eigen::SparseMatrix<double> build_w_matrix(int total_frames, int feature_dim);

        /**
         * @brief Build precision matrix (inverse covariance) from variances
         * 
         * @param variances Variance vectors from HMM states
         * @param durations State durations
         * @return Block diagonal precision matrix
         */
        Eigen::SparseMatrix<double> build_precision_matrix(
            const std::vector<Eigen::VectorXd>& variances,
            const std::vector<int>& durations
        );

        /**
         * @brief Build mean observation vector from HMM statistics
         * 
         * @param means Mean vectors from HMM states
         * @param durations State durations
         * @return Expanded mean vector for all frames
         */
        Eigen::VectorXd build_observation_vector(
            const std::vector<Eigen::VectorXd>& means,
            const std::vector<int>& durations
        );

        /**
         * @brief Solve the MLPG optimization problem
         * 
         * @param W W matrix for feature constraints
         * @param precision_matrix Precision (inverse covariance) matrix
         * @param observation_vector Target observations
         * @param stats Optional statistics output
         * @return Optimal parameter trajectory (flattened)
         */
        Eigen::VectorXd solve_mlpg_system(
            const Eigen::SparseMatrix<double>& W,
            const Eigen::SparseMatrix<double>& precision_matrix,
            const Eigen::VectorXd& observation_vector,
            TrajectoryStats* stats
        );

        /**
         * @brief Apply global variance constraints
         * 
         * @param trajectory Initial trajectory
         * @param target_gv Target global variance statistics
         * @param stats Optional statistics output
         * @return GV-constrained trajectory
         */
        std::vector<Eigen::VectorXd> apply_global_variance_constraints(
            const std::vector<Eigen::VectorXd>& trajectory,
            const Eigen::VectorXd& target_gv,
            TrajectoryStats* stats
        );

        // Utility functions
        /**
         * @brief Convert flattened trajectory to vector sequence
         * 
         * @param flattened_trajectory Flattened parameter vector
         * @param frames Number of frames
         * @param feature_dim Feature dimension
         * @return Sequence of parameter vectors
         */
        std::vector<Eigen::VectorXd> reshape_trajectory(
            const Eigen::VectorXd& flattened_trajectory,
            int frames, 
            int feature_dim
        );

        /**
         * @brief Calculate trajectory smoothness metrics
         * 
         * @param trajectory Parameter trajectory
         * @return Delta smoothness score (lower = smoother)
         */
        double calculate_trajectory_smoothness(const std::vector<Eigen::VectorXd>& trajectory);

        /**
         * @brief Validate MLPG inputs for consistency
         * 
         * @param means Mean vectors
         * @param variances Variance vectors  
         * @param durations State durations
         * @throws std::invalid_argument on validation failure
         */
        void validate_inputs(
            const std::vector<Eigen::VectorXd>& means,
            const std::vector<Eigen::VectorXd>& variances,
            const std::vector<int>& durations
        );
    };

} // namespace mlpg
} // namespace nexussynth
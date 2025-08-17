#pragma once

#include <vector>
#include <memory>
#include <numeric>
#include <cmath>
#include <limits>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Cholesky>

namespace nexussynth {
namespace hmm {

    /**
     * @brief Sufficient statistics for EM algorithm training
     * 
     * Accumulates statistics needed for Maximum Likelihood estimation
     * of Gaussian mixture model parameters using EM algorithm
     */
    struct SufficientStatistics {
        double gamma;               // Responsibility sum
        Eigen::VectorXd gamma_x;    // Weighted observation sum
        Eigen::MatrixXd gamma_xx;   // Weighted observation covariance sum
        
        SufficientStatistics() : gamma(0.0) {}
        
        explicit SufficientStatistics(int dimension) 
            : gamma(0.0)
            , gamma_x(Eigen::VectorXd::Zero(dimension))
            , gamma_xx(Eigen::MatrixXd::Zero(dimension, dimension)) {}
        
        void clear() {
            gamma = 0.0;
            gamma_x.setZero();
            gamma_xx.setZero();
        }
        
        void accumulate(const Eigen::VectorXd& observation, double responsibility) {
            gamma += responsibility;
            gamma_x += responsibility * observation;
            gamma_xx += responsibility * observation * observation.transpose();
        }
        
        // Update parameters from accumulated statistics
        void update_parameters(Eigen::VectorXd& mean, Eigen::MatrixXd& covariance) const {
            if (gamma > 0.0) {
                mean = gamma_x / gamma;
                covariance = (gamma_xx / gamma) - mean * mean.transpose();
                
                // Ensure positive definiteness
                make_positive_definite(covariance);
            }
        }
        
    private:
        void make_positive_definite(Eigen::MatrixXd& matrix) const {
            const double min_eigenvalue = 1e-6;
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(matrix);
            
            if (solver.info() == Eigen::Success) {
                Eigen::VectorXd eigenvalues = solver.eigenvalues();
                eigenvalues = eigenvalues.cwiseMax(min_eigenvalue);
                matrix = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
            } else {
                // Fallback: add diagonal regularization
                matrix += min_eigenvalue * Eigen::MatrixXd::Identity(matrix.rows(), matrix.cols());
            }
        }
    };

    /**
     * @brief Enhanced Gaussian component with advanced capabilities
     * 
     * Single Gaussian distribution with mean, covariance, and weight
     * Includes numerical stability features and efficient computation
     */
    class GaussianComponent {
    public:
        GaussianComponent();
        explicit GaussianComponent(int dimension);
        GaussianComponent(const Eigen::VectorXd& mean, const Eigen::MatrixXd& covariance, double weight = 1.0);
        
        // Parameter access
        const Eigen::VectorXd& mean() const { return mean_; }
        const Eigen::MatrixXd& covariance() const { return covariance_; }
        double weight() const { return weight_; }
        int dimension() const { return mean_.size(); }
        
        // Parameter modification
        void set_mean(const Eigen::VectorXd& mean);
        void set_covariance(const Eigen::MatrixXd& covariance);
        void set_weight(double weight);
        void set_parameters(const Eigen::VectorXd& mean, const Eigen::MatrixXd& covariance, double weight);
        
        // Probability calculations
        double log_pdf(const Eigen::VectorXd& observation) const;
        double pdf(const Eigen::VectorXd& observation) const;
        double mahalanobis_distance(const Eigen::VectorXd& observation) const;
        
        // Sample generation
        Eigen::VectorXd sample() const;
        std::vector<Eigen::VectorXd> sample(int num_samples) const;
        
        // Statistics and properties
        double determinant() const { return determinant_; }
        double log_determinant() const { return log_determinant_; }
        const Eigen::MatrixXd& precision() const { return precision_; }
        
        // Validation and regularization
        bool is_valid() const;
        void regularize(double min_variance = 1e-6);
        
        // Serialization support
        void serialize(std::vector<double>& buffer) const;
        void deserialize(const std::vector<double>& buffer, size_t& offset);
        
    private:
        Eigen::VectorXd mean_;
        Eigen::MatrixXd covariance_;
        double weight_;
        
        // Cached computations for efficiency
        Eigen::MatrixXd precision_;     // Inverse covariance
        double determinant_;            // Covariance determinant
        double log_determinant_;        // Log determinant
        double normalization_constant_; // 1/sqrt((2π)^k * |Σ|)
        bool cache_valid_;
        
        // Update cached values
        void update_cache();
        void invalidate_cache() { cache_valid_ = false; }
        
        // Regularization helpers
        void ensure_positive_definite();
        void add_regularization(double epsilon);
        
        // Constants for numerical stability
        static constexpr double MIN_VARIANCE = 1e-6;
    };

    /**
     * @brief Gaussian Mixture Model for HMM emission probabilities
     * 
     * Complete GMM implementation with EM training support,
     * numerical stability, and efficient likelihood computation
     */
    class GaussianMixture {
    public:
        GaussianMixture();
        explicit GaussianMixture(int num_components, int dimension);
        GaussianMixture(const std::vector<GaussianComponent>& components);
        
        // Component management
        size_t num_components() const { return components_.size(); }
        int dimension() const { return dimension_; }
        const GaussianComponent& component(size_t index) const;
        GaussianComponent& component(size_t index);
        
        void add_component(const GaussianComponent& component);
        void remove_component(size_t index);
        void clear_components();
        
        // Mixture weight management
        const std::vector<double>& weights() const { return weights_; }
        double weight(size_t index) const;
        void set_weight(size_t index, double weight);
        void set_weights(const std::vector<double>& weights);
        void normalize_weights();
        
        // Probability calculations
        double log_likelihood(const Eigen::VectorXd& observation) const;
        double likelihood(const Eigen::VectorXd& observation) const;
        std::vector<double> component_likelihoods(const Eigen::VectorXd& observation) const;
        std::vector<double> responsibilities(const Eigen::VectorXd& observation) const;
        
        // Batch processing
        double log_likelihood_sequence(const std::vector<Eigen::VectorXd>& observations) const;
        std::vector<double> log_likelihood_batch(const std::vector<Eigen::VectorXd>& observations) const;
        
        // Most likely component
        size_t most_likely_component(const Eigen::VectorXd& observation) const;
        
        // Sample generation
        Eigen::VectorXd sample() const;
        std::vector<Eigen::VectorXd> sample(int num_samples) const;
        
        // EM Algorithm support
        void initialize_from_data(const std::vector<Eigen::VectorXd>& data, int num_components);
        void initialize_kmeans(const std::vector<Eigen::VectorXd>& data, int num_components, int max_iterations = 100);
        
        double em_step(const std::vector<Eigen::VectorXd>& observations);
        double train_em(const std::vector<Eigen::VectorXd>& observations, int max_iterations = 100, double tolerance = 1e-6);
        
        // Model properties
        double aic(const std::vector<Eigen::VectorXd>& observations) const;  // Akaike Information Criterion
        double bic(const std::vector<Eigen::VectorXd>& observations) const;  // Bayesian Information Criterion
        int effective_parameters() const;
        
        // Validation and regularization
        bool is_valid() const;
        void regularize(double min_variance = 1e-6);
        void remove_empty_components(double min_weight = 1e-10);
        
        // Serialization
        std::vector<double> serialize() const;
        void deserialize(const std::vector<double>& buffer);
        
        // Statistics
        Eigen::VectorXd mean() const;  // Overall mixture mean
        Eigen::MatrixXd covariance() const;  // Overall mixture covariance
        
    private:
        std::vector<GaussianComponent> components_;
        std::vector<double> weights_;
        int dimension_;
        
        // EM algorithm internals
        std::vector<SufficientStatistics> accumulate_statistics(const std::vector<Eigen::VectorXd>& observations) const;
        void update_parameters(const std::vector<SufficientStatistics>& statistics);
        
        // Numerical stability helpers
        double log_sum_exp(const std::vector<double>& log_values) const;
        std::vector<double> log_normalize(const std::vector<double>& log_values) const;
        
        // Initialization helpers
        void initialize_components_randomly(const std::vector<Eigen::VectorXd>& data, int num_components);
        std::vector<size_t> kmeans_clustering(const std::vector<Eigen::VectorXd>& data, int num_clusters, int max_iterations) const;
        
        // Validation helpers
        void validate_dimension_consistency() const;
        void ensure_valid_weights();
        
        // Constants for numerical stability
        static constexpr double LOG_EPSILON = -700.0;  // Prevents log(0)
        static constexpr double MIN_WEIGHT = 1e-10;    // Minimum component weight
        static constexpr double MIN_VARIANCE = 1e-6;   // Minimum variance for regularization
    };

    /**
     * @brief Factory functions for common GMM configurations
     */
    namespace gmm_factory {
        
        // Create single Gaussian (special case of GMM)
        GaussianMixture create_single_gaussian(const Eigen::VectorXd& mean, const Eigen::MatrixXd& covariance);
        
        // Create diagonal covariance GMM
        GaussianMixture create_diagonal_gmm(int num_components, int dimension);
        
        // Create full covariance GMM
        GaussianMixture create_full_gmm(int num_components, int dimension);
        
        // Create from training data with automatic component selection
        GaussianMixture create_from_data(const std::vector<Eigen::VectorXd>& data, 
                                        int max_components = 8, 
                                        const std::string& selection_criterion = "bic");
        
        // Common speech synthesis configurations
        GaussianMixture create_speech_spectrum_gmm(int num_components = 3);  // For spectral features
        GaussianMixture create_f0_gmm(int num_components = 2);               // For F0 modeling
        GaussianMixture create_duration_gmm(int num_components = 2);         // For duration modeling
        
    } // namespace gmm_factory

} // namespace hmm
} // namespace nexussynth
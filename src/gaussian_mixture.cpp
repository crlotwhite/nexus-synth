#include "nexussynth/gaussian_mixture.h"
#include <random>
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace nexussynth {
namespace hmm {

// GaussianComponent implementation
GaussianComponent::GaussianComponent() 
    : weight_(1.0), determinant_(1.0), log_determinant_(0.0), normalization_constant_(1.0), cache_valid_(false) {
    // Default to 1D Gaussian
    mean_ = Eigen::VectorXd::Zero(1);
    covariance_ = Eigen::MatrixXd::Identity(1, 1);
    precision_ = Eigen::MatrixXd::Identity(1, 1);
    update_cache();
}

GaussianComponent::GaussianComponent(int dimension) 
    : weight_(1.0), cache_valid_(false) {
    mean_ = Eigen::VectorXd::Zero(dimension);
    covariance_ = Eigen::MatrixXd::Identity(dimension, dimension);
    precision_ = Eigen::MatrixXd::Identity(dimension, dimension);
    update_cache();
}

GaussianComponent::GaussianComponent(const Eigen::VectorXd& mean, const Eigen::MatrixXd& covariance, double weight)
    : mean_(mean), covariance_(covariance), weight_(weight), cache_valid_(false) {
    if (mean.size() != covariance.rows() || covariance.rows() != covariance.cols()) {
        throw std::invalid_argument("Dimension mismatch between mean and covariance");
    }
    update_cache();
}

void GaussianComponent::set_mean(const Eigen::VectorXd& mean) {
    if (mean.size() != dimension()) {
        throw std::invalid_argument("Mean dimension mismatch");
    }
    mean_ = mean;
    // No need to invalidate cache - only mean changed
}

void GaussianComponent::set_covariance(const Eigen::MatrixXd& covariance) {
    if (covariance.rows() != dimension() || covariance.cols() != dimension()) {
        throw std::invalid_argument("Covariance dimension mismatch");
    }
    covariance_ = covariance;
    invalidate_cache();
    update_cache();
}

void GaussianComponent::set_weight(double weight) {
    if (weight < 0.0) {
        throw std::invalid_argument("Weight must be non-negative");
    }
    weight_ = weight;
}

void GaussianComponent::set_parameters(const Eigen::VectorXd& mean, const Eigen::MatrixXd& covariance, double weight) {
    set_mean(mean);
    set_covariance(covariance);
    set_weight(weight);
}

double GaussianComponent::log_pdf(const Eigen::VectorXd& observation) const {
    if (observation.size() != dimension()) {
        throw std::invalid_argument("Observation dimension mismatch");
    }
    
    if (!cache_valid_) {
        const_cast<GaussianComponent*>(this)->update_cache();
    }
    
    Eigen::VectorXd diff = observation - mean_;
    double mahal_dist = diff.transpose() * precision_ * diff;
    
    return std::log(normalization_constant_) - 0.5 * mahal_dist;
}

double GaussianComponent::pdf(const Eigen::VectorXd& observation) const {
    return std::exp(log_pdf(observation));
}

double GaussianComponent::mahalanobis_distance(const Eigen::VectorXd& observation) const {
    if (!cache_valid_) {
        const_cast<GaussianComponent*>(this)->update_cache();
    }
    
    Eigen::VectorXd diff = observation - mean_;
    return std::sqrt(diff.transpose() * precision_ * diff);
}

Eigen::VectorXd GaussianComponent::sample() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::normal_distribution<double> normal(0.0, 1.0);
    
    // Generate standard normal vector
    Eigen::VectorXd z(dimension());
    for (int i = 0; i < dimension(); ++i) {
        z[i] = normal(gen);
    }
    
    // Transform using Cholesky decomposition
    Eigen::LLT<Eigen::MatrixXd> llt(covariance_);
    if (llt.info() == Eigen::Success) {
        return mean_ + llt.matrixL() * z;
    } else {
        // Fallback: use diagonal approximation
        Eigen::VectorXd std_dev = covariance_.diagonal().cwiseSqrt();
        return mean_ + std_dev.cwiseProduct(z);
    }
}

std::vector<Eigen::VectorXd> GaussianComponent::sample(int num_samples) const {
    std::vector<Eigen::VectorXd> samples;
    samples.reserve(num_samples);
    
    for (int i = 0; i < num_samples; ++i) {
        samples.push_back(sample());
    }
    
    return samples;
}

bool GaussianComponent::is_valid() const {
    // Check for finite values
    if (!mean_.allFinite() || !covariance_.allFinite() || !std::isfinite(weight_)) {
        return false;
    }
    
    // Check weight is non-negative
    if (weight_ < 0.0) {
        return false;
    }
    
    // Check covariance is positive definite
    Eigen::LLT<Eigen::MatrixXd> llt(covariance_);
    return llt.info() == Eigen::Success;
}

void GaussianComponent::regularize(double min_variance) {
    // Ensure minimum variance on diagonal
    for (int i = 0; i < dimension(); ++i) {
        if (covariance_(i, i) < min_variance) {
            covariance_(i, i) = min_variance;
        }
    }
    
    ensure_positive_definite();
    invalidate_cache();
    update_cache();
}

void GaussianComponent::update_cache() {
    try {
        // Compute precision matrix (inverse covariance)
        Eigen::LLT<Eigen::MatrixXd> llt(covariance_);
        if (llt.info() != Eigen::Success) {
            ensure_positive_definite();
            llt.compute(covariance_);
        }
        
        precision_ = covariance_.inverse();
        
        // Compute determinant
        determinant_ = covariance_.determinant();
        log_determinant_ = std::log(determinant_);
        
        // Compute normalization constant: 1/sqrt((2π)^k * |Σ|)
        double k = static_cast<double>(dimension());
        double log_norm = -0.5 * (k * std::log(2.0 * M_PI) + log_determinant_);
        normalization_constant_ = std::exp(log_norm);
        
        cache_valid_ = true;
        
    } catch (const std::exception&) {
        // Fallback regularization
        add_regularization(MIN_VARIANCE);
        cache_valid_ = false;  // Will retry on next access
    }
}

void GaussianComponent::ensure_positive_definite() {
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(covariance_);
    if (solver.info() == Eigen::Success) {
        Eigen::VectorXd eigenvalues = solver.eigenvalues();
        
        // Check if any eigenvalue is too small
        bool needs_fix = false;
        for (int i = 0; i < eigenvalues.size(); ++i) {
            if (eigenvalues[i] < MIN_VARIANCE) {
                eigenvalues[i] = MIN_VARIANCE;
                needs_fix = true;
            }
        }
        
        if (needs_fix) {
            covariance_ = solver.eigenvectors() * eigenvalues.asDiagonal() * solver.eigenvectors().transpose();
        }
    } else {
        add_regularization(MIN_VARIANCE);
    }
}

void GaussianComponent::add_regularization(double epsilon) {
    covariance_ += epsilon * Eigen::MatrixXd::Identity(dimension(), dimension());
}

// GaussianMixture implementation
GaussianMixture::GaussianMixture() : dimension_(0) {
}

GaussianMixture::GaussianMixture(int num_components, int dimension) : dimension_(dimension) {
    components_.reserve(num_components);
    weights_.reserve(num_components);
    
    double uniform_weight = 1.0 / num_components;
    for (int i = 0; i < num_components; ++i) {
        components_.emplace_back(dimension);
        weights_.push_back(uniform_weight);
    }
}

GaussianMixture::GaussianMixture(const std::vector<GaussianComponent>& components) 
    : components_(components) {
    
    if (components.empty()) {
        dimension_ = 0;
        return;
    }
    
    dimension_ = components[0].dimension();
    validate_dimension_consistency();
    
    // Initialize uniform weights
    double uniform_weight = 1.0 / components.size();
    weights_.assign(components.size(), uniform_weight);
}

const GaussianComponent& GaussianMixture::component(size_t index) const {
    if (index >= components_.size()) {
        throw std::out_of_range("Component index out of range");
    }
    return components_[index];
}

GaussianComponent& GaussianMixture::component(size_t index) {
    if (index >= components_.size()) {
        throw std::out_of_range("Component index out of range");
    }
    return components_[index];
}

void GaussianMixture::add_component(const GaussianComponent& component) {
    if (components_.empty()) {
        dimension_ = component.dimension();
    } else if (component.dimension() != dimension_) {
        throw std::invalid_argument("Component dimension mismatch");
    }
    
    components_.push_back(component);
    weights_.push_back(1.0 / components_.size());
    normalize_weights();
}

void GaussianMixture::remove_component(size_t index) {
    if (index >= components_.size()) {
        throw std::out_of_range("Component index out of range");
    }
    
    components_.erase(components_.begin() + index);
    weights_.erase(weights_.begin() + index);
    normalize_weights();
}

void GaussianMixture::clear_components() {
    components_.clear();
    weights_.clear();
    dimension_ = 0;
}

double GaussianMixture::weight(size_t index) const {
    if (index >= weights_.size()) {
        throw std::out_of_range("Weight index out of range");
    }
    return weights_[index];
}

void GaussianMixture::set_weight(size_t index, double weight) {
    if (index >= weights_.size()) {
        throw std::out_of_range("Weight index out of range");
    }
    if (weight < 0.0) {
        throw std::invalid_argument("Weight must be non-negative");
    }
    weights_[index] = weight;
}

void GaussianMixture::set_weights(const std::vector<double>& weights) {
    if (weights.size() != components_.size()) {
        throw std::invalid_argument("Weight vector size mismatch");
    }
    
    for (double w : weights) {
        if (w < 0.0) {
            throw std::invalid_argument("All weights must be non-negative");
        }
    }
    
    weights_ = weights;
    normalize_weights();
}

void GaussianMixture::normalize_weights() {
    if (weights_.empty()) return;
    
    double sum = std::accumulate(weights_.begin(), weights_.end(), 0.0);
    if (sum > 0.0) {
        for (double& w : weights_) {
            w /= sum;
        }
    } else {
        // Uniform weights if all are zero
        std::fill(weights_.begin(), weights_.end(), 1.0 / weights_.size());
    }
}

double GaussianMixture::log_likelihood(const Eigen::VectorXd& observation) const {
    if (components_.empty()) {
        return LOG_EPSILON;
    }
    
    std::vector<double> log_weighted_likelihoods;
    log_weighted_likelihoods.reserve(components_.size());
    
    for (size_t i = 0; i < components_.size(); ++i) {
        double log_weight = (weights_[i] > 0.0) ? std::log(weights_[i]) : LOG_EPSILON;
        double log_comp_likelihood = components_[i].log_pdf(observation);
        log_weighted_likelihoods.push_back(log_weight + log_comp_likelihood);
    }
    
    return log_sum_exp(log_weighted_likelihoods);
}

double GaussianMixture::likelihood(const Eigen::VectorXd& observation) const {
    return std::exp(log_likelihood(observation));
}

std::vector<double> GaussianMixture::component_likelihoods(const Eigen::VectorXd& observation) const {
    std::vector<double> likelihoods;
    likelihoods.reserve(components_.size());
    
    for (const auto& component : components_) {
        likelihoods.push_back(component.pdf(observation));
    }
    
    return likelihoods;
}

std::vector<double> GaussianMixture::responsibilities(const Eigen::VectorXd& observation) const {
    std::vector<double> log_responsibilities;
    log_responsibilities.reserve(components_.size());
    
    // Compute log(weight * P(x|component)) for each component
    for (size_t i = 0; i < components_.size(); ++i) {
        double log_weight = (weights_[i] > 0.0) ? std::log(weights_[i]) : LOG_EPSILON;
        double log_comp_likelihood = components_[i].log_pdf(observation);
        log_responsibilities.push_back(log_weight + log_comp_likelihood);
    }
    
    // Normalize to get responsibilities
    return log_normalize(log_responsibilities);
}

double GaussianMixture::log_likelihood_sequence(const std::vector<Eigen::VectorXd>& observations) const {
    double total_log_likelihood = 0.0;
    
    for (const auto& obs : observations) {
        total_log_likelihood += log_likelihood(obs);
    }
    
    return total_log_likelihood;
}

std::vector<double> GaussianMixture::log_likelihood_batch(const std::vector<Eigen::VectorXd>& observations) const {
    std::vector<double> likelihoods;
    likelihoods.reserve(observations.size());
    
    for (const auto& obs : observations) {
        likelihoods.push_back(log_likelihood(obs));
    }
    
    return likelihoods;
}

size_t GaussianMixture::most_likely_component(const Eigen::VectorXd& observation) const {
    std::vector<double> resp = responsibilities(observation);
    return std::distance(resp.begin(), std::max_element(resp.begin(), resp.end()));
}

Eigen::VectorXd GaussianMixture::sample() const {
    if (components_.empty()) {
        return Eigen::VectorXd();
    }
    
    // Select component based on weights
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::discrete_distribution<size_t> dist(weights_.begin(), weights_.end());
    
    size_t selected_component = dist(gen);
    return components_[selected_component].sample();
}

std::vector<Eigen::VectorXd> GaussianMixture::sample(int num_samples) const {
    std::vector<Eigen::VectorXd> samples;
    samples.reserve(num_samples);
    
    for (int i = 0; i < num_samples; ++i) {
        samples.push_back(sample());
    }
    
    return samples;
}

double GaussianMixture::em_step(const std::vector<Eigen::VectorXd>& observations) {
    if (observations.empty() || components_.empty()) {
        return 0.0;
    }
    
    // E-step: Accumulate sufficient statistics
    auto statistics = accumulate_statistics(observations);
    
    // M-step: Update parameters
    update_parameters(statistics);
    
    // Compute log-likelihood for convergence checking
    return log_likelihood_sequence(observations);
}

double GaussianMixture::train_em(const std::vector<Eigen::VectorXd>& observations, int max_iterations, double tolerance) {
    if (observations.empty()) {
        return 0.0;
    }
    
    double prev_log_likelihood = log_likelihood_sequence(observations);
    double log_likelihood = prev_log_likelihood;
    
    for (int iter = 0; iter < max_iterations; ++iter) {
        log_likelihood = em_step(observations);
        
        // Check for convergence
        if (std::abs(log_likelihood - prev_log_likelihood) < tolerance) {
            break;
        }
        
        prev_log_likelihood = log_likelihood;
    }
    
    return log_likelihood;
}

bool GaussianMixture::is_valid() const {
    if (components_.size() != weights_.size()) {
        return false;
    }
    
    for (const auto& component : components_) {
        if (!component.is_valid()) {
            return false;
        }
    }
    
    double weight_sum = std::accumulate(weights_.begin(), weights_.end(), 0.0);
    return std::abs(weight_sum - 1.0) < 1e-6;
}

void GaussianMixture::regularize(double min_variance) {
    for (auto& component : components_) {
        component.regularize(min_variance);
    }
    
    remove_empty_components();
    normalize_weights();
}

void GaussianMixture::remove_empty_components(double min_weight) {
    for (int i = static_cast<int>(components_.size()) - 1; i >= 0; --i) {
        if (weights_[i] < min_weight) {
            remove_component(i);
        }
    }
}

// Private helper methods
std::vector<SufficientStatistics> GaussianMixture::accumulate_statistics(
    const std::vector<Eigen::VectorXd>& observations) const {
    
    std::vector<SufficientStatistics> statistics(components_.size(), SufficientStatistics(dimension_));
    
    for (const auto& obs : observations) {
        auto resp = responsibilities(obs);
        
        for (size_t i = 0; i < components_.size(); ++i) {
            statistics[i].accumulate(obs, resp[i]);
        }
    }
    
    return statistics;
}

void GaussianMixture::update_parameters(const std::vector<SufficientStatistics>& statistics) {
    double total_gamma = 0.0;
    
    // Calculate total responsibilities
    for (const auto& stat : statistics) {
        total_gamma += stat.gamma;
    }
    
    // Update each component
    for (size_t i = 0; i < components_.size(); ++i) {
        const auto& stat = statistics[i];
        
        if (stat.gamma > MIN_WEIGHT) {
            // Update weight
            weights_[i] = stat.gamma / total_gamma;
            
            // Update component parameters
            Eigen::VectorXd new_mean = components_[i].mean();
            Eigen::MatrixXd new_covariance = components_[i].covariance();
            
            stat.update_parameters(new_mean, new_covariance);
            components_[i].set_parameters(new_mean, new_covariance, weights_[i]);
        }
    }
    
    normalize_weights();
}

double GaussianMixture::log_sum_exp(const std::vector<double>& log_values) const {
    if (log_values.empty()) {
        return LOG_EPSILON;
    }
    
    double max_log = *std::max_element(log_values.begin(), log_values.end());
    
    if (max_log <= LOG_EPSILON) {
        return LOG_EPSILON;
    }
    
    double sum = 0.0;
    for (double log_val : log_values) {
        sum += std::exp(log_val - max_log);
    }
    
    return max_log + std::log(sum);
}

std::vector<double> GaussianMixture::log_normalize(const std::vector<double>& log_values) const {
    double log_sum = log_sum_exp(log_values);
    
    std::vector<double> normalized;
    normalized.reserve(log_values.size());
    
    for (double log_val : log_values) {
        normalized.push_back(std::exp(log_val - log_sum));
    }
    
    return normalized;
}

void GaussianMixture::validate_dimension_consistency() const {
    for (const auto& component : components_) {
        if (component.dimension() != dimension_) {
            throw std::invalid_argument("Inconsistent component dimensions");
        }
    }
}

Eigen::VectorXd GaussianMixture::mean() const {
    if (components_.empty()) {
        return Eigen::VectorXd();
    }
    
    Eigen::VectorXd mixture_mean = Eigen::VectorXd::Zero(dimension_);
    
    for (size_t i = 0; i < components_.size(); ++i) {
        mixture_mean += weights_[i] * components_[i].mean();
    }
    
    return mixture_mean;
}

Eigen::MatrixXd GaussianMixture::covariance() const {
    if (components_.empty()) {
        return Eigen::MatrixXd();
    }
    
    Eigen::VectorXd mixture_mean = mean();
    Eigen::MatrixXd mixture_cov = Eigen::MatrixXd::Zero(dimension_, dimension_);
    
    for (size_t i = 0; i < components_.size(); ++i) {
        Eigen::VectorXd diff = components_[i].mean() - mixture_mean;
        mixture_cov += weights_[i] * (components_[i].covariance() + diff * diff.transpose());
    }
    
    return mixture_cov;
}

// Factory functions
namespace gmm_factory {

GaussianMixture create_single_gaussian(const Eigen::VectorXd& mean, const Eigen::MatrixXd& covariance) {
    return GaussianMixture({GaussianComponent(mean, covariance, 1.0)});
}

GaussianMixture create_diagonal_gmm(int num_components, int dimension) {
    GaussianMixture gmm(num_components, dimension);
    
    // Initialize with diagonal covariances
    for (size_t i = 0; i < gmm.num_components(); ++i) {
        Eigen::MatrixXd diag_cov = Eigen::MatrixXd::Identity(dimension, dimension);
        gmm.component(i).set_covariance(diag_cov);
    }
    
    return gmm;
}

GaussianMixture create_full_gmm(int num_components, int dimension) {
    return GaussianMixture(num_components, dimension);
}

GaussianMixture create_speech_spectrum_gmm(int num_components) {
    return create_diagonal_gmm(num_components, 25);  // Typical spectral feature dimension
}

GaussianMixture create_f0_gmm(int num_components) {
    return create_diagonal_gmm(num_components, 3);   // F0 + delta + delta-delta
}

GaussianMixture create_duration_gmm(int num_components) {
    return create_diagonal_gmm(num_components, 1);   // Duration feature
}

} // namespace gmm_factory

} // namespace hmm
} // namespace nexussynth
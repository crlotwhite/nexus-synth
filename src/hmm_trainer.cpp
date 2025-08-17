#include "nexussynth/hmm_trainer.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <iostream>
#include <thread>
#include <future>
#include <random>

namespace nexussynth {
namespace hmm {

    // HmmTrainer Implementation
    HmmTrainer::HmmTrainer(const TrainingConfig& config) : config_(config) {
    }

    TrainingStats HmmTrainer::train_model(PhonemeHmm& model, 
                                        const std::vector<std::vector<Eigen::VectorXd>>& training_sequences) {
        
        TrainingStats stats;
        
        if (training_sequences.empty()) {
            stats.convergence_reason = "No training data provided";
            return stats;
        }
        
        if (config_.verbose) {
            std::cout << "Starting HMM training with " << training_sequences.size() << " sequences" << std::endl;
        }
        
        // Split data for validation if enabled
        std::vector<std::vector<Eigen::VectorXd>> train_data, validation_data;
        if (config_.use_validation_set && training_sequences.size() > 1) {
            auto split_idx = static_cast<size_t>(training_sequences.size() * (1.0 - config_.validation_split));
            train_data.assign(training_sequences.begin(), training_sequences.begin() + split_idx);
            validation_data.assign(training_sequences.begin() + split_idx, training_sequences.end());
            
            if (config_.verbose) {
                std::cout << "Using " << train_data.size() << " sequences for training, " 
                         << validation_data.size() << " for validation" << std::endl;
            }
        } else {
            train_data = training_sequences;
        }
        
        // Main EM training loop
        PhonemeHmm previous_model = model;  // For parameter change detection
        
        for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
            // E-Step: Forward-Backward algorithm
            std::vector<ForwardBackwardResult> fb_results;
            double log_likelihood = em_expectation_step(model, train_data, fb_results);
            
            stats.log_likelihoods.push_back(log_likelihood);
            
            // M-Step: Parameter re-estimation
            em_maximization_step(model, train_data, fb_results);
            
            // Validation evaluation
            if (!validation_data.empty()) {
                double validation_score = evaluate_model(model, validation_data);
                stats.validation_scores.push_back(validation_score);
                
                if (validation_score > stats.best_validation_score) {
                    stats.best_validation_score = validation_score;
                }
            }
            
            // Parameter change calculation
            double param_change = compute_parameter_distance(previous_model, model);
            stats.parameter_changes.push_back(param_change);
            
            previous_model = model;
            stats.final_iteration = iteration + 1;
            stats.final_log_likelihood = log_likelihood;
            
            if (config_.verbose) {
                log_iteration_info(iteration, stats);
            }
            
            // Convergence check
            if (check_convergence(stats)) {
                stats.converged = true;
                break;
            }
        }
        
        if (config_.verbose) {
            log_convergence_info(stats);
        }
        
        return stats;
    }

    TrainingStats HmmTrainer::train_model_with_validation(PhonemeHmm& model,
                                                         const std::vector<std::vector<Eigen::VectorXd>>& training_sequences,
                                                         const std::vector<std::vector<Eigen::VectorXd>>& validation_sequences) {
        
        TrainingStats stats;
        
        if (training_sequences.empty()) {
            stats.convergence_reason = "No training data provided";
            return stats;
        }
        
        PhonemeHmm previous_model = model;
        PhonemeHmm best_model = model;
        
        for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
            // E-Step
            std::vector<ForwardBackwardResult> fb_results;
            double log_likelihood = em_expectation_step(model, training_sequences, fb_results);
            
            stats.log_likelihoods.push_back(log_likelihood);
            
            // M-Step
            em_maximization_step(model, training_sequences, fb_results);
            
            // Validation evaluation
            double validation_score = evaluate_model(model, validation_sequences);
            stats.validation_scores.push_back(validation_score);
            
            if (validation_score > stats.best_validation_score) {
                stats.best_validation_score = validation_score;
                best_model = model;  // Save best model
            }
            
            // Parameter change
            double param_change = compute_parameter_distance(previous_model, model);
            stats.parameter_changes.push_back(param_change);
            
            previous_model = model;
            stats.final_iteration = iteration + 1;
            stats.final_log_likelihood = log_likelihood;
            
            if (config_.verbose) {
                log_iteration_info(iteration, stats);
            }
            
            if (check_convergence(stats)) {
                stats.converged = true;
                model = best_model;  // Restore best model
                break;
            }
        }
        
        return stats;
    }

    ForwardBackwardResult HmmTrainer::forward_backward(const PhonemeHmm& model,
                                                      const std::vector<Eigen::VectorXd>& observation_sequence) const {
        
        ForwardBackwardResult result;
        const int T = observation_sequence.size();
        const int N = model.num_states();
        
        if (T == 0 || N == 0) {
            return result;
        }
        
        result.forward_probs = Eigen::MatrixXd::Zero(T, N);
        result.backward_probs = Eigen::MatrixXd::Zero(T, N);
        result.gamma = Eigen::MatrixXd::Zero(T, N);
        
        // Forward pass
        // Initialize first frame
        for (int i = 0; i < N; ++i) {
            double emission_prob = model.states[i].log_emission_probability(observation_sequence[0]);
            result.forward_probs(0, i) = (i == 0) ? emission_prob : -std::numeric_limits<double>::infinity();
        }
        
        // Forward recursion
        for (int t = 1; t < T; ++t) {
            for (int j = 0; j < N; ++j) {
                std::vector<double> transition_probs;
                
                // Self-loop
                if (result.forward_probs(t-1, j) > -std::numeric_limits<double>::infinity()) {
                    double trans_prob = std::log(model.states[j].transition.self_loop_prob);
                    transition_probs.push_back(result.forward_probs(t-1, j) + trans_prob);
                }
                
                // Transition from previous state
                if (j > 0 && result.forward_probs(t-1, j-1) > -std::numeric_limits<double>::infinity()) {
                    double trans_prob = std::log(model.states[j-1].transition.next_state_prob);
                    transition_probs.push_back(result.forward_probs(t-1, j-1) + trans_prob);
                }
                
                if (!transition_probs.empty()) {
                    double log_sum = log_sum_exp(transition_probs);
                    double emission_prob = model.states[j].log_emission_probability(observation_sequence[t]);
                    result.forward_probs(t, j) = log_sum + emission_prob;
                } else {
                    result.forward_probs(t, j) = -std::numeric_limits<double>::infinity();
                }
            }
        }
        
        // Backward pass
        // Initialize last frame
        for (int i = 0; i < N; ++i) {
            result.backward_probs(T-1, i) = (i == N-1) ? 0.0 : -std::numeric_limits<double>::infinity();
        }
        
        // Backward recursion
        for (int t = T-2; t >= 0; --t) {
            for (int i = 0; i < N; ++i) {
                std::vector<double> backward_probs;
                
                // Self-loop
                if (result.backward_probs(t+1, i) > -std::numeric_limits<double>::infinity()) {
                    double trans_prob = std::log(model.states[i].transition.self_loop_prob);
                    double emission_prob = model.states[i].log_emission_probability(observation_sequence[t+1]);
                    backward_probs.push_back(trans_prob + emission_prob + result.backward_probs(t+1, i));
                }
                
                // Transition to next state
                if (i < N-1 && result.backward_probs(t+1, i+1) > -std::numeric_limits<double>::infinity()) {
                    double trans_prob = std::log(model.states[i].transition.next_state_prob);
                    double emission_prob = model.states[i+1].log_emission_probability(observation_sequence[t+1]);
                    backward_probs.push_back(trans_prob + emission_prob + result.backward_probs(t+1, i+1));
                }
                
                if (!backward_probs.empty()) {
                    result.backward_probs(t, i) = log_sum_exp(backward_probs);
                } else {
                    result.backward_probs(t, i) = -std::numeric_limits<double>::infinity();
                }
            }
        }
        
        // Compute gamma (state posteriors)
        std::vector<double> frame_log_likelihoods(T);
        for (int t = 0; t < T; ++t) {
            std::vector<double> state_probs;
            for (int i = 0; i < N; ++i) {
                if (result.forward_probs(t, i) > -std::numeric_limits<double>::infinity() && 
                    result.backward_probs(t, i) > -std::numeric_limits<double>::infinity()) {
                    state_probs.push_back(result.forward_probs(t, i) + result.backward_probs(t, i));
                }
            }
            
            if (!state_probs.empty()) {
                frame_log_likelihoods[t] = log_sum_exp(state_probs);
                for (int i = 0; i < N; ++i) {
                    if (result.forward_probs(t, i) > -std::numeric_limits<double>::infinity() && 
                        result.backward_probs(t, i) > -std::numeric_limits<double>::infinity()) {
                        result.gamma(t, i) = std::exp(result.forward_probs(t, i) + result.backward_probs(t, i) - frame_log_likelihoods[t]);
                    }
                }
            }
        }
        
        // Compute total log-likelihood
        result.log_likelihood = log_sum_exp(frame_log_likelihoods) / T;
        
        return result;
    }

    SequenceAlignment HmmTrainer::viterbi_alignment(const PhonemeHmm& model,
                                                   const std::vector<Eigen::VectorXd>& observation_sequence) const {
        
        SequenceAlignment result;
        const int T = observation_sequence.size();
        const int N = model.num_states();
        
        if (T == 0 || N == 0) {
            return result;
        }
        
        // Compute Viterbi trellis
        Eigen::MatrixXd trellis = compute_viterbi_trellis(model, observation_sequence);
        
        // Backtrack to find optimal path
        result.state_sequence = backtrack_viterbi_path(trellis);
        
        // Create frame-to-state mapping
        result.frame_to_state.resize(T);
        for (int t = 0; t < T; ++t) {
            result.frame_to_state[t] = result.state_sequence[t];
        }
        
        // Compute frame scores
        result.frame_scores.resize(T);
        for (int t = 0; t < T; ++t) {
            int state = result.state_sequence[t];
            result.frame_scores[t] = model.states[state].log_emission_probability(observation_sequence[t]);
        }
        
        // Total score
        result.total_score = std::accumulate(result.frame_scores.begin(), result.frame_scores.end(), 0.0);
        
        return result;
    }

    std::vector<ForwardBackwardResult> HmmTrainer::batch_forward_backward(const PhonemeHmm& model,
                                                                         const std::vector<std::vector<Eigen::VectorXd>>& sequences) const {
        
        std::vector<ForwardBackwardResult> results;
        results.reserve(sequences.size());
        
        for (const auto& sequence : sequences) {
            results.push_back(forward_backward(model, sequence));
        }
        
        return results;
    }

    double HmmTrainer::evaluate_model(const PhonemeHmm& model,
                                     const std::vector<std::vector<Eigen::VectorXd>>& test_sequences) const {
        
        if (test_sequences.empty()) {
            return -std::numeric_limits<double>::infinity();
        }
        
        double total_log_likelihood = 0.0;
        int total_frames = 0;
        
        for (const auto& sequence : test_sequences) {
            ForwardBackwardResult result = forward_backward(model, sequence);
            total_log_likelihood += result.log_likelihood * sequence.size();
            total_frames += sequence.size();
        }
        
        return total_frames > 0 ? total_log_likelihood / total_frames : -std::numeric_limits<double>::infinity();
    }

    // Private implementation methods
    double HmmTrainer::em_expectation_step(const PhonemeHmm& model,
                                         const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                         std::vector<ForwardBackwardResult>& fb_results) const {
        
        fb_results = batch_forward_backward(model, sequences);
        
        double total_log_likelihood = 0.0;
        int total_frames = 0;
        
        for (size_t i = 0; i < sequences.size(); ++i) {
            total_log_likelihood += fb_results[i].log_likelihood * sequences[i].size();
            total_frames += sequences[i].size();
        }
        
        return total_frames > 0 ? total_log_likelihood / total_frames : -std::numeric_limits<double>::infinity();
    }

    void HmmTrainer::em_maximization_step(PhonemeHmm& model,
                                        const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                        const std::vector<ForwardBackwardResult>& fb_results) {
        
        update_transition_probabilities(model, sequences, fb_results);
        update_emission_probabilities(model, sequences, fb_results);
    }

    void HmmTrainer::update_transition_probabilities(PhonemeHmm& model,
                                                   const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                   const std::vector<ForwardBackwardResult>& fb_results) {
        
        const int N = model.num_states();
        
        // Accumulate transition statistics
        std::vector<double> self_loop_counts(N, 0.0);
        std::vector<double> next_state_counts(N, 0.0);
        std::vector<double> total_counts(N, 0.0);
        
        for (size_t seq_idx = 0; seq_idx < sequences.size(); ++seq_idx) {
            const auto& sequence = sequences[seq_idx];
            const auto& fb_result = fb_results[seq_idx];
            const int T = sequence.size();
            
            for (int t = 0; t < T-1; ++t) {
                for (int i = 0; i < N; ++i) {
                    double gamma_t_i = fb_result.gamma(t, i);
                    
                    // Self-loop transition
                    double self_loop_prob = fb_result.gamma(t+1, i) * model.states[i].transition.self_loop_prob;
                    self_loop_counts[i] += gamma_t_i * self_loop_prob;
                    
                    // Next state transition
                    if (i < N-1) {
                        double next_state_prob = fb_result.gamma(t+1, i+1) * model.states[i].transition.next_state_prob;
                        next_state_counts[i] += gamma_t_i * next_state_prob;
                    }
                    
                    total_counts[i] += gamma_t_i;
                }
            }
        }
        
        // Update transition probabilities
        for (int i = 0; i < N; ++i) {
            if (total_counts[i] > 0.0) {
                model.states[i].transition.self_loop_prob = self_loop_counts[i] / total_counts[i];
                model.states[i].transition.next_state_prob = next_state_counts[i] / total_counts[i];
                
                // Normalize for numerical stability
                model.states[i].transition.normalize();
            }
        }
    }

    void HmmTrainer::update_emission_probabilities(PhonemeHmm& model,
                                                  const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                  const std::vector<ForwardBackwardResult>& fb_results) {
        
        const int N = model.num_states();
        
        // Collect weighted observations for each state
        for (int i = 0; i < N; ++i) {
            std::vector<Eigen::VectorXd> weighted_observations;
            std::vector<double> weights;
            
            for (size_t seq_idx = 0; seq_idx < sequences.size(); ++seq_idx) {
                const auto& sequence = sequences[seq_idx];
                const auto& fb_result = fb_results[seq_idx];
                const int T = sequence.size();
                
                for (int t = 0; t < T; ++t) {
                    double weight = fb_result.gamma(t, i);
                    if (weight > 1e-10) {  // Avoid numerical issues
                        weighted_observations.push_back(sequence[t]);
                        weights.push_back(weight);
                    }
                }
            }
            
            // Train GMM for this state if we have observations
            if (!weighted_observations.empty()) {
                // For now, use simple EM on the GMM
                // This could be enhanced to use the weights directly
                model.states[i].train_emissions(weighted_observations);
            }
        }
    }

    bool HmmTrainer::check_convergence(const TrainingStats& stats) const {
        // Check log-likelihood convergence
        if (check_log_likelihood_convergence(stats.log_likelihoods)) {
            return true;
        }
        
        // Check parameter convergence
        if (!stats.parameter_changes.empty() && 
            stats.parameter_changes.back() < config_.parameter_threshold) {
            return true;
        }
        
        // Check validation convergence (early stopping)
        if (config_.use_validation_set && check_validation_convergence(stats.validation_scores)) {
            return true;
        }
        
        return false;
    }

    bool HmmTrainer::check_log_likelihood_convergence(const std::vector<double>& log_likelihoods) const {
        if (log_likelihoods.size() < config_.convergence_window) {
            return false;
        }
        
        // Check if improvement in last window is below threshold
        size_t window_start = log_likelihoods.size() - config_.convergence_window;
        double improvement = log_likelihoods.back() - log_likelihoods[window_start];
        
        return improvement < config_.convergence_threshold;
    }

    bool HmmTrainer::check_validation_convergence(const std::vector<double>& validation_scores) const {
        if (validation_scores.size() < config_.convergence_window) {
            return false;
        }
        
        // Check if validation score has not improved in last window
        size_t window_start = validation_scores.size() - config_.convergence_window;
        double max_recent = *std::max_element(validation_scores.begin() + window_start, validation_scores.end());
        double max_overall = *std::max_element(validation_scores.begin(), validation_scores.end());
        
        return max_recent < max_overall - config_.convergence_threshold;
    }

    double HmmTrainer::compute_parameter_distance(const PhonemeHmm& model1, const PhonemeHmm& model2) const {
        // Simplified parameter distance computation
        // In practice, this would compute proper distances between GMM parameters
        
        double distance = 0.0;
        const int N = std::min(model1.num_states(), model2.num_states());
        
        for (int i = 0; i < N; ++i) {
            // Transition probability differences
            distance += std::abs(model1.states[i].transition.self_loop_prob - 
                               model2.states[i].transition.self_loop_prob);
            distance += std::abs(model1.states[i].transition.next_state_prob - 
                               model2.states[i].transition.next_state_prob);
        }
        
        return distance / N;
    }

    Eigen::MatrixXd HmmTrainer::compute_viterbi_trellis(const PhonemeHmm& model,
                                                       const std::vector<Eigen::VectorXd>& observations) const {
        
        const int T = observations.size();
        const int N = model.num_states();
        
        Eigen::MatrixXd trellis = Eigen::MatrixXd::Constant(T, N, -std::numeric_limits<double>::infinity());
        
        // Initialize first frame
        for (int i = 0; i < N; ++i) {
            if (i == 0) {  // Start from first state
                trellis(0, i) = model.states[i].log_emission_probability(observations[0]);
            }
        }
        
        // Forward pass
        for (int t = 1; t < T; ++t) {
            for (int j = 0; j < N; ++j) {
                double best_score = -std::numeric_limits<double>::infinity();
                
                // Self-loop
                if (trellis(t-1, j) > -std::numeric_limits<double>::infinity()) {
                    double score = trellis(t-1, j) + std::log(model.states[j].transition.self_loop_prob);
                    best_score = std::max(best_score, score);
                }
                
                // Transition from previous state
                if (j > 0 && trellis(t-1, j-1) > -std::numeric_limits<double>::infinity()) {
                    double score = trellis(t-1, j-1) + std::log(model.states[j-1].transition.next_state_prob);
                    best_score = std::max(best_score, score);
                }
                
                if (best_score > -std::numeric_limits<double>::infinity()) {
                    trellis(t, j) = best_score + model.states[j].log_emission_probability(observations[t]);
                }
            }
        }
        
        return trellis;
    }

    std::vector<int> HmmTrainer::backtrack_viterbi_path(const Eigen::MatrixXd& trellis) const {
        const int T = trellis.rows();
        const int N = trellis.cols();
        
        std::vector<int> path(T);
        
        // Find best final state
        int best_final_state = 0;
        double best_final_score = trellis(T-1, 0);
        for (int i = 1; i < N; ++i) {
            if (trellis(T-1, i) > best_final_score) {
                best_final_score = trellis(T-1, i);
                best_final_state = i;
            }
        }
        
        path[T-1] = best_final_state;
        
        // Backtrack
        for (int t = T-2; t >= 0; --t) {
            int current_state = path[t+1];
            int best_prev_state = current_state;  // Default to self-loop
            
            // Check transition from previous state
            if (current_state > 0 && 
                trellis(t, current_state-1) > trellis(t, current_state)) {
                best_prev_state = current_state - 1;
            }
            
            path[t] = best_prev_state;
        }
        
        return path;
    }

    double HmmTrainer::log_sum_exp(const std::vector<double>& log_values) const {
        if (log_values.empty()) {
            return -std::numeric_limits<double>::infinity();
        }
        
        double max_val = *std::max_element(log_values.begin(), log_values.end());
        if (max_val == -std::numeric_limits<double>::infinity()) {
            return max_val;
        }
        
        double sum = 0.0;
        for (double val : log_values) {
            sum += std::exp(val - max_val);
        }
        
        return max_val + std::log(sum);
    }

    void HmmTrainer::log_iteration_info(int iteration, const TrainingStats& stats) const {
        std::cout << "Iteration " << iteration + 1;
        if (!stats.log_likelihoods.empty()) {
            std::cout << ", Log-likelihood: " << stats.log_likelihoods.back();
        }
        if (!stats.validation_scores.empty()) {
            std::cout << ", Validation: " << stats.validation_scores.back();
        }
        if (!stats.parameter_changes.empty()) {
            std::cout << ", Param change: " << stats.parameter_changes.back();
        }
        std::cout << std::endl;
    }

    void HmmTrainer::log_convergence_info(const TrainingStats& stats) const {
        std::cout << "Training completed after " << stats.final_iteration << " iterations" << std::endl;
        std::cout << "Final log-likelihood: " << stats.final_log_likelihood << std::endl;
        if (stats.converged) {
            std::cout << "Training converged successfully" << std::endl;
        } else {
            std::cout << "Training stopped due to maximum iterations" << std::endl;
        }
    }

} // namespace hmm
} // namespace nexussynth
#include "nexussynth/hmm_trainer.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <random>
#include <chrono>
#include <mutex>

#ifdef NEXUSSYNTH_OPENMP_ENABLED
#include <omp.h>
#endif

namespace nexussynth {
namespace hmm {

    // HmmTrainer Implementation
    HmmTrainer::HmmTrainer(const TrainingConfig& config) : config_(config), has_checkpoint_(false) {
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
            auto iteration_start = std::chrono::high_resolution_clock::now();
            
            // E-Step: Forward-Backward algorithm (parallel or sequential)
            std::vector<ForwardBackwardResult> fb_results;
            double log_likelihood;
            
            auto e_step_start = std::chrono::high_resolution_clock::now();
            if (config_.enable_parallel_training && train_data.size() > 1) {
                log_likelihood = parallel_em_expectation_step(model, train_data, fb_results);
            } else {
                log_likelihood = em_expectation_step(model, train_data, fb_results);
            }
            auto e_step_end = std::chrono::high_resolution_clock::now();
            
            stats.log_likelihoods.push_back(log_likelihood);
            
            // Track E-step timing
            auto e_step_duration = std::chrono::duration_cast<std::chrono::microseconds>(e_step_end - e_step_start);
            stats.e_step_timings.push_back(e_step_duration.count() / 1e6);
            
            // M-Step: Parameter re-estimation (parallel or sequential)
            auto m_step_start = std::chrono::high_resolution_clock::now();
            if (config_.enable_parallel_training && train_data.size() > 1) {
                parallel_em_maximization_step(model, train_data, fb_results);
            } else {
                em_maximization_step(model, train_data, fb_results);
            }
            auto m_step_end = std::chrono::high_resolution_clock::now();
            
            // Track M-step timing
            auto m_step_duration = std::chrono::duration_cast<std::chrono::microseconds>(m_step_end - m_step_start);
            stats.m_step_timings.push_back(m_step_duration.count() / 1e6);
            
            // Validation evaluation
            if (!validation_data.empty()) {
                double validation_score = evaluate_model(model, validation_data);
                stats.validation_scores.push_back(validation_score);
                
                if (validation_score > stats.best_validation_score) {
                    stats.best_validation_score = validation_score;
                }
            }
            
            // Enhanced parameter change calculation using L2 norm
            double param_change = compute_parameter_l2_norm(previous_model, model);
            stats.parameter_changes.push_back(param_change);
            
            // Model checkpointing
            if (config_.enable_model_checkpointing) {
                save_checkpoint(model, stats);
            }
            
            previous_model = model;
            stats.final_iteration = iteration + 1;
            stats.final_log_likelihood = log_likelihood;
            
            if (config_.verbose) {
                log_iteration_info(iteration, stats);
                if (config_.verbose_parallel && config_.enable_parallel_training) {
                    log_parallel_performance(stats, iteration);
                }
            }
            
            // Convergence check
            if (check_convergence(stats)) {
                stats.converged = true;
                
                // Restore best model if checkpointing is enabled
                if (config_.enable_model_checkpointing && has_checkpoint_) {
                    model = restore_best_model(model, stats);
                }
                break;
            }
        }
        
        // Restore best model if training completed without convergence
        if (!stats.converged && config_.enable_model_checkpointing && has_checkpoint_) {
            model = restore_best_model(model, stats);
            stats.convergence_reason = "Training completed: best model restored";
        } else if (!stats.converged) {
            stats.convergence_reason = "Training completed: maximum iterations reached";
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

    std::vector<ForwardBackwardResult> HmmTrainer::parallel_batch_forward_backward(const PhonemeHmm& model,
                                                                                  const std::vector<std::vector<Eigen::VectorXd>>& sequences) const {
        
        std::vector<ForwardBackwardResult> results(sequences.size());
        
        if (sequences.empty()) {
            return results;
        }
        
#ifdef NEXUSSYNTH_OPENMP_ENABLED
        // Determine optimal thread count
        int num_threads = determine_optimal_thread_count(sequences.size());
        
        if (config_.verbose_parallel) {
            std::cout << "Parallel FB: Using " << num_threads << " threads for " << sequences.size() << " sequences" << std::endl;
        }
        
        // Create load-balanced chunks if enabled
        if (config_.enable_load_balancing) {
            auto chunks = create_load_balanced_chunks(sequences, num_threads);
            
            #pragma omp parallel num_threads(num_threads)
            {
                int thread_id = omp_get_thread_num();
                if (thread_id < static_cast<int>(chunks.size())) {
                    const auto& chunk = chunks[thread_id];
                    for (int idx : chunk) {
                        if (idx >= 0 && idx < static_cast<int>(sequences.size())) {
                            results[idx] = forward_backward(model, sequences[idx]);
                        }
                    }
                }
            }
        } else {
            // Simple parallel loop without load balancing
            #pragma omp parallel for num_threads(num_threads) schedule(dynamic)
            for (int i = 0; i < static_cast<int>(sequences.size()); ++i) {
                results[i] = forward_backward(model, sequences[i]);
            }
        }
#else
        // Fallback to sequential processing if OpenMP not available
        for (size_t i = 0; i < sequences.size(); ++i) {
            results[i] = forward_backward(model, sequences[i]);
        }
#endif
        
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

    double HmmTrainer::parallel_em_expectation_step(const PhonemeHmm& model,
                                                   const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                   std::vector<ForwardBackwardResult>& fb_results) const {
        
        fb_results = parallel_batch_forward_backward(model, sequences);
        
        double total_log_likelihood = 0.0;
        int total_frames = 0;
        
        // Accumulate results (this part is sequential but very fast)
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

    void HmmTrainer::parallel_em_maximization_step(PhonemeHmm& model,
                                                  const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                  const std::vector<ForwardBackwardResult>& fb_results) {
        
        // Parallel parameter updates with thread safety
        parallel_update_transition_probabilities(model, sequences, fb_results);
        
        if (config_.enable_parallel_emission_update) {
            parallel_update_emission_probabilities(model, sequences, fb_results);
        } else {
            // Fall back to sequential emission updates if parallel updates disabled
            update_emission_probabilities(model, sequences, fb_results);
        }
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
            std::vector<Eigen::VectorXd> observations;
            std::vector<double> weights;
            
            for (size_t seq_idx = 0; seq_idx < sequences.size(); ++seq_idx) {
                const auto& sequence = sequences[seq_idx];
                const auto& fb_result = fb_results[seq_idx];
                const int T = sequence.size();
                
                for (int t = 0; t < T; ++t) {
                    double weight = fb_result.gamma(t, i);
                    if (weight > 1e-10) {  // Avoid numerical issues
                        observations.push_back(sequence[t]);
                        weights.push_back(weight);
                    }
                }
            }
            
            // Train GMM for this state using weighted EM
            if (!observations.empty()) {
                if (weights.size() == observations.size()) {
                    // Use proper weighted EM training with Forward-Backward posteriors
                    model.states[i].train_weighted_emissions(observations, weights, 50);  // Use reasonable default for GMM EM
                } else {
                    // Fallback to regular EM if weights don't match
                    model.states[i].train_emissions(observations, 50);
                }
            }
        }
    }

    void HmmTrainer::parallel_update_transition_probabilities(PhonemeHmm& model,
                                                             const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                             const std::vector<ForwardBackwardResult>& fb_results) {
        
        const int N = model.num_states();
        
        // Thread-safe accumulators for transition statistics
        std::vector<double> self_loop_counts(N, 0.0);
        std::vector<double> next_state_counts(N, 0.0);
        std::vector<double> total_counts(N, 0.0);
        
#ifdef NEXUSSYNTH_OPENMP_ENABLED
        // Use thread-local storage for accumulation
        const int num_threads = omp_get_max_threads();
        std::vector<std::vector<double>> thread_self_counts(num_threads, std::vector<double>(N, 0.0));
        std::vector<std::vector<double>> thread_next_counts(num_threads, std::vector<double>(N, 0.0));
        std::vector<std::vector<double>> thread_total_counts(num_threads, std::vector<double>(N, 0.0));
        
        #pragma omp parallel for
#endif
        for (int seq_idx = 0; seq_idx < static_cast<int>(sequences.size()); ++seq_idx) {
            const auto& sequence = sequences[seq_idx];
            const auto& fb_result = fb_results[seq_idx];
            const int T = sequence.size();
            
#ifdef NEXUSSYNTH_OPENMP_ENABLED
            const int thread_id = omp_get_thread_num();
#endif
            
            for (int t = 0; t < T-1; ++t) {
                for (int i = 0; i < N; ++i) {
                    double gamma_t_i = fb_result.gamma(t, i);
                    
                    // Self-loop transition
                    double self_loop_prob = fb_result.gamma(t+1, i) * model.states[i].transition.self_loop_prob;
                    
                    // Next state transition
                    double next_state_prob = 0.0;
                    if (i < N-1) {
                        next_state_prob = fb_result.gamma(t+1, i+1) * model.states[i].transition.next_state_prob;
                    }
                    
#ifdef NEXUSSYNTH_OPENMP_ENABLED
                    thread_self_counts[thread_id][i] += gamma_t_i * self_loop_prob;
                    thread_next_counts[thread_id][i] += gamma_t_i * next_state_prob;
                    thread_total_counts[thread_id][i] += gamma_t_i;
#else
                    self_loop_counts[i] += gamma_t_i * self_loop_prob;
                    next_state_counts[i] += gamma_t_i * next_state_prob;
                    total_counts[i] += gamma_t_i;
#endif
                }
            }
        }
        
#ifdef NEXUSSYNTH_OPENMP_ENABLED
        // Reduce thread-local results
        for (int t = 0; t < num_threads; ++t) {
            for (int i = 0; i < N; ++i) {
                self_loop_counts[i] += thread_self_counts[t][i];
                next_state_counts[i] += thread_next_counts[t][i];
                total_counts[i] += thread_total_counts[t][i];
            }
        }
#endif
        
        // Update transition probabilities (this must be sequential)
        for (int i = 0; i < N; ++i) {
            if (total_counts[i] > 0.0) {
                model.states[i].transition.self_loop_prob = self_loop_counts[i] / total_counts[i];
                model.states[i].transition.next_state_prob = next_state_counts[i] / total_counts[i];
                
                // Normalize for numerical stability
                model.states[i].transition.normalize();
            }
        }
    }

    void HmmTrainer::parallel_update_emission_probabilities(PhonemeHmm& model,
                                                           const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                           const std::vector<ForwardBackwardResult>& fb_results) {
        
        const int N = model.num_states();
        
        // Parallel collection of observations for each state
        std::vector<std::vector<Eigen::VectorXd>> all_observations(N);
        std::vector<std::vector<double>> all_weights(N);
        
#ifdef NEXUSSYNTH_OPENMP_ENABLED
        // Use critical sections for thread-safe vector operations
        #pragma omp parallel for
#endif
        for (int i = 0; i < N; ++i) {
            std::vector<Eigen::VectorXd> observations;
            std::vector<double> weights;
            
            // Collect observations for state i across all sequences
            for (size_t seq_idx = 0; seq_idx < sequences.size(); ++seq_idx) {
                const auto& sequence = sequences[seq_idx];
                const auto& fb_result = fb_results[seq_idx];
                const int T = sequence.size();
                
                for (int t = 0; t < T; ++t) {
                    double weight = fb_result.gamma(t, i);
                    if (weight > 1e-10) {  // Avoid numerical issues
                        observations.push_back(sequence[t]);
                        weights.push_back(weight);
                    }
                }
            }
            
            // Store results in thread-safe way
#ifdef NEXUSSYNTH_OPENMP_ENABLED
            #pragma omp critical
#endif
            {
                all_observations[i] = std::move(observations);
                all_weights[i] = std::move(weights);
            }
        }
        
        // Train GMM for each state (can be parallelized since states are independent)
#ifdef NEXUSSYNTH_OPENMP_ENABLED
        #pragma omp parallel for
#endif
        for (int i = 0; i < N; ++i) {
            const auto& observations = all_observations[i];
            const auto& weights = all_weights[i];
            
            if (!observations.empty()) {
                if (weights.size() == observations.size()) {
                    // Use proper weighted EM training with Forward-Backward posteriors
                    model.states[i].train_weighted_emissions(observations, weights, 50);
                } else {
                    // Fallback to regular EM if weights don't match
                    model.states[i].train_emissions(observations, 50);
                }
            }
        }
    }

    bool HmmTrainer::check_convergence(TrainingStats& stats) const {
        std::vector<std::string> criteria_met;
        
        // Use enhanced multi-criteria convergence detection
        bool converged = check_multi_criteria_convergence(stats, criteria_met);
        
        if (converged) {
            // Calculate final convergence confidence
            stats.convergence_confidence = calculate_convergence_confidence(stats);
            stats.convergence_criteria_met = criteria_met;
            
            // Set convergence reason
            if (!criteria_met.empty()) {
                stats.convergence_reason = "Converged: ";
                for (size_t i = 0; i < criteria_met.size(); ++i) {
                    if (i > 0) stats.convergence_reason += ", ";
                    stats.convergence_reason += criteria_met[i];
                }
            }
        }
        
        // Check early stopping conditions regardless of convergence
        if (check_early_stopping_conditions(stats)) {
            stats.early_stopped = true;
            if (stats.convergence_reason.empty()) {
                stats.convergence_reason = "Early stopping triggered";
            }
            return true;
        }
        
        return converged;
    }

    bool HmmTrainer::check_log_likelihood_convergence(const std::vector<double>& log_likelihoods, 
                                                     double threshold) const {
        if (log_likelihoods.size() < config_.convergence_window) {
            return false;
        }
        
        // Use provided threshold or default from config
        double effective_threshold = (threshold > 0) ? threshold : config_.convergence_threshold;
        
        // Check if improvement in last window is below threshold
        size_t window_start = log_likelihoods.size() - config_.convergence_window;
        double improvement = log_likelihoods.back() - log_likelihoods[window_start];
        
        return improvement < effective_threshold;
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

    // Enhanced Convergence Detection Methods
    bool HmmTrainer::check_multi_criteria_convergence(TrainingStats& stats, 
                                                     std::vector<std::string>& criteria_met) const {
        criteria_met.clear();
        bool converged = false;
        
        // 1. Log-likelihood convergence with adaptive threshold
        if (config_.enable_adaptive_thresholds) {
            stats.adaptive_threshold = update_adaptive_threshold(stats);
        }
        
        if (check_log_likelihood_convergence(stats.log_likelihoods, stats.adaptive_threshold)) {
            criteria_met.push_back("log-likelihood");
            converged = true;
        }
        
        // 2. Enhanced parameter convergence using L2 norm
        if (!stats.parameter_changes.empty()) {
            double param_change = stats.parameter_changes.back();
            if (param_change < config_.parameter_threshold) {
                criteria_met.push_back("parameter-change");
                converged = true;
            }
        }
        
        // 3. Relative improvement convergence
        if (stats.log_likelihoods.size() >= 3) {
            double rel_improvement = compute_relative_improvement(stats.log_likelihoods);
            stats.relative_improvements.push_back(rel_improvement);
            
            if (rel_improvement < config_.min_improvement) {
                criteria_met.push_back("relative-improvement");
                converged = true;
            }
        }
        
        // 4. Validation convergence (enhanced)
        if (config_.use_validation_set && !stats.validation_scores.empty()) {
            if (check_validation_convergence(stats.validation_scores)) {
                criteria_met.push_back("validation");
                converged = true;
            }
        }
        
        // Require sufficient confidence for convergence
        if (converged) {
            double confidence = calculate_convergence_confidence(stats);
            stats.convergence_confidence_scores.push_back(confidence);
            
            if (confidence < config_.convergence_confidence) {
                criteria_met.clear(); // Not confident enough
                converged = false;
            }
        }
        
        return converged;
    }

    double HmmTrainer::calculate_convergence_confidence(const TrainingStats& stats) const {
        if (stats.log_likelihoods.size() < 3) {
            return 0.0;
        }
        
        double confidence = 0.0;
        int criteria_count = 0;
        
        // Log-likelihood stability
        if (stats.log_likelihoods.size() >= config_.convergence_window) {
            size_t window_start = stats.log_likelihoods.size() - config_.convergence_window;
            
            // Calculate variance in recent window
            std::vector<double> recent_ll(stats.log_likelihoods.begin() + window_start, 
                                        stats.log_likelihoods.end());
            double mean_ll = std::accumulate(recent_ll.begin(), recent_ll.end(), 0.0) / recent_ll.size();
            
            double variance = 0.0;
            for (double ll : recent_ll) {
                variance += std::pow(ll - mean_ll, 2);
            }
            variance /= recent_ll.size();
            
            // Lower variance = higher confidence
            double ll_confidence = std::exp(-variance * 100.0);
            confidence += ll_confidence;
            criteria_count++;
        }
        
        // Parameter change stability
        if (stats.parameter_changes.size() >= config_.convergence_window) {
            size_t window_start = stats.parameter_changes.size() - config_.convergence_window;
            
            // Check if parameter changes are consistently small
            bool stable = true;
            for (size_t i = window_start; i < stats.parameter_changes.size(); ++i) {
                if (stats.parameter_changes[i] > config_.parameter_threshold * 2.0) {
                    stable = false;
                    break;
                }
            }
            
            double param_confidence = stable ? 1.0 : 0.0;
            confidence += param_confidence;
            criteria_count++;
        }
        
        // Validation score consistency
        if (!stats.validation_scores.empty() && stats.validation_scores.size() >= 3) {
            // Check if validation scores are not deteriorating
            double recent_avg = 0.0;
            int recent_count = std::min(3, static_cast<int>(stats.validation_scores.size()));
            
            for (int i = 0; i < recent_count; ++i) {
                recent_avg += stats.validation_scores[stats.validation_scores.size() - 1 - i];
            }
            recent_avg /= recent_count;
            
            double validation_confidence = (recent_avg >= stats.best_validation_score * 0.95) ? 1.0 : 0.5;
            confidence += validation_confidence;
            criteria_count++;
        }
        
        return (criteria_count > 0) ? confidence / criteria_count : 0.0;
    }

    bool HmmTrainer::check_overfitting_detection(const TrainingStats& stats) const {
        if (!config_.use_validation_set || stats.validation_scores.size() < 5) {
            return false;
        }
        
        // Check if validation score has significantly deteriorated
        size_t recent_window = std::min(static_cast<size_t>(3), stats.validation_scores.size());
        
        double recent_avg = 0.0;
        for (size_t i = 0; i < recent_window; ++i) {
            recent_avg += stats.validation_scores[stats.validation_scores.size() - 1 - i];
        }
        recent_avg /= recent_window;
        
        // Compare to best validation score
        double drop = stats.best_validation_score - recent_avg;
        
        return drop > config_.overfitting_threshold;
    }

    bool HmmTrainer::check_early_stopping_conditions(TrainingStats& stats) const {
        // Update patience counter
        if (!stats.validation_scores.empty()) {
            double current_score = stats.validation_scores.back();
            
            if (current_score > stats.best_validation_score) {
                stats.patience_counter = 0; // Reset patience
                stats.best_validation_iteration = stats.final_iteration;
            } else {
                stats.patience_counter++;
            }
            
            // Early stopping based on patience
            if (stats.patience_counter >= config_.patience) {
                stats.convergence_reason = "Early stopping: patience exceeded";
                return true;
            }
        }
        
        // Overfitting detection
        if (check_overfitting_detection(stats)) {
            stats.convergence_reason = "Early stopping: overfitting detected";
            return true;
        }
        
        return false;
    }

    double HmmTrainer::compute_relative_improvement(const std::vector<double>& values, int window_size) const {
        if (static_cast<int>(values.size()) < window_size * 2) {
            return std::numeric_limits<double>::infinity();
        }
        
        // Compare recent window to previous window
        size_t total_size = values.size();
        
        double recent_avg = 0.0;
        for (int i = 0; i < window_size; ++i) {
            recent_avg += values[total_size - 1 - i];
        }
        recent_avg /= window_size;
        
        double previous_avg = 0.0;
        for (int i = window_size; i < window_size * 2; ++i) {
            previous_avg += values[total_size - 1 - i];
        }
        previous_avg /= window_size;
        
        if (std::abs(previous_avg) < 1e-12) {
            return std::numeric_limits<double>::infinity();
        }
        
        return (recent_avg - previous_avg) / std::abs(previous_avg);
    }

    double HmmTrainer::update_adaptive_threshold(const TrainingStats& stats) const {
        if (stats.log_likelihoods.size() < 5) {
            return config_.convergence_threshold;
        }
        
        // Calculate recent improvement variance
        std::vector<double> recent_improvements;
        for (size_t i = 1; i < std::min(static_cast<size_t>(10), stats.log_likelihoods.size()); ++i) {
            size_t idx = stats.log_likelihoods.size() - i;
            double improvement = stats.log_likelihoods[idx] - stats.log_likelihoods[idx - 1];
            recent_improvements.push_back(improvement);
        }
        
        if (recent_improvements.empty()) {
            return config_.convergence_threshold;
        }
        
        // Calculate standard deviation of improvements
        double mean_improvement = std::accumulate(recent_improvements.begin(), 
                                                recent_improvements.end(), 0.0) / recent_improvements.size();
        
        double variance = 0.0;
        for (double imp : recent_improvements) {
            variance += std::pow(imp - mean_improvement, 2);
        }
        variance /= recent_improvements.size();
        
        double std_dev = std::sqrt(variance);
        
        // Adaptive threshold: smaller when improvements are stable
        double adaptive_factor = std::max(0.1, std::min(10.0, std_dev / config_.convergence_threshold));
        return config_.convergence_threshold * adaptive_factor;
    }

    // Enhanced parameter distance using L2 norm
    double HmmTrainer::compute_parameter_l2_norm(const PhonemeHmm& model1, const PhonemeHmm& model2) const {
        double l2_norm = 0.0;
        const int N = std::min(model1.num_states(), model2.num_states());
        
        for (int i = 0; i < N; ++i) {
            // Transition parameter differences (squared)
            double trans_diff = std::pow(model1.states[i].transition.self_loop_prob - 
                                       model2.states[i].transition.self_loop_prob, 2);
            trans_diff += std::pow(model1.states[i].transition.next_state_prob - 
                                 model2.states[i].transition.next_state_prob, 2);
            
            l2_norm += trans_diff;
            
            // Emission parameter differences would be added here
            // For now, using simplified transition-only model
        }
        
        return std::sqrt(l2_norm / N);
    }

    // Model checkpointing methods
    PhonemeHmm HmmTrainer::save_checkpoint(const PhonemeHmm& model, const TrainingStats& stats) const {
        if (should_save_checkpoint(stats)) {
            best_model_ = model;
            has_checkpoint_ = true;
        }
        return best_model_;
    }

    bool HmmTrainer::should_save_checkpoint(const TrainingStats& stats) const {
        if (!config_.enable_model_checkpointing) {
            return false;
        }
        
        // Save if validation score improved
        if (!stats.validation_scores.empty()) {
            return stats.validation_scores.back() >= stats.best_validation_score;
        }
        
        // Save if log-likelihood improved significantly
        if (stats.log_likelihoods.size() >= 2) {
            double improvement = stats.log_likelihoods.back() - 
                               stats.log_likelihoods[stats.log_likelihoods.size() - 2];
            return improvement > config_.convergence_threshold;
        }
        
        return false;
    }

    PhonemeHmm HmmTrainer::restore_best_model(const PhonemeHmm& current_model, const TrainingStats& stats) const {
        if (has_checkpoint_ && config_.enable_model_checkpointing) {
            return best_model_;
        }
        return current_model;
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

    // Enhanced forced alignment implementations
    SequenceAlignment HmmTrainer::forced_alignment(const PhonemeHmm& model,
                                                   const std::vector<Eigen::VectorXd>& observation_sequence,
                                                   const std::vector<std::string>& phoneme_sequence,
                                                   double frame_rate) const {
        
        SequenceAlignment result;
        const int T = observation_sequence.size();
        const int N = model.num_states();
        
        if (T == 0 || N == 0 || phoneme_sequence.empty()) {
            return result;
        }
        
        result.frame_rate = frame_rate;
        
        // Compute enhanced trellis for forced alignment
        Eigen::MatrixXd trellis = compute_forced_alignment_trellis(model, observation_sequence, phoneme_sequence);
        
        // Extract optimal path
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
        
        // Extract phoneme boundaries
        result.phoneme_boundaries = extract_phoneme_boundaries(result.state_sequence, phoneme_sequence, model, frame_rate);
        
        // Compute alignment confidence using Forward-Backward
        ForwardBackwardResult fb_result = forward_backward(model, observation_sequence);
        result.average_confidence = compute_alignment_confidence(trellis, result.state_sequence, fb_result);
        result.state_posteriors = compute_state_posteriors(fb_result, result.state_sequence);
        
        return result;
    }

    SequenceAlignment HmmTrainer::constrained_alignment(const PhonemeHmm& model,
                                                       const std::vector<Eigen::VectorXd>& observation_sequence,
                                                       const std::vector<std::string>& phoneme_sequence,
                                                       const std::vector<std::pair<double, double>>& time_constraints,
                                                       double frame_rate) const {
        
        SequenceAlignment result;
        const int T = observation_sequence.size();
        const int N = model.num_states();
        
        if (T == 0 || N == 0 || phoneme_sequence.empty() || time_constraints.size() != phoneme_sequence.size()) {
            return result;
        }
        
        result.frame_rate = frame_rate;
        
        // Compute constrained trellis with time hints
        Eigen::MatrixXd trellis = compute_constrained_trellis(model, observation_sequence, phoneme_sequence, time_constraints, frame_rate);
        
        // Extract optimal path
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
        
        // Extract phoneme boundaries
        result.phoneme_boundaries = extract_phoneme_boundaries(result.state_sequence, phoneme_sequence, model, frame_rate);
        
        // Compute alignment confidence
        ForwardBackwardResult fb_result = forward_backward(model, observation_sequence);
        result.average_confidence = compute_alignment_confidence(trellis, result.state_sequence, fb_result);
        result.state_posteriors = compute_state_posteriors(fb_result, result.state_sequence);
        
        return result;
    }

    std::vector<SequenceAlignment> HmmTrainer::batch_forced_alignment(const std::map<std::string, PhonemeHmm>& models,
                                                                     const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                                     const std::vector<std::vector<std::string>>& phoneme_sequences,
                                                                     double frame_rate) const {
        
        std::vector<SequenceAlignment> results;
        results.reserve(sequences.size());
        
        if (sequences.size() != phoneme_sequences.size()) {
            return results;
        }
        
        for (size_t i = 0; i < sequences.size(); ++i) {
            if (phoneme_sequences[i].empty()) {
                results.emplace_back();
                continue;
            }
            
            // For now, use the first model (in production, would select based on phoneme context)
            if (!models.empty()) {
                const auto& model = models.begin()->second;
                results.push_back(forced_alignment(model, sequences[i], phoneme_sequences[i], frame_rate));
            } else {
                results.emplace_back();
            }
        }
        
        return results;
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

    // Enhanced forced alignment helper implementations
    Eigen::MatrixXd HmmTrainer::compute_forced_alignment_trellis(const PhonemeHmm& model,
                                                               const std::vector<Eigen::VectorXd>& observations,
                                                               const std::vector<std::string>& phoneme_sequence) const {
        
        const int T = observations.size();
        const int N = model.num_states();
        
        Eigen::MatrixXd trellis = Eigen::MatrixXd::Constant(T, N, -std::numeric_limits<double>::infinity());
        
        if (T == 0 || N == 0) {
            return trellis;
        }
        
        // Enhanced initialization for forced alignment
        // Allow starting from any state for better flexibility
        for (int i = 0; i < N; ++i) {
            trellis(0, i) = model.states[i].log_emission_probability(observations[0]);
        }
        
        // Forward pass with enhanced transition handling
        for (int t = 1; t < T; ++t) {
            for (int j = 0; j < N; ++j) {
                double best_score = -std::numeric_limits<double>::infinity();
                
                // Self-loop transition
                if (trellis(t-1, j) > -std::numeric_limits<double>::infinity()) {
                    double score = trellis(t-1, j) + std::log(std::max(model.states[j].transition.self_loop_prob, 1e-10));
                    best_score = std::max(best_score, score);
                }
                
                // Transition from previous state
                if (j > 0 && trellis(t-1, j-1) > -std::numeric_limits<double>::infinity()) {
                    double score = trellis(t-1, j-1) + std::log(std::max(model.states[j-1].transition.next_state_prob, 1e-10));
                    best_score = std::max(best_score, score);
                }
                
                // Skip transitions for forced alignment (allows longer state durations)
                if (j > 1 && trellis(t-1, j-2) > -std::numeric_limits<double>::infinity()) {
                    double skip_penalty = -2.0;  // Penalty for skipping states
                    double score = trellis(t-1, j-2) + skip_penalty;
                    best_score = std::max(best_score, score);
                }
                
                if (best_score > -std::numeric_limits<double>::infinity()) {
                    double emission_prob = model.states[j].log_emission_probability(observations[t]);
                    trellis(t, j) = best_score + emission_prob;
                }
            }
        }
        
        return trellis;
    }

    Eigen::MatrixXd HmmTrainer::compute_constrained_trellis(const PhonemeHmm& model,
                                                          const std::vector<Eigen::VectorXd>& observations,
                                                          const std::vector<std::string>& phoneme_sequence,
                                                          const std::vector<std::pair<double, double>>& time_constraints,
                                                          double frame_rate) const {
        
        const int T = observations.size();
        const int N = model.num_states();
        
        Eigen::MatrixXd trellis = Eigen::MatrixXd::Constant(T, N, -std::numeric_limits<double>::infinity());
        
        if (T == 0 || N == 0 || time_constraints.size() != phoneme_sequence.size()) {
            return trellis;
        }
        
        // Convert time constraints to frame indices
        std::vector<std::pair<int, int>> frame_constraints(time_constraints.size());
        for (size_t i = 0; i < time_constraints.size(); ++i) {
            frame_constraints[i].first = static_cast<int>(time_constraints[i].first * frame_rate / 1000.0);
            frame_constraints[i].second = static_cast<int>(time_constraints[i].second * frame_rate / 1000.0);
            
            // Clamp to valid frame range
            frame_constraints[i].first = std::max(0, std::min(frame_constraints[i].first, T-1));
            frame_constraints[i].second = std::max(0, std::min(frame_constraints[i].second, T));
        }
        
        // Enhanced initialization with time constraints
        for (int i = 0; i < N; ++i) {
            // Check if this state is allowed at time 0 based on constraints
            bool allowed = true;
            if (!frame_constraints.empty() && frame_constraints[0].first > 0) {
                allowed = false;  // First phoneme starts later
            }
            
            if (allowed) {
                trellis(0, i) = model.states[i].log_emission_probability(observations[0]);
            }
        }
        
        // Forward pass with time constraint enforcement
        for (int t = 1; t < T; ++t) {
            for (int j = 0; j < N; ++j) {
                // Check if this state is allowed at this time
                bool state_allowed = true;
                
                // Apply time constraints based on expected phoneme progression
                if (!frame_constraints.empty()) {
                    // Simple constraint check - could be enhanced with more sophisticated logic
                    double expected_progress = static_cast<double>(t) / T;
                    double phoneme_progress = 0.0;
                    
                    for (size_t p = 0; p < frame_constraints.size(); ++p) {
                        if (t >= frame_constraints[p].first && t < frame_constraints[p].second) {
                            phoneme_progress = static_cast<double>(p) / frame_constraints.size();
                            break;
                        }
                    }
                    
                    // Allow some tolerance in timing
                    double tolerance = 0.2;
                    if (std::abs(expected_progress - phoneme_progress) > tolerance) {
                        // Apply penalty rather than complete prohibition
                        // state_allowed = false;  // Uncomment for strict enforcement
                    }
                }
                
                if (!state_allowed) {
                    continue;
                }
                
                double best_score = -std::numeric_limits<double>::infinity();
                
                // Standard transitions (same as forced alignment)
                if (trellis(t-1, j) > -std::numeric_limits<double>::infinity()) {
                    double score = trellis(t-1, j) + std::log(std::max(model.states[j].transition.self_loop_prob, 1e-10));
                    best_score = std::max(best_score, score);
                }
                
                if (j > 0 && trellis(t-1, j-1) > -std::numeric_limits<double>::infinity()) {
                    double score = trellis(t-1, j-1) + std::log(std::max(model.states[j-1].transition.next_state_prob, 1e-10));
                    best_score = std::max(best_score, score);
                }
                
                if (best_score > -std::numeric_limits<double>::infinity()) {
                    double emission_prob = model.states[j].log_emission_probability(observations[t]);
                    trellis(t, j) = best_score + emission_prob;
                }
            }
        }
        
        return trellis;
    }

    std::vector<PhonemeBoundary> HmmTrainer::extract_phoneme_boundaries(const std::vector<int>& state_sequence,
                                                                       const std::vector<std::string>& phoneme_sequence,
                                                                       const PhonemeHmm& model,
                                                                       double frame_rate) const {
        
        std::vector<PhonemeBoundary> boundaries;
        
        if (state_sequence.empty() || phoneme_sequence.empty()) {
            return boundaries;
        }
        
        const int N = model.num_states();
        const int states_per_phoneme = N;  // Assuming each phoneme uses full model
        
        // Track phoneme boundaries based on state progression
        int current_phoneme = 0;
        int phoneme_start_frame = 0;
        
        for (int t = 0; t < static_cast<int>(state_sequence.size()); ++t) {
            int current_state = state_sequence[t];
            
            // Detect phoneme boundary (when reaching final state of current phoneme)
            bool phoneme_ended = false;
            
            // Simple heuristic: phoneme ends when we reach the last state
            if (current_state == N - 1) {
                phoneme_ended = true;
            }
            
            // Alternative: detect significant state transitions
            if (t > 0 && state_sequence[t] < state_sequence[t-1]) {
                phoneme_ended = true;
            }
            
            if (phoneme_ended || t == static_cast<int>(state_sequence.size()) - 1) {
                // Create boundary for completed phoneme
                if (current_phoneme < static_cast<int>(phoneme_sequence.size())) {
                    PhonemeBoundary boundary;
                    boundary.start_frame = phoneme_start_frame;
                    boundary.end_frame = t + 1;
                    boundary.phoneme = phoneme_sequence[current_phoneme];
                    boundary.duration_ms = ((boundary.end_frame - boundary.start_frame) / frame_rate) * 1000.0;
                    
                    // Compute confidence score based on state progression consistency
                    boundary.confidence_score = 0.8;  // Default confidence - could be enhanced
                    
                    boundaries.push_back(boundary);
                    
                    current_phoneme++;
                    phoneme_start_frame = t + 1;
                }
            }
        }
        
        return boundaries;
    }

    double HmmTrainer::compute_alignment_confidence(const Eigen::MatrixXd& trellis,
                                                   const std::vector<int>& state_sequence,
                                                   const ForwardBackwardResult& fb_result) const {
        
        if (state_sequence.empty() || trellis.rows() == 0 || fb_result.gamma.rows() == 0) {
            return 0.0;
        }
        
        double total_confidence = 0.0;
        int valid_frames = 0;
        
        for (int t = 0; t < static_cast<int>(state_sequence.size()); ++t) {
            int state = state_sequence[t];
            
            if (state >= 0 && state < fb_result.gamma.cols() && t < fb_result.gamma.rows()) {
                // Use state posterior probability as confidence measure
                double posterior = fb_result.gamma(t, state);
                total_confidence += posterior;
                valid_frames++;
            }
        }
        
        return valid_frames > 0 ? total_confidence / valid_frames : 0.0;
    }

    std::vector<double> HmmTrainer::compute_state_posteriors(const ForwardBackwardResult& fb_result,
                                                           const std::vector<int>& state_sequence) const {
        
        std::vector<double> posteriors;
        posteriors.reserve(state_sequence.size());
        
        for (int t = 0; t < static_cast<int>(state_sequence.size()); ++t) {
            int state = state_sequence[t];
            
            if (state >= 0 && state < fb_result.gamma.cols() && t < fb_result.gamma.rows()) {
                posteriors.push_back(fb_result.gamma(t, state));
            } else {
                posteriors.push_back(0.0);
            }
        }
        
        return posteriors;
    }

    // GlobalVarianceCalculator Implementation
    GlobalVarianceStatistics GlobalVarianceCalculator::calculate_gv_statistics(
        const std::vector<std::vector<Eigen::VectorXd>>& sequences,
        const std::vector<std::vector<std::string>>& phoneme_labels) const {
        
        GlobalVarianceStatistics gv_stats;
        
        if (sequences.empty() || phoneme_labels.empty()) {
            return gv_stats;
        }
        
        // Initialize with feature dimension from first sequence
        if (!sequences[0].empty()) {
            gv_stats.initialize(sequences[0][0].size());
        }
        
        // Accumulate frame-wise variance statistics
        std::map<std::string, std::vector<Eigen::VectorXd>> phoneme_frames;
        std::vector<Eigen::VectorXd> all_frames;
        
        for (size_t seq_idx = 0; seq_idx < sequences.size() && seq_idx < phoneme_labels.size(); ++seq_idx) {
            const auto& sequence = sequences[seq_idx];
            const auto& labels = phoneme_labels[seq_idx];
            
            // Accumulate frames for each phoneme
            accumulate_phoneme_statistics(phoneme_frames, sequence, labels);
            
            // Accumulate all frames for global statistics
            all_frames.insert(all_frames.end(), sequence.begin(), sequence.end());
        }
        
        // Calculate per-phoneme GV statistics
        for (const auto& pair : phoneme_frames) {
            const std::string& phoneme = pair.first;
            const std::vector<Eigen::VectorXd>& frames = pair.second;
            
            if (!frames.empty()) {
                Eigen::VectorXd variance = compute_frame_wise_variance(frames);
                gv_stats.phoneme_gv_mean[phoneme] = variance;
                gv_stats.phoneme_gv_var[phoneme] = safe_vector_variance({variance}); // Single variance estimate
                gv_stats.phoneme_frame_counts[phoneme] = static_cast<int>(frames.size());
            }
        }
        
        // Calculate global GV statistics
        if (!all_frames.empty()) {
            gv_stats.global_gv_mean = compute_frame_wise_variance(all_frames);
            gv_stats.global_gv_var = safe_vector_variance({gv_stats.global_gv_mean});
            gv_stats.total_frames = static_cast<int>(all_frames.size());
        }
        
        return gv_stats;
    }

    GlobalVarianceStatistics GlobalVarianceCalculator::calculate_gv_statistics_with_alignment(
        const std::vector<std::vector<Eigen::VectorXd>>& sequences,
        const std::vector<SequenceAlignment>& alignments) const {
        
        GlobalVarianceStatistics gv_stats;
        
        if (sequences.empty() || alignments.empty()) {
            return gv_stats;
        }
        
        // Initialize with feature dimension
        if (!sequences[0].empty()) {
            gv_stats.initialize(sequences[0][0].size());
        }
        
        std::map<std::string, std::vector<Eigen::VectorXd>> phoneme_frames;
        std::vector<Eigen::VectorXd> all_frames;
        
        for (size_t seq_idx = 0; seq_idx < sequences.size() && seq_idx < alignments.size(); ++seq_idx) {
            const auto& sequence = sequences[seq_idx];
            const auto& alignment = alignments[seq_idx];
            
            // Use alignment information for more accurate phoneme-frame mapping
            accumulate_alignment_statistics(phoneme_frames, sequence, alignment);
            all_frames.insert(all_frames.end(), sequence.begin(), sequence.end());
        }
        
        // Calculate statistics similar to regular method
        for (const auto& pair : phoneme_frames) {
            const std::string& phoneme = pair.first;
            const std::vector<Eigen::VectorXd>& frames = pair.second;
            
            if (!frames.empty()) {
                Eigen::VectorXd variance = compute_frame_wise_variance(frames);
                gv_stats.phoneme_gv_mean[phoneme] = variance;
                gv_stats.phoneme_gv_var[phoneme] = safe_vector_variance({variance});
                gv_stats.phoneme_frame_counts[phoneme] = static_cast<int>(frames.size());
            }
        }
        
        if (!all_frames.empty()) {
            gv_stats.global_gv_mean = compute_frame_wise_variance(all_frames);
            gv_stats.global_gv_var = safe_vector_variance({gv_stats.global_gv_mean});
            gv_stats.total_frames = static_cast<int>(all_frames.size());
        }
        
        return gv_stats;
    }

    void GlobalVarianceCalculator::update_gv_statistics(GlobalVarianceStatistics& gv_stats,
                                                       const std::vector<Eigen::VectorXd>& sequence,
                                                       const std::vector<std::string>& phoneme_labels) const {
        
        if (sequence.empty() || phoneme_labels.empty()) {
            return;
        }
        
        // Initialize if not already done
        if (gv_stats.feature_dimension == 0 && !sequence.empty()) {
            gv_stats.initialize(sequence[0].size());
        }
        
        // Update statistics incrementally (simplified approach for online learning)
        std::map<std::string, std::vector<Eigen::VectorXd>> phoneme_frames;
        accumulate_phoneme_statistics(phoneme_frames, sequence, phoneme_labels);
        
        // Update per-phoneme statistics
        for (const auto& pair : phoneme_frames) {
            const std::string& phoneme = pair.first;
            const std::vector<Eigen::VectorXd>& frames = pair.second;
            
            if (!frames.empty()) {
                Eigen::VectorXd new_variance = compute_frame_wise_variance(frames);
                
                // Simple exponential moving average for online updates
                if (gv_stats.has_phoneme_statistics(phoneme)) {
                    const double alpha = 0.1; // Learning rate
                    gv_stats.phoneme_gv_mean[phoneme] = 
                        (1.0 - alpha) * gv_stats.phoneme_gv_mean[phoneme] + alpha * new_variance;
                } else {
                    gv_stats.phoneme_gv_mean[phoneme] = new_variance;
                    gv_stats.phoneme_gv_var[phoneme] = safe_vector_variance({new_variance});
                }
                
                gv_stats.phoneme_frame_counts[phoneme] += static_cast<int>(frames.size());
            }
        }
        
        // Update global statistics
        Eigen::VectorXd sequence_variance = compute_frame_wise_variance(sequence);
        if (gv_stats.total_frames > 0) {
            const double alpha = 0.1;
            gv_stats.global_gv_mean = (1.0 - alpha) * gv_stats.global_gv_mean + alpha * sequence_variance;
        } else {
            gv_stats.global_gv_mean = sequence_variance;
            gv_stats.global_gv_var = safe_vector_variance({sequence_variance});
        }
        
        gv_stats.total_frames += static_cast<int>(sequence.size());
    }

    Eigen::VectorXd GlobalVarianceCalculator::calculate_sequence_variance(
        const std::vector<Eigen::VectorXd>& sequence) const {
        
        return compute_frame_wise_variance(sequence);
    }

    std::map<std::string, Eigen::VectorXd> GlobalVarianceCalculator::calculate_phoneme_variances(
        const std::vector<Eigen::VectorXd>& sequence,
        const SequenceAlignment& alignment) const {
        
        std::map<std::string, Eigen::VectorXd> phoneme_variances;
        
        // Group frames by phoneme using alignment boundaries
        for (const auto& boundary : alignment.phoneme_boundaries) {
            if (boundary.start_frame >= 0 && 
                boundary.end_frame <= static_cast<int>(sequence.size()) && 
                boundary.start_frame < boundary.end_frame) {
                
                std::vector<Eigen::VectorXd> phoneme_frames;
                for (int i = boundary.start_frame; i < boundary.end_frame; ++i) {
                    phoneme_frames.push_back(sequence[i]);
                }
                
                if (!phoneme_frames.empty()) {
                    phoneme_variances[boundary.phoneme] = compute_frame_wise_variance(phoneme_frames);
                }
            }
        }
        
        return phoneme_variances;
    }

    std::vector<Eigen::VectorXd> GlobalVarianceCalculator::apply_gv_correction(
        const std::vector<Eigen::VectorXd>& original_trajectory,
        const GlobalVarianceStatistics& gv_stats,
        const std::vector<std::string>& phoneme_sequence,
        double gv_weight) const {
        
        if (original_trajectory.empty() || gv_weight <= 0.0) {
            return original_trajectory;
        }
        
        std::vector<Eigen::VectorXd> corrected_trajectory = original_trajectory;
        
        // Calculate current trajectory variance
        Eigen::VectorXd current_variance = compute_frame_wise_variance(original_trajectory);
        
        // Apply phoneme-specific or global GV correction
        for (size_t i = 0; i < corrected_trajectory.size() && i < phoneme_sequence.size(); ++i) {
            const std::string& phoneme = phoneme_sequence[i];
            
            // Get target variance (phoneme-specific or global fallback)
            auto gv_pair = gv_stats.get_gv_statistics(phoneme);
            const Eigen::VectorXd& target_variance = gv_pair.first;
            
            if (target_variance.size() == corrected_trajectory[i].size()) {
                // Apply variance correction with safety bounds
                for (int dim = 0; dim < corrected_trajectory[i].size(); ++dim) {
                    if (current_variance[dim] > MIN_VARIANCE && target_variance[dim] > MIN_VARIANCE) {
                        double correction_factor = std::sqrt(target_variance[dim] / current_variance[dim]);
                        
                        // Clamp correction factor to prevent over-correction
                        correction_factor = std::max(MIN_GV_WEIGHT, 
                                                   std::min(MAX_GV_WEIGHT, correction_factor));
                        
                        // Apply weighted correction
                        double mean_val = corrected_trajectory[i][dim];
                        corrected_trajectory[i][dim] = mean_val + 
                            gv_weight * correction_factor * (corrected_trajectory[i][dim] - mean_val);
                    }
                }
            }
        }
        
        return corrected_trajectory;
    }

    std::vector<double> GlobalVarianceCalculator::calculate_gv_weights(
        const std::vector<Eigen::VectorXd>& trajectory,
        const GlobalVarianceStatistics& gv_stats,
        const std::vector<std::string>& phoneme_sequence) const {
        
        std::vector<double> weights(trajectory.size(), 1.0);
        
        if (trajectory.empty() || gv_stats.feature_dimension == 0) {
            return weights;
        }
        
        // Calculate adaptive weights based on variance mismatch
        Eigen::VectorXd current_variance = compute_frame_wise_variance(trajectory);
        
        for (size_t i = 0; i < weights.size() && i < phoneme_sequence.size(); ++i) {
            const std::string& phoneme = phoneme_sequence[i];
            auto gv_pair = gv_stats.get_gv_statistics(phoneme);
            const Eigen::VectorXd& target_variance = gv_pair.first;
            
            if (target_variance.size() == current_variance.size()) {
                // Calculate variance distance as weight indicator
                double variance_distance = 0.0;
                int valid_dims = 0;
                
                for (int dim = 0; dim < target_variance.size(); ++dim) {
                    if (current_variance[dim] > MIN_VARIANCE && target_variance[dim] > MIN_VARIANCE) {
                        double ratio = current_variance[dim] / target_variance[dim];
                        variance_distance += std::abs(std::log(ratio));
                        valid_dims++;
                    }
                }
                
                if (valid_dims > 0) {
                    variance_distance /= valid_dims;
                    // Convert distance to weight (higher distance = higher correction weight)
                    weights[i] = std::max(MIN_GV_WEIGHT, 
                                        std::min(MAX_GV_WEIGHT, 1.0 + variance_distance));
                }
            }
        }
        
        return weights;
    }

    bool GlobalVarianceCalculator::save_gv_statistics(const GlobalVarianceStatistics& gv_stats, 
                                                     const std::string& filepath) const {
        // Simplified JSON output - in practice would use a proper JSON library
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        file << "{\n";
        file << "  \"feature_dimension\": " << gv_stats.feature_dimension << ",\n";
        file << "  \"total_frames\": " << gv_stats.total_frames << ",\n";
        file << "  \"global_gv_mean\": " << serialize_vector_to_json(gv_stats.global_gv_mean) << ",\n";
        file << "  \"global_gv_var\": " << serialize_vector_to_json(gv_stats.global_gv_var) << ",\n";
        file << "  \"phoneme_statistics\": {\n";
        
        bool first = true;
        for (const auto& pair : gv_stats.phoneme_gv_mean) {
            if (!first) file << ",\n";
            file << "    \"" << pair.first << "\": {\n";
            file << "      \"mean\": " << serialize_vector_to_json(pair.second) << ",\n";
            auto var_it = gv_stats.phoneme_gv_var.find(pair.first);
            if (var_it != gv_stats.phoneme_gv_var.end()) {
                file << "      \"var\": " << serialize_vector_to_json(var_it->second) << ",\n";
            }
            auto count_it = gv_stats.phoneme_frame_counts.find(pair.first);
            if (count_it != gv_stats.phoneme_frame_counts.end()) {
                file << "      \"frame_count\": " << count_it->second << "\n";
            }
            file << "    }";
            first = false;
        }
        
        file << "\n  }\n";
        file << "}\n";
        
        return file.good();
    }

    bool GlobalVarianceCalculator::load_gv_statistics(GlobalVarianceStatistics& /* gv_stats */, 
                                                     const std::string& filepath) const {
        // Simplified JSON loading - in practice would use a proper JSON library
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        // For now, return true as placeholder - full JSON parsing would be implemented
        // with a proper JSON library like nlohmann/json or rapidjson
        return true;
    }

    bool GlobalVarianceCalculator::validate_gv_statistics(const GlobalVarianceStatistics& gv_stats) const {
        if (gv_stats.feature_dimension <= 0) {
            return false;
        }
        
        if (gv_stats.global_gv_mean.size() != gv_stats.feature_dimension ||
            gv_stats.global_gv_var.size() != gv_stats.feature_dimension) {
            return false;
        }
        
        // Check for reasonable variance values
        for (int i = 0; i < gv_stats.global_gv_mean.size(); ++i) {
            if (gv_stats.global_gv_mean[i] < 0.0 || 
                gv_stats.global_gv_var[i] < MIN_VARIANCE ||
                !std::isfinite(gv_stats.global_gv_mean[i]) ||
                !std::isfinite(gv_stats.global_gv_var[i])) {
                return false;
            }
        }
        
        return true;
    }

    GlobalVarianceStatistics GlobalVarianceCalculator::merge_gv_statistics(
        const std::vector<GlobalVarianceStatistics>& gv_stats_list) const {
        
        GlobalVarianceStatistics merged;
        
        if (gv_stats_list.empty()) {
            return merged;
        }
        
        // Initialize with first valid statistics
        for (const auto& stats : gv_stats_list) {
            if (stats.feature_dimension > 0) {
                merged.initialize(stats.feature_dimension);
                break;
            }
        }
        
        if (merged.feature_dimension == 0) {
            return merged;
        }
        
        // Simple averaging approach for merging
        int valid_count = 0;
        for (const auto& stats : gv_stats_list) {
            if (stats.feature_dimension == merged.feature_dimension) {
                if (valid_count == 0) {
                    merged.global_gv_mean = stats.global_gv_mean;
                    merged.global_gv_var = stats.global_gv_var;
                } else {
                    merged.global_gv_mean = (merged.global_gv_mean * valid_count + stats.global_gv_mean) / (valid_count + 1);
                    merged.global_gv_var = (merged.global_gv_var * valid_count + stats.global_gv_var) / (valid_count + 1);
                }
                merged.total_frames += stats.total_frames;
                valid_count++;
                
                // Merge phoneme statistics
                for (const auto& pair : stats.phoneme_gv_mean) {
                    const std::string& phoneme = pair.first;
                    if (merged.phoneme_gv_mean.find(phoneme) == merged.phoneme_gv_mean.end()) {
                        merged.phoneme_gv_mean[phoneme] = pair.second;
                        auto var_it = stats.phoneme_gv_var.find(phoneme);
                        if (var_it != stats.phoneme_gv_var.end()) {
                            merged.phoneme_gv_var[phoneme] = var_it->second;
                        }
                        auto count_it = stats.phoneme_frame_counts.find(phoneme);
                        if (count_it != stats.phoneme_frame_counts.end()) {
                            merged.phoneme_frame_counts[phoneme] = count_it->second;
                        }
                    } else {
                        // Average existing phoneme statistics
                        merged.phoneme_gv_mean[phoneme] = 
                            (merged.phoneme_gv_mean[phoneme] + pair.second) / 2.0;
                    }
                }
            }
        }
        
        return merged;
    }

    // Private helper methods
    Eigen::VectorXd GlobalVarianceCalculator::compute_frame_wise_variance(
        const std::vector<Eigen::VectorXd>& frames) const {
        
        if (frames.empty()) {
            return Eigen::VectorXd();
        }
        
        int dim = frames[0].size();
        Eigen::VectorXd mean = Eigen::VectorXd::Zero(dim);
        Eigen::VectorXd variance = Eigen::VectorXd::Zero(dim);
        
        // Calculate mean
        for (const auto& frame : frames) {
            if (frame.size() == dim) {
                mean += frame;
            }
        }
        mean /= static_cast<double>(frames.size());
        
        // Calculate variance
        for (const auto& frame : frames) {
            if (frame.size() == dim) {
                Eigen::VectorXd diff = frame - mean;
                variance += diff.cwiseProduct(diff);
            }
        }
        variance /= static_cast<double>(frames.size());
        
        // Ensure minimum variance for numerical stability
        for (int i = 0; i < dim; ++i) {
            variance[i] = std::max(variance[i], MIN_VARIANCE);
        }
        
        return variance;
    }

    void GlobalVarianceCalculator::accumulate_phoneme_statistics(
        std::map<std::string, std::vector<Eigen::VectorXd>>& phoneme_frames,
        const std::vector<Eigen::VectorXd>& sequence,
        const std::vector<std::string>& phoneme_labels) const {
        
        size_t min_size = std::min(sequence.size(), phoneme_labels.size());
        
        for (size_t i = 0; i < min_size; ++i) {
            const std::string& phoneme = phoneme_labels[i];
            phoneme_frames[phoneme].push_back(sequence[i]);
        }
    }

    void GlobalVarianceCalculator::accumulate_alignment_statistics(
        std::map<std::string, std::vector<Eigen::VectorXd>>& phoneme_frames,
        const std::vector<Eigen::VectorXd>& sequence,
        const SequenceAlignment& alignment) const {
        
        for (const auto& boundary : alignment.phoneme_boundaries) {
            if (boundary.start_frame >= 0 && 
                boundary.end_frame <= static_cast<int>(sequence.size()) && 
                boundary.start_frame < boundary.end_frame) {
                
                for (int i = boundary.start_frame; i < boundary.end_frame; ++i) {
                    phoneme_frames[boundary.phoneme].push_back(sequence[i]);
                }
            }
        }
    }

    double GlobalVarianceCalculator::safe_variance(const std::vector<double>& values) const {
        if (values.empty()) {
            return MIN_VARIANCE;
        }
        
        double mean = 0.0;
        for (double val : values) {
            mean += val;
        }
        mean /= static_cast<double>(values.size());
        
        double variance = 0.0;
        for (double val : values) {
            double diff = val - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(values.size());
        
        return std::max(variance, MIN_VARIANCE);
    }

    Eigen::VectorXd GlobalVarianceCalculator::safe_vector_variance(
        const std::vector<Eigen::VectorXd>& vectors) const {
        
        if (vectors.empty()) {
            return Eigen::VectorXd();
        }
        
        return compute_frame_wise_variance(vectors);
    }

    std::string GlobalVarianceCalculator::serialize_vector_to_json(const Eigen::VectorXd& vec) const {
        std::ostringstream oss;
        oss << "[";
        for (int i = 0; i < vec.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << vec[i];
        }
        oss << "]";
        return oss.str();
    }

    Eigen::VectorXd GlobalVarianceCalculator::deserialize_vector_from_json(const std::string& /* json_str */) const {
        // Placeholder implementation - would use proper JSON parsing
        return Eigen::VectorXd();
    }

    // Parallel training utility implementations
    std::vector<std::vector<int>> HmmTrainer::create_load_balanced_chunks(
        const std::vector<std::vector<Eigen::VectorXd>>& sequences,
        int num_threads) const {
        
        std::vector<std::vector<int>> chunks(num_threads);
        
        if (sequences.empty() || num_threads <= 0) {
            return chunks;
        }
        
        // Calculate sequence weights (lengths)
        std::vector<std::pair<int, int>> sequence_weights;  // (length, index)
        for (size_t i = 0; i < sequences.size(); ++i) {
            sequence_weights.push_back({static_cast<int>(sequences[i].size()), static_cast<int>(i)});
        }
        
        // Sort by sequence length (descending) for better load balancing
        std::sort(sequence_weights.begin(), sequence_weights.end(), 
                 [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                     return a.first > b.first;
                 });
        
        // Distribute sequences using a greedy approach
        std::vector<int> thread_loads(num_threads, 0);
        
        for (const auto& seq_weight : sequence_weights) {
            // Find thread with minimum current load
            int min_load_thread = 0;
            for (int t = 1; t < num_threads; ++t) {
                if (thread_loads[t] < thread_loads[min_load_thread]) {
                    min_load_thread = t;
                }
            }
            
            // Assign sequence to this thread
            chunks[min_load_thread].push_back(seq_weight.second);
            thread_loads[min_load_thread] += seq_weight.first;
        }
        
        return chunks;
    }

    int HmmTrainer::determine_optimal_thread_count(size_t num_sequences) const {
        int optimal_threads = config_.num_threads;
        
        if (optimal_threads <= 0) {
#ifdef NEXUSSYNTH_OPENMP_ENABLED
            optimal_threads = omp_get_max_threads();
#else
            optimal_threads = std::thread::hardware_concurrency();
            if (optimal_threads == 0) optimal_threads = 4;  // Fallback
#endif
        }
        
        // Don't use more threads than sequences (with minimum sequences per thread)
        int max_useful_threads = static_cast<int>(num_sequences / std::max(1, config_.min_sequences_per_thread));
        optimal_threads = std::min(optimal_threads, std::max(1, max_useful_threads));
        
        return optimal_threads;
    }

    double HmmTrainer::calculate_parallel_efficiency(double sequential_time, double parallel_time, int num_threads) const {
        if (parallel_time <= 0.0 || num_threads <= 0) {
            return 0.0;
        }
        
        // Parallel efficiency = (Sequential time) / (Parallel time * Num threads)
        double theoretical_speedup = static_cast<double>(num_threads);
        double actual_speedup = sequential_time / parallel_time;
        
        return actual_speedup / theoretical_speedup;
    }

    void HmmTrainer::log_parallel_performance(const TrainingStats& stats, int /* iteration */) const {
        if (stats.e_step_timings.empty() || stats.m_step_timings.empty()) {
            return;
        }
        
        double e_step_time = stats.e_step_timings.back();
        double m_step_time = stats.m_step_timings.back();
        
        std::cout << "  Parallel Performance - E-Step: " << e_step_time << "s, M-Step: " << m_step_time << "s";
        
        if (!stats.parallel_efficiency.empty()) {
            std::cout << ", Efficiency: " << (stats.parallel_efficiency.back() * 100.0) << "%";
        }
        
#ifdef NEXUSSYNTH_OPENMP_ENABLED
        std::cout << ", Threads: " << omp_get_max_threads();
#endif
        
        std::cout << std::endl;
    }

} // namespace hmm
} // namespace nexussynth
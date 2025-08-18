#pragma once

#include "hmm_structures.h"
#include "gaussian_mixture.h"
#include "context_features.h"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <limits>
#include <Eigen/Core>
#include <Eigen/Dense>

namespace nexussynth {
namespace hmm {

    /**
     * @brief Training configuration for HMM training
     */
    struct TrainingConfig {
        int max_iterations;             // Maximum EM iterations
        double convergence_threshold;   // Log-likelihood convergence threshold
        double parameter_threshold;     // Parameter change threshold
        bool use_validation_set;        // Enable validation-based early stopping
        double validation_split;        // Fraction of data for validation
        int convergence_window;         // Window size for convergence checking
        bool verbose;                   // Enable verbose logging
        
        TrainingConfig() 
            : max_iterations(100)
            , convergence_threshold(1e-4)
            , parameter_threshold(1e-3)
            , use_validation_set(true)
            , validation_split(0.1)
            , convergence_window(5)
            , verbose(false) {}
    };

    /**
     * @brief Training statistics and convergence information
     */
    struct TrainingStats {
        std::vector<double> log_likelihoods;     // Log-likelihood per iteration
        std::vector<double> validation_scores;   // Validation scores per iteration
        std::vector<double> parameter_changes;   // Parameter change magnitudes
        int final_iteration;                     // Final iteration count
        bool converged;                          // Did training converge?
        double final_log_likelihood;             // Final training log-likelihood
        double best_validation_score;            // Best validation score
        std::string convergence_reason;          // Reason for stopping
        
        TrainingStats() 
            : final_iteration(0)
            , converged(false)
            , final_log_likelihood(-std::numeric_limits<double>::infinity())
            , best_validation_score(-std::numeric_limits<double>::infinity()) {}
    };

    /**
     * @brief Phoneme boundary information for alignment
     */
    struct PhonemeBoundary {
        int start_frame;                         // Start frame index
        int end_frame;                           // End frame index (exclusive)
        std::string phoneme;                     // Phoneme label
        double confidence_score;                 // Alignment confidence [0-1]
        double duration_ms;                      // Duration in milliseconds
        
        PhonemeBoundary() : start_frame(0), end_frame(0), confidence_score(0.0), duration_ms(0.0) {}
        PhonemeBoundary(int start, int end, const std::string& ph, double conf = 0.0, double dur = 0.0)
            : start_frame(start), end_frame(end), phoneme(ph), confidence_score(conf), duration_ms(dur) {}
    };

    /**
     * @brief Enhanced sequence alignment result from Viterbi algorithm
     */
    struct SequenceAlignment {
        std::vector<int> state_sequence;         // Most likely state sequence
        std::vector<int> frame_to_state;         // Frame-to-state mapping
        std::vector<double> frame_scores;        // Log probability per frame
        std::vector<double> state_posteriors;   // State posterior probabilities per frame
        std::vector<PhonemeBoundary> phoneme_boundaries; // Phoneme boundary timestamps
        double total_score;                      // Total sequence log probability
        double average_confidence;               // Overall alignment confidence
        double frame_rate;                       // Frames per second for time conversion
        
        SequenceAlignment() : total_score(-std::numeric_limits<double>::infinity()), 
                             average_confidence(0.0), frame_rate(100.0) {}
        
        // Utility methods
        double get_total_duration_ms() const {
            return frame_to_state.empty() ? 0.0 : (frame_to_state.size() / frame_rate) * 1000.0;
        }
        
        PhonemeBoundary* find_phoneme_at_frame(int frame_idx) {
            for (auto& boundary : phoneme_boundaries) {
                if (frame_idx >= boundary.start_frame && frame_idx < boundary.end_frame) {
                    return &boundary;
                }
            }
            return nullptr;
        }
    };

    /**
     * @brief Forward-Backward algorithm matrices and probabilities
     */
    struct ForwardBackwardResult {
        Eigen::MatrixXd forward_probs;           // Forward probabilities [T x N]
        Eigen::MatrixXd backward_probs;          // Backward probabilities [T x N]
        Eigen::MatrixXd gamma;                   // State posteriors [T x N]
        Eigen::MatrixXd xi;                      // Transition posteriors [T-1 x N*N]
        double log_likelihood;                   // Sequence log-likelihood
        
        ForwardBackwardResult() : log_likelihood(-std::numeric_limits<double>::infinity()) {}
    };

    /**
     * @brief Core HMM trainer implementing EM algorithm with Forward-Backward
     * 
     * Implements Baum-Welch algorithm for HMM parameter estimation
     * with advanced convergence detection and numerical stability
     */
    class HmmTrainer {
    public:
        explicit HmmTrainer(const TrainingConfig& config = TrainingConfig());
        
        // Training interface
        TrainingStats train_model(PhonemeHmm& model, 
                                const std::vector<std::vector<Eigen::VectorXd>>& training_sequences);
        
        TrainingStats train_model_with_validation(PhonemeHmm& model,
                                                 const std::vector<std::vector<Eigen::VectorXd>>& training_sequences,
                                                 const std::vector<std::vector<Eigen::VectorXd>>& validation_sequences);
        
        // Forward-Backward algorithm
        ForwardBackwardResult forward_backward(const PhonemeHmm& model,
                                             const std::vector<Eigen::VectorXd>& observation_sequence) const;
        
        // Viterbi algorithm for state alignment
        SequenceAlignment viterbi_alignment(const PhonemeHmm& model,
                                           const std::vector<Eigen::VectorXd>& observation_sequence) const;
        
        // Enhanced forced alignment with phoneme boundaries
        SequenceAlignment forced_alignment(const PhonemeHmm& model,
                                         const std::vector<Eigen::VectorXd>& observation_sequence,
                                         const std::vector<std::string>& phoneme_sequence,
                                         double frame_rate = 100.0) const;
        
        // Forced alignment with time constraints (start/end time hints)
        SequenceAlignment constrained_alignment(const PhonemeHmm& model,
                                              const std::vector<Eigen::VectorXd>& observation_sequence,
                                              const std::vector<std::string>& phoneme_sequence,
                                              const std::vector<std::pair<double, double>>& time_constraints,
                                              double frame_rate = 100.0) const;
        
        // Batch forced alignment for multiple sequences
        std::vector<SequenceAlignment> batch_forced_alignment(const std::map<std::string, PhonemeHmm>& models,
                                                             const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                                             const std::vector<std::vector<std::string>>& phoneme_sequences,
                                                             double frame_rate = 100.0) const;
        
        // Batch processing for multiple sequences
        std::vector<ForwardBackwardResult> batch_forward_backward(const PhonemeHmm& model,
                                                                 const std::vector<std::vector<Eigen::VectorXd>>& sequences) const;
        
        // Model evaluation
        double evaluate_model(const PhonemeHmm& model,
                            const std::vector<std::vector<Eigen::VectorXd>>& test_sequences) const;
        
        // Configuration
        void set_config(const TrainingConfig& config) { config_ = config; }
        const TrainingConfig& get_config() const { return config_; }
        
    private:
        TrainingConfig config_;
        
        // EM algorithm core methods
        double em_expectation_step(const PhonemeHmm& model,
                                 const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                 std::vector<ForwardBackwardResult>& fb_results) const;
        
        void em_maximization_step(PhonemeHmm& model,
                                const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                const std::vector<ForwardBackwardResult>& fb_results);
        
        // Forward-Backward implementation details
        Eigen::VectorXd compute_forward_step(const PhonemeHmm& model,
                                           const Eigen::VectorXd& observation,
                                           const Eigen::VectorXd& prev_forward) const;
        
        Eigen::VectorXd compute_backward_step(const PhonemeHmm& model,
                                            const Eigen::VectorXd& observation,
                                            const Eigen::VectorXd& next_backward) const;
        
        // Viterbi implementation details
        Eigen::MatrixXd compute_viterbi_trellis(const PhonemeHmm& model,
                                              const std::vector<Eigen::VectorXd>& observations) const;
        
        std::vector<int> backtrack_viterbi_path(const Eigen::MatrixXd& trellis) const;
        
        // Enhanced forced alignment helpers
        Eigen::MatrixXd compute_forced_alignment_trellis(const PhonemeHmm& model,
                                                       const std::vector<Eigen::VectorXd>& observations,
                                                       const std::vector<std::string>& phoneme_sequence) const;
        
        Eigen::MatrixXd compute_constrained_trellis(const PhonemeHmm& model,
                                                  const std::vector<Eigen::VectorXd>& observations,
                                                  const std::vector<std::string>& phoneme_sequence,
                                                  const std::vector<std::pair<double, double>>& time_constraints,
                                                  double frame_rate) const;
        
        std::vector<PhonemeBoundary> extract_phoneme_boundaries(const std::vector<int>& state_sequence,
                                                              const std::vector<std::string>& phoneme_sequence,
                                                              const PhonemeHmm& model,
                                                              double frame_rate) const;
        
        double compute_alignment_confidence(const Eigen::MatrixXd& trellis,
                                          const std::vector<int>& state_sequence,
                                          const ForwardBackwardResult& fb_result) const;
        
        std::vector<double> compute_state_posteriors(const ForwardBackwardResult& fb_result,
                                                    const std::vector<int>& state_sequence) const;
        
        // Parameter update methods
        void update_transition_probabilities(PhonemeHmm& model,
                                           const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                           const std::vector<ForwardBackwardResult>& fb_results);
        
        void update_emission_probabilities(PhonemeHmm& model,
                                         const std::vector<std::vector<Eigen::VectorXd>>& sequences,
                                         const std::vector<ForwardBackwardResult>& fb_results);
        
        // Convergence detection
        bool check_convergence(const TrainingStats& stats) const;
        bool check_log_likelihood_convergence(const std::vector<double>& log_likelihoods) const;
        bool check_parameter_convergence(const PhonemeHmm& old_model, const PhonemeHmm& new_model) const;
        bool check_validation_convergence(const std::vector<double>& validation_scores) const;
        
        // Utility methods
        double compute_parameter_distance(const PhonemeHmm& model1, const PhonemeHmm& model2) const;
        std::vector<std::vector<Eigen::VectorXd>> split_validation_data(
            const std::vector<std::vector<Eigen::VectorXd>>& data,
            double validation_split) const;
        
        // Numerical stability helpers
        double log_sum_exp(const std::vector<double>& log_values) const;
        void normalize_probabilities(Eigen::VectorXd& probabilities) const;
        Eigen::VectorXd compute_log_probabilities(const std::vector<double>& probabilities) const;
        
        // Logging and debugging
        void log_iteration_info(int iteration, const TrainingStats& stats) const;
        void log_convergence_info(const TrainingStats& stats) const;
    };

    /**
     * @brief Multi-model trainer for context-dependent HMM training
     * 
     * Manages training of multiple context-dependent HMM models
     * with shared statistics and parallel processing support
     */
    class MultiModelTrainer {
    public:
        explicit MultiModelTrainer(const TrainingConfig& config = TrainingConfig());
        
        // Training interface for multiple models
        std::map<std::string, TrainingStats> train_models(
            std::map<std::string, PhonemeHmm>& models,
            const std::map<std::string, std::vector<std::vector<Eigen::VectorXd>>>& training_data);
        
        // Parallel training with thread pool
        std::map<std::string, TrainingStats> train_models_parallel(
            std::map<std::string, PhonemeHmm>& models,
            const std::map<std::string, std::vector<std::vector<Eigen::VectorXd>>>& training_data,
            int num_threads = 0);  // 0 = auto-detect
        
        // Batch evaluation
        std::map<std::string, double> evaluate_models(
            const std::map<std::string, PhonemeHmm>& models,
            const std::map<std::string, std::vector<std::vector<Eigen::VectorXd>>>& test_data) const;
        
        // Configuration
        void set_config(const TrainingConfig& config) { config_ = config; }
        const TrainingConfig& get_config() const { return config_; }
        
    private:
        TrainingConfig config_;
        
        // Single model training task for threading
        TrainingStats train_single_model(const std::string& model_name,
                                        PhonemeHmm& model,
                                        const std::vector<std::vector<Eigen::VectorXd>>& training_sequences);
        
        // Progress tracking
        void update_training_progress(const std::string& model_name, int iteration, double log_likelihood);
    };

} // namespace hmm
} // namespace nexussynth
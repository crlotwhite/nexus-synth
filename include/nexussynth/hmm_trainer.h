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
        double parameter_threshold;     // Parameter change threshold (L2 norm)
        bool use_validation_set;        // Enable validation-based early stopping
        double validation_split;        // Fraction of data for validation
        int convergence_window;         // Window size for convergence checking
        bool verbose;                   // Enable verbose logging
        
        // Enhanced convergence detection parameters
        bool enable_adaptive_thresholds;    // Enable adaptive threshold adjustment
        double overfitting_threshold;       // Validation score drop threshold for overfitting
        int patience;                       // Early stopping patience (iterations)
        double min_improvement;             // Minimum relative improvement required
        bool enable_model_checkpointing;    // Save best models during training
        double convergence_confidence;      // Required confidence for convergence [0-1]
        
        TrainingConfig() 
            : max_iterations(100)
            , convergence_threshold(1e-4)
            , parameter_threshold(1e-3)
            , use_validation_set(true)
            , validation_split(0.1)
            , convergence_window(5)
            , verbose(false)
            , enable_adaptive_thresholds(true)
            , overfitting_threshold(0.005)
            , patience(10)
            , min_improvement(1e-5)
            , enable_model_checkpointing(true)
            , convergence_confidence(0.95) {}
    };

    /**
     * @brief Training statistics and convergence information
     */
    struct TrainingStats {
        std::vector<double> log_likelihoods;     // Log-likelihood per iteration
        std::vector<double> validation_scores;   // Validation scores per iteration
        std::vector<double> parameter_changes;   // Parameter change magnitudes (L2 norm)
        int final_iteration;                     // Final iteration count
        bool converged;                          // Did training converge?
        double final_log_likelihood;             // Final training log-likelihood
        double best_validation_score;            // Best validation score
        std::string convergence_reason;          // Reason for stopping
        
        // Enhanced convergence tracking
        std::vector<double> convergence_confidence_scores;  // Confidence per iteration
        std::vector<std::string> convergence_criteria_met;  // Which criteria triggered convergence
        int best_validation_iteration;          // Iteration with best validation score
        double convergence_confidence;           // Final convergence confidence
        bool early_stopped;                     // Was training early stopped?
        int patience_counter;                   // Current patience counter
        double adaptive_threshold;              // Current adaptive threshold
        std::vector<double> relative_improvements; // Relative improvement per iteration
        
        TrainingStats() 
            : final_iteration(0)
            , converged(false)
            , final_log_likelihood(-std::numeric_limits<double>::infinity())
            , best_validation_score(-std::numeric_limits<double>::infinity())
            , best_validation_iteration(0)
            , convergence_confidence(0.0)
            , early_stopped(false)
            , patience_counter(0)
            , adaptive_threshold(1e-4) {}
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
        
        // Model checkpointing
        mutable PhonemeHmm best_model_;
        mutable bool has_checkpoint_;
        
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
        
        // Enhanced convergence detection
        bool check_convergence(TrainingStats& stats) const;
        bool check_log_likelihood_convergence(const std::vector<double>& log_likelihoods, 
                                            double threshold = -1.0) const;
        bool check_parameter_convergence(const PhonemeHmm& old_model, const PhonemeHmm& new_model) const;
        bool check_validation_convergence(const std::vector<double>& validation_scores) const;
        
        // Advanced convergence detection methods
        bool check_multi_criteria_convergence(TrainingStats& stats, 
                                            std::vector<std::string>& criteria_met) const;
        double calculate_convergence_confidence(const TrainingStats& stats) const;
        bool check_overfitting_detection(const TrainingStats& stats) const;
        bool check_early_stopping_conditions(TrainingStats& stats) const;
        double compute_relative_improvement(const std::vector<double>& values, int window_size = 3) const;
        double update_adaptive_threshold(const TrainingStats& stats) const;
        
        // Model checkpointing and management
        PhonemeHmm save_checkpoint(const PhonemeHmm& model, const TrainingStats& stats) const;
        bool should_save_checkpoint(const TrainingStats& stats) const;
        PhonemeHmm restore_best_model(const PhonemeHmm& current_model, const TrainingStats& stats) const;
        
        // Utility methods
        double compute_parameter_distance(const PhonemeHmm& model1, const PhonemeHmm& model2) const;
        double compute_parameter_l2_norm(const PhonemeHmm& model1, const PhonemeHmm& model2) const;
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

    /**
     * @brief Global Variance (GV) statistics for spectral parameter correction
     * 
     * Manages calculation and storage of Global Variance statistics to prevent
     * over-smoothing in HMM-based parameter generation. Critical for natural
     * sounding synthesis output.
     */
    struct GlobalVarianceStatistics {
        std::map<std::string, Eigen::VectorXd> phoneme_gv_mean;    // Per-phoneme GV means
        std::map<std::string, Eigen::VectorXd> phoneme_gv_var;     // Per-phoneme GV variances
        Eigen::VectorXd global_gv_mean;                           // Overall GV means
        Eigen::VectorXd global_gv_var;                            // Overall GV variances
        std::map<std::string, int> phoneme_frame_counts;          // Frame counts per phoneme
        int total_frames;                                         // Total frame count
        int feature_dimension;                                    // Feature vector dimension
        
        GlobalVarianceStatistics() : total_frames(0), feature_dimension(0) {}
        
        // Initialize with feature dimension
        void initialize(int dim) {
            feature_dimension = dim;
            global_gv_mean = Eigen::VectorXd::Zero(dim);
            global_gv_var = Eigen::VectorXd::Zero(dim);
            total_frames = 0;
        }
        
        // Clear all statistics
        void clear() {
            phoneme_gv_mean.clear();
            phoneme_gv_var.clear();
            phoneme_frame_counts.clear();
            global_gv_mean.setZero();
            global_gv_var.setZero();
            total_frames = 0;
        }
        
        // Check if statistics are available for a phoneme
        bool has_phoneme_statistics(const std::string& phoneme) const {
            return phoneme_gv_mean.find(phoneme) != phoneme_gv_mean.end();
        }
        
        // Get GV statistics for a specific phoneme (fallback to global if not available)
        std::pair<Eigen::VectorXd, Eigen::VectorXd> get_gv_statistics(const std::string& phoneme) const {
            auto it = phoneme_gv_mean.find(phoneme);
            if (it != phoneme_gv_mean.end()) {
                auto var_it = phoneme_gv_var.find(phoneme);
                return std::make_pair(it->second, var_it->second);
            }
            return std::make_pair(global_gv_mean, global_gv_var);
        }
    };

    /**
     * @brief Global Variance calculator for HMM parameter generation
     * 
     * Calculates frame-wise variance statistics for spectral parameters
     * to prevent over-smoothing in MLPG parameter generation
     */
    class GlobalVarianceCalculator {
    public:
        GlobalVarianceCalculator() = default;
        
        // Calculate GV statistics from training sequences
        GlobalVarianceStatistics calculate_gv_statistics(
            const std::vector<std::vector<Eigen::VectorXd>>& sequences,
            const std::vector<std::vector<std::string>>& phoneme_labels) const;
        
        // Calculate GV statistics with phoneme alignment information
        GlobalVarianceStatistics calculate_gv_statistics_with_alignment(
            const std::vector<std::vector<Eigen::VectorXd>>& sequences,
            const std::vector<SequenceAlignment>& alignments) const;
        
        // Update existing GV statistics with new data (incremental)
        void update_gv_statistics(GlobalVarianceStatistics& gv_stats,
                                const std::vector<Eigen::VectorXd>& sequence,
                                const std::vector<std::string>& phoneme_labels) const;
        
        // Calculate frame-wise variance for a single sequence
        Eigen::VectorXd calculate_sequence_variance(const std::vector<Eigen::VectorXd>& sequence) const;
        
        // Calculate phoneme-specific variances from aligned data
        std::map<std::string, Eigen::VectorXd> calculate_phoneme_variances(
            const std::vector<Eigen::VectorXd>& sequence,
            const SequenceAlignment& alignment) const;
        
        // Apply GV correction to parameter trajectory
        std::vector<Eigen::VectorXd> apply_gv_correction(
            const std::vector<Eigen::VectorXd>& original_trajectory,
            const GlobalVarianceStatistics& gv_stats,
            const std::vector<std::string>& phoneme_sequence,
            double gv_weight = 1.0) const;
        
        // Calculate GV correction weights based on confidence
        std::vector<double> calculate_gv_weights(
            const std::vector<Eigen::VectorXd>& trajectory,
            const GlobalVarianceStatistics& gv_stats,
            const std::vector<std::string>& phoneme_sequence) const;
        
        // Save/Load GV statistics to/from JSON file
        bool save_gv_statistics(const GlobalVarianceStatistics& gv_stats, 
                               const std::string& filepath) const;
        bool load_gv_statistics(GlobalVarianceStatistics& gv_stats, 
                               const std::string& filepath) const;
        
        // Validate GV statistics integrity
        bool validate_gv_statistics(const GlobalVarianceStatistics& gv_stats) const;
        
        // Merge multiple GV statistics (for ensemble training)
        GlobalVarianceStatistics merge_gv_statistics(
            const std::vector<GlobalVarianceStatistics>& gv_stats_list) const;
        
    private:
        // Helper methods for variance calculation
        Eigen::VectorXd compute_frame_wise_variance(const std::vector<Eigen::VectorXd>& frames) const;
        
        void accumulate_phoneme_statistics(std::map<std::string, std::vector<Eigen::VectorXd>>& phoneme_frames,
                                         const std::vector<Eigen::VectorXd>& sequence,
                                         const std::vector<std::string>& phoneme_labels) const;
        
        void accumulate_alignment_statistics(std::map<std::string, std::vector<Eigen::VectorXd>>& phoneme_frames,
                                           const std::vector<Eigen::VectorXd>& sequence,
                                           const SequenceAlignment& alignment) const;
        
        // Statistical utilities
        double safe_variance(const std::vector<double>& values) const;
        Eigen::VectorXd safe_vector_variance(const std::vector<Eigen::VectorXd>& vectors) const;
        
        // JSON serialization helpers
        std::string serialize_vector_to_json(const Eigen::VectorXd& vec) const;
        Eigen::VectorXd deserialize_vector_from_json(const std::string& json_str) const;
        
        // Numerical stability constants
        static constexpr double MIN_VARIANCE = 1e-6;
        static constexpr double MIN_GV_WEIGHT = 0.01;
        static constexpr double MAX_GV_WEIGHT = 2.0;
    };

} // namespace hmm
} // namespace nexussynth
#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "hmm_structures.h"
#include "context_features.h"
#include "mlpg_engine.h"

namespace nexussynth {
namespace mlpg {

    /**
     * @brief Context-HMM-MLPG integration bridge
     * 
     * Provides the integration layer between context features,
     * HMM state sequences, and MLPG trajectory generation
     */
    class ContextMlpgBridge {
    public:
        /**
         * @brief Configuration for context-based trajectory generation
         */
        struct BridgeConfig {
            bool use_context_dependent_duration = true;    // Context-based duration modeling
            bool cache_hmm_sequences = true;               // Cache frequent HMM patterns
            bool enable_real_time_mode = false;            // Optimize for real-time synthesis
            
            // Duration modeling parameters
            double base_duration_ms = 100.0;               // Base phoneme duration
            double duration_variance = 0.3;                // Duration variability factor
            
            // Caching parameters
            size_t max_cache_size = 1000;                  // Maximum cached sequences
            
            MlpgConfig mlpg_config;                        // MLPG engine configuration
        };

        explicit ContextMlpgBridge(const BridgeConfig& config = BridgeConfig{});
        ~ContextMlpgBridge() = default;

        /**
         * @brief Generate trajectory from context features and HMM models
         * 
         * @param context_sequence Sequence of context features (phonemes with context)
         * @param hmm_models Available HMM models mapped by phoneme/context
         * @param stats Optional statistics output
         * @return Generated parameter trajectory
         */
        std::vector<Eigen::VectorXd> generate_trajectory_from_context(
            const std::vector<ContextFeatures>& context_sequence,
            const std::unordered_map<std::string, std::shared_ptr<hmm::PhonemeHmm>>& hmm_models,
            mlpg::TrajectoryStats* stats = nullptr
        );

        /**
         * @brief Select optimal HMM states based on context features
         * 
         * @param context_features Input context information
         * @param hmm_models Available HMM models
         * @return Selected HMM state sequence
         */
        std::vector<hmm::HmmState> select_hmm_sequence(
            const ContextFeatures& context_features,
            const std::unordered_map<std::string, std::shared_ptr<hmm::PhonemeHmm>>& hmm_models
        );

        /**
         * @brief Calculate context-dependent state durations
         * 
         * @param context_sequence Context features for duration calculation
         * @param base_durations Base state durations (in frames)
         * @return Context-adjusted durations
         */
        std::vector<std::vector<int>> calculate_context_durations(
            const std::vector<ContextFeatures>& context_sequence,
            const std::vector<std::vector<int>>& base_durations
        );

        /**
         * @brief Generate HMM model key for context-dependent lookup
         * 
         * @param context Context features
         * @return String key for HMM model selection
         */
        std::string generate_hmm_key(const ContextFeatures& context);

        // Configuration management
        void set_config(const BridgeConfig& config) { config_ = config; }
        const BridgeConfig& get_config() const { return config_; }

        // Cache management
        void clear_cache();
        size_t cache_size() const;
        double cache_hit_rate() const;

    private:
        BridgeConfig config_;
        std::unique_ptr<MlpgEngine> mlpg_engine_;

        // Caching system
        struct SequenceCache {
            std::string context_key;
            std::vector<hmm::HmmState> hmm_sequence;
            std::vector<int> durations;
        };

        std::unordered_map<std::string, SequenceCache> sequence_cache_;
        mutable size_t cache_hits_ = 0;
        mutable size_t cache_requests_ = 0;

        /**
         * @brief Duration modeling based on context features
         * 
         * @param context Context features for duration calculation
         * @param base_duration Base duration in frames
         * @return Context-adjusted duration
         */
        int calculate_phoneme_duration(const ContextFeatures& context, int base_duration);

        /**
         * @brief Get cached HMM sequence if available
         * 
         * @param context_key Context-based cache key
         * @return Cached sequence if found, nullptr otherwise
         */
        const SequenceCache* get_cached_sequence(const std::string& context_key) const;

        /**
         * @brief Cache HMM sequence for future use
         * 
         * @param context_key Context-based cache key
         * @param hmm_sequence HMM state sequence
         * @param durations State durations
         */
        void cache_sequence(const std::string& context_key,
                           const std::vector<hmm::HmmState>& hmm_sequence,
                           const std::vector<int>& durations);

        /**
         * @brief Apply context-dependent modifications to HMM states
         * 
         * @param hmm_states Base HMM states
         * @param context Context features for modification
         * @return Modified HMM states
         */
        std::vector<hmm::HmmState> apply_context_modifications(
            const std::vector<hmm::HmmState>& hmm_states,
            const ContextFeatures& context
        );

        /**
         * @brief Calculate prosodic duration scaling factor
         * 
         * @param context Context features
         * @return Duration scaling factor (1.0 = no change)
         */
        double calculate_prosodic_duration_factor(const ContextFeatures& context);

        /**
         * @brief Validate context sequence for consistency
         * 
         * @param context_sequence Input context sequence
         * @throws std::invalid_argument on validation failure
         */
        void validate_context_sequence(const std::vector<ContextFeatures>& context_sequence);
    };

    /**
     * @brief Utility functions for context-based synthesis
     */
    namespace context_utils {
        
        /**
         * @brief Create default HMM model mappings
         * 
         * @param phoneme_list List of phonemes to create models for
         * @param feature_dim Feature dimension for models
         * @return Default HMM model mapping
         */
        std::unordered_map<std::string, std::shared_ptr<hmm::PhonemeHmm>> 
        create_default_hmm_models(const std::vector<std::string>& phoneme_list, int feature_dim = 39);

        /**
         * @brief Extract unique phonemes from context sequence
         * 
         * @param context_sequence Input context sequence
         * @return Set of unique phonemes
         */
        std::vector<std::string> extract_phonemes(const std::vector<ContextFeatures>& context_sequence);

        /**
         * @brief Calculate total synthesis duration from context
         * 
         * @param context_sequence Input context sequence
         * @param base_frame_rate Frame rate in Hz (default: 200 Hz = 5ms frames)
         * @return Total duration in frames
         */
        int calculate_total_duration(const std::vector<ContextFeatures>& context_sequence, 
                                   double base_frame_rate = 200.0);

        /**
         * @brief Generate test context sequence for development
         * 
         * @param phonemes Phoneme sequence
         * @param num_contexts Number of context features per phoneme
         * @return Test context sequence
         */
        std::vector<ContextFeatures> generate_test_context_sequence(
            const std::vector<std::string>& phonemes,
            int num_contexts = 1
        );

    } // namespace context_utils

} // namespace mlpg
} // namespace nexussynth
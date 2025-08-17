#pragma once

#include <vector>
#include <memory>
#include <string>
#include <Eigen/Core>
#include <Eigen/Dense>
#include "gaussian_mixture.h"

namespace nexussynth {
namespace hmm {


    /**
     * @brief State transition probabilities for HMM
     * 
     * Models the transition probabilities between HMM states
     * in the left-to-right topology typical of speech synthesis
     */
    struct StateTransition {
        double self_loop_prob;          // Probability of staying in current state
        double next_state_prob;         // Probability of advancing to next state
        double exit_prob;               // Probability of exiting (for final states)
        
        StateTransition() 
            : self_loop_prob(0.6)
            , next_state_prob(0.4) 
            , exit_prob(0.0) {}
        
        // Normalize probabilities to sum to 1.0
        void normalize() {
            double sum = self_loop_prob + next_state_prob + exit_prob;
            if (sum > 0.0) {
                self_loop_prob /= sum;
                next_state_prob /= sum;
                exit_prob /= sum;
            }
        }
    };

    /**
     * @brief Single HMM state with Gaussian mixture output distribution
     * 
     * Represents one state in the HTS-style 5-state left-to-right HMM
     * Each state models acoustic features using advanced Gaussian mixtures
     */
    struct HmmState {
        GaussianMixture output_distribution;  // GMM for emission probabilities
        StateTransition transition;           // Transition probabilities
        int state_id;                        // Unique state identifier
        
        HmmState() : state_id(-1) {}
        
        explicit HmmState(int id, int num_mixtures = 1, int feature_dim = 39)
            : output_distribution(num_mixtures, feature_dim), state_id(id) {
        }
        
        // Convenience methods
        size_t num_mixtures() const { return output_distribution.num_components(); }
        int feature_dimension() const { return output_distribution.dimension(); }
        
        // Probability calculations
        double log_emission_probability(const Eigen::VectorXd& observation) const {
            return output_distribution.log_likelihood(observation);
        }
        
        double emission_probability(const Eigen::VectorXd& observation) const {
            return output_distribution.likelihood(observation);
        }
        
        // Training support
        double train_emissions(const std::vector<Eigen::VectorXd>& observations, int max_iterations = 50) {
            return output_distribution.train_em(observations, max_iterations);
        }
        
        // Sample generation
        Eigen::VectorXd sample() const {
            return output_distribution.sample();
        }
    };

    /**
     * @brief Context-dependent feature vector for phoneme modeling
     * 
     * Defines the linguistic and acoustic context used for 
     * context-dependent HMM selection in speech synthesis
     */
    struct ContextFeature {
        // Phonetic context
        std::string current_phoneme;        // Central phoneme
        std::string left_phoneme;           // Previous phoneme  
        std::string right_phoneme;          // Next phoneme
        
        // Prosodic context
        int position_in_syllable;           // 1-based position in syllable
        int syllable_length;                // Total syllables in word
        int position_in_word;               // 1-based position in word
        int word_length;                    // Total words in phrase
        
        // Musical context (UTAU-specific)
        double pitch_cents;                 // Pitch in cents (relative to base)
        double note_duration_ms;            // Note duration in milliseconds
        std::string lyric;                  // Lyrics text
        
        // Timing context
        double tempo_bpm;                   // Tempo in beats per minute
        int beat_position;                  // Position within musical beat
        
        ContextFeature() 
            : position_in_syllable(1)
            , syllable_length(1)
            , position_in_word(1)
            , word_length(1)
            , pitch_cents(0.0)
            , note_duration_ms(500.0)
            , tempo_bpm(120.0)
            , beat_position(1) {}
    };

    /**
     * @brief Complete HMM model for a phoneme/context unit
     * 
     * Standard 5-state left-to-right HMM used in HTS synthesis
     * States typically represent: [entry, attack, steady, release, exit]
     */
    struct PhonemeHmm {
        std::vector<HmmState> states;       // Typically 5 states for HTS
        ContextFeature context;             // Context-dependent features
        std::string model_name;             // Unique identifier for this HMM
        
        static constexpr int DEFAULT_NUM_STATES = 5;
        
        PhonemeHmm() {
            initialize_default();
        }
        
        explicit PhonemeHmm(const ContextFeature& ctx, int num_states = DEFAULT_NUM_STATES)
            : context(ctx) {
            initialize_states(num_states);
            generate_model_name();
        }
        
        void initialize_default() {
            initialize_states(DEFAULT_NUM_STATES);
        }
        
        void initialize_states(int num_states) {
            states.clear();
            states.reserve(num_states);
            
            for (int i = 0; i < num_states; ++i) {
                states.emplace_back(i);
                
                // Configure transitions for left-to-right topology
                if (i == num_states - 1) {
                    // Final state - higher exit probability
                    states[i].transition.self_loop_prob = 0.3;
                    states[i].transition.next_state_prob = 0.0;
                    states[i].transition.exit_prob = 0.7;
                } else {
                    // Intermediate states
                    states[i].transition.self_loop_prob = 0.6;
                    states[i].transition.next_state_prob = 0.4;
                    states[i].transition.exit_prob = 0.0;
                }
            }
        }
        
        void generate_model_name() {
            model_name = context.left_phoneme + "-" + 
                        context.current_phoneme + "+" + 
                        context.right_phoneme;
        }
        
        size_t num_states() const { return states.size(); }
    };

} // namespace hmm
} // namespace nexussynth
#pragma once

#include <string>
#include <memory>
#include "hmm_structures.h"

namespace nexussynth {
    
    /**
     * @brief Main NexusSynth Engine class
     * 
     * This class provides the main interface for the NexusSynth vocal synthesis
     * resampler engine, implementing UTAU-compatible functionality with advanced
     * parametric synthesis capabilities.
     */
    class NexusSynthEngine {
    public:
        NexusSynthEngine();
        ~NexusSynthEngine();
        
        /**
         * @brief Initialize the synthesis engine
         * @param voice_bank_path Path to UTAU voice bank directory
         * @return true if initialization successful
         */
        bool initialize(const std::string& voice_bank_path);
        
        /**
         * @brief Process UTAU resampler command
         * @param input_wav Input wav file path
         * @param output_wav Output wav file path  
         * @param pitch_params Pitch modification parameters
         * @param flags UTAU expression flags (g, bre, bri, t, etc.)
         * @return true if synthesis successful
         */
        bool synthesize(const std::string& input_wav,
                       const std::string& output_wav,
                       const std::string& pitch_params,
                       const std::string& flags);
        
    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };
    
} // namespace nexussynth
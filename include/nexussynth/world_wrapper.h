#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

namespace nexussynth {

    /**
     * @brief Exception class for WORLD parameter extraction errors
     */
    class WorldExtractionError : public std::runtime_error {
    public:
        explicit WorldExtractionError(const std::string& message) 
            : std::runtime_error("WORLD Extraction Error: " + message) {}
    };

    /**
     * @brief Audio parameters extracted by WORLD vocoder
     */
    struct AudioParameters {
        std::vector<double> f0;                    // Fundamental frequency per frame
        std::vector<std::vector<double>> spectrum; // Spectral envelope per frame
        std::vector<std::vector<double>> aperiodicity; // Aperiodicity per frame
        
        double frame_period;     // Frame period in milliseconds
        int sample_rate;         // Sample rate in Hz
        int fft_size;           // FFT size used for analysis
        
        // Timing information
        std::vector<double> time_axis; // Time axis for each frame
        int length;                    // Number of frames
        
        AudioParameters() : frame_period(5.0), sample_rate(44100), fft_size(2048), length(0) {}
    };

    /**
     * @brief Configuration for WORLD analysis
     */
    struct WorldConfig {
        double frame_period;      // Frame period in milliseconds (default: 5.0ms)
        double f0_floor;         // F0 lower bound in Hz (default: 71.0)
        double f0_ceil;          // F0 upper bound in Hz (default: 800.0)
        double allowed_range;    // Allowed range for F0 estimation (default: 0.1)
        
        // CheapTrick parameters
        double q1;               // Q1 parameter for CheapTrick (default: -0.15)
        
        // D4C parameters
        double threshold;        // Threshold for D4C (default: 0.85)
        
        WorldConfig() 
            : frame_period(5.0), f0_floor(71.0), f0_ceil(800.0), 
              allowed_range(0.1), q1(-0.15), threshold(0.85) {}
    };

    /**
     * @brief Base class for WORLD algorithm wrappers
     */
    class WorldWrapper {
    public:
        WorldWrapper(int sample_rate, const WorldConfig& config = WorldConfig());
        virtual ~WorldWrapper() = default;

        /**
         * @brief Extract all parameters from audio data
         * @param audio_data Raw audio samples
         * @param length Number of samples
         * @return Complete audio parameters
         */
        virtual AudioParameters extractParameters(const double* audio_data, int length) = 0;

        /**
         * @brief Get the sample rate
         */
        int getSampleRate() const { return sample_rate_; }

        /**
         * @brief Get the configuration
         */
        const WorldConfig& getConfig() const { return config_; }

    protected:
        int sample_rate_;
        WorldConfig config_;
        
        // Helper methods for memory management
        void allocateMemory(int length);
        void deallocateMemory();
        
        // Common buffers
        double* time_axis_;
        double* f0_data_;
        double** spectrum_data_;
        double** aperiodicity_data_;
        int allocated_length_;
        int fft_size_;

    private:
        bool memory_allocated_;
    };

    /**
     * @brief DIO F0 estimation wrapper
     */
    class DioWrapper : public WorldWrapper {
    public:
        DioWrapper(int sample_rate, const WorldConfig& config = WorldConfig());
        ~DioWrapper() override;

        /**
         * @brief Extract F0 using DIO algorithm
         * @param audio_data Raw audio samples
         * @param length Number of samples
         * @return F0 contour
         */
        std::vector<double> extractF0(const double* audio_data, int length);

        AudioParameters extractParameters(const double* audio_data, int length) override;

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

    /**
     * @brief CheapTrick spectral envelope estimation wrapper
     */
    class CheapTrickWrapper : public WorldWrapper {
    public:
        CheapTrickWrapper(int sample_rate, const WorldConfig& config = WorldConfig());
        ~CheapTrickWrapper() override;

        /**
         * @brief Extract spectral envelope using CheapTrick
         * @param audio_data Raw audio samples
         * @param length Number of samples
         * @param f0_data F0 contour (from DIO)
         * @param f0_length Number of F0 frames
         * @return Spectral envelope per frame
         */
        std::vector<std::vector<double>> extractSpectrum(
            const double* audio_data, int length,
            const double* f0_data, int f0_length);

        AudioParameters extractParameters(const double* audio_data, int length) override;

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

    /**
     * @brief D4C aperiodicity estimation wrapper
     */
    class D4CWrapper : public WorldWrapper {
    public:
        D4CWrapper(int sample_rate, const WorldConfig& config = WorldConfig());
        ~D4CWrapper() override;

        /**
         * @brief Extract aperiodicity using D4C
         * @param audio_data Raw audio samples
         * @param length Number of samples
         * @param f0_data F0 contour (from DIO)
         * @param f0_length Number of F0 frames
         * @return Aperiodicity per frame
         */
        std::vector<std::vector<double>> extractAperiodicity(
            const double* audio_data, int length,
            const double* f0_data, int f0_length);

        AudioParameters extractParameters(const double* audio_data, int length) override;

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

    /**
     * @brief Complete WORLD parameter extractor
     */
    class WorldParameterExtractor {
    public:
        WorldParameterExtractor(int sample_rate, const WorldConfig& config = WorldConfig());
        ~WorldParameterExtractor();

        /**
         * @brief Extract all WORLD parameters from audio
         * @param audio_data Raw audio samples
         * @param length Number of samples
         * @return Complete audio parameters
         */
        AudioParameters extractAll(const double* audio_data, int length);

        /**
         * @brief Extract parameters from WAV file
         * @param wav_filename Path to WAV file
         * @return Complete audio parameters
         */
        AudioParameters extractFromFile(const std::string& wav_filename);

        /**
         * @brief Save parameters to JSON file
         * @param parameters Audio parameters to save
         * @param json_filename Output JSON file path
         * @return true if successful
         */
        bool saveToJson(const AudioParameters& parameters, const std::string& json_filename);

        /**
         * @brief Load parameters from JSON file
         * @param json_filename Input JSON file path
         * @return Loaded audio parameters
         */
        AudioParameters loadFromJson(const std::string& json_filename);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

} // namespace nexussynth
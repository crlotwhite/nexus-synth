#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <cstdint>

namespace nexussynth {

    /**
     * @brief Exception class for audio processing errors
     */
    class AudioError : public std::runtime_error {
    public:
        explicit AudioError(const std::string& message) 
            : std::runtime_error("Audio Error: " + message) {}
    };

    /**
     * @brief Audio format information
     */
    struct AudioFormat {
        uint32_t sample_rate;      // Sample rate in Hz
        uint16_t channels;         // Number of channels (1=mono, 2=stereo)
        uint16_t bits_per_sample;  // Bits per sample (8, 16, 24, 32)
        uint32_t length_samples;   // Total number of samples per channel
        double duration;           // Duration in seconds
        
        AudioFormat() 
            : sample_rate(44100), channels(1), bits_per_sample(16), 
              length_samples(0), duration(0.0) {}
              
        bool isValid() const {
            return sample_rate > 0 && channels > 0 && 
                   (bits_per_sample == 8 || bits_per_sample == 16 || 
                    bits_per_sample == 24 || bits_per_sample == 32);
        }
    };

    /**
     * @brief Audio buffer with automatic memory management
     */
    class AudioBuffer {
    public:
        AudioBuffer();
        AudioBuffer(uint32_t sample_rate, uint16_t channels, uint32_t length_samples);
        ~AudioBuffer() = default;

        // Copy and move constructors/operators
        AudioBuffer(const AudioBuffer& other);
        AudioBuffer& operator=(const AudioBuffer& other);
        AudioBuffer(AudioBuffer&& other) noexcept;
        AudioBuffer& operator=(AudioBuffer&& other) noexcept;

        /**
         * @brief Initialize buffer with given format
         */
        void initialize(uint32_t sample_rate, uint16_t channels, uint32_t length_samples);

        /**
         * @brief Resize buffer while preserving existing data
         */
        void resize(uint32_t new_length_samples);

        /**
         * @brief Clear buffer and reset to default state
         */
        void clear();

        /**
         * @brief Get audio data for a specific channel
         * @param channel Channel index (0-based)
         * @return Pointer to channel data
         */
        double* getChannelData(uint16_t channel);
        const double* getChannelData(uint16_t channel) const;

        /**
         * @brief Get interleaved audio data
         * @return Pointer to interleaved audio data
         */
        const std::vector<double>& getData() const { return data_; }
        std::vector<double>& getData() { return data_; }

        /**
         * @brief Convert to mono by averaging channels
         */
        void convertToMono();

        /**
         * @brief Resample to target sample rate using linear interpolation
         */
        void resample(uint32_t target_sample_rate);

        /**
         * @brief Normalize audio data to range [-1.0, 1.0]
         */
        void normalize();

        // Getters
        const AudioFormat& getFormat() const { return format_; }
        uint32_t getSampleRate() const { return format_.sample_rate; }
        uint16_t getChannels() const { return format_.channels; }
        uint32_t getLengthSamples() const { return format_.length_samples; }
        double getDuration() const { return format_.duration; }
        bool isEmpty() const { return data_.empty(); }

    private:
        AudioFormat format_;
        std::vector<double> data_;  // Interleaved audio data
        
        void updateDuration();
    };

    /**
     * @brief WAV file loader with support for various formats
     */
    class WavLoader {
    public:
        WavLoader() = default;
        ~WavLoader() = default;

        /**
         * @brief Load WAV file into audio buffer
         * @param filename Path to WAV file
         * @return Audio buffer with loaded data
         */
        AudioBuffer loadFile(const std::string& filename);

        /**
         * @brief Get WAV file format information without loading data
         * @param filename Path to WAV file
         * @return Audio format information
         */
        AudioFormat getFileInfo(const std::string& filename);

        /**
         * @brief Save audio buffer to WAV file
         * @param buffer Audio buffer to save
         * @param filename Output WAV file path
         * @param bits_per_sample Output bit depth (16, 24, or 32)
         */
        void saveFile(const AudioBuffer& buffer, const std::string& filename, 
                     uint16_t bits_per_sample = 16);

        /**
         * @brief Check if file is a valid WAV file
         * @param filename Path to file
         * @return true if valid WAV file
         */
        bool isValidWavFile(const std::string& filename);

    private:
        struct WavHeader {
            char riff_id[4];           // "RIFF"
            uint32_t file_size;        // File size - 8
            char wave_id[4];           // "WAVE"
            char fmt_id[4];            // "fmt "
            uint32_t fmt_size;         // Format chunk size
            uint16_t audio_format;     // Audio format (1 = PCM)
            uint16_t num_channels;     // Number of channels
            uint32_t sample_rate;      // Sample rate
            uint32_t byte_rate;        // Byte rate
            uint16_t block_align;      // Block align
            uint16_t bits_per_sample;  // Bits per sample
            char data_id[4];           // "data"
            uint32_t data_size;        // Data size
        };

        bool readWavHeader(std::ifstream& file, WavHeader& header);
        void writeWavHeader(std::ofstream& file, const AudioFormat& format, 
                           uint16_t bits_per_sample);
        std::vector<double> readAudioData(std::ifstream& file, const WavHeader& header);
        void writeAudioData(std::ofstream& file, const AudioBuffer& buffer, 
                           uint16_t bits_per_sample);
        
        // Helper functions for different bit depths
        double convertSampleToDouble(const void* sample, uint16_t bits_per_sample);
        void convertDoubleToSample(double value, void* sample, uint16_t bits_per_sample);
    };

    /**
     * @brief Audio buffer pool for efficient memory management
     */
    class AudioBufferPool {
    public:
        AudioBufferPool(size_t initial_pool_size = 4);
        ~AudioBufferPool() = default;

        /**
         * @brief Get a buffer from the pool or create new one
         * @param sample_rate Required sample rate
         * @param channels Required number of channels  
         * @param length_samples Required length in samples
         * @return Shared pointer to audio buffer
         */
        std::shared_ptr<AudioBuffer> getBuffer(uint32_t sample_rate, uint16_t channels, 
                                              uint32_t length_samples);

        /**
         * @brief Return buffer to pool for reuse
         * @param buffer Buffer to return
         */
        void returnBuffer(std::shared_ptr<AudioBuffer> buffer);

        /**
         * @brief Clear all buffers from pool
         */
        void clear();

        /**
         * @brief Get current pool size
         */
        size_t getPoolSize() const { return available_buffers_.size(); }

        /**
         * @brief Get number of buffers currently in use
         */
        size_t getInUseCount() const { return buffers_in_use_; }

    private:
        std::vector<std::shared_ptr<AudioBuffer>> available_buffers_;
        size_t buffers_in_use_;
        size_t max_pool_size_;
        
        bool isBufferCompatible(const AudioBuffer& buffer, uint32_t sample_rate, 
                               uint16_t channels, uint32_t length_samples);
    };

} // namespace nexussynth
#include "nexussynth/audio_utils.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace nexussynth {

    // AudioBuffer implementation
    AudioBuffer::AudioBuffer() {
        format_ = AudioFormat();
    }

    AudioBuffer::AudioBuffer(uint32_t sample_rate, uint16_t channels, uint32_t length_samples) {
        initialize(sample_rate, channels, length_samples);
    }

    AudioBuffer::AudioBuffer(const AudioBuffer& other) 
        : format_(other.format_), data_(other.data_) {
    }

    AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) {
        if (this != &other) {
            format_ = other.format_;
            data_ = other.data_;
        }
        return *this;
    }

    AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept 
        : format_(std::move(other.format_)), data_(std::move(other.data_)) {
    }

    AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
        if (this != &other) {
            format_ = std::move(other.format_);
            data_ = std::move(other.data_);
        }
        return *this;
    }

    void AudioBuffer::initialize(uint32_t sample_rate, uint16_t channels, uint32_t length_samples) {
        format_.sample_rate = sample_rate;
        format_.channels = channels;
        format_.length_samples = length_samples;
        format_.bits_per_sample = 32; // Internal format is always 32-bit float (double)
        
        if (!format_.isValid()) {
            throw AudioError("Invalid audio format parameters");
        }
        
        data_.resize(length_samples * channels);
        std::fill(data_.begin(), data_.end(), 0.0);
        
        updateDuration();
        
        std::cout << "AudioBuffer initialized: " << sample_rate << "Hz, " 
                  << channels << " channels, " << length_samples << " samples" << std::endl;
    }

    void AudioBuffer::resize(uint32_t new_length_samples) {
        uint32_t old_length = format_.length_samples;
        format_.length_samples = new_length_samples;
        
        data_.resize(new_length_samples * format_.channels);
        
        // Zero out new samples if buffer grew
        if (new_length_samples > old_length) {
            size_t new_samples_start = old_length * format_.channels;
            std::fill(data_.begin() + new_samples_start, data_.end(), 0.0);
        }
        
        updateDuration();
    }

    void AudioBuffer::clear() {
        data_.clear();
        format_ = AudioFormat();
    }

    double* AudioBuffer::getChannelData(uint16_t channel) {
        if (channel >= format_.channels) {
            throw AudioError("Channel index out of range: " + std::to_string(channel));
        }
        
        if (format_.channels == 1) {
            return data_.data();
        } else {
            // For interleaved data, this is more complex - for now throw error
            throw AudioError("getChannelData not implemented for interleaved multi-channel data");
        }
    }

    const double* AudioBuffer::getChannelData(uint16_t channel) const {
        if (channel >= format_.channels) {
            throw AudioError("Channel index out of range: " + std::to_string(channel));
        }
        
        if (format_.channels == 1) {
            return data_.data();
        } else {
            throw AudioError("getChannelData not implemented for interleaved multi-channel data");
        }
    }

    void AudioBuffer::convertToMono() {
        if (format_.channels == 1) {
            return; // Already mono
        }
        
        std::vector<double> mono_data(format_.length_samples);
        
        for (uint32_t i = 0; i < format_.length_samples; ++i) {
            double sum = 0.0;
            for (uint16_t ch = 0; ch < format_.channels; ++ch) {
                sum += data_[i * format_.channels + ch];
            }
            mono_data[i] = sum / format_.channels;
        }
        
        data_ = std::move(mono_data);
        format_.channels = 1;
        
        std::cout << "Converted to mono: " << format_.length_samples << " samples" << std::endl;
    }

    void AudioBuffer::resample(uint32_t target_sample_rate) {
        if (target_sample_rate == format_.sample_rate) {
            return; // No resampling needed
        }
        
        double ratio = static_cast<double>(target_sample_rate) / format_.sample_rate;
        uint32_t new_length = static_cast<uint32_t>(format_.length_samples * ratio);
        
        std::vector<double> resampled_data(new_length * format_.channels);
        
        // Simple linear interpolation resampling
        for (uint32_t i = 0; i < new_length; ++i) {
            double src_index = i / ratio;
            uint32_t src_index_int = static_cast<uint32_t>(src_index);
            double frac = src_index - src_index_int;
            
            for (uint16_t ch = 0; ch < format_.channels; ++ch) {
                if (src_index_int >= format_.length_samples - 1) {
                    resampled_data[i * format_.channels + ch] = 
                        data_[(format_.length_samples - 1) * format_.channels + ch];
                } else {
                    double sample1 = data_[src_index_int * format_.channels + ch];
                    double sample2 = data_[(src_index_int + 1) * format_.channels + ch];
                    resampled_data[i * format_.channels + ch] = 
                        sample1 + frac * (sample2 - sample1);
                }
            }
        }
        
        data_ = std::move(resampled_data);
        format_.sample_rate = target_sample_rate;
        format_.length_samples = new_length;
        updateDuration();
        
        std::cout << "Resampled to " << target_sample_rate << "Hz: " 
                  << new_length << " samples" << std::endl;
    }

    void AudioBuffer::normalize() {
        if (data_.empty()) return;
        
        // Find peak amplitude
        double peak = 0.0;
        for (double sample : data_) {
            peak = std::max(peak, std::abs(sample));
        }
        
        if (peak > 0.0) {
            double scale = 1.0 / peak;
            for (double& sample : data_) {
                sample *= scale;
            }
            std::cout << "Normalized audio: peak was " << peak << std::endl;
        }
    }

    void AudioBuffer::updateDuration() {
        format_.duration = static_cast<double>(format_.length_samples) / format_.sample_rate;
    }

    // WavLoader implementation
    AudioBuffer WavLoader::loadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw AudioError("Cannot open WAV file: " + filename);
        }
        
        WavHeader header;
        if (!readWavHeader(file, header)) {
            throw AudioError("Invalid WAV file format: " + filename);
        }
        
        std::cout << "Loading WAV file: " << filename << std::endl;
        std::cout << "  Format: " << header.sample_rate << "Hz, " 
                  << header.num_channels << " channels, " 
                  << header.bits_per_sample << " bits" << std::endl;
        
        // Read audio data
        std::vector<double> audio_data = readAudioData(file, header);
        
        // Create audio buffer
        uint32_t length_samples = header.data_size / (header.num_channels * header.bits_per_sample / 8);
        AudioBuffer buffer(header.sample_rate, header.num_channels, length_samples);
        
        buffer.getData() = std::move(audio_data);
        
        std::cout << "Loaded " << length_samples << " samples (" 
                  << buffer.getDuration() << " seconds)" << std::endl;
        
        return buffer;
    }

    AudioFormat WavLoader::getFileInfo(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw AudioError("Cannot open WAV file: " + filename);
        }
        
        WavHeader header;
        if (!readWavHeader(file, header)) {
            throw AudioError("Invalid WAV file format: " + filename);
        }
        
        AudioFormat format;
        format.sample_rate = header.sample_rate;
        format.channels = header.num_channels;
        format.bits_per_sample = header.bits_per_sample;
        format.length_samples = header.data_size / (header.num_channels * header.bits_per_sample / 8);
        format.duration = static_cast<double>(format.length_samples) / format.sample_rate;
        
        return format;
    }

    void WavLoader::saveFile(const AudioBuffer& buffer, const std::string& filename, 
                            uint16_t bits_per_sample) {
        if (buffer.isEmpty()) {
            throw AudioError("Cannot save empty audio buffer");
        }
        
        if (bits_per_sample != 16 && bits_per_sample != 24 && bits_per_sample != 32) {
            throw AudioError("Unsupported bit depth: " + std::to_string(bits_per_sample));
        }
        
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw AudioError("Cannot create WAV file: " + filename);
        }
        
        writeWavHeader(file, buffer.getFormat(), bits_per_sample);
        writeAudioData(file, buffer, bits_per_sample);
        
        std::cout << "Saved WAV file: " << filename << " (" 
                  << bits_per_sample << " bits)" << std::endl;
    }

    bool WavLoader::isValidWavFile(const std::string& filename) {
        try {
            getFileInfo(filename);
            return true;
        } catch (const AudioError&) {
            return false;
        }
    }

    bool WavLoader::readWavHeader(std::ifstream& file, WavHeader& header) {
        file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
        
        // Check RIFF and WAVE identifiers
        if (strncmp(header.riff_id, "RIFF", 4) != 0 || 
            strncmp(header.wave_id, "WAVE", 4) != 0) {
            return false;
        }
        
        // Handle different chunk orders and sizes
        if (strncmp(header.fmt_id, "fmt ", 4) != 0) {
            // Try to find fmt chunk
            file.seekg(12, std::ios::beg); // Start after RIFF header
            char chunk_id[4];
            uint32_t chunk_size;
            
            while (file.read(chunk_id, 4) && file.read(reinterpret_cast<char*>(&chunk_size), 4)) {
                if (strncmp(chunk_id, "fmt ", 4) == 0) {
                    // Read format chunk
                    file.read(reinterpret_cast<char*>(&header.audio_format), chunk_size);
                    break;
                }
                file.seekg(chunk_size, std::ios::cur); // Skip this chunk
            }
        }
        
        // Find data chunk
        if (strncmp(header.data_id, "data", 4) != 0) {
            file.seekg(12, std::ios::beg); // Start after RIFF header
            char chunk_id[4];
            uint32_t chunk_size;
            
            while (file.read(chunk_id, 4) && file.read(reinterpret_cast<char*>(&chunk_size), 4)) {
                if (strncmp(chunk_id, "data", 4) == 0) {
                    header.data_size = chunk_size;
                    strncpy(header.data_id, "data", 4);
                    break;
                }
                file.seekg(chunk_size, std::ios::cur); // Skip this chunk
            }
        }
        
        // Validate format
        return header.audio_format == 1 && // PCM
               header.num_channels > 0 && header.num_channels <= 2 &&
               header.sample_rate > 0 &&
               (header.bits_per_sample == 8 || header.bits_per_sample == 16 || 
                header.bits_per_sample == 24 || header.bits_per_sample == 32);
    }

    void WavLoader::writeWavHeader(std::ofstream& file, const AudioFormat& format, 
                                  uint16_t bits_per_sample) {
        WavHeader header = {};
        
        uint32_t data_size = format.length_samples * format.channels * (bits_per_sample / 8);
        
        strncpy(header.riff_id, "RIFF", 4);
        header.file_size = 36 + data_size;
        strncpy(header.wave_id, "WAVE", 4);
        strncpy(header.fmt_id, "fmt ", 4);
        header.fmt_size = 16;
        header.audio_format = 1; // PCM
        header.num_channels = format.channels;
        header.sample_rate = format.sample_rate;
        header.bits_per_sample = bits_per_sample;
        header.block_align = format.channels * (bits_per_sample / 8);
        header.byte_rate = format.sample_rate * header.block_align;
        strncpy(header.data_id, "data", 4);
        header.data_size = data_size;
        
        file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    }

    std::vector<double> WavLoader::readAudioData(std::ifstream& file, const WavHeader& header) {
        uint32_t num_samples = header.data_size / (header.bits_per_sample / 8);
        std::vector<double> audio_data(num_samples);
        
        switch (header.bits_per_sample) {
            case 8: {
                std::vector<uint8_t> raw_data(num_samples);
                file.read(reinterpret_cast<char*>(raw_data.data()), num_samples);
                for (size_t i = 0; i < num_samples; ++i) {
                    audio_data[i] = (raw_data[i] - 128.0) / 128.0;
                }
                break;
            }
            case 16: {
                std::vector<int16_t> raw_data(num_samples);
                file.read(reinterpret_cast<char*>(raw_data.data()), num_samples * 2);
                for (size_t i = 0; i < num_samples; ++i) {
                    audio_data[i] = raw_data[i] / 32768.0;
                }
                break;
            }
            case 24: {
                std::vector<uint8_t> raw_data(num_samples * 3);
                file.read(reinterpret_cast<char*>(raw_data.data()), num_samples * 3);
                for (size_t i = 0; i < num_samples; ++i) {
                    int32_t sample = (raw_data[i*3] << 8) | (raw_data[i*3+1] << 16) | (raw_data[i*3+2] << 24);
                    sample >>= 8; // Sign extend
                    audio_data[i] = sample / 8388608.0;
                }
                break;
            }
            case 32: {
                std::vector<int32_t> raw_data(num_samples);
                file.read(reinterpret_cast<char*>(raw_data.data()), num_samples * 4);
                for (size_t i = 0; i < num_samples; ++i) {
                    audio_data[i] = raw_data[i] / 2147483648.0;
                }
                break;
            }
            default:
                throw AudioError("Unsupported bit depth: " + std::to_string(header.bits_per_sample));
        }
        
        return audio_data;
    }

    void WavLoader::writeAudioData(std::ofstream& file, const AudioBuffer& buffer, 
                                  uint16_t bits_per_sample) {
        const std::vector<double>& data = buffer.getData();
        
        switch (bits_per_sample) {
            case 16: {
                for (double sample : data) {
                    int16_t int_sample = static_cast<int16_t>(
                        std::clamp(sample * 32767.0, -32768.0, 32767.0));
                    file.write(reinterpret_cast<const char*>(&int_sample), 2);
                }
                break;
            }
            case 24: {
                for (double sample : data) {
                    int32_t int_sample = static_cast<int32_t>(
                        std::clamp(sample * 8388607.0, -8388608.0, 8388607.0));
                    uint8_t bytes[3] = {
                        static_cast<uint8_t>(int_sample & 0xFF),
                        static_cast<uint8_t>((int_sample >> 8) & 0xFF),
                        static_cast<uint8_t>((int_sample >> 16) & 0xFF)
                    };
                    file.write(reinterpret_cast<const char*>(bytes), 3);
                }
                break;
            }
            case 32: {
                for (double sample : data) {
                    int32_t int_sample = static_cast<int32_t>(
                        std::clamp(sample * 2147483647.0, -2147483648.0, 2147483647.0));
                    file.write(reinterpret_cast<const char*>(&int_sample), 4);
                }
                break;
            }
            default:
                throw AudioError("Unsupported output bit depth: " + std::to_string(bits_per_sample));
        }
    }

    // AudioBufferPool implementation
    AudioBufferPool::AudioBufferPool(size_t initial_pool_size) 
        : buffers_in_use_(0), max_pool_size_(initial_pool_size * 2) {
        available_buffers_.reserve(initial_pool_size);
    }

    std::shared_ptr<AudioBuffer> AudioBufferPool::getBuffer(uint32_t sample_rate, 
                                                           uint16_t channels, 
                                                           uint32_t length_samples) {
        // Look for compatible buffer in pool
        for (auto it = available_buffers_.begin(); it != available_buffers_.end(); ++it) {
            if (isBufferCompatible(**it, sample_rate, channels, length_samples)) {
                auto buffer = *it;
                available_buffers_.erase(it);
                
                // Reinitialize buffer if needed
                if (buffer->getSampleRate() != sample_rate || 
                    buffer->getChannels() != channels ||
                    buffer->getLengthSamples() != length_samples) {
                    buffer->initialize(sample_rate, channels, length_samples);
                }
                
                buffers_in_use_++;
                return buffer;
            }
        }
        
        // No compatible buffer found, create new one
        auto buffer = std::make_shared<AudioBuffer>(sample_rate, channels, length_samples);
        buffers_in_use_++;
        return buffer;
    }

    void AudioBufferPool::returnBuffer(std::shared_ptr<AudioBuffer> buffer) {
        if (!buffer) return;
        
        buffers_in_use_--;
        
        if (available_buffers_.size() < max_pool_size_) {
            buffer->clear();
            available_buffers_.push_back(buffer);
        }
        // If pool is full, just let the buffer be destroyed
    }

    void AudioBufferPool::clear() {
        available_buffers_.clear();
        buffers_in_use_ = 0;
    }

    bool AudioBufferPool::isBufferCompatible(const AudioBuffer& buffer, uint32_t /* sample_rate */, 
                                           uint16_t channels, uint32_t /* length_samples */) {
        // Buffer is compatible if it can be reused with minimal reinitialization
        // We allow different sample rates and lengths, but same channel count is preferred
        return buffer.getChannels() == channels;
    }

} // namespace nexussynth
#include "nexussynth/world_wrapper.h"
#include "nexussynth/audio_utils.h"
#include "world/dio.h"
#include "world/cheaptrick.h"
#include "world/d4c.h"
#include <cJSON.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nexussynth {

    class WorldParameterExtractor::Impl {
    public:
        Impl(int sample_rate, const WorldConfig& config)
            : sample_rate_(sample_rate), config_(config),
              dio_wrapper_(sample_rate, config),
              cheaptrick_wrapper_(sample_rate, config),
              d4c_wrapper_(sample_rate, config) {
        }

        AudioParameters extractAll(const double* audio_data, int length) {
            try {
                AudioParameters params;
                params.sample_rate = sample_rate_;
                params.frame_period = config_.frame_period;
                
                std::cout << "Extracting WORLD parameters from " << length << " samples..." << std::endl;
                
                // Step 1: Extract F0 using DIO
                std::cout << "Extracting F0 using DIO..." << std::endl;
                params.f0 = dio_wrapper_.extractF0(audio_data, length);
                params.length = params.f0.size();
                
                // Generate time axis
                params.time_axis.resize(params.length);
                for (int i = 0; i < params.length; ++i) {
                    params.time_axis[i] = i * config_.frame_period / 1000.0;
                }
                
                std::cout << "Extracted " << params.length << " F0 frames" << std::endl;
                
                // Step 2: Extract spectral envelope using CheapTrick
                std::cout << "Extracting spectral envelope using CheapTrick..." << std::endl;
                params.spectrum = cheaptrick_wrapper_.extractSpectrum(
                    audio_data, length, params.f0.data(), params.length);
                
                if (!params.spectrum.empty()) {
                    params.fft_size = params.spectrum[0].size() * 2 - 2;
                    std::cout << "Extracted spectrum with FFT size: " << params.fft_size << std::endl;
                }
                
                // Step 3: Extract aperiodicity using D4C
                std::cout << "Extracting aperiodicity using D4C..." << std::endl;
                params.aperiodicity = d4c_wrapper_.extractAperiodicity(
                    audio_data, length, params.f0.data(), params.length);
                
                std::cout << "WORLD parameter extraction completed successfully!" << std::endl;
                
                return params;
                
            } catch (const std::exception& e) {
                throw WorldExtractionError("Failed to extract parameters: " + std::string(e.what()));
            }
        }

        AudioParameters extractFromFile(const std::string& wav_filename) {
            try {
                // Load WAV file using the new audio utilities
                WavLoader loader;
                AudioBuffer buffer = loader.loadFile(wav_filename);
                
                // Convert to mono if stereo (WORLD works with mono audio)
                if (buffer.getChannels() > 1) {
                    std::cout << "Converting stereo to mono for WORLD analysis..." << std::endl;
                    buffer.convertToMono();
                }
                
                // Resample if needed to match extractor's sample rate
                if (buffer.getSampleRate() != static_cast<uint32_t>(sample_rate_)) {
                    std::cout << "Resampling from " << buffer.getSampleRate() 
                              << "Hz to " << sample_rate_ << "Hz..." << std::endl;
                    buffer.resample(static_cast<uint32_t>(sample_rate_));
                }
                
                // Normalize audio for consistent processing
                buffer.normalize();
                
                // Extract parameters from the loaded audio
                const std::vector<double>& audio_data = buffer.getData();
                return extractAll(audio_data.data(), buffer.getLengthSamples());
                
            } catch (const AudioError& e) {
                throw WorldExtractionError("Failed to load WAV file: " + std::string(e.what()));
            }
        }

        bool saveToJson(const AudioParameters& parameters, const std::string& json_filename) {
            try {
                cJSON* root = cJSON_CreateObject();
                
                // Add metadata
                cJSON_AddNumberToObject(root, "sample_rate", parameters.sample_rate);
                cJSON_AddNumberToObject(root, "frame_period", parameters.frame_period);
                cJSON_AddNumberToObject(root, "fft_size", parameters.fft_size);
                cJSON_AddNumberToObject(root, "length", parameters.length);
                
                // Add time axis
                cJSON* time_array = cJSON_CreateArray();
                for (double time : parameters.time_axis) {
                    cJSON_AddItemToArray(time_array, cJSON_CreateNumber(time));
                }
                cJSON_AddItemToObject(root, "time_axis", time_array);
                
                // Add F0 data
                cJSON* f0_array = cJSON_CreateArray();
                for (double f0 : parameters.f0) {
                    cJSON_AddItemToArray(f0_array, cJSON_CreateNumber(f0));
                }
                cJSON_AddItemToObject(root, "f0", f0_array);
                
                // Add spectrum data (simplified - store only first few frames for size)
                cJSON* spectrum_array = cJSON_CreateArray();
                int max_frames = std::min(10, (int)parameters.spectrum.size()); // Limit for file size
                for (int i = 0; i < max_frames; ++i) {
                    cJSON* frame_array = cJSON_CreateArray();
                    for (double value : parameters.spectrum[i]) {
                        cJSON_AddItemToArray(frame_array, cJSON_CreateNumber(value));
                    }
                    cJSON_AddItemToArray(spectrum_array, frame_array);
                }
                cJSON_AddItemToObject(root, "spectrum_sample", spectrum_array);
                
                // Add aperiodicity data (simplified)
                cJSON* aperiodicity_array = cJSON_CreateArray();
                for (int i = 0; i < max_frames; ++i) {
                    cJSON* frame_array = cJSON_CreateArray();
                    for (double value : parameters.aperiodicity[i]) {
                        cJSON_AddItemToArray(frame_array, cJSON_CreateNumber(value));
                    }
                    cJSON_AddItemToArray(aperiodicity_array, frame_array);
                }
                cJSON_AddItemToObject(root, "aperiodicity_sample", aperiodicity_array);
                
                // Write to file
                char* json_string = cJSON_Print(root);
                std::ofstream file(json_filename);
                if (file.is_open()) {
                    file << json_string;
                    file.close();
                    
                    std::cout << "Parameters saved to: " << json_filename << std::endl;
                    free(json_string);
                    cJSON_Delete(root);
                    return true;
                } else {
                    free(json_string);
                    cJSON_Delete(root);
                    throw WorldExtractionError("Could not open file for writing: " + json_filename);
                }
                
            } catch (const std::exception& e) {
                throw WorldExtractionError("Failed to save JSON: " + std::string(e.what()));
            }
        }

        AudioParameters loadFromJson(const std::string& json_filename) {
            try {
                std::ifstream file(json_filename);
                if (!file.is_open()) {
                    throw WorldExtractionError("Could not open file for reading: " + json_filename);
                }
                
                std::string json_content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                file.close();
                
                cJSON* root = cJSON_Parse(json_content.c_str());
                if (!root) {
                    throw WorldExtractionError("Invalid JSON format");
                }
                
                AudioParameters params;
                
                // Load metadata
                cJSON* item = cJSON_GetObjectItem(root, "sample_rate");
                if (item) params.sample_rate = item->valueint;
                
                item = cJSON_GetObjectItem(root, "frame_period");
                if (item) params.frame_period = item->valuedouble;
                
                item = cJSON_GetObjectItem(root, "fft_size");
                if (item) params.fft_size = item->valueint;
                
                item = cJSON_GetObjectItem(root, "length");
                if (item) params.length = item->valueint;
                
                // Load F0 data
                cJSON* f0_array = cJSON_GetObjectItem(root, "f0");
                if (f0_array) {
                    int f0_size = cJSON_GetArraySize(f0_array);
                    params.f0.resize(f0_size);
                    for (int i = 0; i < f0_size; ++i) {
                        cJSON* f0_item = cJSON_GetArrayItem(f0_array, i);
                        if (f0_item) params.f0[i] = f0_item->valuedouble;
                    }
                }
                
                // Load time axis
                cJSON* time_array = cJSON_GetObjectItem(root, "time_axis");
                if (time_array) {
                    int time_size = cJSON_GetArraySize(time_array);
                    params.time_axis.resize(time_size);
                    for (int i = 0; i < time_size; ++i) {
                        cJSON* time_item = cJSON_GetArrayItem(time_array, i);
                        if (time_item) params.time_axis[i] = time_item->valuedouble;
                    }
                }
                
                cJSON_Delete(root);
                std::cout << "Parameters loaded from: " << json_filename << std::endl;
                
                return params;
                
            } catch (const std::exception& e) {
                throw WorldExtractionError("Failed to load JSON: " + std::string(e.what()));
            }
        }

    private:
        int sample_rate_;
        WorldConfig config_;
        DioWrapper dio_wrapper_;
        CheapTrickWrapper cheaptrick_wrapper_;
        D4CWrapper d4c_wrapper_;
    };

    WorldParameterExtractor::WorldParameterExtractor(int sample_rate, const WorldConfig& config)
        : pimpl_(std::make_unique<Impl>(sample_rate, config)) {
    }

    WorldParameterExtractor::~WorldParameterExtractor() = default;

    AudioParameters WorldParameterExtractor::extractAll(const double* audio_data, int length) {
        return pimpl_->extractAll(audio_data, length);
    }

    AudioParameters WorldParameterExtractor::extractFromFile(const std::string& wav_filename) {
        return pimpl_->extractFromFile(wav_filename);
    }

    bool WorldParameterExtractor::saveToJson(const AudioParameters& parameters, const std::string& json_filename) {
        return pimpl_->saveToJson(parameters, json_filename);
    }

    AudioParameters WorldParameterExtractor::loadFromJson(const std::string& json_filename) {
        return pimpl_->loadFromJson(json_filename);
    }

} // namespace nexussynth
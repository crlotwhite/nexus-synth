#include "nexussynth/world_wrapper.h"
#include "world/dio.h"
#include "world/harvest.h"
#include "world/cheaptrick.h"
#include "world/d4c.h"
#include "world/synthesis.h"
#include "cJSON.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

namespace nexussynth {

    // WorldWrapper base class implementation
    WorldWrapper::WorldWrapper(int sample_rate, const WorldConfig& config)
        : sample_rate_(sample_rate), config_(config), 
          time_axis_(nullptr), f0_data_(nullptr), 
          spectrum_data_(nullptr), aperiodicity_data_(nullptr),
          allocated_length_(0), fft_size_(0), memory_allocated_(false) {
        
        if (sample_rate <= 0) {
            throw WorldExtractionError("Invalid sample rate: " + std::to_string(sample_rate));
        }
        
        // Calculate FFT size based on sample rate
        fft_size_ = static_cast<int>(pow(2.0, 1.0 + static_cast<int>(log(3.0 * sample_rate / config.f0_floor + 1) / log(2.0))));
        
        std::cout << "WorldWrapper initialized with sample rate: " << sample_rate 
                  << ", FFT size: " << fft_size_ << std::endl;
    }

    void WorldWrapper::allocateMemory(int length) {
        if (memory_allocated_ && allocated_length_ >= length) {
            return; // Already allocated enough memory
        }
        
        deallocateMemory(); // Clean up previous allocation
        
        int f0_length = GetSamplesForDIO(sample_rate_, length, config_.frame_period);
        allocated_length_ = f0_length;
        
        // Allocate memory for time axis and F0
        time_axis_ = new double[f0_length];
        f0_data_ = new double[f0_length];
        
        // Allocate memory for spectrum
        spectrum_data_ = new double*[f0_length];
        for (int i = 0; i < f0_length; ++i) {
            spectrum_data_[i] = new double[fft_size_ / 2 + 1];
        }
        
        // Allocate memory for aperiodicity
        aperiodicity_data_ = new double*[f0_length];
        for (int i = 0; i < f0_length; ++i) {
            aperiodicity_data_[i] = new double[fft_size_ / 2 + 1];
        }
        
        memory_allocated_ = true;
        
        std::cout << "Allocated memory for " << f0_length << " frames, FFT size: " << fft_size_ << std::endl;
    }

    void WorldWrapper::deallocateMemory() {
        if (!memory_allocated_) return;
        
        delete[] time_axis_;
        delete[] f0_data_;
        
        if (spectrum_data_) {
            for (int i = 0; i < allocated_length_; ++i) {
                delete[] spectrum_data_[i];
            }
            delete[] spectrum_data_;
        }
        
        if (aperiodicity_data_) {
            for (int i = 0; i < allocated_length_; ++i) {
                delete[] aperiodicity_data_[i];
            }
            delete[] aperiodicity_data_;
        }
        
        time_axis_ = nullptr;
        f0_data_ = nullptr;
        spectrum_data_ = nullptr;
        aperiodicity_data_ = nullptr;
        allocated_length_ = 0;
        memory_allocated_ = false;
    }

    // DioWrapper implementation
    class DioWrapper::Impl {
    public:
        Impl(int sample_rate, const WorldConfig& config) 
            : sample_rate_(sample_rate), config_(config) {
            InitializeDioOption(&dio_option_);
            dio_option_.frame_period = config.frame_period;
            dio_option_.speed = 1;
            dio_option_.f0_floor = config.f0_floor;
            dio_option_.f0_ceil = config.f0_ceil;
            dio_option_.allowed_range = config.allowed_range;
        }
        
        std::vector<double> extractF0(const double* audio_data, int length, 
                                    double* time_axis, double* f0_data) {
            // Get F0 length
            int f0_length = GetSamplesForDIO(sample_rate_, length, config_.frame_period);
            
            // Generate time axis
            for (int i = 0; i < f0_length; ++i) {
                time_axis[i] = i * config_.frame_period / 1000.0;
            }
            
            // Extract F0 using DIO
            Dio(audio_data, length, sample_rate_, &dio_option_, time_axis, f0_data);
            
            // Convert to vector
            std::vector<double> result(f0_data, f0_data + f0_length);
            return result;
        }
        
    private:
        int sample_rate_;
        WorldConfig config_;
        DioOption dio_option_;
    };

    DioWrapper::DioWrapper(int sample_rate, const WorldConfig& config)
        : WorldWrapper(sample_rate, config), pimpl_(std::make_unique<Impl>(sample_rate, config)) {
    }

    DioWrapper::~DioWrapper() {
        deallocateMemory();
    }

    std::vector<double> DioWrapper::extractF0(const double* audio_data, int length) {
        allocateMemory(length);
        return pimpl_->extractF0(audio_data, length, time_axis_, f0_data_);
    }

    AudioParameters DioWrapper::extractParameters(const double* audio_data, int length) {
        AudioParameters params;
        params.sample_rate = sample_rate_;
        params.frame_period = config_.frame_period;
        params.fft_size = fft_size_;
        
        allocateMemory(length);
        
        // Extract F0
        params.f0 = pimpl_->extractF0(audio_data, length, time_axis_, f0_data_);
        params.length = params.f0.size();
        
        // Copy time axis
        params.time_axis.assign(time_axis_, time_axis_ + params.length);
        
        return params;
    }

    // CheapTrickWrapper implementation
    class CheapTrickWrapper::Impl {
    public:
        Impl(int sample_rate, const WorldConfig& config, int fft_size) 
            : sample_rate_(sample_rate), config_(config), fft_size_(fft_size) {
            InitializeCheapTrickOption(sample_rate, &cheaptrick_option_);
            cheaptrick_option_.q1 = config.q1;
            cheaptrick_option_.f0_floor = config.f0_floor;
        }
        
        std::vector<std::vector<double>> extractSpectrum(
                const double* audio_data, int length,
                const double* f0_data, const double* time_axis, int f0_length,
                double** spectrum_data) {
            
            // Extract spectral envelope using CheapTrick
            CheapTrick(audio_data, length, sample_rate_, time_axis, f0_data, f0_length,
                      &cheaptrick_option_, spectrum_data);
            
            // Convert to vector of vectors
            std::vector<std::vector<double>> result(f0_length);
            for (int i = 0; i < f0_length; ++i) {
                result[i].assign(spectrum_data[i], spectrum_data[i] + fft_size_ / 2 + 1);
            }
            
            return result;
        }
        
    private:
        int sample_rate_;
        int fft_size_;
        WorldConfig config_;
        CheapTrickOption cheaptrick_option_;
    };

    CheapTrickWrapper::CheapTrickWrapper(int sample_rate, const WorldConfig& config)
        : WorldWrapper(sample_rate, config), 
          pimpl_(std::make_unique<Impl>(sample_rate, config, fft_size_)) {
    }

    CheapTrickWrapper::~CheapTrickWrapper() {
        deallocateMemory();
    }

    std::vector<std::vector<double>> CheapTrickWrapper::extractSpectrum(
            const double* audio_data, int length,
            const double* f0_data, int f0_length) {
        
        allocateMemory(length);
        
        // Generate time axis
        for (int i = 0; i < f0_length; ++i) {
            time_axis_[i] = i * config_.frame_period / 1000.0;
        }
        
        return pimpl_->extractSpectrum(audio_data, length, f0_data, time_axis_, 
                                     f0_length, spectrum_data_);
    }

    AudioParameters CheapTrickWrapper::extractParameters(const double* audio_data, int length) {
        throw WorldExtractionError("CheapTrickWrapper::extractParameters requires F0 data");
    }

    // D4CWrapper implementation
    class D4CWrapper::Impl {
    public:
        Impl(int sample_rate, const WorldConfig& config, int fft_size) 
            : sample_rate_(sample_rate), config_(config), fft_size_(fft_size) {
            InitializeD4COption(&d4c_option_);
            d4c_option_.threshold = config.threshold;
        }
        
        std::vector<std::vector<double>> extractAperiodicity(
                const double* audio_data, int length,
                const double* f0_data, const double* time_axis, int f0_length,
                double** aperiodicity_data) {
            
            // Extract aperiodicity using D4C
            D4C(audio_data, length, sample_rate_, time_axis, f0_data, f0_length,
                fft_size_, &d4c_option_, aperiodicity_data);
            
            // Convert to vector of vectors
            std::vector<std::vector<double>> result(f0_length);
            for (int i = 0; i < f0_length; ++i) {
                result[i].assign(aperiodicity_data[i], aperiodicity_data[i] + fft_size_ / 2 + 1);
            }
            
            return result;
        }
        
    private:
        int sample_rate_;
        int fft_size_;
        WorldConfig config_;
        D4COption d4c_option_;
    };

    D4CWrapper::D4CWrapper(int sample_rate, const WorldConfig& config)
        : WorldWrapper(sample_rate, config), 
          pimpl_(std::make_unique<Impl>(sample_rate, config, fft_size_)) {
    }

    D4CWrapper::~D4CWrapper() {
        deallocateMemory();
    }

    std::vector<std::vector<double>> D4CWrapper::extractAperiodicity(
            const double* audio_data, int length,
            const double* f0_data, int f0_length) {
        
        allocateMemory(length);
        
        // Generate time axis
        for (int i = 0; i < f0_length; ++i) {
            time_axis_[i] = i * config_.frame_period / 1000.0;
        }
        
        return pimpl_->extractAperiodicity(audio_data, length, f0_data, time_axis_, 
                                         f0_length, aperiodicity_data_);
    }

    AudioParameters D4CWrapper::extractParameters(const double* audio_data, int length) {
        throw WorldExtractionError("D4CWrapper::extractParameters requires F0 data");
    }

} // namespace nexussynth
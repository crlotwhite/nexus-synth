#include "audio_comparator.h"
#include <gtest/gtest.h>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace nexussynth {
namespace integration_test {

AudioComparator::AudioComparator() = default;
AudioComparator::~AudioComparator() = default;

bool AudioComparator::load_audio_file(const std::string& file_path, AudioData& audio_data) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read WAV header (simplified)
    char header[44];
    file.read(header, 44);
    
    if (std::string(header, 4) != "RIFF" || std::string(header + 8, 4) != "WAVE") {
        return false;
    }
    
    // Extract format information
    uint16_t channels = *reinterpret_cast<uint16_t*>(header + 22);
    uint32_t sample_rate = *reinterpret_cast<uint32_t*>(header + 24);
    uint16_t bits_per_sample = *reinterpret_cast<uint16_t*>(header + 34);
    uint32_t data_size = *reinterpret_cast<uint32_t*>(header + 40);
    
    audio_data.sample_rate = sample_rate;
    audio_data.channels = channels;
    audio_data.bits_per_sample = bits_per_sample;
    
    // Read audio data
    size_t num_samples = data_size / (bits_per_sample / 8);
    audio_data.samples.resize(num_samples);
    
    if (bits_per_sample == 16) {
        std::vector<int16_t> raw_samples(num_samples);
        file.read(reinterpret_cast<char*>(raw_samples.data()), data_size);
        
        // Convert to float [-1.0, 1.0]
        for (size_t i = 0; i < num_samples; ++i) {
            audio_data.samples[i] = static_cast<float>(raw_samples[i]) / 32768.0f;
        }
    }
    
    return true;
}

ComparisonResult AudioComparator::compare_audio_files(const std::string& file1, 
                                                     const std::string& file2) {
    ComparisonResult result;
    result.files_comparable = false;
    result.similarity_score = 0.0;
    result.snr_db = -std::numeric_limits<double>::infinity();
    
    AudioData audio1, audio2;
    
    if (!load_audio_file(file1, audio1) || !load_audio_file(file2, audio2)) {
        result.error_message = "Failed to load one or more audio files";
        return result;
    }
    
    if (audio1.sample_rate != audio2.sample_rate) {
        result.error_message = "Sample rate mismatch";
        return result;
    }
    
    if (audio1.channels != audio2.channels) {
        result.error_message = "Channel count mismatch"; 
        return result;
    }
    
    // Align lengths (pad shorter with zeros or truncate longer)
    size_t min_length = std::min(audio1.samples.size(), audio2.samples.size());
    if (audio1.samples.size() != audio2.samples.size()) {
        result.length_difference = static_cast<int>(audio1.samples.size()) - static_cast<int>(audio2.samples.size());
        audio1.samples.resize(min_length);
        audio2.samples.resize(min_length);
    }
    
    result.files_comparable = true;
    result.sample_count = min_length;
    
    // Calculate SNR
    result.snr_db = calculate_snr(audio1.samples, audio2.samples);
    
    // Calculate cross-correlation for similarity
    result.similarity_score = calculate_similarity(audio1.samples, audio2.samples);
    
    // Calculate RMS difference
    result.rms_difference = calculate_rms_difference(audio1.samples, audio2.samples);
    
    // Calculate spectral metrics
    calculate_spectral_metrics(audio1, audio2, result);
    
    return result;
}

double AudioComparator::calculate_snr(const std::vector<float>& signal1, 
                                     const std::vector<float>& signal2) {
    if (signal1.size() != signal2.size() || signal1.empty()) {
        return -std::numeric_limits<double>::infinity();
    }
    
    double signal_power = 0.0;
    double noise_power = 0.0;
    
    for (size_t i = 0; i < signal1.size(); ++i) {
        double signal = static_cast<double>(signal1[i]);
        double noise = static_cast<double>(signal1[i] - signal2[i]);
        
        signal_power += signal * signal;
        noise_power += noise * noise;
    }
    
    if (noise_power == 0.0) {
        return std::numeric_limits<double>::infinity(); // Perfect match
    }
    
    signal_power /= signal1.size();
    noise_power /= signal1.size();
    
    return 10.0 * std::log10(signal_power / noise_power);
}

double AudioComparator::calculate_similarity(const std::vector<float>& signal1,
                                           const std::vector<float>& signal2) {
    if (signal1.size() != signal2.size() || signal1.empty()) {
        return 0.0;
    }
    
    // Calculate normalized cross-correlation at zero lag
    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    
    for (size_t i = 0; i < signal1.size(); ++i) {
        double x = static_cast<double>(signal1[i]);
        double y = static_cast<double>(signal2[i]);
        
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }
    
    if (sum_x2 == 0.0 || sum_y2 == 0.0) {
        return 0.0;
    }
    
    return sum_xy / std::sqrt(sum_x2 * sum_y2);
}

double AudioComparator::calculate_rms_difference(const std::vector<float>& signal1,
                                               const std::vector<float>& signal2) {
    if (signal1.size() != signal2.size() || signal1.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    
    double sum_diff_squared = 0.0;
    
    for (size_t i = 0; i < signal1.size(); ++i) {
        double diff = static_cast<double>(signal1[i] - signal2[i]);
        sum_diff_squared += diff * diff;
    }
    
    return std::sqrt(sum_diff_squared / signal1.size());
}

void AudioComparator::calculate_spectral_metrics(const AudioData& audio1,
                                                const AudioData& audio2,
                                                ComparisonResult& result) {
    // Simplified spectral analysis using basic frequency domain calculations
    // In a full implementation, this would use FFT for proper spectral analysis
    
    // Calculate spectral centroid approximation
    result.spectral_centroid_diff = 0.0; // Placeholder
    result.spectral_rolloff_diff = 0.0;  // Placeholder
    result.spectral_bandwidth_diff = 0.0; // Placeholder
    
    // For now, set reasonable defaults
    result.frequency_response_similarity = result.similarity_score;
}

QualityMetrics AudioComparator::analyze_audio_quality(const std::string& file_path) {
    QualityMetrics metrics;
    metrics.is_valid = false;
    
    AudioData audio;
    if (!load_audio_file(file_path, audio)) {
        return metrics;
    }
    
    metrics.is_valid = true;
    metrics.duration_seconds = static_cast<double>(audio.samples.size()) / 
                              (audio.sample_rate * audio.channels);
    metrics.sample_rate = audio.sample_rate;
    metrics.channels = audio.channels;
    metrics.bits_per_sample = audio.bits_per_sample;
    
    // Calculate RMS level
    double sum_squares = 0.0;
    for (float sample : audio.samples) {
        sum_squares += static_cast<double>(sample * sample);
    }
    metrics.rms_level = std::sqrt(sum_squares / audio.samples.size());
    
    // Calculate peak level
    float max_abs = 0.0f;
    for (float sample : audio.samples) {
        max_abs = std::max(max_abs, std::abs(sample));
    }
    metrics.peak_level = static_cast<double>(max_abs);
    
    // Calculate dynamic range approximation
    metrics.dynamic_range_db = 20.0 * std::log10(metrics.peak_level / (metrics.rms_level + 1e-10));
    
    // Estimate THD+N (simplified)
    metrics.thd_n_db = -40.0; // Placeholder - would need proper harmonic analysis
    
    // Estimate noise floor
    metrics.noise_floor_db = -60.0; // Placeholder - would need silence detection
    
    return metrics;
}

bool AudioComparator::meets_quality_threshold(const ComparisonResult& result,
                                             const QualityThreshold& threshold) {
    if (!result.files_comparable) {
        return false;
    }
    
    // Check SNR threshold
    if (result.snr_db < threshold.min_snr_db) {
        return false;
    }
    
    // Check similarity threshold
    if (result.similarity_score < threshold.min_similarity) {
        return false;
    }
    
    // Check RMS difference threshold
    if (result.rms_difference > threshold.max_rms_difference) {
        return false;
    }
    
    // Check length difference
    if (std::abs(result.length_difference) > threshold.max_length_difference_samples) {
        return false;
    }
    
    return true;
}

} // namespace integration_test
} // namespace nexussynth
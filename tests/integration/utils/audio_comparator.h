#pragma once

#include <string>
#include <vector>
#include <limits>

namespace nexussynth {
namespace integration_test {

    /**
     * @brief Audio data structure for comparison
     */
    struct AudioData {
        std::vector<float> samples;
        uint32_t sample_rate;
        uint16_t channels;
        uint16_t bits_per_sample;
    };

    /**
     * @brief Audio comparison result
     */
    struct ComparisonResult {
        bool files_comparable;
        double similarity_score;
        double snr_db;
        double rms_difference;
        int length_difference; // samples
        size_t sample_count;
        
        // Spectral analysis
        double spectral_centroid_diff;
        double spectral_rolloff_diff;
        double spectral_bandwidth_diff;
        double frequency_response_similarity;
        
        std::string error_message;
    };

    /**
     * @brief Quality metrics for single audio file
     */
    struct QualityMetrics {
        bool is_valid;
        double duration_seconds;
        uint32_t sample_rate;
        uint16_t channels;
        uint16_t bits_per_sample;
        double rms_level;
        double peak_level;
        double dynamic_range_db;
        double thd_n_db;
        double noise_floor_db;
    };

    /**
     * @brief Quality threshold definition
     */
    struct QualityThreshold {
        double min_snr_db = 20.0;
        double min_similarity = 0.85;
        double max_rms_difference = 0.1;
        int max_length_difference_samples = 1000;
    };

    /**
     * @brief Audio file comparator for quality analysis
     */
    class AudioComparator {
    public:
        AudioComparator();
        ~AudioComparator();

        // File comparison
        ComparisonResult compare_audio_files(const std::string& file1, const std::string& file2);
        
        // Quality analysis  
        QualityMetrics analyze_audio_quality(const std::string& file_path);
        
        // Threshold checking
        bool meets_quality_threshold(const ComparisonResult& result, const QualityThreshold& threshold);

    private:
        bool load_audio_file(const std::string& file_path, AudioData& audio_data);
        double calculate_snr(const std::vector<float>& signal1, const std::vector<float>& signal2);
        double calculate_similarity(const std::vector<float>& signal1, const std::vector<float>& signal2);
        double calculate_rms_difference(const std::vector<float>& signal1, const std::vector<float>& signal2);
        void calculate_spectral_metrics(const AudioData& audio1, const AudioData& audio2, ComparisonResult& result);
    };

} // namespace integration_test
} // namespace nexussynth
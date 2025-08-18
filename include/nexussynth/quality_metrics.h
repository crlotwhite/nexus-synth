#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>

namespace NexusSynth {

struct QualityMetrics {
    double mcd_score = 0.0;
    double f0_rmse = 0.0;
    double spectral_correlation = 0.0;
    double total_frames = 0;
    double valid_frames = 0;
    
    bool is_valid() const {
        return total_frames > 0 && valid_frames > 0;
    }
    
    double frame_validity_ratio() const {
        return total_frames > 0 ? valid_frames / total_frames : 0.0;
    }
};

struct ValidationReport {
    QualityMetrics overall_metrics;
    std::map<std::string, QualityMetrics> per_phoneme_metrics;
    std::vector<double> frame_mcd_scores;
    std::vector<double> frame_f0_errors;
    std::string report_timestamp;
    std::string model_version;
    
    void save_to_json(const std::string& filepath) const;
    bool load_from_json(const std::string& filepath);
};

class QualityEvaluator {
public:
    QualityEvaluator();
    ~QualityEvaluator();

    // MCD 계산 (Mel-Cepstral Distortion)
    double calculate_mcd(
        const std::vector<std::vector<double>>& reference_mfcc,
        const std::vector<std::vector<double>>& synthesized_mfcc,
        int ignore_c0 = 1  // C0 계수 제외 여부
    );
    
    // F0 RMSE 계산
    double calculate_f0_rmse(
        const std::vector<double>& reference_f0,
        const std::vector<double>& synthesized_f0,
        double unvoiced_threshold = 50.0
    );
    
    // 스펙트럼 상관관계 계산
    double calculate_spectral_correlation(
        const std::vector<std::vector<double>>& reference_spectrum,
        const std::vector<std::vector<double>>& synthesized_spectrum
    );
    
    // 종합적인 품질 평가
    QualityMetrics evaluate_synthesis_quality(
        const std::vector<std::vector<double>>& reference_mfcc,
        const std::vector<std::vector<double>>& synthesized_mfcc,
        const std::vector<double>& reference_f0,
        const std::vector<double>& synthesized_f0
    );
    
    // 검증 세트 기반 종합 평가
    ValidationReport run_validation_suite(
        const std::string& reference_dir,
        const std::string& synthesized_dir,
        const std::string& phoneme_alignment_file = ""
    );

private:
    // MFCC 추출 (WORLD SP 파라미터로부터)
    std::vector<std::vector<double>> extract_mfcc_from_spectrum(
        const std::vector<std::vector<double>>& spectrum,
        int num_coefficients = 13,
        double sample_rate = 16000.0
    );
    
    // 프레임 정렬 및 길이 조정
    void align_sequences(
        std::vector<std::vector<double>>& seq1,
        std::vector<std::vector<double>>& seq2
    );
    
    void align_f0_sequences(
        std::vector<double>& f0_1,
        std::vector<double>& f0_2
    );
    
    // DCT 변환 (MFCC 계산용)
    std::vector<double> compute_dct(const std::vector<double>& log_spectrum, int num_coefficients);
    
    // 유성음/무성음 구분
    bool is_voiced_frame(double f0_value, double threshold = 50.0);
    
    // 통계 계산 헬퍼
    double compute_mean(const std::vector<double>& values);
    double compute_std(const std::vector<double>& values);
    double compute_correlation(const std::vector<double>& x, const std::vector<double>& y);
};

// 유틸리티 함수들
namespace QualityUtils {
    // 오디오 파일에서 특징 추출
    std::vector<std::vector<double>> extract_features_from_wav(
        const std::string& wav_filepath,
        const std::string& feature_type = "mfcc"  // "mfcc", "spectrum", "f0"
    );
    
    // 품질 메트릭 시각화 (CSV 출력)
    void export_metrics_to_csv(
        const ValidationReport& report,
        const std::string& output_filepath
    );
    
    // 임계값 기반 품질 평가
    std::string assess_quality_level(const QualityMetrics& metrics);
}

} // namespace NexusSynth